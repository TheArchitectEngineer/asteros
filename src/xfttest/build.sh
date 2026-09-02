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

	<!-- The only font this project ships is DejaVu Sans, registered
	     under its own real family name -- nothing here maps the
	     generic CSS-style family names ("Sans", "Serif", "Monospace",
	     and their lowercase X/fontconfig-style spellings) to it. A
	     real, full fontconfig install normally supplies that mapping
	     via its own compiled-in conf.d rules; this project's fonts.conf
	     is deliberately minimal and doesn't. Any client requesting a
	     generic family (WindowMaker's own Defaults, "Sans:bold:
	     pixelsize=12" etc -- see WindowMaker/Defaults/WindowMaker.in)
	     therefore gets no real-face match, and libXft silently falls
	     back to wrapping a legacy X core bitmap font instead of real
	     antialiased FreeType rendering -- ground-truthed live via
	     WindowMaker's own root menu coming up in a blocky, non-
	     antialiased fallback font despite linking real Xft/fontconfig/
	     FreeType and successfully opening *a* font (XftFontOpenName
	     doesn't return NULL for this case -- it returns the core-font
	     wrapper, which is why a naive "did WMCreateFont succeed" check
	     doesn't catch it). <alias>, not <match>, is fontconfig's own
	     dedicated, canonical mechanism for exactly this generic-name-
	     to-real-family mapping (what every real system's own
	     conf.d/*-family-*.conf uses) -- fixed at the root here rather
	     than per-client, aliasing every generic family to the one real
	     font actually installed. -->
	<alias>
		<family>Sans</family>
		<prefer><family>DejaVu Sans</family></prefer>
	</alias>
	<alias>
		<family>sans-serif</family>
		<prefer><family>DejaVu Sans</family></prefer>
	</alias>
	<alias>
		<family>Serif</family>
		<prefer><family>DejaVu Sans</family></prefer>
	</alias>
	<alias>
		<family>serif</family>
		<prefer><family>DejaVu Sans</family></prefer>
	</alias>
	<alias>
		<family>Monospace</family>
		<prefer><family>DejaVu Sans</family></prefer>
	</alias>
	<alias>
		<family>monospace</family>
		<prefer><family>DejaVu Sans</family></prefer>
	</alias>

	<!-- Belt and suspenders: force antialiasing on for every font
	     request that doesn't already say otherwise. A real fontconfig
	     install's own default conf.d chain always sets this
	     explicitly; this minimal fonts.conf otherwise leaves the
	     "antialias" property unset on most patterns. -->
	<match target="font">
		<test name="antialias" qual="all" compare="not_eq">
			<bool>false</bool>
		</test>
		<edit name="antialias" mode="assign">
			<bool>true</bool>
		</edit>
	</match>
</fontconfig>
FONTSEOF

echo "installed: $DESTDIR/bin/xfttest, $DESTDIR/usr/share/fonts/truetype/DejaVuSans.ttf, $DESTDIR/usr/etc/fonts/fonts.conf"
