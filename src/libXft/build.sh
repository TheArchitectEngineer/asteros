#!/bin/bash
# Cross-compiles real upstream libXft 2.3.4
# (gitlab.freedesktop.org/xorg/lib/libxft, tag libXft-2.3.4) against
# the freshly-built freetype2/fontconfig (build/gtk-deps-install) and
# this project's vendored X11 stack + real libXrender
# (build/xorg-deps-install) -- the final piece of the real font-
# rendering stack this GTK-port phase builds, replacing src/xft-stub
# for anything linked against this prefix (xft-stub itself is left
# alone for existing consumers -- see src/xft-stub/build.sh).
#
# `configure` was pre-generated the same way as libXrender's (see that
# build.sh's comment for the exact autogen.sh invocation).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang -std=gnu23"
PREFIX="$ROOT/build/gtk-deps-install"
X11_PREFIX="$ROOT/build/xorg-deps-install"

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$X11_PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

./configure --host=x86_64-apple-darwin19 --prefix="$PREFIX" \
	CC="$CLANG" \
	CFLAGS="-I$X11_PREFIX/include" LDFLAGS="-L$X11_PREFIX/lib" \
	--disable-shared --enable-static

make -j"$(sysctl -n hw.ncpu)"
make install

echo "installed: $PREFIX/lib/libXft.a, $PREFIX/include/X11/Xft/Xft.h"
