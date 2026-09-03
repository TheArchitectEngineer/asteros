#!/bin/bash
# Installs real upstream compositeproto 0.4.2
# (gitlab.freedesktop.org/xorg/proto/compositeproto, tag
# compositeproto-0.4.2) headers + .pc file -- the wire-protocol
# definitions the Composite X extension needs, both server-side (to
# build xorg-server with --enable-composite) and client-side (for
# libXcomposite). Headers-only, no compiled code, same
# --host=x86_64-apple-darwin19 recipe as every other *proto package
# already vendored here (see src/damageproto/config.log for the
# precedent this mirrors).
#
# `configure` was pre-generated with `NOCONFIGURE=1
# ACLOCAL_PATH=$ROOT/src/xorg-util-macros LIBTOOLIZE=glibtoolize
# ./autogen.sh`, same as every libX* build.sh here.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

PREFIX="$ROOT/build/xorg-deps-install"

./configure --host=x86_64-apple-darwin19 --prefix="$PREFIX"

make install

echo "installed: $PREFIX/lib/pkgconfig/compositeproto.pc, $PREFIX/include/X11/extensions/composite*.h"
