#!/bin/bash
# Builds securitytest: a normal dynamically-linked executable exercising
# SecRandomCopyBytes and the full SecItem lifecycle against
# libSecurity.dylib + libCoreFoundation.dylib + libSystem.B.dylib -- same
# pattern as userland/CoreFoundation/test/build.sh.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../../.. && pwd)"

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/security_obj"
mkdir -p "$OUT"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -nostdlibinc -I "$ROOT/userland/Security/include"
        -I "$ROOT/userland/CoreFoundation/include"
        -isystem "$ROOT/userland/libc/include" -O1 -g -Wall -Wextra -Wno-unused-parameter)

"$CLANG" "${CFLAGS[@]}" -c securitytest.c -o "$OUT/securitytest_main.o"

CRT0="$ROOT/build/libc_obj/crt0.o"
LIBC_START="$ROOT/build/libc_obj/libc_start.o"
LIBSYSTEM="$ROOT/build/libSystem_obj/libSystem.B.dylib"
LIBCF="$ROOT/build/corefoundation_obj/libCoreFoundation.dylib"
LIBSECURITY="$OUT/libSecurity.dylib"

"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -Wl,-no_pie -Wl,-bind_at_load -e _start \
	"$CRT0" "$LIBC_START" "$OUT/securitytest_main.o" "$LIBSECURITY" "$LIBCF" "$LIBSYSTEM" -o "$OUT/securitytest"

echo "built: $OUT/securitytest"
file "$OUT/securitytest"
otool -l "$OUT/securitytest" | grep -A2 LC_LOAD_DYLIB
