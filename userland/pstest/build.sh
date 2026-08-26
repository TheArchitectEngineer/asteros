#!/bin/bash
# Builds pstest: real /dev/psevent read() proof, static -nostdlib -e _start,
# no dyld -- same recipe as userland/fbtest/build.sh.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG=/Applications/Xcode-beta.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang
OUT="$ROOT/build/pstest"
mkdir -p "$OUT"

CFLAGS=(-target x86_64-apple-macos10.15 -ffreestanding -fno-stack-protector
        -fno-builtin -nostdlibinc -isystem "$ROOT/userland/libc/include"
        -O1 -g -Wall -Wextra)

"$CLANG" "${CFLAGS[@]}" -c pstest.c -o "$OUT/pstest.o"

"$CLANG" -target x86_64-apple-macos10.15 -nostdlib -static -e _start \
	"$OUT/pstest.o" "$ROOT"/build/libc_obj/*.o -o "$OUT/pstest"

echo "built: $OUT/pstest"
file "$OUT/pstest"
