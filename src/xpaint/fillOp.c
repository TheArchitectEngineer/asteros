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

/* $Id: fillOp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtWindow XTWINDOW
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#endif
#ifdef __EMX__
#include <float.h>
#endif
#include "Paint.h"
#include "protocol.h"
#include "xpaint.h"
#include "ops.h"
#include "image.h"
#include "misc.h"
#include "palette.h"
#include "color.h"

typedef struct {
    Boolean done;
    int type;			/* 0 = plain fill, 1 = gradient fill, 2 = fractal fill */
} LocalInfo;

#define ZINDEX8(x, y, img)  ((y) * img->bytes_per_line) + (x)

#define CLAMP(low, value, high) \
	if (value < low) value = low; else if (value > high) value = high
#define DEG2RAD	(M_PI/180.0)

static Pixmap buildGradientPixmap(Widget w, OpInfo * info,
			     int width, int height, int depth, int mode);
#if FEATURE_FRACTAL
static Pixmap buildFractalPixmap(Widget w, OpInfo * info,
			     int width, int height, int depth, int mode);
#endif

static Drawable drawable;
static Display *dpy;
static XImage *source;
static Boolean *mask;
static int maskW, maskH;
static int xMin, yMin, xMax, yMax;
static int timeCount = 0;
static int fillMode = 0, gfillMode = GFILL_RADIAL;
#if FEATURE_FRACTAL
static int ffillMode = 0;
#endif
static int deltaR, deltaG, deltaB;
static int deltaRV, deltaGV, deltaBV;
static Palette *deltaPal;

/*
 * A Seed Fill Algorithm
 * by Paul Heckbert
 * from "Graphics Gems", Academic Press, 1990
 */

/*
 * fill.c : simple seed fill program
 * Calls pixelread() to read pixels, pixelwrite() to write pixels.
 *
 * Paul Heckbert        13 Sept 1982, 28 Jan 1987
 */

#define pixelwrite(x,y)	do {	\
  mask[y * maskW + x] = 255;	\
  xMin = MIN(xMin, x);		\
  yMin = MIN(yMin, y);		\
  xMax = MAX(xMax, x);		\
  yMax = MAX(yMax, y);		\
} while (0)

#define STEP	32

static Pixel
pixelread(int x, int y, Boolean first)
{
    static Pixel ov;
    static int xMin, yMin, xMax, yMax;
    Pixel p;
    int n;

    if (first) {
	xMin = MAX(x - STEP, 0);
	yMin = MAX(y - STEP, 0);
	xMax = MIN(x + STEP, maskW);
	yMax = MIN(y + STEP, maskH);

	XGetSubImage(dpy, drawable, xMin, yMin,
		     xMax - xMin, yMax - yMin,
		     AllPlanes, ZPixmap, source,
		     xMin, yMin);

	ov = XGetPixel(source, x, y);

	return ov;
    } else if (mask[y * maskW + x]) {
	return (Pixel) - 1;
    }
    if (x < xMin) {
	n = MAX(x - STEP, 0);
	XGetSubImage(dpy, drawable, n, yMin,
		     xMin - n, yMax - yMin,
		     AllPlanes, ZPixmap, source,
		     n, yMin);
	xMin = n;
    }
    if (y < yMin) {
	n = MAX(y - STEP, 0);
	XGetSubImage(dpy, drawable, xMin, n,
		     xMax - xMin, yMin - n,
		     AllPlanes, ZPixmap, source,
		     xMin, n);
	yMin = n;
    }
    if (x >= xMax) {
	n = MIN(x + STEP, maskW);
	XGetSubImage(dpy, drawable, xMax, yMin,
		     n - xMax, yMax - yMin,
		     AllPlanes, ZPixmap, source,
		     xMax, yMin);
	xMax = n;
    }
    if (y >= yMax) {
	n = MIN(y + STEP, maskH);
	XGetSubImage(dpy, drawable, xMin, yMax,
		     xMax - xMin, n - yMax,
		     AllPlanes, ZPixmap, source,
		     xMin, yMax);
	yMax = n;
    }
    /*
    **  Do it fast for those 8 bit displays...
     */
    if (source->bits_per_pixel == 8)
	/*
	 * This is disgusting. 'data' is declared as _signed_ char,
	 * so values above 127 become negative if you're not careful.
	 */
	p = (unsigned char) source->data[ZINDEX8(x, y, source)];
    else
	p = XGetPixel(source, x, y);

    return p;
}

/*
**  Exchange pixel 1 for filled pixel value 2
 */
static void 
change(int sx, int sy, int width, int height)
{
    int x, y;
    Pixel pix = XGetPixel(source, sx, sy);

    if (source->bits_per_pixel == 8) {
	for (y = 0; y < height; y++) {
	    for (x = 0; x < width; x++)
		if (((unsigned char) source->data[ZINDEX8(x, y, source)]) == pix)
		    pixelwrite(x, y);
	    if (y % 100 == 0)
		StateTimeStep();
	}
    } else {
	for (y = 0; y < height; y++) {
	    for (x = 0; x < width; x++)
		if (XGetPixel(source, x, y) == pix)
		    pixelwrite(x, y);
	    if (y % 64 == 0)
		StateTimeStep();
	}
    }
}

typedef struct {
    short y, xl, xr, dy;
} Segment;

/*
 * Filled horizontal segment of scanline y for xl<=x<=xr.
 * Parent segment was on line y-dy.  dy=1 or -1
 */

#define STACKSIZE 20000		/* max depth of stack */

#define PUSH(Y, XL, XR, DY)  /* push new segment on stack */ \
    if (sp<stack+STACKSIZE && Y+(DY)>=0 && Y+(DY)<=height) \
    {sp->y = Y; sp->xl = XL; sp->xr = XR; sp->dy = DY; sp++;}

#define POP(Y, XL, XR, DY)    /* pop segment off stack */ \
    {sp--; Y = sp->y+(DY = sp->dy); XL = sp->xl; XR = sp->xr;}

Boolean *
setDrawable(Widget w, Drawable d, XImage *s)
{ 
    dpy = XtDisplay(w);
    drawable = d;
    source = s;
    maskW = s->width;
    maskH = s->height;
    if ((mask = (Boolean *) XtCalloc(sizeof(Boolean), maskW * maskH)) != NULL)
        memset(mask, False, maskW * maskH * sizeof(Boolean));
    return mask;
}

/*
 * fill: set the pixel at (x,y) and all of its 4-connected neighbors
 * with the same pixel value to the new pixel value nv.
 * A 4-connected neighbor is a pixel above, below, left, or right of a pixel.
 */

void 
fill(int x, int y, int width, int height)
{
    int start, x1, x2, dy = 0;
    Pixel ov;			/* old pixel value */
    Segment stack[STACKSIZE], *sp = stack;	/* stack of filled segments */

    ov = pixelread(x, y, True);	/* read pv at seed point */
    PUSH(y, x, x, 1);		/* needed in some cases */
    PUSH(y + 1, x, x, -1);	/* seed segment (popped 1st) */

    while (sp > stack) {
	/* pop segment off stack and fill a neighboring scan line */
	POP(y, x1, x2, dy);
	/*
	 * segment of scan line y-dy for x1<=x<=x2 was previously filled,
	 */
	for (x = x1; x >= 0 && pixelread(x, y, False) == ov; x--)
	    pixelwrite(x, y);
	if (x >= x1) {
	    for (x++; x <= x2 && pixelread(x, y, False) != ov; x++);
	    start = x;
	    if (x > x2)
		continue;
	} else {
	    start = x + 1;
	    if (start < x1)
		PUSH(y, start, x1 - 1, -dy);	/* leak on left? */
	    x = x1 + 1;
	}
	do {
	    for (; x <= width && pixelread(x, y, False) == ov; x++)
		pixelwrite(x, y);
	    PUSH(y, start, x - 1, dy);
	    if (x > x2 + 1)
		PUSH(y, x2 + 1, x - 1, -dy);	/* leak on right? */
	    for (x++; x <= x2 && pixelread(x, y, False) != ov; x++);
	    start = x;
	}
	while (x <= x2);

	if (++timeCount % 32 == 0)
	    StateTimeStep();
    }
}

static int 
similar(Pixel p)
{
    XColor *col;
    int r, g, b;

    if (p == ((Pixel) - 1))
	return 0;
    col = PaletteLookup(deltaPal, p);
    r = col->red / 256;
    g = col->green / 256;
    b = col->blue / 256;

    return (deltaR - deltaRV <= r) && (r <= deltaR + deltaRV)
	&& (deltaG - deltaGV <= g) && (g <= deltaG + deltaGV)
	&& (deltaB - deltaBV <= b) && (b <= deltaB + deltaBV);
}

/*
 * deltafill: set the pixel at (x,y) and all of its 4-connected neighbors
 * with similar pixel values to the new pixel value nv.
 * A 4-connected neighbor is a pixel above, below, left, or right of a pixel.
 */

static void 
deltafill(int x, int y, int width, int height)
{
    int start, x1, x2, dy = 0;
    Pixel ov;			/* old pixel value */
    Segment stack[STACKSIZE], *sp = stack;	/* stack of filled segments */
    XColor *col;

    ov = pixelread(x, y, True);	/* read pv at seed point */
    col = PaletteLookup(deltaPal, ov);
    deltaR = col->red / 256;
    deltaG = col->green / 256;
    deltaB = col->blue / 256;
    GetChromaDelta(&deltaRV, &deltaGV, &deltaBV);
    PUSH(y, x, x, 1);		/* needed in some cases */
    PUSH(y + 1, x, x, -1);	/* seed segment (popped 1st) */

    while (sp > stack) {
	/* pop segment off stack and fill a neighboring scan line */
	POP(y, x1, x2, dy);
	/*
	 * segment of scan line y-dy for x1<=x<=x2 was previously filled,
	 */
	for (x = x1; x >= 0 && similar(pixelread(x, y, False)); x--)
	    pixelwrite(x, y);
	if (x >= x1) {
	    for (x++; x <= x2 && !similar(pixelread(x, y, False)); x++);
	    start = x;
	    if (x > x2)
		continue;
	} else {
	    start = x + 1;
	    if (start < x1)
		PUSH(y, start, x1 - 1, -dy);	/* leak on left? */
	    x = x1 + 1;
	}
	do {
	    for (; x <= width && similar(pixelread(x, y, False)); x++)
		pixelwrite(x, y);
	    PUSH(y, start, x - 1, dy);
	    if (x > x2 + 1)
		PUSH(y, x2 + 1, x - 1, -dy);	/* leak on right? */
	    for (x++; x <= x2 && !similar(pixelread(x, y, False)); x++);
	    start = x;
	}
	while (x <= x2);

	if (++timeCount % 32 == 0)
	    StateTimeStep();
    }
}

static void 
motion(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
}

static void 
press(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    XRectangle undo, rect;
    int width, height;
    GC dgc;
    int isBusy, depth;
    Pixmap pix, gradient;
    Colormap cmap;

    if (event->type == ButtonRelease) {
        if (Global.popped_up)
            PopdownMenusGlobal();
        event->type = None;
        return;
    }

    if (event->button == Button1)
	dgc = info->first_gc;
    else if (event->button == Button2)
	dgc = info->second_gc;
    else
	return;

    dpy = XtDisplay(w);
    drawable = info->drawable;

    UndoStart(w, info);

    XtVaGetValues(w, XtNdrawWidth, &width,
		  XtNdrawHeight, &height,
		  XtNdepth, &depth,
		  NULL);
    if ((isBusy = (width * height > 6400)) != 0)
	StateSetBusy(True);

    if ((mask = (Boolean *) XtCalloc(sizeof(Boolean), width * height)) == NULL)
	return;
    maskW = width;
    maskH = height;

    memset(mask, False, width * height * sizeof(Boolean));

    if (fillMode != 1) {
	source = NewXImage(dpy, NULL, depth, width, height);
    } else {
	source = PwGetImage(w, NULL);
    }

    xMin = xMax = info->realX;
    yMin = yMax = info->realY;

    switch (fillMode) {
    case 0:
	fill(info->realX, info->realY, width - 1, height - 1);
	break;
    case 1:
	change(info->realX, info->realY, width, height);
	break;
    default:			/* 2 */
	XtVaGetValues(w, XtNcolormap, &cmap, NULL);
	deltaPal = PaletteFind(w, cmap);
	deltafill(info->realX, info->realY, width - 1, height - 1);
	break;
    }

    rect.x = xMin;
    rect.y = yMin;
    rect.width = (xMax - xMin) + 1;
    rect.height = (yMax - yMin) + 1;

    pix = AlphaToPixmap(w, width, height, mask, &rect);

    XSetClipOrigin(dpy, dgc, rect.x, rect.y);
    XSetClipMask(dpy, dgc, pix);

    switch (l->type) {
    case 0:
	XFillRectangles(dpy, info->drawable, dgc, &rect, 1);
	if (!info->isFat)
	    XFillRectangles(dpy, XtWindow(w), dgc, &rect, 1);
	break;

    case 1:
    case 2:
	/*
	 * Gradient fill: Build a pixmap of the required size and
	 * copy that to the region to be filled
	 */
	if (l->type == 1)
	    gradient = buildGradientPixmap(w, info, rect.width, rect.height,
					   depth, gfillMode);
#ifdef FEATURE_FRACTAL
	else
	    gradient = buildFractalPixmap(w, info, rect.width, rect.height,
					  depth, ffillMode);
#endif
	XCopyArea(dpy, gradient, info->drawable, dgc,
		  0, 0, rect.width, rect.height, xMin, yMin);
	if (!info->isFat)
	    XCopyArea(dpy, gradient, XtWindow(w), dgc,
		      0, 0, rect.width, rect.height, xMin, yMin);
	XFreePixmap(dpy, gradient);
	break;
    }


    XSetClipMask(dpy, dgc, None);
    XFreePixmap(dpy, pix);

    XYtoRECT(xMin, yMin, xMax + 1, yMax + 1, &undo);
    UndoSetRectangle(w, &undo);
    PwUpdate(w, &undo, False);

    if (fillMode != 1)
	XDestroyImage(source);

    XtFree((char *)mask);

    if (isBusy)
	StateSetBusy(False);
}

void *
FillAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));
    l->done = False;
    l->type = 0;
    OpAddEventHandler(w, opPixmap, ButtonPressMask|ButtonReleaseMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opPixmap, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    SetCrossHairCursor(w);
    return l;
}

void 
FillRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opPixmap, ButtonPressMask|ButtonReleaseMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opPixmap, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    XtFree((XtPointer) l);
}

void 
FillSetMode(int value)
{
    fillMode = value;
}

/*
 * Gradient fill
 */
static float GradientFillQuant = 300.0;
static double GradientFillVoffset = 0.0;
static double GradientFillHoffset = 0.0;
static double GradientFillPad = 0.0;
static int GradientFillTilt = 0;

void *
GradientFillAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));
    l->done = False;
    l->type = 1;
    OpAddEventHandler(w, opPixmap, ButtonPressMask|ButtonReleaseMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opPixmap, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    SetCrossHairCursor(w);
    return l;
}

void 
GradientFillRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opPixmap, ButtonPressMask|ButtonReleaseMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opPixmap, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    XtFree((XtPointer) l);
}

void 
GradientFillSetMode(int value)
{
    gfillMode = value;
}

/*
 * voffset, hoffset, pad range from -1.0 to 1.0
 * angle is in degrees
 */
void 
GradientFillSetParameters(float voffset, float hoffset, float pad, float angle, int steps)
{
    GradientFillVoffset = voffset;
    GradientFillHoffset = hoffset;
    GradientFillPad = pad;
    GradientFillTilt = angle;
    GradientFillQuant = (float) steps;
}

/*
 * Fractal fills
 */
#ifdef FEATURE_FRACTAL
void *
FractalFillAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));
    l->done = False;
    l->type = 2;
    OpAddEventHandler(w, opPixmap, ButtonPressMask|ButtonReleaseMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opPixmap, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    SetCrossHairCursor(w);
    return l;
}

void 
FractalFillRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opPixmap, ButtonPressMask|ButtonReleaseMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opPixmap, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    XtFree((XtPointer) l);
}

void 
FractalFillSetMode(int value)
{
    ffillMode = value;
}

#endif

#define SWAP(a, b)	{ int t = (a); (a) = (b); (b) = t; }
#define	SWAP_COLORS	{ SWAP(rp, rs); SWAP(gp, gs); SWAP(bp, bs); }

/*
 * Builds gradient pixmaps.
 */
static Pixmap
buildGradientPixmap(Widget w, OpInfo * info,
		    int width, int height, int depth, int mode)
{
    Pixmap pixmap;
    Image *image;
    Colormap cmap;
    Palette *pal;
    unsigned char *op;
    XGCValues value;
    XColor *col;
    int dx, dy, maxdist, ox, oy, x, y, xx, yy, rp, gp, bp, rs, gs, bs,
     s;
    float fraction, a, cs, sn;


    image = ImageNew(width, height);
    op = image->data;
    XtVaGetValues(w, XtNcolormap, &cmap, NULL);
    pal = PaletteFind(w, cmap);

    /* get primary colour */
    XGetGCValues(XtDisplay(w), info->first_gc, GCForeground, &value);
    col = PaletteLookup(pal, value.foreground);
    rp = col->red / 256;
    gp = col->green / 256;
    bp = col->blue / 256;

    /* get secondary colour */
    XGetGCValues(XtDisplay(w), info->second_gc, GCForeground, &value);
    col = PaletteLookup(pal, value.foreground);
    rs = col->red / 256;
    gs = col->green / 256;
    bs = col->blue / 256;

    if (GradientFillTilt < 0)
	GradientFillTilt += 360;

    /* Some handy values */
    ox = width * (GradientFillHoffset + 0.5);	/* Centre point */
    oy = height * (0.5 - GradientFillVoffset);
    cs = cos(GradientFillTilt * DEG2RAD);	/* For rotation of coords */
    sn = sin(GradientFillTilt * DEG2RAD);
    maxdist = MAX(ox, width - ox);	/* Max distance to any point in pixmap */
    maxdist = MAX(maxdist, oy);
    maxdist = MAX(maxdist, height - oy);

    switch (mode) {
    case GFILL_RADIAL:
	/*
	 * This mixes colours according to distance from the point
	 * in question to a centre point.
	 */
	for (y = 0; y < height; y++) {
	    for (x = 0; x < width; x++) {
		dx = x - ox;
		dy = y - oy;
		fraction = (sqrt(dx * dx + dy * dy) / maxdist - GradientFillPad) /
		    (1.0 - 2 * GradientFillPad);
		CLAMP(0.0, fraction, 1.0);
		fraction = ((int) (fraction * GradientFillQuant)) / GradientFillQuant;
		*op++ = (1 - fraction) * rp + fraction * rs;
		*op++ = (1 - fraction) * gp + fraction * gs;
		*op++ = (1 - fraction) * bp + fraction * bs;
	    }
	}
	break;

    case GFILL_LINEAR:
	/*
	 * This mixes colours according to distance from the point
	 * in question to the x axis (in the transformed system).
	 */
	/*
	 * s is the maximum distance of any point
	 * (distance from the line y = ax+h to (w,0) or
	 * from y = ax to (w,h), whichever is greater)
	 */
	if (GradientFillTilt == 90)	/* tan(90) goes towards infinity */
	    s = width;
	else {
	    a = tan(GradientFillTilt * DEG2RAD);
	    s = abs(a * width + (a < 0 ? -height : height)) / sqrt(a * a + 1);
	}
	for (y = 0; y < height; y++)
	    for (x = 0; x < width; x++) {
		yy = x * sn + y * cs;	/* only transformed y coord is needed */
		fraction = (((float) yy) / s - GradientFillPad) / (1.0 - 2 * GradientFillPad);
		fraction = ((int) (fraction * GradientFillQuant)) / GradientFillQuant;
		CLAMP(0.0, fraction, 1.0);
		*op++ = (1 - fraction) * rp + fraction * rs;
		*op++ = (1 - fraction) * gp + fraction * gs;
		*op++ = (1 - fraction) * bp + fraction * bs;
	    }
	break;

    case GFILL_SQUARE:
	/*
	 * This mixes colours according to distance from the point
	 * in question to the axis farthest away (in the transformed system).
	 */
	for (y = 0; y < height; y++)
	    for (x = 0; x < width; x++) {
		/* transform coords and take absolute value */
		dx = x - ox;
		dy = y - oy;
 		/* work-around for sco v5 cc internal compiler error */
 		xx = dx * cs - dy * sn;
 		if (xx < 0) xx = -xx;
 		yy = dx * sn + dy * cs;
 		if (yy < 0) yy = -yy;
		if (xx < yy)
		    s = yy;
		else
		    s = xx;
		fraction = (((float) s) / maxdist - GradientFillPad) / (1.0 - 2 * GradientFillPad);
		fraction = ((int) (fraction * GradientFillQuant)) / GradientFillQuant;
		CLAMP(0.0, fraction, 1.0);
		*op++ = (1 - fraction) * rp + fraction * rs;
		*op++ = (1 - fraction) * gp + fraction * gs;
		*op++ = (1 - fraction) * bp + fraction * bs;
	    }
	break;

    case GFILL_CONE:
	/*
	 * Convert (x,y) to polar coordinates (r,a) and mix
	 * colours according to the fraction a/360.
	 */
	for (y = 0; y < height; y++)
	    for (x = 0; x < width; x++) {
		dx = x - ox;
		dy = y - oy;
		if (dx == 0)
		    a = dy > 0 ? 270 : 90;
		else {
		    a = atan(((float) dy) / ((float) dx));
		    a *= 1.0 / DEG2RAD;
		    if (dx > 0)	/* atan() returns principal value -pi/2..pi/2 */
			a = a + 180.0;
		    while (a < 0)	/* normalize */
			a += 360.0;
		}
		a += GradientFillTilt;
		while (a > 360.0)
		    a -= 360.0;
		if (a > 180.0)	/* map (180,360) to (180,0) */
		    a = 360.0 - a;
		fraction = (a / 180.0 - GradientFillPad) / (1.0 - 2 * GradientFillPad);
		fraction = ((int) (fraction * GradientFillQuant)) / GradientFillQuant;
		CLAMP(0.0, fraction, 1.0);
		*op++ = (1 - fraction) * rp + fraction * rs;
		*op++ = (1 - fraction) * gp + fraction * gs;
		*op++ = (1 - fraction) * bp + fraction * bs;
	    }
	break;
    }

    pixmap = None;		/* force creation of new pixmap */
    ImageToPixmapCmap(image, w, &pixmap, cmap);
    return pixmap;
}

#ifdef FEATURE_FRACTAL
static Pixmap
buildFractalPixmap(Widget w, OpInfo * info,
		   int width, int height, int depth, int mode)
{
    Image *image;
    Pixmap pixmap;
    Colormap cmap;

    switch (ffillMode) {
    case 0:
	image = draw_plasma(width, height);
	break;
    case 1:
	image = draw_landscape(width, height, 1);
	break;
    case 2:
    default:
	image = draw_landscape(width, height, 0);
	break;
    }

    XtVaGetValues(w, XtNcolormap, &cmap, NULL);
    pixmap = None;		/* force creation of new pixmap */
    ImageToPixmapCmap(image, w, &pixmap, cmap);
    return pixmap;
}
#endif
