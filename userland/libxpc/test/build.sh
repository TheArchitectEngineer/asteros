#!/bin/bash
# Builds xpctest: a real client<->service xpc_connection_t round trip
# across a fork()'d pair of processes, plus an in-process object-model
# self-check -- against real libxpc.dylib + libdispatch.dylib +
# libSystem.B.dylib. Same pattern as userland/Security/test/build.sh.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../../.. && pwd)"

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/xpc_obj"
mkdir -p "$OUT"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -fblocks -nostdlibinc -I "$ROOT/userland/libxpc/include"
        -I "$ROOT/userland/libdispatch/include"
        -isystem "$ROOT/userland/libc/include" -O1 -g -Wall -Wextra -Wno-unused-parameter)

"$CLANG" "${CFLAGS[@]}" -c xpctest.c -o "$OUT/xpctest_main.o"

CRT0="$ROOT/build/libc_obj/crt0.o"
LIBC_START="$ROOT/build/libc_obj/libc_start.o"
LIBSYSTEM="$ROOT/build/libSystem_obj/libSystem.B.dylib"
LIBDISPATCH="$ROOT/build/dispatch_obj/libdispatch.dylib"
LIBXPC="$OUT/libxpc.dylib"

"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -Wl,-no_pie -Wl,-bind_at_load -e _start \
	"$CRT0" "$LIBC_START" "$OUT/xpctest_main.o" "$LIBXPC" "$LIBDISPATCH" "$LIBSYSTEM" -o "$OUT/xpctest"

echo "built: $OUT/xpctest"
file "$OUT/xpctest"
otool -l "$OUT/xpctest" | grep -A2 LC_LOAD_DYLIB
