#!/bin/bash
# Cross-compiles real upstream FreeType 2.13.3
# (gitlab.freedesktop.org/freetype/freetype, tag VER-2-13-3) against
# this project's vendored zlib and the freshly-built libpng, using its
# committed autotools `configure` (upstream's own Meson support is
# still marked experimental; 2.13.x's autotools/builds/unix path is
# the proven, actively-used build for this version).
#
# freetype's own build unconditionally compiles a small internal `dlg`
# logging library (github.com/nyorain/dlg) via a git submodule
# upstream normally checks out at build time; vendored directly here
# instead (include/dlg/{dlg,output}.h + src/dlg/dlg.c, MIT-licensed)
# since this tree has no .git in src/freetype2 for `git submodule
# update` to work against.
#
# harfbuzz is explicitly disabled here (--with-harfbuzz=no) -- it
# isn't vendored until a later phase of the GTK port, and freetype's
# harfbuzz integration is a real, but optional, circular dependency
# (harfbuzz can also use freetype). Documented gap: no harfbuzz-
# assisted autohinting until that phase lands.
#
# Installed into build/gtk-deps-install (see src/expat/build.sh's
# comment for why this is a separate prefix from xorg-deps-install).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang -std=gnu23"
PREFIX="$ROOT/build/gtk-deps-install"
ZLIB_PREFIX="$ROOT/build/xorg-deps-install"

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$ZLIB_PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

./configure --host=x86_64-apple-darwin19 --prefix="$PREFIX" \
	CC="$CLANG" \
	CPPFLAGS="-I$ZLIB_PREFIX/include" LDFLAGS="-L$ZLIB_PREFIX/lib" \
	--disable-shared --enable-static \
	--with-zlib=yes --with-png=yes --with-harfbuzz=no \
	--with-bzip2=no --with-brotli=no

make -j"$(sysctl -n hw.ncpu)"
make install

echo "installed: $PREFIX/lib/libfreetype.a, $PREFIX/include/freetype2/ft2build.h"
