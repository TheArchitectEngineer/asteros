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

/* $Id: sprayOp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtWindow XTWINDOW
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <X11/Xos.h>
#include <sys/time.h>
#include <math.h>

#include "xpaint.h"
#include "Paint.h"
#include "misc.h"
#include "ops.h"

#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#endif

typedef struct {
    Widget w;
    int useGauss;
    int x, y;
    XtIntervalId id;
    Boolean gcFlag, isTiled, trackDrawn, drawing;
    int lastX, lastY;
    int zoom;
    /*
    **  Borrowed from the info
     */
    GC gc, gcx;
    Pixmap drawable;
    Boolean isFat;
} LocalInfo;

static int radius = 10, density = 10, radius2 = 10 * 10, rate = 100;
static Boolean style = True;


static void 
draw(LocalInfo * l)
{
    int i;
    XRectangle rect;
    union {
	XSegment s[512];
	XPoint p[512];
    } p;

    UndoGrow(l->w, l->x - radius, l->y - radius);
    UndoGrow(l->w, l->x + radius, l->y + radius);

    for (i = 0; i < density; i++) {
	int rx, ry;

	do {
	    if (l->useGauss) {
		rx = gaussclamp(radius);
		ry = gaussclamp(radius);
	    } else {
		rx = RANDOMI2(-radius, radius);
		ry = RANDOMI2(-radius, radius);
	    }
	}
	while (rx * rx + ry * ry > radius2);

	if (l->isTiled) {
	    p.s[i].x1 = l->x + rx;
	    p.s[i].y1 = l->y + ry;
	    p.s[i].x2 = l->x + rx;
	    p.s[i].y2 = l->y + ry;
	} else {
	    p.p[i].x = l->x + rx;
	    p.p[i].y = l->y + ry;
	}
    }

    XYtoRECT(l->x - radius, l->y - radius,
	     l->x + radius, l->y + radius, &rect);

    if (l->isTiled) {
	XDrawSegments(XtDisplay(l->w), l->drawable, l->gc, p.s, density);
	if (!l->isFat)
	    XDrawSegments(XtDisplay(l->w), XtWindow(l->w), l->gc, p.s, density);
    } else {
	XDrawPoints(XtDisplay(l->w), l->drawable, l->gc, p.p, density, CoordModeOrigin);
	if (!l->isFat)
	    XDrawPoints(XtDisplay(l->w), XtWindow(l->w), l->gc,
			p.p, density, CoordModeOrigin);
    }

    PwUpdate(l->w, &rect, False);
}

static void 
drawEvent(LocalInfo * l)
{
    draw(l);
    l->id = XtAppAddTimeOut(XtWidgetToApplicationContext(l->w),
		   rate, (XtTimerCallbackProc) drawEvent, (XtPointer) l);
}

static void 
drawOutline(Widget w, LocalInfo * l, int x, int y, Boolean flag)
{
    Display *dpy = XtDisplay(w);
    Window window = XtWindow(w);
    XArc arc;

    if (l->zoom>0) {
        arc.width = 2 * radius * l->zoom;
        arc.height = 2 * radius * l->zoom;
    } else {
        arc.width = 2 * radius / (-l->zoom);
        arc.height = 2 * radius / (-l->zoom);
    }
    arc.angle1 = 0;
    arc.angle2 = 360 * 64;

    if (l->trackDrawn) {
        if (l->zoom>0) {
	    arc.x = l->lastX - radius * l->zoom;
	    arc.y = l->lastY - radius * l->zoom;
	} else {
	    arc.x = l->lastX - radius / (-l->zoom);
	    arc.y = l->lastY - radius / (-l->zoom);
	}
	XDrawArcs(dpy, window, l->gcx, &arc, 1);
	l->trackDrawn = False;
    }
    if (flag) {
        if (l->zoom>0) {
	    arc.x = x - radius * l->zoom;
	    arc.y = y - radius * l->zoom;
	} else {
	    arc.x = x - radius / (-l->zoom);
	    arc.y = y - radius / (-l->zoom);
	}
	XDrawArcs(dpy, window, l->gcx, &arc, 1);

	l->lastX = x;
	l->lastY = y;
	l->trackDrawn = True;
    }
}


static void 
press(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    GC sgc;
    int width, rule, rule2;

    /*
    **  Check to make sure all buttons are up, before doing this
     */
    if ((event->state & AllButtonsMask) != 0)
	return;
    if (event->button == Button3) return;

    l->x = event->x;
    l->y = event->y;

    l->useGauss = style;
    l->drawing = True;
    drawOutline(w, l, 0, 0, False);

    UndoStartPoint(w, info, event->x, event->y);

    l->drawable = info->drawable;
    l->isFat = info->isFat;

    XtVaGetValues(w, XtNlineWidth, &width,
		  XtNfillRule, &rule,
		  XtNlineFillRule, &rule2,
		  NULL);
    if (!l->gcFlag)
	l->gcFlag = (width != 0);

    if (event->button == Button2) {
	sgc = info->second_gc;
	l->isTiled = (rule2 != FillSolid);
    } else if (event->button == Button1) {
	sgc = info->first_gc;
	l->isTiled = (rule != FillSolid);
    } else
	return;

    if (l->gcFlag) {
	if (l->gc == None)
	    l->gc = XCreateGC(XtDisplay(w), info->drawable, 0, NULL);
	XCopyGC(XtDisplay(w), sgc, ~GCLineWidth, l->gc);
    } else {
	l->gc = sgc;
    }

    if (l->id == (XtIntervalId) NULL)
	drawEvent(l);
    else
	draw(l);
}

static void 
motion(Widget w, LocalInfo * l, XMotionEvent * event, OpInfo * info)
{
    if (l->drawing && info->surface == opPixmap) {
	l->x = event->x;
	l->y = event->y;

	draw(l);
    } else if (!l->drawing && info->surface == opWindow) {
	l->zoom = info->zoom;
	drawOutline(w, l, event->x, event->y, True);
    }
}

static void 
release(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    int mask;

    /*
    **  Check to make sure all buttons are up, before doing this
     */
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

    if (l->id != (XtIntervalId) NULL)
	XtRemoveTimeOut(l->id);

    l->id = (XtIntervalId) NULL;
    l->drawing = False;
}

static void 
leave(Widget w, LocalInfo * l, XEvent * event, OpInfo * info)
{
    drawOutline(w, l, 0, 0, False);
}


/*
**  Those public functions
 */
void 
SpraySetParameters(int r, int d, int sp)
{
    radius = r;
    radius2 = r * r;
    density = d;
    rate = sp * 10;
}

Boolean
SprayGetStyle(void)
{
    return style;
}

void 
SpraySetStyle(Boolean flag)
{
    style = flag;
}

static void *
commonSprayAdd(Widget w, Boolean flag)
{
    LocalInfo *l = XtNew(LocalInfo);

    l->w = w;
    l->id = (XtIntervalId) NULL;
    l->gc = None;
    l->gcx = GetGCX(w);
    l->gcFlag = False;
    l->useGauss = flag;
    l->trackDrawn = False;
    l->drawing = False;

    XtVaSetValues(w, XtNcompress, False,
    XtVaTypedArg, XtNcursor, XtRString, "spraycan", sizeof(Cursor), NULL);

    OpAddEventHandler(w, opPixmap, ButtonPressMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opWindow | opPixmap, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) release, l);
    OpAddEventHandler(w, opWindow, LeaveWindowMask, FALSE,
		      (OpEventProc) leave, l);

    return l;
}

void *
SprayAdd(Widget w)
{
    return commonSprayAdd(w, style);
}

void 
SprayRemove(Widget w, void *p)
{
    LocalInfo *l = (LocalInfo *) p;

    OpRemoveEventHandler(w, opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);
    OpRemoveEventHandler(w, opWindow, LeaveWindowMask, FALSE,
			 (OpEventProc) leave, l);

    if (l->gcFlag)
	XFreeGC(XtDisplay(w), l->gc);
    if (l->id != (XtIntervalId) NULL)
	XtRemoveTimeOut(l->id);

    XtFree((XtPointer) l);
}
