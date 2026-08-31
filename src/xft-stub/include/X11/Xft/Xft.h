/* Honest stub of libXft2's public API, not a real vendor of upstream
 * Xft -- see src/xft-stub/xft_stub.c for the reasoning. XftFontOpenName
 * always "succeeds" with fake-but-plausible metrics (so WINGs' internal
 * assumption that a font handle is always obtainable holds, rather than
 * NULL-crashing every widget that creates one), while the actual glyph
 * draw calls are no-ops: this project has zero real font files vendored
 * anywhere, so real Xft would have nothing to rasterize here either
 * (same gap already documented for xterm in TODO.md Phase 35).
 */
#ifndef _XFT_H_
#define _XFT_H_

#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XFT_VERSION 20301

typedef unsigned char XftChar8;

typedef struct {
	unsigned short red, green, blue, alpha;
} XRenderColor;

typedef struct {
	XRenderColor color;
	unsigned long pixel;
} XftColor;

typedef struct _XftFont {
	int ascent;
	int descent;
	int height;
	int max_advance_width;
	FcPattern *pattern;
} XftFont;

typedef struct _XftDraw XftDraw;

typedef struct _XGlyphInfo {
	unsigned short width, height;
	short x, y;
	short xOff, yOff;
} XGlyphInfo;

FcPattern *XftXlfdParse(const char *xlfd_name, Bool ignore_scalable, Bool complete);

XftFont *XftFontOpenName(Display *dpy, int screen, const char *name);
XftFont *XftFontOpenPattern(Display *dpy, FcPattern *pattern);
void XftFontClose(Display *dpy, XftFont *pub);

XftDraw *XftDrawCreate(Display *dpy, Drawable drawable, Visual *visual, Colormap colormap);
void XftDrawChange(XftDraw *draw, Drawable drawable);
void XftDrawDestroy(XftDraw *draw);

void XftDrawRect(XftDraw *draw, const XftColor *color, int x, int y, unsigned int width, unsigned int height);
void XftDrawStringUtf8(XftDraw *draw, const XftColor *color, XftFont *pub, int x, int y,
			const XftChar8 *string, int len);

void XftTextExtentsUtf8(Display *dpy, XftFont *pub, const XftChar8 *string, int len, XGlyphInfo *extents);

#ifdef __cplusplus
}
#endif

#endif
