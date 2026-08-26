#!/bin/bash
# Builds unixtest: real AF_UNIX two-process round trip, static -nostdlib
# -e _start, no dyld -- same recipe as userland/network_test/build.sh.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/unixtest"
mkdir -p "$OUT"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -nostdlibinc -isystem "$ROOT/userland/libc/include"
        -O1 -g -Wall -Wextra)

"$CLANG" "${CFLAGS[@]}" -c unixtest.c -o "$OUT/unixtest.o"

"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -static -e _start \
	"$OUT/unixtest.o" "$ROOT"/build/libc_obj/*.o -o "$OUT/unixtest"

echo "built: $OUT/unixtest"
file "$OUT/unixtest"
