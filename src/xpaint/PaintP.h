#ifndef _PaintP_h
#define _PaintP_h

/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)	       | */
/* | Copyright 1995, 1996 Torsten Martinsen (bullestock@dk-online.dk)  | */
/* |								       | */
/* | Permission to use, copy, modify, and to distribute this software  | */
/* | and its documentation for any purpose is hereby granted without   | */
/* | fee, provided that the above copyright notice appear in all       | */
/* | copies and that both that copyright notice and this permission    | */
/* | notice appear in supporting documentation.	 There is no	       | */
/* | representations about the suitability of this software for	       | */
/* | any purpose.  this software is provided "as is" without express   | */
/* | or implied warranty.					       | */
/* |								       | */
/* +-------------------------------------------------------------------+ */

/* $Id: PaintP.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include "Paint.h"

#include <X11/IntrinsicP.h>	/* necessary for CoreP.h */
/* include superclass private header file */
#include <X11/CoreP.h>
#include <X11/CompositeP.h>
#include <X11/Xutil.h>

/* define unique representation types not found in <X11/StringDefs.h> */

#define XtRPaintResource "PaintResource"

typedef struct {
    int empty;
} PaintClassPart;

typedef struct _PaintClassRec {
    CoreClassPart core_class;
    CompositeClassPart composite_class;
    PaintClassPart paint_class;
} PaintClassRec;

extern PaintClassRec paintClassRec;

typedef struct s_undoStack {
    XRectangle box;
    AlphaPixmap alphapix;
    Boolean pushed;
    struct s_undoStack *next;
} UndoStack;

typedef struct {
    int drawWidth, drawHeight;
    int fillRule, lineFillRule;
    GC gc, igc, fgc, sgc;
    Pixmap sourcePixmap;
    Pixmap pattern, linePattern;
    int lineWidth;
    Pixel foreground, background, lineForeground;

    /*
    **	The undo system
     */
    AlphaPixmap current;	/* The currently displayed pixmap */
    UndoStack * undo, * head;
    /* The undo/redo stacks: */
    AlphaPixmap * undostack, * redostack;
    int undobot, undoitems, redobot, redoitems;
    int nlevels;		/* Maximum stack depth */
    int undoSize;		/* number of undo stacks */
    int alpha_mode;             /* alpha_mode = 0,1,2,3 default 0 */

    Boolean dirty, locked, menubar, fullmenu, snapOn, grid, compress;
    int snap_x, snap_y, gridmode, interpolation, transparent;
    void *eventList;
    XtCallbackList fatcalls;
    XtCallbackList sizecalls;
    XtCallbackList regionCalls;

    int zoomX, zoomY, zoom;
    GC tgc;			/* scratch GC, for stuffing clip masks into */
    GC mgc;			/* region mask GC, 1 bit deep */
    GC xgc;			/* XOR (invert) GC */
    Widget paint;
    int paintChildrenSize;
    Widget *paintChildren;
    Pixel linePixel;
    Pixel gridcolor;
    Cursor cursor;

    /*
    **	Region info
     */
    struct {
	Widget child;
	Widget grip[9];
	Position offX, offY;
	Position baseX, baseY;
	XRectangle rect, orig;
	GC fg_gc, bg_gc;

	Boolean isTracking, isDrawn, isRotate;
	float lastAngle;
	Position lastX, lastY;
	int fixedPoint;
	float lineDelta[4], lineBase[2];
	float startScaleX, startScaleY;

	Pixmap source, mask, notMask;
	XImage *sourceImg, *maskImg, *notMaskImg;
        unsigned char *alpha;

	Boolean isAttached, isVisible, needResize;
	Pixmap unfilterPixmap;
        AlphaPixmap undo_alphapix;

	/*
	**  The scale{X,Y} are combined with the rotMat
	**    to produce mat.
	 */
	float centerX, centerY;
	float scaleX, scaleY;
	pwMatrix rotMat, mat;

	int curZoom;
	pwRegionDoneProc *proc;
    } region;

    Boolean invalidateRegion;
    Region imageRegion;
    XImage *image;
    Position downX, downY;	/* Last button down X & Y pos */

    /*
    **	Taken from our parent shell
     */
    Visual *visual;

    /*
     * Used by the revert function
     */
    char *filename;

    WidgetList menuwidgets;
} PaintPart;

typedef struct _PaintRec {
    CorePart core;
    CompositePart composite;
    PaintPart paint;
} PaintRec;


#define _GET_PIXMAP(pw) ((pw)->paint.current.pixmap)
#define GET_PIXMAP(pw) (pw->paint.paint ? \
		_GET_PIXMAP((PaintWidget)pw->paint.paint) : _GET_PIXMAP(pw))

#define _GET_ZOOM(pw)	pw->paint.zoom
#define GET_ZOOM(pw) ((pw->paint.zoom == PwZoomParent) ? \
		_GET_ZOOM(((PaintWidget)pw->paint.paint)) : _GET_ZOOM(pw))

#define GET_MGC(pw, msk)	((pw)->paint.mgc == None ? \
	(pw->paint.mgc = XCreateGC(XtDisplay(pw), msk, 0, 0)) : pw->paint.mgc)

#define ALPHA_PIXEL_MODE(x, y)     ((((x)/12+(y)/12)&1)? 153:102)

void PwRegionZoomPosChanged(PaintWidget);
void PwZoomDraw(PaintWidget, Widget, GC, XImage *, XImage *,
		Boolean, int, int, int, XRectangle *);

/*
**  Try to optimize XGetPixel and XPutPixel
**
 */
#define ZINDEX8(x, y, img)  ((y) * img->bytes_per_line) + (x)
#define xxGetPixel(img, x, y)					\
		((img->bits_per_pixel == 8) ?			\
			img->data[ZINDEX8(x, y, img)] :		\
			XGetPixel(img, x, y))
#define xxPutPixel(img, x, y, p) {				\
		if (img->bits_per_pixel == 8)			\
			img->data[ZINDEX8(x, y, img)] = p;	\
		else						\
			XPutPixel(img, x, y, p);		\
	}

#endif				/* _PaintP_h */
