#!/bin/bash
# Builds rendertest, the CreateGlyphSet-isolation diagnostic client
# (see rendertest.c's own comment). Same recipe as xfttest/build.sh.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang"
X11_DEPS="$ROOT/build/xorg-deps-install"
OUT="$ROOT/build/xfttest_obj"
mkdir -p "$OUT"

"$CLANG" -std=gnu23 -O0 -g -Wall \
	-I "$X11_DEPS/include" \
	-c rendertest.c -o "$OUT/rendertest.o"

"$CLANG" -std=gnu23 -O0 -g \
	"$OUT/rendertest.o" \
	-L "$X11_DEPS/lib" \
	-lXrender -lX11 -lxcb -lXau -lXdmcp -lm \
	-o "$OUT/rendertest"

echo "linked: $OUT/rendertest"
file "$OUT/rendertest"

DESTDIR="$ROOT/build/xorg-target-root"
install -d "$DESTDIR/bin"
install "$OUT/rendertest" "$DESTDIR/bin/rendertest"
echo "installed: $DESTDIR/bin/rendertest"
