#!/bin/bash
# Builds libSecurity.dylib: v1-scoped Security.framework (SecRandomCopyBytes
# on real kernel entropy, an in-memory kSecClassGenericPassword keychain --
# see SecItem.c's header comment for what's deliberately out of scope) on
# top of libCoreFoundation.dylib + the real libSystem.B.dylib -- same
# dependency pattern as Foundation's own build.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"
LIBSYSTEM="$ROOT/build/libSystem_obj/libSystem.B.dylib"
LIBCOREFOUNDATION="$ROOT/build/corefoundation_obj/libCoreFoundation.dylib"

if [ ! -f "$LIBSYSTEM" ]; then
	echo "error: $LIBSYSTEM not found -- build userland/libSystem first" >&2
	exit 1
fi
if [ ! -f "$LIBCOREFOUNDATION" ]; then
	echo "error: $LIBCOREFOUNDATION not found -- build userland/CoreFoundation first" >&2
	exit 1
fi

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/security_obj"
mkdir -p "$OUT"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -fPIC -nostdlibinc
        -I "$PWD/include" -I "$ROOT/userland/CoreFoundation/include"
        -isystem "$ROOT/userland/libc/include"
        -O1 -g -Wall -Wextra -Wno-unused-parameter -std=gnu11)

OBJS=()
for f in SecRandom SecItem; do
	"$CLANG" "${CFLAGS[@]}" -c "$f.c" -o "$OUT/$f.o"
	OBJS+=("$OUT/$f.o")
done

# -bind_at_load: same host ld64 lazy-stub crash as every other dylib in
# this tree (userland/dyld/test/build.sh) -- no real dyld_stub_binder yet.
"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -dynamiclib -Wl,-bind_at_load \
	-install_name /usr/lib/libSecurity.dylib \
	"${OBJS[@]}" "$LIBCOREFOUNDATION" "$LIBSYSTEM" -o "$OUT/libSecurity.dylib"

echo "built: $OUT/libSecurity.dylib"
file "$OUT/libSecurity.dylib"
otool -l "$OUT/libSecurity.dylib" | grep -A2 LC_LOAD_DYLIB
