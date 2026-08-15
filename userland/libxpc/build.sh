#!/bin/bash
# Builds libxpc.dylib: v1-scoped libxpc (real xpc_object_t dictionary/
# array/scalars, a real recursive TLV wire format, and real
# xpc_connection_t over this tree's Mach IPC + libdispatch -- see
# include/xpc/xpc.h's header comment for exact scope) on top of
# libdispatch.dylib (Block_copy'd event handlers, dispatch_async_f
# delivery) + the real libSystem.B.dylib -- same dependency-chain pattern
# as Security's build (libCoreFoundation.dylib + libSystem.B.dylib).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"
LIBSYSTEM="$ROOT/build/libSystem_obj/libSystem.B.dylib"
LIBDISPATCH="$ROOT/build/dispatch_obj/libdispatch.dylib"

if [ ! -f "$LIBSYSTEM" ]; then
	echo "error: $LIBSYSTEM not found -- build userland/libSystem first" >&2
	exit 1
fi
if [ ! -f "$LIBDISPATCH" ]; then
	echo "error: $LIBDISPATCH not found -- build userland/libdispatch first" >&2
	exit 1
fi

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/xpc_obj"
mkdir -p "$OUT"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -fPIC -fblocks -nostdlibinc
        -I "$PWD" -I "$PWD/include" -I "$ROOT/userland/libdispatch/include"
        -isystem "$ROOT/userland/libc/include"
        -O1 -g -Wall -Wextra -Wno-unused-parameter -std=gnu11)

OBJS=()
for f in xpc_object xpc_array xpc_dictionary xpc_serialize xpc_connection; do
	"$CLANG" "${CFLAGS[@]}" -c "$f.c" -o "$OUT/$f.o"
	OBJS+=("$OUT/$f.o")
done

# -bind_at_load: same host ld64 lazy-stub crash as every other dylib in
# this tree (userland/dyld/test/build.sh) -- no real dyld_stub_binder yet.
"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -dynamiclib -Wl,-bind_at_load \
	-install_name /usr/lib/libxpc.dylib \
	"${OBJS[@]}" "$LIBDISPATCH" "$LIBSYSTEM" -o "$OUT/libxpc.dylib"

echo "built: $OUT/libxpc.dylib"
file "$OUT/libxpc.dylib"
otool -l "$OUT/libxpc.dylib" | grep -A2 LC_LOAD_DYLIB
