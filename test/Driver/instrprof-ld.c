// Test instrumented profiling ld flags.
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target i386-unknown-linux -fprofile-instr-generate \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-LINUX-I386 %s
//
// CHECK-LINUX-I386: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-LINUX-I386: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}linux{{/|\\\\}}libclang_rt.profile-i386.a" {{.*}} "-lc"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target x86_64-unknown-linux -fprofile-instr-generate \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-LINUX-X86-64 %s
//
// CHECK-LINUX-X86-64: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-LINUX-X86-64: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}linux{{/|\\\\}}libclang_rt.profile-x86_64.a" {{.*}} "-lc"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target x86_64-unknown-linux -fprofile-instr-generate -nostdlib \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-LINUX-NOSTDLIB-X86-64 %s
//
// CHECK-LINUX-NOSTDLIB-X86-64: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-LINUX-NOSTDLIB-X86-64: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}linux{{/|\\\\}}libclang_rt.profile-x86_64.a"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target x86_64-unknown-freebsd -fprofile-instr-generate \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_freebsd64_tree \
// RUN:   | FileCheck --check-prefix=CHECK-FREEBSD-X86-64 %s
//
// CHECK-FREEBSD-X86-64: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-FREEBSD-X86-64: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}freebsd{{/|\\\\}}libclang_rt.profile-x86_64.a"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -shared \
// RUN:     -target i386-unknown-linux -fprofile-instr-generate \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-LINUX-I386-SHARED %s
//
// CHECK-LINUX-I386-SHARED: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-LINUX-I386-SHARED: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}linux{{/|\\\\}}libclang_rt.profile-i386.a" {{.*}} "-lc"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -shared \
// RUN:     -target x86_64-unknown-linux -fprofile-instr-generate \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_linux_tree \
// RUN:   | FileCheck --check-prefix=CHECK-LINUX-X86-64-SHARED %s
//
// CHECK-LINUX-X86-64-SHARED: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-LINUX-X86-64-SHARED: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}linux{{/|\\\\}}libclang_rt.profile-x86_64.a" {{.*}} "-lc"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -shared \
// RUN:     -target x86_64-unknown-freebsd -fprofile-instr-generate \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:     --sysroot=%S/Inputs/basic_freebsd64_tree \
// RUN:   | FileCheck --check-prefix=CHECK-FREEBSD-X86-64-SHARED %s
//
// CHECK-FREEBSD-X86-64-SHARED: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-FREEBSD-X86-64-SHARED: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}freebsd{{/|\\\\}}libclang_rt.profile-x86_64.a"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target x86_64-apple-darwin14 -fprofile-instr-generate \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:   | FileCheck --check-prefix=CHECK-DARWIN-X86-64 %s
//
// CHECK-DARWIN-X86-64: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-DARWIN-X86-64: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}darwin{{/|\\\\}}libclang_rt.profile_osx.a"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target x86_64-apple-darwin14 -fprofile-instr-generate -nostdlib \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:   | FileCheck --check-prefix=CHECK-DARWIN-NOSTDLIB-X86-64 %s
//
// CHECK-DARWIN-NOSTDLIB-X86-64: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-DARWIN-NOSTDLIB-X86-64: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}darwin{{/|\\\\}}libclang_rt.profile_osx.a"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target arm64-apple-ios -fprofile-instr-generate \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:   | FileCheck --check-prefix=CHECK-DARWIN-ARM64 %s
//
// CHECK-DARWIN-ARM64: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-DARWIN-ARM64: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}darwin{{/|\\\\}}libclang_rt.profile_ios.a"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target armv7-apple-darwin -mtvos-version-min=8.3 -fprofile-instr-generate \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:   | FileCheck --check-prefix=CHECK-TVOS-ARMV7 %s
//
// CHECK-TVOS-ARMV7: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-TVOS-ARMV7: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}darwin{{/|\\\\}}libclang_rt.profile_tvos.a"
//
// RUN: %clang -no-canonical-prefixes %s -### -o %t.o 2>&1 \
// RUN:     -target armv7s-apple-darwin10 -mwatchos-version-min=2.0 -arch armv7k -fprofile-instr-generate \
// RUN:     -resource-dir=%S/Inputs/resource_dir \
// RUN:   | FileCheck --check-prefix=CHECK-WATCHOS-ARMV7 %s
//
// CHECK-WATCHOS-ARMV7: "{{(.*[^-.0-9A-Z_a-z])?}}ld{{(.exe)?}}"
// CHECK-WATCHOS-ARMV7: "{{.*}}/Inputs/resource_dir{{/|\\\\}}lib{{/|\\\\}}darwin{{/|\\\\}}libclang_rt.profile_watchos.a"
