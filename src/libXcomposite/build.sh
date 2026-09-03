#!/bin/bash
# Cross-compiles real upstream libXcomposite 0.4.7
# (gitlab.freedesktop.org/xorg/lib/libxcomposite, tag libXcomposite-0.4.7)
# against this project's vendored X11 stack, same autotools-via-cross-clang
# recipe as libXrender/libXfixes. Depends on libXext + libXfixes (both
# already vendored) + compositeproto (src/compositeproto). Needed for
# Phase 40's compositing manager (xcompmgr) -- this is the client-side
# library that actually issues XCompositeRedirectSubwindows() etc; the
# server-side Composite extension itself lives in xorg-server and was
# re-enabled separately (previously built with --disable-composite).
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

echo "installed: $PREFIX/lib/libXcomposite.a, $PREFIX/include/X11/extensions/Xcomposite.h"
