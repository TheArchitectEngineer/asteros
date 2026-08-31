
/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)	       | */
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

/* $Id: freehandOp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtWindow XTWINDOW
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include "xpaint.h"
#include "misc.h"
#include "Paint.h"
#include "ops.h"

#define FILL		0x2
#define POLY		0x1
#define IsPoly(x)	(x & POLY)
#define IsFill(x)	(x & FILL)

typedef struct {
    Boolean flag, tracking;
    int startX, startY, lastX, lastY;
    GC gc1, gc2;
    int npoints, size;
    XPoint *points, *real;
} LocalInfo;

static void 
press(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    XRectangle undo;
    int save;

    if (event->button >= Button4) return;

    if (info->surface == opWindow)
	return;

    if ((event->state & AllButtonsMask) != 0)
	return;
    if (event->button == Button3) return;
    Global.escape = 0;

    l->lastX = l->startX = event->x;
    l->lastY = l->startY = event->y;
    l->npoints = 0;
    l->tracking = True;

    if (IsFill(l->flag)) {
	l->points[l->npoints].x = event->x;
	l->points[l->npoints].y = event->y;
	l->real[l->npoints].x = info->x;
	l->real[l->npoints].y = info->y;
	l->npoints++;
    }
    undo.x = event->x;
    undo.y = event->y;
    undo.width = 1;
    undo.height = 1;

    if (event->button == Button2) {
	l->gc1 = info->second_gc;
	l->gc2 = info->first_gc;
    } else {
	l->gc1 = info->first_gc;
	l->gc2 = info->second_gc;
    }

    save = Global.dashnumber;
    Global.dashnumber = 0;
    SetCapAndJoin(w, l->gc1,
                  ((Global.cap)?Global.cap-1:CapButt),
		  ((Global.join)?Global.join-1:JoinRound));
    Global.dashnumber = save;

    UndoStartPoint(w, info, event->x, event->y);

    XDrawLine(XtDisplay(w), info->drawable, l->gc1,
	      l->lastX, l->lastY, event->x, event->y);
    if (!info->isFat)
	XDrawLine(XtDisplay(w), XtWindow(w), l->gc1,
		  l->lastX, l->lastY, event->x, event->y);

    PwUpdate(w, &undo, False);
}

static void 
release(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    XRectangle undo;

    int mask;
    /*
    **	Check to make sure all buttons are up, before doing this
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
    if (Global.escape) {
       Global.escape =0;
       return;
    }

    l->points[l->npoints].x = event->x;
    l->points[l->npoints].y = event->y;
    l->real[l->npoints].x = info->x;
    l->real[l->npoints].y = info->y;
    l->npoints++;

    UndoGrow(w, event->x, event->y);

    undo.x = MIN(l->lastX, event->x);
    undo.y = MIN(l->lastY, event->y);
    undo.width = MAX(l->lastX, event->x) - undo.x + 1;
    undo.height = MAX(l->lastY, event->y) - undo.y + 1;

    if (IsFill(l->flag)) {
	if (!info->isFat)
	    XFillPolygon(XtDisplay(w), XtWindow(w), l->gc2,
			 l->real, l->npoints, Complex, CoordModeOrigin);
	XFillPolygon(XtDisplay(w), info->drawable, l->gc2,
		     l->real, l->npoints, Complex, CoordModeOrigin);
	l->points[l->npoints].x = l->points[0].x;
	l->points[l->npoints].y = l->points[0].y;
	l->real[l->npoints].x = l->real[0].x;
	l->real[l->npoints].y = l->real[0].y;
	l->npoints++;
	if (!info->isFat)
	    XDrawLines(XtDisplay(w), XtWindow(w), l->gc1,
		       l->real, l->npoints, CoordModeOrigin);
	XDrawLines(XtDisplay(w), info->drawable, l->gc1,
		   l->real, l->npoints, CoordModeOrigin);
    } else {
	l->lastX = event->x;
	l->lastY = event->y;
	XDrawLine(XtDisplay(w), info->drawable, l->gc1,
		  l->lastX, l->lastY, l->startX, l->startY);
	if (!info->isFat)
	    XDrawLine(XtDisplay(w), XtWindow(w), l->gc1,
		      l->lastX, l->lastY, l->startX, l->startY);

    }

    PwUpdate(w, NULL, False);
}

static void 
motion(Widget w, LocalInfo * l, XMotionEvent * event, OpInfo * info)
{
    XRectangle undo;

    if (!l->tracking) return;
    if (l->npoints == -1) return;
    if (Global.escape) return;

    if (info->surface == opWindow)
	return;

    XDrawLine(XtDisplay(w), info->drawable, l->gc1,
	      l->lastX, l->lastY, event->x, event->y);
    if (!info->isFat)
	XDrawLine(XtDisplay(w), XtWindow(w), l->gc1,
		  l->lastX, l->lastY, event->x, event->y);

    UndoGrow(w, event->x, event->y);

    undo.x = MIN(l->lastX, event->x);
    undo.y = MIN(l->lastY, event->y);
    undo.width = MAX(l->lastX, event->x) - undo.x + 1;
    undo.height = MAX(l->lastY, event->y) - undo.y + 1;

    l->lastX = event->x;
    l->lastY = event->y;
    if (IsFill(l->flag)) {
	l->points[l->npoints].x = event->x;
	l->points[l->npoints].y = event->y;
	l->real[l->npoints].x = info->x;
	l->real[l->npoints].y = info->y;
	l->npoints++;
	if (l->npoints > l->size - 3) {
	    l->size += 256;
	    l->real = (XPoint *) XtRealloc((XtPointer) l->real,
					   sizeof(XPoint) * l->size);
	    l->points = (XPoint *) XtRealloc((XtPointer) l->points,
					     sizeof(XPoint) * l->size);
	}
    }
    PwUpdate(w, &undo, False);
}

/*
**  Those public functions
 */
void *
FreehandAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));

    XtVaSetValues(w, XtNcompress, False, NULL);
    l->flag = 0;

    l->size = 256;
    l->real = (XPoint *) XtCalloc(sizeof(XPoint), l->size);
    l->points = (XPoint *) XtCalloc(sizeof(XPoint), l->size);
    l->npoints = -1;
    l->tracking = False;

    OpAddEventHandler(w, opPixmap, ButtonPressMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opPixmap, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) release, l);
    SetPencilCursor(w);

    return l;
}
void 
FreehandRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opPixmap, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);

    XtFree((XtPointer) ((LocalInfo *) l)->real);
    XtFree((XtPointer) ((LocalInfo *) l)->points);
    XtFree((XtPointer) l);
}

void *
FilledFreehandAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));
    l->flag = FILL;

    l->size = 256;
    l->real = (XPoint *) XtCalloc(sizeof(XPoint), l->size);
    l->points = (XPoint *) XtCalloc(sizeof(XPoint), l->size);
    l->npoints = -1;
    l->tracking = False;

    XtVaSetValues(w, XtNcompress, True, NULL);

    OpAddEventHandler(w, opPixmap, ButtonPressMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opPixmap, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) release, l);

    SetPencilCursor(w);

    return l;
}

void 
FilledFreehandRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opPixmap, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);
    XtFree((XtPointer) ((LocalInfo *) l)->real);
    XtFree((XtPointer) ((LocalInfo *) l)->points);
    XtFree((XtPointer) l);
}
