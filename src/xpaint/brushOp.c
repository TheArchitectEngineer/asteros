/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)            | */
/* | Copyright 1995, 1996 Torsten Martinsen (bullestock@dk-online.dk)  | */
/* |                                                                   | */
/* | Permission to use, copy, modify, and to distribute this software  | */
/* | and its documentation for any purpose is hereby granted without   | */
/* | fee, provided that the above copyright notice appear in all       | */
/* | copies and that both that copyright notice and this permission    | */
/* | notice appear in supporting documentation.  There is no           | */
/* | representations about the suitability of this software for        | */
/* | any purpose.  this software is provided "as is" without express   | */
/* | or implied warranty.                                              | */
/* |                                                                   | */
/* +-------------------------------------------------------------------+ */

/* $Id: brushOp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <stdlib.h>
#include <math.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/xpm.h>

#include "xaw_incdir/Box.h"
#include "xaw_incdir/Form.h"
#include "xaw_incdir/Command.h"
#include "xaw_incdir/Toggle.h"

#include "xpaint.h"
#include "misc.h"
#include "Paint.h"
#include "PaintP.h"
#include "palette.h"
#include "graphic.h"
#include "protocol.h"
#include "image.h"
#include "ops.h"
#include "rc.h"

enum { OPAQUE, TRANSPARENT, STAIN };

/* this blend function ensures to reach the final color - ACZ: */
#define BLEND(a, b, x)	((x)*(b) + (1.0039-(x))*(a))

static XpmColorSymbol monoColorSymbols[5] =
{
    {"A", NULL, 0},
    {"B", NULL, 1},
    {"C", NULL, 1},
    {"D", NULL, 1},
    {"E", NULL, 1}
};

typedef enum {
    ERASE, SMEAR, PLAIN
} BrushType;

typedef struct {
    Widget shell, form, box, close;   
} BrushboxInfo;

typedef struct {
    int useSecond;
    BrushType brushtype;
    Boolean tracking;
    Pixmap pixmap;
    int width, height;
    int lastX, lastY;
    Palette *brushPalette;
} LocalInfo;

static BrushItem * brushList = NULL;
static int brushnum = 0, BBNUM = 0;
static int brushdef = 0;
static int currentBrush;
static float brushOpacity = 0.2;
static XImage *brushImage;
static Boolean eraseMode = True;
static int transparentMode = 0;

/* RPC */
static void stain(Widget w, OpInfo * info, GC gc, LocalInfo * l, int sx, int sy);
/* RPC */

static void smear(Widget w, OpInfo * info, GC gc, LocalInfo * l, int sx, int sy);
static void wbrush(Widget w, OpInfo * info, GC gc,
		   LocalInfo * l, int sx, int sy);

static void 
draw(Widget w, OpInfo * info, LocalInfo * l, int x, int y)
{
    PaintWidget pw = (PaintWidget) w;
    XRectangle undo;
    int sx = x - l->width / 2;
    int sy = y - l->height / 2;
    GC gc;

    if (l->brushtype == ERASE)
	gc = info->base_gc;
    else
	gc = l->useSecond ? info->second_gc : info->first_gc;

    XSetClipOrigin(XtDisplay(w), gc, sx, sy);

    if ((l->brushtype == ERASE) && eraseMode && (info->base != None)) {
	XCopyArea(XtDisplay(w), info->base, info->drawable,
		  gc, sx, sy, l->width, l->height, sx, sy);
    } else if (l->brushtype == SMEAR) {
	smear(w, info, gc, l, sx, sy);
    } else if (transparentMode==TRANSPARENT) {
	wbrush(w, info, gc, l, sx, sy);
/* RPC */
    } else if (transparentMode==STAIN) {
	stain(w, info, gc, l, sx, sy );
/* RPC */
    } else {			/* plain opaque brush */
        XFillRectangle(XtDisplay(w), info->drawable,
		       gc, sx, sy, l->width, l->height);
    }

    if (info->surface == opPixmap || pw->paint.grid) {
        XYtoRECT(sx, sy, sx + l->width, sy + l->height, &undo);
        UndoGrow(w, sx, sy);
        UndoGrow(w, sx + l->width, sy + l->height);
        PwUpdate(w, &undo, pw->paint.grid);
    }
}

static void 
press(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    BrushItem * brush = &brushList[currentBrush];
    /*
    **  Check to make sure all buttons are up, before doing this
     */

    if (event->button >= Button4) return;   
    if ((event->state & (Button1Mask | Button2Mask | Button3Mask |
			 Button4Mask | Button5Mask)) != 0)
	return;
    if (event->button == Button3) return;

    if (info->surface == opWindow && info->isFat)
	return;

    l->useSecond = (event->button == Button2);
    l->width = brush->width;
    l->height = brush->height;
    l->tracking = True;

    XSetClipMask(XtDisplay(w), info->first_gc, l->pixmap);
    XSetClipMask(XtDisplay(w), info->second_gc, l->pixmap);
    XSetClipMask(XtDisplay(w), info->base_gc, l->pixmap);

    UndoStart(w, info);

    draw(w, info, l, event->x, event->y);
    if (info->surface == opPixmap) {
	l->lastX = event->x;
	l->lastY = event->y;
    }
}

static void 
motion(Widget w, LocalInfo * l, XMotionEvent * event, OpInfo * info)
{
    int x = l->lastX;
    int y = l->lastY;
    int d, dx, dy, x_incrE, y_incrE = 0, x_incrNE, y_incrNE, incrE, incrNE;
   
    if (!l->tracking) return;
    if (l->useSecond == -1) return;
    if (!event->state || ((info->surface == opWindow) && info->isFat))
	return;

    x_incrE = x_incrNE = x < event->x ? 1 : (x == event->x ? 0 : -1);
    y_incrNE = y < event->y ? 1 : (y == event->y ? 0 : -1);
    dx = abs(event->x - x);
    dy = abs(event->y - y);
    if (dy > dx) {
	int t = dx;
	dx = dy;
	dy = t;
	x_incrE = 0;
	y_incrE = y_incrNE;
    }
    d = dy * 2 - dx;
    incrE = dy*2;
    incrNE = (dy - dx) * 2;
    do {
	if (d <= 0) {
	    d += incrE;
	    x += x_incrE;
	    y += y_incrE;
	}
	else {
	    d += incrNE;
	    x += x_incrNE;
	    y += y_incrNE;
	}
	draw(w, info, l, x, y);
    }
    while (x != event->x || y != event->y);
    if (info->surface == opPixmap) {
	l->lastX = event->x;
	l->lastY = event->y;
    }
}

static void 
release(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    int mask;
    /*
    **  Check to make sure all buttons are up, before doing this
     */
    if (event->button >= Button4) return;   
    mask = AllButtonsMask;   
    switch (event->button) {
    case Button1:
	mask ^= Button1Mask;
	break;
    case Button2:
	mask ^= Button2Mask;
	break;
    case Button3:
	mask ^= Button3Mask;
	break;
    case Button4:
	mask ^= Button4Mask;
	break;
    case Button5:
	mask ^= Button5Mask;
	break;
    }

    if ((event->state & mask) != 0)
	return;
    if (event->button == Button3) return;
    l->tracking = False;

    XSetClipMask(XtDisplay(w), info->first_gc, None);
    XSetClipMask(XtDisplay(w), info->second_gc, None);
    XSetClipMask(XtDisplay(w), info->base_gc, None);
}

/* RPC */

static void 
rgbTOhsv(float r, float g, float b, float *h, float *s, float *v)
{
    float max = MAX(r, MAX(g, b));
    float min = MIN(r, MIN(g, b));
    float delta;

    *v = max;
    if (max != 0.0)
	*s = (max - min) / max;
    else
	*s = 0.0;

    if (*s == 0.0) {
	*h = 0.0;
    } else {
	delta = max - min;
	if (r == max)
	    *h = (g - b) / delta;
	else if (g == max)
	    *h = 2.0 + (b - r) / delta;
	else			/* if (b == max) */
	    *h = 4.0 + (r - g) / delta;
	*h *= 60.0;
	if (*h < 0.0)
	    *h += 360.0;
    }
}

static void 
hsvTOrgb(float h, float s, float v, float *r, float *g, float *b)
{
    int i;
    float f, p, q, t;

    if (s == 0 && h == 0) {
	*r = *g = *b = v;
    } else {
	if (h >= 360.0)
	    h = 0.0;
	h /= 60.0;

	i = h;
	f = h - i;
	p = v * (1 - s);
	q = v * (1 - (s * f));
	t = v * (1 - (s * (1 - f)));
	switch (i) {
	case 0:
	    *r = v;
	    *g = t;
	    *b = p;
	    break;
	case 1:
	    *r = q;
	    *g = v;
	    *b = p;
	    break;
	case 2:
	    *r = p;
	    *g = v;
	    *b = t;
	    break;
	case 3:
	    *r = p;
	    *g = q;
	    *b = v;
	    break;
	case 4:
	    *r = t;
	    *g = p;
	    *b = v;
	    break;
	case 5:
	    *r = v;
	    *g = p;
	    *b = q;
	    break;
	}
    }
}
/*
** drawing routine for stain operator
 */
static void
stain(Widget wid, OpInfo * info, GC gc, LocalInfo * l, int sx, int sy)
{
    BrushItem * brush = &brushList[currentBrush];
    int x, y, dx, dy, w, h, d;
    unsigned char *brushbits;
    Display *dpy = XtDisplay(wid);
    XGCValues gcValues;
    XColor *brushCol, *col;
    Pixel p;
    float r, g, b, H, S, V;
    float brushH, brushS, brushV;

    /*
     * Perform manual clipping to avoid XPutImage crashing on us
     */

    XGetGCValues( dpy, gc, GCForeground, &gcValues );

    brushCol = PaletteLookup(l->brushPalette, gcValues.foreground);

    r = brushCol->red / 65535.0;
    g = brushCol->green / 65535.0;
    b = brushCol->blue / 65535.0;

    rgbTOhsv( r, g, b, &brushH, &brushS, &brushV );

    dx = dy = 0;
    w = l->width;
    h = l->height;

    if (sx < 0) {
	dx = -sx;
	w += sx;
	sx = 0;
    }
    if ((d = (sx + w - ((PaintWidget) wid)->paint.drawWidth)) > 0)
	w -= d;
    if (sy < 0) {
	dy = -sy;
	h += sy;
	sy = 0;
    }
    if ((d = (sy + h - ((PaintWidget) wid)->paint.drawHeight)) > 0)
	h -= d;
    if ((w <= 0) || (h <= 0))
	return;

    /* copy portion of image under brush into brushImage */
    XGetSubImage(dpy, info->drawable, sx, sy, w, h,
		 AllPlanes, ZPixmap, brushImage, dx, dy);

    brushbits = (unsigned char *) brush->brushbits;

    for (y = 0; y < l->height; ++y)
	for (x = 0; x < l->width; ++x)
	    if (*brushbits++) {
		p = XGetPixel(brushImage, x, y);
		col = PaletteLookup(l->brushPalette, p);

    		r = col->red;
    		g = col->green;
    		b = col->blue;

		rgbTOhsv( r, g, b, &H, &S, &V );

		H = brushH;
		S = brushS;

		hsvTOrgb( H, S, V, &r, &g, &b );

		col->red = (unsigned short int) (r);
		col->green = (unsigned short int) (g);
		col->blue = (unsigned short int) (b);

		p = PaletteAlloc(l->brushPalette, col);
		XPutPixel(brushImage, x, y, p );
	    }

    XPutImage(dpy, info->drawable, gc, brushImage, dx, dy, sx, sy, w, h);
}

/* RPC */

/*
** drawing routine for smear operator
 */
static void 
smear(Widget wid, OpInfo * info, GC gc, LocalInfo * l, int sx, int sy)
{
    BrushItem * brush = &brushList[currentBrush];
    int x, y, n, dx, dy, w, h, d, m;
    unsigned long r, g, b;
    unsigned char *brushbits;
    Pixel p;
    Display *dpy = XtDisplay(wid);
    XColor *col, newcol;

    /*
     * Perform manual clipping to avoid XPutImage crashing on us
     */
    dx = dy = 0;
    w = l->width;
    h = l->height;

    if (sx < 0) {
	dx = -sx;
	w += sx;
	sx = 0;
    }
    if ((d = (sx + w - ((PaintWidget) wid)->paint.drawWidth)) > 0)
	w -= d;
    if (sy < 0) {
	dy = -sy;
	h += sy;
	sy = 0;
    }
    if ((d = (sy + h - ((PaintWidget) wid)->paint.drawHeight)) > 0)
	h -= d;
    if ((w <= 0) || (h <= 0))
	return;

    /* copy portion of image under brush into brushImage */
    XGetSubImage(dpy, info->drawable, sx, sy, w, h,
		 AllPlanes, ZPixmap, brushImage, dx, dy);

    /* compute average of pixels inside brush */
    r = g = b = 0;
    brushbits = (unsigned char *) brush->brushbits;
    for (y = 0; y < l->height; ++y)
	for (x = 0; x < l->width; ++x)
	    if (*brushbits++) {
		p = XGetPixel(brushImage, x, y);
		col = PaletteLookup(l->brushPalette, p);
		r += col->red;
		g += col->green;
		b += col->blue;
	    }
    n = brush->numpixels;
    r = r / 256 / n;
    g = g / 256 / n;
    b = b / 256 / n;

    /* now blend each surface pixel with average */
    brushbits = (unsigned char *) brush->brushbits;
    for (y = 0; y < l->height; ++y)
	for (x = 0; x < l->width; ++x) {
	    if ((m = *brushbits++) != 0) {
		float mix = m / 5.0;

		p = XGetPixel(brushImage, x, y);
		col = PaletteLookup(l->brushPalette, p);
		newcol.red = 256 * BLEND(col->red / 256, r, mix);
		newcol.green = 256 * BLEND(col->green / 256, g, mix);
		newcol.blue = 256 * BLEND(col->blue / 256, b, mix);
		p = PaletteAlloc(l->brushPalette, &newcol);
		XPutPixel(brushImage, x, y, p);
	    }
	}
    XPutImage(dpy, info->drawable, gc, brushImage, dx, dy, sx, sy, w, h);
}


/*
** drawing routine for transparent brush
 */
static void 
wbrush(Widget wid, OpInfo * info, GC gc, LocalInfo * l, int sx, int sy)
{
    BrushItem * brush = &brushList[currentBrush];
    int x, y, dx, dy, m, w, h, d;
    unsigned long r, g, b;
    unsigned char *brushbits;
    Pixel p;
    Display *dpy = XtDisplay(wid);
    XColor *col, newcol;
    XGCValues gcval;


    /*
     * Perform manual clipping to avoid XPutImage crashing on us
     */
    dx = dy = 0;
    w = l->width;
    h = l->height;

    if (sx < 0) {
	dx = -sx;
	w += sx;
	sx = 0;
    }
    if ((d = (sx + w - ((PaintWidget) wid)->paint.drawWidth)) > 0)
	w -= d;
    if (sy < 0) {
	dy = -sy;
	h += sy;
	sy = 0;
    }
    if ((d = (sy + h - ((PaintWidget) wid)->paint.drawHeight)) > 0)
	h -= d;
    if ((w <= 0) || (h <= 0))
	return;

    /* copy portion of image under brush into brushImage */
    XGetSubImage(dpy, info->drawable, sx, sy, w, h,
		 AllPlanes, ZPixmap, brushImage, dx, dy);

    /* get current colour */
    XGetGCValues(dpy, gc, GCForeground, &gcval);
    col = PaletteLookup(l->brushPalette, gcval.foreground);
    r = col->red / 256;
    g = col->green / 256;
    b = col->blue / 256;
    brushbits = (unsigned char *) brush->brushbits;
    for (y = 0; y < l->height; ++y)
	for (x = 0; x < l->width; ++x)
	    if ((m = *brushbits++) != 0) {
		float mix = m / 5.0 * brushOpacity;

		p = XGetPixel(brushImage, x, y);
		col = PaletteLookup(l->brushPalette, p);
		newcol.red = 256 * BLEND(col->red / 256, r, mix);
		newcol.green = 256 * BLEND(col->green / 256, g, mix);
		newcol.blue = 256 * BLEND(col->blue / 256, b, mix);
		p = PaletteAlloc(l->brushPalette, &newcol);
		XPutPixel(brushImage, x, y, p);
	    }
    XPutImage(dpy, info->drawable, gc, brushImage, dx, dy, sx, sy, w, h);
}


static void 
setPixmap(Widget w, void *brushArg)
{
    BrushItem *brush = (BrushItem *) brushArg;
    LocalInfo *l = (LocalInfo *) GraphicGetData(w);

    l->pixmap = brush->pixmap;
}

static void 
setCursor(Widget wid, void *brushArg)
{
    static Boolean inited = False;
    static XColor xcols[2];
    BrushItem *brush = (BrushItem *) brushArg;
    Display *dpy = XtDisplay(wid);
    PaintWidget paint = (PaintWidget) wid;

    if (!inited) {
	Colormap map;
	Screen *screen = XtScreen(wid);

	inited = True;
	xcols[0].pixel = WhitePixelOfScreen(screen);
	xcols[1].pixel = BlackPixelOfScreen(screen);

	XtVaGetValues(wid, XtNcolormap, &map, NULL);

	XQueryColors(dpy, map, xcols, XtNumber(xcols));
    }
    if (brush->cursor == None) {
	Pixmap src, mask;
	XImage *im_src, *im_mask;
	GC gc;
	int x, y, w, h, ow, oh, n;
	unsigned char *brushbits;
	XpmAttributes xpmAttr;
	static XpmColorSymbol colorsymbols[5] =
	{
	    {"A", NULL, 0},
	    {"B", NULL, 1},
	    {"C", NULL, 2},
	    {"D", NULL, 3},
	    {"E", NULL, 4}
	};

	ow = brush->width;
	oh = brush->height;
	w = ow + 2;
	h = oh + 2;		/* add 1 pixel border for mask */

	/* get full depth pixmap */
	xpmAttr.valuemask = XpmColorSymbols;
	xpmAttr.numsymbols = 5;
	xpmAttr.colorsymbols = colorsymbols;
	/*
	XpmCreatePixmapFromData(dpy, RootWindowOfScreen(XtScreen(wid)),
				brush->bits, &src, NULL, &xpmAttr);
	*/
	GetPixmapWHD(dpy, brush->pixmap, NULL, NULL, (int *) &xpmAttr.depth);
	im_src = NewXImage(dpy, NULL, xpmAttr.depth, w, h);
	memset(im_src->data, 0, im_src->bytes_per_line * h);

	/* copy colour pixmap to center of XImage */
	XGetSubImage(dpy, brush->pixmap, 0, 0, ow, oh, 
                     AllPlanes, ZPixmap, im_src, 1, 1);
	im_mask = NewXImage(dpy, NULL, 1, w, h);
	brushbits = (unsigned char *) xmalloc(ow * oh);
	brush->brushbits = (char *) brushbits;
	
	n = 0;
	for (y = 0; y < h; y++)
	    for (x = 0; x < w; x++) {
		Pixel p = XGetPixel(im_src, x, y);

		if ((y != 0) && (y != h - 1) && (x != 0) && (x != w - 1))
		    *brushbits++ = p;

		if (p)
		    ++n;

		if (!p && x > 0)
		    p = XGetPixel(im_src, x - 1, y);
		if (!p && x < w - 1)
		    p = XGetPixel(im_src, x + 1, y);
		if (!p && y > 0)
		    p = XGetPixel(im_src, x, y - 1);
		if (!p && y < h - 1)
		    p = XGetPixel(im_src, x, y + 1);
		XPutPixel(im_mask, x, y, p ? 1 : 0);
	    }
	XDestroyImage(im_src);
	brush->numpixels = n;

	src = XCreatePixmap(dpy, brush->pixmap, w, h, 1);
	mask = XCreatePixmap(dpy, brush->pixmap, w, h, 1);
	gc = XCreateGC(dpy, mask, 0, 0);
	XSetForeground(dpy, gc, 0);
	XFillRectangle(dpy, src, gc, 0, 0, w, h);
	XCopyArea(dpy, brush->pixmap, src, gc, 0, 0, ow, oh, 1, 1);
	XPutImage(dpy, mask, gc, im_mask, 0, 0, 0, 0, w, h);
	XDestroyImage(im_mask);
	XFreeGC(dpy, gc);
	brush->cursor = XCreatePixmapCursor(dpy, src, mask,
					    &xcols[1], &xcols[0],
					    w / 2, h / 2);
	XFreePixmap(dpy, src);
	XFreePixmap(dpy, mask);
    }
    FatCursorSet(wid, brush->pixmap);

    /* don't set cursor to brush shape if zoom is larger than 1 */
    if (paint->paint.zoom == 1) {
        XtVaSetValues(wid, XtNcursor, brush->cursor, NULL);
    } else {
	SetCrossHairCursor(wid);
	FatCursorAddZoom((paint->paint.zoom>0)?paint->paint.zoom:2, wid);
        XtAddCallback(wid, XtNdestroyCallback, FatCursorDestroyCallback, wid);
    }
}

/*
**  Those public functions
 */

Boolean
EraseGetMode(void)
{
    return eraseMode;
}

void
EraseSetMode(Boolean mode)
{
    eraseMode = mode;
}

void
BrushSetMode(int mode)
{
    transparentMode = mode;
}

void
BrushSetParameters(float opacity)
{
    brushOpacity = opacity;
}

void *
BrushAdd(Widget w)
{
    static LocalInfo *l;
    Colormap cmap;
    BrushItem * brush = &brushList[currentBrush];

    l = XtNew(LocalInfo);

    XtVaSetValues(w, XtNcompress, False, NULL);

    l->brushtype = PLAIN;
    l->pixmap = brush->pixmap;
    XtVaGetValues(w, XtNcolormap, &cmap, NULL);
    l->brushPalette = PaletteFind(w, cmap);
    l->useSecond = -1;
    l->tracking = False;

    OpAddEventHandler(w, opWindow | opPixmap, ButtonPressMask,
		      FALSE, (OpEventProc) press, (XtPointer) l);
    OpAddEventHandler(w, opWindow | opPixmap, PointerMotionMask,
		      FALSE, (OpEventProc) motion, (XtPointer) l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask,
		      FALSE, (OpEventProc) release, (XtPointer) l);

    setCursor(w, (void *) brush);

    return l;
}

void
BrushRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonPressMask,
			 FALSE, (OpEventProc) press, (XtPointer) l);
    OpRemoveEventHandler(w, opWindow | opPixmap, PointerMotionMask,
			 FALSE, (OpEventProc) motion, (XtPointer) l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask,
			 FALSE, (OpEventProc) release, (XtPointer) l);

    XtFree((XtPointer) l);
    FatCursorOff(w);
}

void *
EraseAdd(Widget w)
{
    LocalInfo *l = XtNew(LocalInfo);
    BrushItem * brush = &brushList[currentBrush];

    XtVaSetValues(w, XtNcompress, False, NULL);

    l->brushtype = ERASE;
    l->useSecond = -1;
    l->pixmap = brush->pixmap;
    l->tracking = False;

    OpAddEventHandler(w, opWindow | opPixmap, ButtonPressMask,
		      FALSE, (OpEventProc) press, (XtPointer) l);
    OpAddEventHandler(w, opWindow | opPixmap, PointerMotionMask,
		      FALSE, (OpEventProc) motion, (XtPointer) l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask,
		      FALSE, (OpEventProc) release, (XtPointer) l);

    setCursor(w, (void *) brush);

    return l;
}

void
EraseRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonPressMask,
			 FALSE, (OpEventProc) press, (XtPointer) l);
    OpRemoveEventHandler(w, opWindow | opPixmap, PointerMotionMask,
			 FALSE, (OpEventProc) motion, (XtPointer) l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask,
			 FALSE, (OpEventProc) release, (XtPointer) l);

    XtFree((XtPointer) l);
    FatCursorOff(w);
}

void *
SmearAdd(Widget w)
{
    LocalInfo *l = XtNew(LocalInfo);
    Colormap cmap;
    BrushItem * brush = &brushList[currentBrush];

    XtVaSetValues(w, XtNcompress, False, NULL);

    l->brushtype = SMEAR;
    l->pixmap = brush->pixmap;
    l->useSecond = -1;
    l->tracking = False;

    XtVaGetValues(w, XtNcolormap, &cmap, NULL);
    l->brushPalette = PaletteFind(w, cmap);

    OpAddEventHandler(w, opWindow | opPixmap, ButtonPressMask,
		      FALSE, (OpEventProc) press, (XtPointer) l);
    OpAddEventHandler(w, opWindow | opPixmap, PointerMotionMask,
		      FALSE, (OpEventProc) motion, (XtPointer) l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask,
		      FALSE, (OpEventProc) release, (XtPointer) l);

    setCursor(w, (void *) brush);

    return l;
}

void
SmearRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonPressMask,
			 FALSE, (OpEventProc) press, (XtPointer) l);
    OpRemoveEventHandler(w, opWindow | opPixmap, PointerMotionMask,
			 FALSE, (OpEventProc) motion, (XtPointer) l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask,
			 FALSE, (OpEventProc) release, (XtPointer) l);

    XtFree((XtPointer) l);
    FatCursorOff(w);
}

/*
**  The brush selection dialog
 */

static void
closePopup(Widget button, BrushboxInfo * info)
{
    XtPopdown(info->shell);
}

void
setStandardCursor(Widget w)
{
    BrushItem * brush;
    int zoom;

    if ((CurrentOp->add == BrushAdd) ||
	(CurrentOp->add == EraseAdd) ||
	(CurrentOp->add == SmearAdd)) {
        brush =  &brushList[currentBrush];
        setCursor(w, (void *) brush);
        XtVaGetValues(w, XtNzoom, &zoom, NULL);
        if (zoom == 1)
            FatCursorRemoveZoom(w);
        else {
            SetCrossHairCursor(w);
            FatCursorAddZoom((zoom>0)?zoom:2, w);
        }
    } else
    if ((CurrentOp->add == PencilAdd) ||
        (CurrentOp->add == DotPencilAdd))
	SetPencilCursor(w);
    else
    if (CurrentOp->add == SprayAdd)
        XtVaSetValues(w, XtNcompress, False,
                      XtVaTypedArg, XtNcursor, XtRString, "spraycan", 
                      sizeof(Cursor), NULL);
    else
    if (CurrentOp->add == FontAdd)
        SetIBeamCursor(w);
    else
	SetCrossHairCursor(w);
}

void
selectBrush(Widget w, XtPointer nc)
{
    Display *dpy = XtDisplay(w);
    GC gc;
    BrushItem * brush;
    int i, j, s, t, u, v;
    Pixmap oldpix = Global.brushpix;

    currentBrush = (int)((long)nc);
    brush =  &brushList[currentBrush];

    for (i=0; i<brushnum; i++)
        XtVaSetValues(brushList[i].icon, XtNstate, (i==currentBrush), NULL);

    /* Create ad hoc icon for canvas button */
    Global.brushpix = XCreatePixmap(dpy, DefaultRootWindow(dpy),
                                    ICONWIDTH-8, ICONHEIGHT-8, 
                                    1);
    gc = XCreateGC(dpy, Global.brushpix, 0, 0);
    XFillRectangle(dpy, Global.brushpix,  gc, 0, 0, ICONWIDTH-8, ICONHEIGHT-8);
  
    if (brush->width >= ICONWIDTH-8) {
        i = 0;
        s = (brush->width-ICONWIDTH+8)/2;
        u = ICONWIDTH-8;
    } else {
        i = (ICONWIDTH-7-brush->width)/2;
        s = 0;
        u = brush->width;
    }
    if (brush->height >= ICONHEIGHT-8) {
        j = 0;
        t = (brush->height-ICONHEIGHT+8)/2;
        v = ICONHEIGHT-8;
    } else {
        j = (ICONHEIGHT-7-brush->height)/2;
        t = 0;
        v = brush->height;
    }
    XCopyArea(dpy, brush->pixmap, Global.brushpix, gc, s, t, u, v, i, j);
    XFreeGC(dpy, gc);

    if ((CurrentOp->add == BrushAdd) ||
	(CurrentOp->add == EraseAdd) ||
	(CurrentOp->add == SmearAdd)) {
	GraphicAll(setCursor, (void *) brush);
	GraphicAll(setPixmap, (void *) brush);
    }
    GraphicAll(setBrushIconPixmap, (void *) brush);

    if (oldpix)
        XFreePixmap(dpy, oldpix);

    if (brushImage != NULL)
	XDestroyImage(brushImage);
    brushImage = NewXImage(XtDisplay(brush->icon), NULL,
			   DefaultDepthOfScreen(XtScreen(brush->icon)),
			   brush->width, brush->height);
}

static void
brushboxResized(Widget w, BrushboxInfo * l, XConfigureEvent * event, Boolean * flg)
{
    Dimension width, height;
    XtVaGetValues(w, XtNwidth, &width,
                     XtNheight, &height, NULL);
    if (width<10) width = 10;
    if (height<35) height = 35;
    XtResizeWidget(l->box, width-6, height-32,
#ifdef XAW3D		   
		   1
#else
		   0
#endif		   
		   );
    XtUnmanageChild(l->close);
    XMapWindow(XtDisplay(l->close), XtWindow(l->close));
    XtMoveWidget(l->close, 4, height-25);
}

static void
ReadSystemBrushes(Widget w)
{
    Display *dpy = XtDisplay(w);
    Pixmap pix;
    char brushcfg[256], brushfile[256], buf[256];
    FILE *fd;
    char *ptr;
    XpmAttributes xpmAttr;
    int i;

    sprintf(brushcfg, "%s/bitmaps/brushbox.cfg", SHAREDIR);
    fd = fopen(brushcfg, "r");
    if (!fd) {
        fprintf(stderr, "\
Brush configuration file\n  %s\ncouldn't be read !! \
Will use square 9x9 as the brush ...\n", brushcfg);
        return;
    }

    /* First line of brushbox.cfg is default brush */
    *buf = '\0';
    fgets(buf, 255, fd);
    ptr = index(buf, ' ');
    if (!ptr) ptr = index(buf, '\t');
    if (ptr) *ptr = '\0';
    brushdef = atoi(buf);
    while(fgets(buf, 255, fd)) {
	    ptr = index(buf, '\n');
        if (ptr) *ptr = '\0';  
        if (!strstr(buf, ".xpm")) continue;
        sprintf(brushfile, "%s/bitmaps/%s", SHAREDIR, buf);
	/* force depth of one */
	xpmAttr.depth = 1;
	xpmAttr.colorsymbols = monoColorSymbols;
	xpmAttr.numsymbols = 5;
	xpmAttr.valuemask = XpmDepth | XpmColorSymbols;
	if (XpmReadFileToPixmap(dpy, RootWindowOfScreen(XtScreen(w)),
	    brushfile, &pix, NULL, &xpmAttr)==XpmSuccess) {
            i = BBNUM;
            ++BBNUM;
            brushList = (BrushItem *) 
	            realloc(brushList, BBNUM*sizeof(BrushItem));
	    brushList[i].width = xpmAttr.width;
	    brushList[i].height = xpmAttr.height;
            brushList[i].pixmap = pix;
            brushList[i].brushbits = NULL;
            brushList[i].bw = None;
            brushList[i].cursor = None;
            brushList[i].icon = None;
	} else
	    fprintf(stderr, "Brush file %s couldn't be read !!\n", brushfile);
    }
    brushnum = BBNUM;
}

Widget
createBrushDialog(Widget w)
{
    Display *dpy = XtDisplay(w);
    Widget icon;
    static Widget firstIcon = None;
    static BrushboxInfo *info = NULL;
    RCInfo *rcInfo = NULL;
    GC gc;
    XGCValues values;
    int i, j, newbrushnum;
    Pixel fg, bg;
    int nw, nh, ox, oy;
    Arg args[4];
    int nargs = 0;
    
    if (!info) {
        info = (BrushboxInfo *)XtMalloc(sizeof(BrushboxInfo));
        info->shell = XtVisCreatePopupShell("brush",
	 			 topLevelShellWidgetClass, w,
				 args, nargs);

        info->form = XtVaCreateManagedWidget(NULL,
				   formWidgetClass, info->shell,
				   NULL);

        info->box = XtVaCreateManagedWidget("box",
				  boxWidgetClass, info->form,
				  NULL);
        AddDestroyCallback(info->shell, (DestroyCallbackFunc) closePopup, info);
        XtAddEventHandler(info->shell, StructureNotifyMask, False,
		          (XtEventHandler) brushboxResized, (XtPointer) info);
        info->close = XtVaCreateManagedWidget("close",
				    commandWidgetClass, info->form,
				    XtNfromVert, info->box,
				    XtNtop, XtChainBottom,
				    NULL);

        XtAddCallback(info->close, XtNcallback, (XtCallbackProc) closePopup,
		      (XtPointer) info);
    }

    /* Read system wide brushes from SHAREDIR */
    if (BBNUM == 0)
        ReadSystemBrushes(w);

    /* If this fails, create a default brush to be a 9x9 square */
    if (BBNUM == 0) {
        GC gc1;
        XGCValues values1;
        int d = 9;
        brushnum = ++BBNUM;
        brushdef = 0;
        brushList = (BrushItem *) 
	            realloc(brushList, BBNUM*sizeof(BrushItem));
	brushList[0].width = d;
	brushList[0].height = d;
        brushList[0].brushbits = NULL;
        brushList[0].bw = None;
        brushList[0].cursor = None;
        brushList[0].icon = None;
        brushList[0].pixmap =
            XCreatePixmap(dpy, RootWindowOfScreen(XtScreen(w)), d, d, 1);
        values1.foreground = values.background;
        values1.background = values.foreground;
  	gc1 = XCreateGC(dpy, brushList[0].pixmap,
                        GCForeground | GCBackground, &values1);
        XFillRectangle(dpy, brushList[0].pixmap, gc1, 0, 0, d+1, d+1);
	XFreeGC(dpy, gc1);
    }

    /* 
     *  Clear everything that has been set beyond the system brushes, 
     *  and read again.
     */
    for (i = BBNUM; i < brushnum; i++) {
        if (brushList[i].pixmap != None) XFreePixmap(dpy, brushList[i].pixmap);
        if (brushList[i].bw != None) XFreePixmap(dpy, brushList[i].bw);
        if (brushList[i].icon != None) XtUnrealizeWidget(brushList[i].icon);
	bzero(&brushList[i], sizeof(BrushItem));
    }

    /* Read RC file for possible brushes there */
    rcInfo = ReadDefaultRC();

    newbrushnum = BBNUM+rcInfo->nbrushes+Global.nbrushes;

    if (brushnum<newbrushnum) {
        brushList = (BrushItem *) 
	    realloc(brushList, newbrushnum*sizeof(BrushItem));
	for (i=brushnum; i<newbrushnum; i++)
	    bzero(&brushList[i], sizeof(BrushItem));
        brushnum = newbrushnum;
    }

    for (i = BBNUM; i < brushnum; i++) {
	GC gc0, gc1;
        XGCValues values1;
	int u, v, k, scale;
	Image * brush;
	char *ptr;

	j = i - BBNUM;
	if (j < rcInfo->nbrushes) 
	    brush = rcInfo->brushes[j];
	else
	    brush = (Image *) (Global.brushes[j-rcInfo->nbrushes]);
	brushList[i].width = brush->width;
        brushList[i].height = brush->height;
	brushList[i].pixmap = XCreatePixmap(dpy,
	                          RootWindowOfScreen(XtScreen(w)),
	                          brushList[i].width, brushList[i].height, 1);
	scale = brush->scale;
	ptr = (char*) brush->data;

        values.background = WhitePixelOfScreen(XtScreen(w));
        values.foreground = BlackPixelOfScreen(XtScreen(w));
	gc0 = XCreateGC(dpy, brushList[i].pixmap, 
                        GCForeground | GCBackground, &values);
        values1.foreground = values.background;
        values1.background = values.foreground;
	gc1 = XCreateGC(dpy, brushList[i].pixmap, 
                        GCForeground | GCBackground, &values1);
	for (v = 0; v < brushList[i].height; v++)
	    for (u = 0; u < brushList[i].width; u++) {
	        k = 0;
		while (k<scale && !ptr[k]) ++k;
		ptr = ptr+scale;
		if (k==scale)
                    XDrawPoint(dpy, brushList[i].pixmap, gc1, u, v);
		else
                    XDrawPoint(dpy, brushList[i].pixmap, gc0, u, v);
	    }
	XFreeGC(dpy, gc0);
	XFreeGC(dpy, gc1);
    }

    /* Now create icons and stuff */

    values.foreground = WhitePixelOfScreen(XtScreen(w));
    values.background = BlackPixelOfScreen(XtScreen(w));

    gc = XCreateGC(XtDisplay(w),
		   RootWindowOfScreen(XtScreen(w)),
		   GCForeground | GCBackground, &values);
    for (i=0; i<brushnum; i++) {
        if (brushList[i].icon == None) {
            icon = XtVaCreateManagedWidget("icon",
				       toggleWidgetClass, info->box,
				       XtNradioGroup, firstIcon,
#ifdef XAW3D
				       XtNforeground, 0xc0c0c0,
#endif				       
				       NULL);
	} else
 	    icon = brushList[i].icon;
	if (firstIcon == None) firstIcon = icon;

        if (brushList[i].bw == None) {
	    nw = brushList[i].width;
	    nh = brushList[i].height;
	    ox = oy = 0;
	    if (nw < 16) {
	        ox = (16 - nw) / 2;
	        nw = 16;
	    }
	    if (nh < 16) {
	        oy = (16 - nh) / 2;
	        nh = 16;
	    }
	    brushList[i].bw = XCreatePixmap(dpy, 
                                  RootWindowOfScreen(XtScreen(w)),
	                          nw, nh,
		                  DefaultDepthOfScreen(XtScreen(w)));
  	    XtVaGetValues(icon, XtNbackground, &bg, NULL);
            fg = XtScreen(w)->black_pixel;
	    /*
	    **  Clear then draw the clipped rectangle in
	     */
	    XSetClipMask(dpy, gc, None);
	    XSetForeground(dpy, gc, bg);
	    XFillRectangle(dpy, brushList[i].bw, gc, 0, 0, nw, nh);
	    XSetClipMask(dpy, gc, brushList[i].pixmap);
	    XSetClipOrigin(dpy, gc, ox, oy);
	    XSetForeground(dpy, gc, fg);
	    XFillRectangle(dpy, brushList[i].bw, gc, 0, 0, nw, nh);
	}

	XtVaSetValues(icon, XtNbitmap, brushList[i].bw, NULL);

        if (brushList[i].icon == None) {
	    XtAddCallback(icon, XtNcallback,
		          (XtCallbackProc) selectBrush, (XtPointer)(long)i);
            brushList[i].icon = icon;
	}
    }
    XtVaSetValues(brushList[brushdef].icon, XtNstate, True, NULL);
    XFreeGC(XtDisplay(w), gc);

    return info->shell;
}

int
GetTotalNumBrushes()
{
    return brushnum;
}

/*
**  Initializer to create a default brush
 */
void
BrushInit(Widget toplevel)
{
    BrushItem * brush;
    Global.brushpopup = createBrushDialog(toplevel);
    selectBrush(toplevel, (void *)(long)brushdef);
    brush = &brushList[brushdef];
    brushImage = NewXImage(XtDisplay(toplevel), NULL,
			   DefaultDepthOfScreen(XtScreen(toplevel)),
			   brush->width, brush->height);
}

void 
BrushSelect(Widget w)
{
    XtPopup(Global.brushpopup, XtGrabNone);
    RaiseWindow(XtDisplay(Global.brushpopup), XtWindow(Global.brushpopup));
}

void setBrushIconOnWidget(Widget w)
{
    if (w != None)
        XtVaSetValues(w, XtNbitmap, Global.brushpix, NULL);
}
