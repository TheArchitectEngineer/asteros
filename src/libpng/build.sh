#!/bin/bash
# Cross-compiles real upstream libpng 1.6.58 (github.com/glennrp/libpng,
# tag v1.6.58) against this project's vendored zlib -- needed by
# freetype2 (embedded-bitmap PNG glyphs) and, later, gdk-pixbuf's PNG
# loader / cairo's PNG surface. Ships a committed `configure` already
# (no autogen needed).
#
# Installed into build/gtk-deps-install (see src/expat/build.sh's
# comment for why this is a separate prefix from xorg-deps-install).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang -std=gnu23"
PREFIX="$ROOT/build/gtk-deps-install"
ZLIB_PREFIX="$ROOT/build/xorg-deps-install"

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"

./configure --host=x86_64-apple-darwin19 --prefix="$PREFIX" \
	CC="$CLANG" \
	CPPFLAGS="-I$ZLIB_PREFIX/include" LDFLAGS="-L$ZLIB_PREFIX/lib" \
	--disable-shared --enable-static --disable-tests --disable-tools

make -j"$(sysctl -n hw.ncpu)"
make install

echo "installed: $PREFIX/lib/libpng16.a, $PREFIX/include/png.h"
