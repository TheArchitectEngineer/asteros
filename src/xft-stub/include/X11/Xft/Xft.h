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
#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XFT_VERSION 20301

typedef unsigned char XftChar8;

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

/* XGlyphInfo/_XGlyphInfo used to be redefined here, but now that real
 * libXrender is vendored (src/libXrender), the #include above resolves
 * to the real, complete Xrender.h -- which already defines this same
 * struct -- instead of this project's old fake stand-in. Redefining it
 * again here is a hard redefinition error, not just redundant, so it's
 * removed; every caller already gets it from the real header's include.
 */

/* XftFontSet/XftResult/XftPatternGetString/XftFontSetDestroy are real
 * Xft's own aliases onto the fontconfig types/calls above -- kept as
 * the same aliases here, not reimplemented.
 */
typedef FcFontSet XftFontSet;
typedef FcResult XftResult;
#define XftResultMatch FcResultMatch
#define XftPatternGetString FcPatternGetString
#define XftFontSetDestroy FcFontSetDestroy

#define XFT_FAMILY FC_FAMILY
#define XFT_FOUNDRY FC_FOUNDRY
#define XFT_STYLE FC_STYLE

FcPattern *XftXlfdParse(const char *xlfd_name, Bool ignore_scalable, Bool complete);

XftFont *XftFontOpenName(Display *dpy, int screen, const char *name);
XftFont *XftFontOpenPattern(Display *dpy, FcPattern *pattern);
void XftFontClose(Display *dpy, XftFont *pub);

Bool XftInitFtLibrary(void);

/* Font *enumeration*, same "no real fontconfig to ask" situation as
 * FcFontList -- but unlike that one, this is the only font list
 * XPaint's own font-selector dialog (fontSelect.c's FontSelect())
 * ever shows the user, so an honestly-empty list would just make that
 * dialog useless. Reporting exactly the one real font
 * XftFontOpenName actually resolves everything to (see xft_stub.c)
 * is the honest answer here -- it is the complete, accurate set of
 * fonts this stub can actually render.
 */
XftFontSet *XftListFonts(Display *dpy, int screen, ...);

XftDraw *XftDrawCreate(Display *dpy, Drawable drawable, Visual *visual, Colormap colormap);
void XftDrawChange(XftDraw *draw, Drawable drawable);
void XftDrawDestroy(XftDraw *draw);

FcBool XftColorAllocValue(Display *dpy, Visual *visual, Colormap cmap,
			   const XRenderColor *color, XftColor *result);
void XftColorFree(Display *dpy, Visual *visual, Colormap cmap, XftColor *color);

void XftDrawRect(XftDraw *draw, const XftColor *color, int x, int y, unsigned int width, unsigned int height);
void XftDrawStringUtf8(XftDraw *draw, const XftColor *color, XftFont *pub, int x, int y,
			const XftChar8 *string, int len);

void XftTextExtents8(Display *dpy, XftFont *pub, const XftChar8 *string, int len, XGlyphInfo *extents);
void XftTextExtentsUtf8(Display *dpy, XftFont *pub, const XftChar8 *string, int len, XGlyphInfo *extents);

#ifdef __cplusplus
}
#endif

#endif
