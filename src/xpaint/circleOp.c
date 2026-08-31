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

/* $Id: circleOp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <math.h>
#include "xpaint.h"
#include "misc.h"
#include "Paint.h"
#include "ops.h"

#define	FILL		0x02
#define CIRCLE		0x01
#define	ISFILL(x)	((x) & FILL)
#define ISCIRCLE(x)	((x) & CIRCLE)

typedef struct {
    Boolean typeFlag, swap;
    int rx, ry, startX, startY, endX, endY, drawn, flag;
    GC gcx;
} LocalInfo;

static int radiusMode = False;

static XRectangle *
draw(Widget w, Drawable d, GC lgc, GC fgc, LocalInfo * l)
{
    static XRectangle rect;
    XArc arc;
    int sx = l->startX, sy = l->startY;
    int ex = l->endX, ey = l->endY;
    int dx = ABS(sx - ex), dy = ABS(sy - ey);

    if ((sx == ex) && (sy == ey))
	return NULL;

    if (ISCIRCLE(l->typeFlag))
	dx = dy = MAX(dx, dy);

    if (radiusMode) {
	arc.x = sx - dx;
	arc.y = sy - dy;
	arc.width = 2 * dx;
	arc.height = 2 * dy;
    } else {
	if (sx > ex)
	    arc.x = ex;
	else
	    arc.x = sx;
	if (sy > ey)
	    arc.y = ey;
	else
	    arc.y = sy;
	arc.width = dx;
	arc.height = dy;
    }
    arc.angle1 = 0;
    arc.angle2 = 360 * 64;

    if (fgc != None)
	XFillArcs(XtDisplay(w), d, fgc, &arc, 1);
    if (lgc != None)
	XDrawArcs(XtDisplay(w), d, lgc, &arc, 1);

    rect.x = arc.x;
    rect.y = arc.y;
    rect.width = arc.width + 1;
    rect.height = arc.height + 1;

    return &rect;
}

static void 
press(Widget w, LocalInfo * l,
      XButtonEvent * event, OpInfo * info)
{
    /*
    **	Check to make sure all buttons are up, before doing this
     */
   
    if (l->drawn==-1) Global.escape = 0;
   
    if (event->button >= Button4) return;      
    if ((event->state & AllButtonsMask) != 0)
	return;
    if (event->button == Button3) return;

    SetCapAndJoin(w, info->first_gc,
                  ((Global.cap)?Global.cap-1:CapButt),
		  ((Global.join)?Global.join-1:JoinRound));
    SetCapAndJoin(w, info->second_gc,
                  ((Global.cap)?Global.cap-1:CapButt),
		  ((Global.join)?Global.join-1:JoinRound));

    l->rx = info->x;
    l->ry = info->y;
    l->endX = l->startX = event->x;
    l->endY = l->startY = event->y;

    l->swap = (event->button == Button2);

    l->typeFlag = (event->state & ShiftMask) ? CIRCLE : 0;

    l->drawn = 0;
}

static void 
motion(Widget w, LocalInfo * l,
       XMotionEvent * event, OpInfo * info)
{
    if (l->drawn == -1) 
        return;

    if (l->drawn)
	draw(w, info->drawable, l->gcx, None, l);

    l->endX = event->x;
    l->endY = event->y;

    /*
    **	Really set this flag in the if statement
     */
    l->typeFlag = (event->state & ShiftMask) ? CIRCLE : 0;
    l->drawn = (draw(w, info->drawable, l->gcx, None, l) != NULL);
}

static void 
release(Widget w, LocalInfo * l,
	XButtonEvent * event, OpInfo * info)
{
    XRectangle *undo;
    int mask;
    GC fgc, lgc;

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

    if (l->drawn && info->surface == opWindow) {
	draw(w, info->drawable, l->gcx, None, l);
        if (Global.escape) return;        
    }

    if (Global.escape && info->surface == opPixmap) {
       Global.escape = 0;
       l->drawn = -1;
       return;
    }
   
    if (info->surface == opWindow && info->isFat)
	return;
   
    UndoStart(w, info);

    l->startX = l->rx;
    l->startY = l->ry;
    l->endX = event->x;
    l->endY = event->y;

    if (l->swap) {
	fgc = info->first_gc;
	lgc = info->second_gc;
    } else {
	fgc = info->second_gc;
	lgc = info->first_gc;
    }

    if (!ISFILL(l->flag))
	fgc = None;

    undo = draw(w, info->drawable, lgc, fgc, l);

    if (info->surface == opPixmap && undo != NULL) {
	UndoSetRectangle(w, undo);
	PwUpdate(w, undo, False);    
    }

    l->drawn = -1;
}

/*
**  Those public functions
 */
void *
CircleAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));
    l->flag = CIRCLE;
    l->drawn = -1;
    l->gcx = GetGCX(w);
    XtVaSetValues(w, XtNcompress, True, NULL);
    OpAddEventHandler(w, opWindow, ButtonPressMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) release, l);
    SetCrossHairCursor(w);
    return l;
}

void 
CircleRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);
    XtFree((XtPointer) l);
}

void *
FilledCircleAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));
    l->flag = CIRCLE | FILL;
    l->drawn = -1;
    l->gcx = GetGCX(w);
    XtVaSetValues(w, XtNcompress, True, NULL);
    OpAddEventHandler(w, opWindow, ButtonPressMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) release, l);
    SetCrossHairCursor(w);
    return l;
}

void 
FilledCircleRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);
    XtFree((XtPointer) l);
}

void *
OvalAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));
    l->flag = 0;
    l->drawn = -1;
    l->gcx = GetGCX(w);
    XtVaSetValues(w, XtNcompress, True, NULL);
    OpAddEventHandler(w, opWindow, ButtonPressMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) release, l);
    SetCrossHairCursor(w);
    return l;
}

void 
OvalRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);
    XtFree((XtPointer) l);
}

void *
FilledOvalAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));
    l->flag = FILL;
    l->drawn = -1;
    l->gcx = GetGCX(w);
    XtVaSetValues(w, XtNcompress, True, NULL);

    OpAddEventHandler(w, opWindow, ButtonPressMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) release, l);
    SetCrossHairCursor(w);
    return l;
}

void 
FilledOvalRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);
    XtFree((XtPointer) l);
}

void 
CircleSetStyle(Boolean value)
{
    radiusMode = value;
}

Boolean
CircleGetStyle(void)
{
    return radiusMode;
}
