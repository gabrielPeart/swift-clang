//===--- SemaCoroutines.cpp - Semantic Analysis for Coroutines ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for C++ Coroutines.
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/SemaInternal.h"
#include "clang/AST/Decl.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/StmtCXX.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Overload.h"
using namespace clang;
using namespace sema;

/// Look up the std::coroutine_traits<...>::promise_type for the given
/// function type.
static QualType lookupPromiseType(Sema &S, const FunctionProtoType *FnType,
                                  SourceLocation Loc) {
  // FIXME: Cache std::coroutine_traits once we've found it.
  NamespaceDecl *Std = S.getStdNamespace();
  if (!Std) {
    S.Diag(Loc, diag::err_implied_std_coroutine_traits_not_found);
    return QualType();
  }

  LookupResult Result(S, &S.PP.getIdentifierTable().get("coroutine_traits"),
                      Loc, Sema::LookupOrdinaryName);
  if (!S.LookupQualifiedName(Result, Std)) {
    S.Diag(Loc, diag::err_implied_std_coroutine_traits_not_found);
    return QualType();
  }

  ClassTemplateDecl *CoroTraits = Result.getAsSingle<ClassTemplateDecl>();
  if (!CoroTraits) {
    Result.suppressDiagnostics();
    // We found something weird. Complain about the first thing we found.
    NamedDecl *Found = *Result.begin();
    S.Diag(Found->getLocation(), diag::err_malformed_std_coroutine_traits);
    return QualType();
  }

  // Form template argument list for coroutine_traits<R, P1, P2, ...>.
  TemplateArgumentListInfo Args(Loc, Loc);
  Args.addArgument(TemplateArgumentLoc(
      TemplateArgument(FnType->getReturnType()),
      S.Context.getTrivialTypeSourceInfo(FnType->getReturnType(), Loc)));
  for (QualType T : FnType->getParamTypes())
    Args.addArgument(TemplateArgumentLoc(
        TemplateArgument(T), S.Context.getTrivialTypeSourceInfo(T, Loc)));

  // Build the template-id.
  QualType CoroTrait =
      S.CheckTemplateIdType(TemplateName(CoroTraits), Loc, Args);
  if (CoroTrait.isNull())
    return QualType();
  if (S.RequireCompleteType(Loc, CoroTrait,
                            diag::err_coroutine_traits_missing_specialization))
    return QualType();

  CXXRecordDecl *RD = CoroTrait->getAsCXXRecordDecl();
  assert(RD && "specialization of class template is not a class?");

  // Look up the ::promise_type member.
  LookupResult R(S, &S.PP.getIdentifierTable().get("promise_type"), Loc,
                 Sema::LookupOrdinaryName);
  S.LookupQualifiedName(R, RD);
  auto *Promise = R.getAsSingle<TypeDecl>();
  if (!Promise) {
    S.Diag(Loc, diag::err_implied_std_coroutine_traits_promise_type_not_found)
      << RD;
    return QualType();
  }

  // The promise type is required to be a class type.
  QualType PromiseType = S.Context.getTypeDeclType(Promise);
  if (!PromiseType->getAsCXXRecordDecl()) {
    S.Diag(Loc, diag::err_implied_std_coroutine_traits_promise_type_not_class)
      << PromiseType;
    return QualType();
  }

  return PromiseType;
}

/// Check that this is a context in which a coroutine suspension can appear.
static FunctionScopeInfo *
checkCoroutineContext(Sema &S, SourceLocation Loc, StringRef Keyword) {
  // 'co_await' and 'co_yield' are permitted in unevaluated operands.
  // FIXME: Not in 'noexcept'.
  if (S.isUnevaluatedContext())
    return nullptr;

  // Any other usage must be within a function.
  auto *FD = dyn_cast<FunctionDecl>(S.CurContext);
  if (!FD) {
    S.Diag(Loc, isa<ObjCMethodDecl>(S.CurContext)
                    ? diag::err_coroutine_objc_method
                    : diag::err_coroutine_outside_function) << Keyword;
  } else if (isa<CXXConstructorDecl>(FD) || isa<CXXDestructorDecl>(FD)) {
    // Coroutines TS [special]/6:
    //   A special member function shall not be a coroutine.
    //
    // FIXME: We assume that this really means that a coroutine cannot
    //        be a constructor or destructor.
    S.Diag(Loc, diag::err_coroutine_ctor_dtor)
      << isa<CXXDestructorDecl>(FD) << Keyword;
  } else if (FD->isConstexpr()) {
    S.Diag(Loc, diag::err_coroutine_constexpr) << Keyword;
  } else if (FD->isVariadic()) {
    S.Diag(Loc, diag::err_coroutine_varargs) << Keyword;
  } else {
    auto *ScopeInfo = S.getCurFunction();
    assert(ScopeInfo && "missing function scope for function");

    // If we don't have a promise variable, build one now.
    if (!ScopeInfo->CoroutinePromise && !FD->getType()->isDependentType()) {
      QualType T =
          lookupPromiseType(S, FD->getType()->castAs<FunctionProtoType>(), Loc);
      if (T.isNull())
        return nullptr;

      // Create and default-initialize the promise.
      ScopeInfo->CoroutinePromise =
          VarDecl::Create(S.Context, FD, FD->getLocation(), FD->getLocation(),
                          &S.PP.getIdentifierTable().get("__promise"), T,
                          S.Context.getTrivialTypeSourceInfo(T, Loc), SC_None);
      S.CheckVariableDeclarationType(ScopeInfo->CoroutinePromise);
      if (!ScopeInfo->CoroutinePromise->isInvalidDecl())
        S.ActOnUninitializedDecl(ScopeInfo->CoroutinePromise, false);
    }

    return ScopeInfo;
  }

  return nullptr;
}

/// Build a call to 'operator co_await' if there is a suitable operator for
/// the given expression.
static ExprResult buildOperatorCoawaitCall(Sema &SemaRef, Scope *S,
                                           SourceLocation Loc, Expr *E) {
  UnresolvedSet<16> Functions;
  SemaRef.LookupOverloadedOperatorName(OO_Coawait, S, E->getType(), QualType(),
                                       Functions);
  return SemaRef.CreateOverloadedUnaryOp(Loc, UO_Coawait, Functions, E);
}

struct ReadySuspendResumeResult {
  bool IsInvalid;
  Expr *Results[3];
};

/// Build calls to await_ready, await_suspend, and await_resume for a co_await
/// expression.
static ReadySuspendResumeResult buildCoawaitCalls(Sema &S, SourceLocation Loc,
                                                  Expr *E) {
  // Assume invalid until we see otherwise.
  ReadySuspendResumeResult Calls = {true, {}};

  const StringRef Funcs[] = {"await_ready", "await_suspend", "await_resume"};
  for (size_t I = 0, N = llvm::array_lengthof(Funcs); I != N; ++I) {
    DeclarationNameInfo NameInfo(&S.PP.getIdentifierTable().get(Funcs[I]), Loc);

    Expr *Operand = new (S.Context) OpaqueValueExpr(
        Loc, E->getType(), E->getValueKind(), E->getObjectKind(), E);

    // FIXME: Fix BuildMemberReferenceExpr to take a const CXXScopeSpec&.
    CXXScopeSpec SS;
    ExprResult Result = S.BuildMemberReferenceExpr(
        Operand, Operand->getType(), Loc, /*IsPtr=*/false, SS,
        SourceLocation(), nullptr, NameInfo, /*TemplateArgs=*/nullptr,
        /*Scope=*/nullptr);
    if (Result.isInvalid())
      return Calls;

    // FIXME: Pass coroutine handle to await_suspend.
    Result = S.ActOnCallExpr(nullptr, Result.get(), Loc, None, Loc, nullptr);
    if (Result.isInvalid())
      return Calls;
    Calls.Results[I] = Result.get();
  }

  Calls.IsInvalid = false;
  return Calls;
}

ExprResult Sema::ActOnCoawaitExpr(Scope *S, SourceLocation Loc, Expr *E) {
  ExprResult Awaitable = buildOperatorCoawaitCall(*this, S, Loc, E);
  if (Awaitable.isInvalid())
    return ExprError();
  return BuildCoawaitExpr(Loc, Awaitable.get());
}
ExprResult Sema::BuildCoawaitExpr(SourceLocation Loc, Expr *E) {
  auto *Coroutine = checkCoroutineContext(*this, Loc, "co_await");

  if (E->getType()->isDependentType()) {
    Expr *Res = new (Context) CoawaitExpr(Loc, Context.DependentTy, E);
    if (Coroutine)
      Coroutine->CoroutineStmts.push_back(Res);
    return Res;
  }

  if (E->getType()->isPlaceholderType()) {
    ExprResult R = CheckPlaceholderExpr(E);
    if (R.isInvalid()) return ExprError();
    E = R.get();
  }

  // FIXME: If E is a prvalue, create a temporary.
  // FIXME: If E is an xvalue, convert to lvalue.

  // Build the await_ready, await_suspend, await_resume calls.
  ReadySuspendResumeResult RSS = buildCoawaitCalls(*this, Loc, E);
  if (RSS.IsInvalid)
    return ExprError();

  Expr *Res = new (Context) CoawaitExpr(Loc, E, RSS.Results[0], RSS.Results[1],
                                        RSS.Results[2]);
  if (Coroutine)
    Coroutine->CoroutineStmts.push_back(Res);
  return Res;
}

ExprResult Sema::ActOnCoyieldExpr(Scope *S, SourceLocation Loc, Expr *E) {
  // FIXME: Build yield_value call.
  ExprResult Awaitable = buildOperatorCoawaitCall(*this, S, Loc, E);
  if (Awaitable.isInvalid())
    return ExprError();
  return BuildCoyieldExpr(Loc, Awaitable.get());
}
ExprResult Sema::BuildCoyieldExpr(SourceLocation Loc, Expr *E) {
  auto *Coroutine = checkCoroutineContext(*this, Loc, "co_yield");

  // FIXME: Build await_* calls.
  Expr *Res = new (Context) CoyieldExpr(Loc, Context.VoidTy, E);
  if (Coroutine)
    Coroutine->CoroutineStmts.push_back(Res);
  return Res;
}

StmtResult Sema::ActOnCoreturnStmt(SourceLocation Loc, Expr *E) {
  return BuildCoreturnStmt(Loc, E);
}
StmtResult Sema::BuildCoreturnStmt(SourceLocation Loc, Expr *E) {
  auto *Coroutine = checkCoroutineContext(*this, Loc, "co_return");

  // FIXME: Build return_* calls.
  Stmt *Res = new (Context) CoreturnStmt(Loc, E);
  if (Coroutine)
    Coroutine->CoroutineStmts.push_back(Res);
  return Res;
}

void Sema::CheckCompletedCoroutineBody(FunctionDecl *FD, Stmt *Body) {
  FunctionScopeInfo *Fn = getCurFunction();
  assert(Fn && !Fn->CoroutineStmts.empty() && "not a coroutine");

  // Coroutines [stmt.return]p1:
  //   A return statement shall not appear in a coroutine.
  if (Fn->FirstReturnLoc.isValid()) {
    Diag(Fn->FirstReturnLoc, diag::err_return_in_coroutine);
    auto *First = Fn->CoroutineStmts[0];
    Diag(First->getLocStart(), diag::note_declared_coroutine_here)
      << (isa<CoawaitExpr>(First) ? 0 :
          isa<CoyieldExpr>(First) ? 1 : 2);
  }

  bool AnyCoawaits = false;
  bool AnyCoyields = false;
  for (auto *CoroutineStmt : Fn->CoroutineStmts) {
    AnyCoawaits |= isa<CoawaitExpr>(CoroutineStmt);
    AnyCoyields |= isa<CoyieldExpr>(CoroutineStmt);
  }

  if (!AnyCoawaits && !AnyCoyields)
    Diag(Fn->CoroutineStmts.front()->getLocStart(),
         diag::ext_coroutine_without_co_await_co_yield);

  // FIXME: Perform analysis of initial and final suspend,
  // and set_exception call.
}
