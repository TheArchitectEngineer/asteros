#!/bin/bash
# Builds sctest: a normal dynamically-linked executable exercising the
# real SCDynamicStoreCreate/SetValue/CopyValue/RemoveValue round trip
# plus a real SetNotificationKeys+NotifyFileDescriptor async wakeup
# against libSystemConfiguration.dylib + libCoreFoundation.dylib +
# libSystem.B.dylib -- same pattern as userland/Security/test/build.sh.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../../.. && pwd)"

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/SystemConfiguration_obj"
mkdir -p "$OUT"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -nostdlibinc -I "$ROOT/userland/SystemConfiguration/include"
        -I "$ROOT/userland/CoreFoundation/include" -I "$ROOT/userland/libdispatch/include"
        -isystem "$ROOT/userland/libc/include" -O1 -g -Wall -Wextra -Wno-unused-parameter)

"$CLANG" "${CFLAGS[@]}" -c sctest.c -o "$OUT/sctest_main.o"

CRT0="$ROOT/build/libc_obj/crt0.o"
LIBC_START="$ROOT/build/libc_obj/libc_start.o"
LIBSYSTEM="$ROOT/build/libSystem_obj/libSystem.B.dylib"
LIBCF="$ROOT/build/corefoundation_obj/libCoreFoundation.dylib"
LIBSC="$OUT/libSystemConfiguration.dylib"

if [ ! -f "$LIBSC" ]; then
	echo "error: $LIBSC not found -- build userland/SystemConfiguration first" >&2
	exit 1
fi

"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -Wl,-no_pie -Wl,-bind_at_load -e _start \
	"$CRT0" "$LIBC_START" "$OUT/sctest_main.o" "$LIBSC" "$LIBCF" "$LIBSYSTEM" -o "$OUT/sctest"

echo "built: $OUT/sctest"
file "$OUT/sctest"
otool -l "$OUT/sctest" | grep -A2 LC_LOAD_DYLIB
