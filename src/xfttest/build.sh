#!/bin/bash
# Builds xfttest, the verification client for this GTK-port phase's
# real FreeType2+fontconfig+Xft stack (see xfttest.c's own comment).
# Same hand-rolled direct-clang recipe as xeyes/xedit/xpaint, but
# linking against BOTH shared dependency prefixes: xorg-deps-install
# (X11 core + the new real libXrender) and gtk-deps-install (the new
# real font stack).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
ROOT="$(cd ../.. && pwd)"

CLANG="$ROOT/build/tools/asteros-sdk/bin/clang"
X11_DEPS="$ROOT/build/xorg-deps-install"
FONT_DEPS="$ROOT/build/gtk-deps-install"
OUT="$ROOT/build/xfttest_obj"
mkdir -p "$OUT"

# FONT_DEPS must come BEFORE X11_DEPS on the include path: xorg-deps-
# install (X11_DEPS) also has an X11/Xft/Xft.h and a fontconfig/
# fontconfig.h -- src/xft-stub's honest stand-ins, with a DIFFERENT,
# smaller struct layout than the real ones. Searching X11_DEPS first
# would silently compile this file against the stub's struct
# definitions while still *linking* against the real libXft.a/
# libfontconfig.a from FONT_DEPS -- same symbol names, mismatched
# struct layouts, real memory corruption (e.g. XftColorAllocValue
# writing a full real XftColor through a pointer to stack space only
# sized for the stub's smaller one). Caught via a build failure
# (FC_FILE undeclared, only defined in the real fontconfig.h) before
# ever shipping; -I order below is the actual fix, not just working
# around that one symbol.
"$CLANG" -std=gnu23 -O0 -g -Wall \
	-I "$FONT_DEPS/include" -I "$FONT_DEPS/include/freetype2" -I "$X11_DEPS/include" \
	-c xfttest.c -o "$OUT/xfttest.o"

"$CLANG" -std=gnu23 -O0 -g \
	"$OUT/xfttest.o" \
	-L "$FONT_DEPS/lib" -L "$X11_DEPS/lib" \
	-lXft -lfontconfig -lfreetype -lexpat -lpng16 -lz \
	-lXrender -lX11 -lxcb -lXau -lXdmcp -lm \
	-o "$OUT/xfttest"

echo "linked: $OUT/xfttest"
file "$OUT/xfttest"

DESTDIR="$ROOT/build/xorg-target-root"
install -d "$DESTDIR/bin"
install "$OUT/xfttest" "$DESTDIR/bin/xfttest"

install -d "$DESTDIR/usr/share/fonts/truetype"
install -m 644 "$ROOT/src/fonts/DejaVuSans.ttf" "$DESTDIR/usr/share/fonts/truetype/DejaVuSans.ttf"

install -d "$DESTDIR/usr/etc/fonts"
cat > "$DESTDIR/usr/etc/fonts/fonts.conf" <<'FONTSEOF'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
	<dir>/usr/share/fonts</dir>
	<cachedir>/usr/var/cache/fontconfig</cachedir>
</fontconfig>
FONTSEOF

echo "installed: $DESTDIR/bin/xfttest, $DESTDIR/usr/share/fonts/truetype/DejaVuSans.ttf, $DESTDIR/usr/etc/fonts/fonts.conf"
