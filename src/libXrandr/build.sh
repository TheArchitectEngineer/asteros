#!/bin/bash
# Cross-compiles real upstream libXrandr 1.5.2
# (gitlab.freedesktop.org/xorg/lib/libxrandr, tag libXrandr-1.5.2 --
# pinned to match this project's already-vendored randrproto 1.5.0
# protocol headers) against this project's vendored X11 stack, same
# autotools-via-cross-clang recipe as libXrender/libXfixes. Needs real
# libXrender (already built) for headers/link -- must build after it.
#
# `configure` was pre-generated the same way as libXrender's (see that
# build.sh's comment for the exact autogen.sh invocation).
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

echo "installed: $PREFIX/lib/libXrandr.a, $PREFIX/include/X11/extensions/Xrandr.h"
