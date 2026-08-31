/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)            | */
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

/* $Id: pencilOp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtWindow XTWINDOW
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include "xpaint.h"
#include "Paint.h"
#include "misc.h"
#include "ops.h"

typedef struct {
    Boolean isDots, tracking;
    int startX, startY;
    GC gc;
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

    l->startX = event->x;
    l->startY = event->y;
    l->tracking = True;

    undo.x = event->x;
    undo.y = event->y;
    undo.width = 1;
    undo.height = 1;

    if (event->button == Button2)
	l->gc = info->second_gc;
    else
	l->gc = info->first_gc;

    save = Global.dashnumber;
    Global.dashnumber = 0;
    SetCapAndJoin(w, l->gc,
                  ((Global.cap)?Global.cap-1:CapRound),
		  ((Global.join)?Global.join-1:JoinRound));
    Global.dashnumber = save;

    if (l->isDots) {
	GC gc = XCreateGC(XtDisplay(w), info->drawable, 0, 0);
	XCopyGC(XtDisplay(w), l->gc, ~GCLineWidth, gc);
	l->gc = gc;
    }
    UndoStartPoint(w, info, event->x, event->y);

    XDrawLine(XtDisplay(w), info->drawable, l->gc,
	      l->startX, l->startY, event->x, event->y);
    if (!info->isFat)
	XDrawLine(XtDisplay(w), XtWindow(w), l->gc,
		  l->startX, l->startY, event->x, event->y);

    PwUpdate(w, &undo, False);
}

static void 
motion(Widget w, LocalInfo * l, XMotionEvent * event, OpInfo * info)
{
    XRectangle undo;

    if (!l->tracking) return;
    if (info->surface == opWindow)
	return;
    if (!l->gc) return;

    if (l->isDots) {
	l->startX = event->x;
	l->startY = event->y;
    }
    XDrawLine(XtDisplay(w), info->drawable, l->gc,
	      l->startX, l->startY, event->x, event->y);
    if (!info->isFat)
	XDrawLine(XtDisplay(w), XtWindow(w), l->gc,
		  l->startX, l->startY, event->x, event->y);

    UndoGrow(w, event->x, event->y);

    undo.x = MIN(l->startX, event->x);
    undo.y = MIN(l->startY, event->y);
    undo.width = MAX(l->startX, event->x) - undo.x + 1;
    undo.height = MAX(l->startY, event->y) - undo.y + 1;

    l->startX = event->x;
    l->startY = event->y;

    PwUpdate(w, &undo, False);
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
    if (event->button >= Button3) return;

    if (l->isDots) {
	XFreeGC(XtDisplay(w), l->gc);
        l->gc = None;
    }
    l->tracking = False;
}

/*
**  Those public functions
 */
void *
PencilAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));

    l->isDots = False;
    l->gc = 0;
    l->tracking = False;
    XtVaSetValues(w, XtNcompress, False, NULL);

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
PencilRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opPixmap, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);
    XtFree((XtPointer) l);
}

void *
DotPencilAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));

    l->isDots = True;
    l->gc = 0;
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
DotPencilRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opPixmap, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);
    XtFree((XtPointer) l);
}
