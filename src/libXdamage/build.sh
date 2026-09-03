#!/bin/bash
# Cross-compiles real upstream libXdamage 1.1.7
# (gitlab.freedesktop.org/xorg/lib/libxdamage, tag libXdamage-1.1.7)
# against this project's vendored X11 stack, same autotools-via-cross-clang
# recipe as libXcomposite/libXrender/libXfixes. Depends on damageproto
# (already vendored) + libXext + libX11. Needed for Phase 40's
# compositing manager (xcompmgr) for damage-driven repaint instead of
# blind per-frame redraw.
#
# `configure` was pre-generated with `NOCONFIGURE=1
# ACLOCAL_PATH=$ROOT/src/xorg-util-macros LIBTOOLIZE=glibtoolize
# ./autogen.sh`, same as every libX* build.sh here.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang -std=gnu23"
PREFIX="$ROOT/build/xorg-deps-install"

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"

./configure --host=x86_64-apple-darwin19 --prefix="$PREFIX" \
	CC="$CLANG" \
	--disable-shared --enable-static

make -j"$(sysctl -n hw.ncpu)"
make install

echo "installed: $PREFIX/lib/libXdamage.a, $PREFIX/include/X11/extensions/Xdamage.h"
