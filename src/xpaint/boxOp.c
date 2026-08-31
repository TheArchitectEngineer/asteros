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

/* $Id: boxOp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtWindow XTWINDOW
#endif

#include <math.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include "xpaint.h"
#include "misc.h"
#include "Paint.h"
#include "ops.h"

typedef struct {
    int rx, ry;
    int startX, startY, endX, endY;
    int drawn;
    Boolean type;		/* 0: rectangle, 1: square,
				   2: centered rectangle, 3: centered square */
    Boolean fill;		/* Filled rectangle? */
    Boolean swap;
    GC gcx;
} LocalInfo;

#define MKRECT(rect, sx, sy, ex, ey, typeFlag) {			\
	if (typeFlag == 1) {	/* square */				\
		(rect)->width  = MAX(ABS(sx - ex),ABS(sy - ey));	\
		(rect)->height = (rect)->width;				\
		(rect)->x = (ex - sx < 0) ? sx - (rect)->width : sx;	\
		(rect)->y = (ey - sy < 0) ? sy - (rect)->height : sy;	\
	} else if (typeFlag == 2) {	/* center */			\
		(rect)->x      = sx - ABS(sx - ex);			\
		(rect)->y      = sy - ABS(sy - ey);			\
		(rect)->width  = 2*ABS(sx - ex);			\
		(rect)->height = 2*ABS(sy - ey);			\
	} else if (typeFlag == 3) {	/* center, square */		\
		(rect)->width  = MAX(ABS(sx - ex),ABS(sy - ey));	\
		(rect)->x      = sx - (rect)->width;			\
		(rect)->y      = sy - (rect)->width;			\
		(rect)->width  *= 2;					\
		(rect)->height = (rect)->width;				\
	} else {		/* no constraints */			\
		(rect)->x      = MIN(sx, ex);				\
		(rect)->y      = MIN(sy, ey);				\
		(rect)->width  = MAX(sx, ex) - (rect)->x;		\
		(rect)->height = MAX(sy, ey) - (rect)->y;		\
	}								\
}

static int boxStyle = 0;
static int boxSize = 7;
static float boxRatio = 0.0;

static void 
press(Widget w, LocalInfo * l,
      XButtonEvent * event, OpInfo * info)
{
    /*
    **	Check to make sure all buttons are up, before doing this
     */
    Global.escape = 0;
   
    if (event->button >= Button4) return;   
    if ((event->state & AllButtonsMask) != 0)
	return;
    if (event->button == Button3) return;

    l->swap = (event->button == Button2);

    l->rx = info->x;
    l->ry = info->y;
    l->endX = l->startX = event->x;
    l->endY = l->startY = event->y;
    SetCapAndJoin(w, info->first_gc,
                  ((Global.cap)?Global.cap-1:CapButt),
		  ((Global.join)?Global.join-1:JoinMiter));
    SetCapAndJoin(w, info->second_gc,
                  ((Global.cap)?Global.cap-1:CapButt),
		  ((Global.join)?Global.join-1:JoinMiter));

    l->type = 2 * (boxStyle & 1) + ((event->state & ShiftMask) ? 1 : 0);
    l->drawn = 0;
}

void
DrawBox(Display *dpy, Drawable d, GC gc, XRectangle *rect, Boolean fill)
{
    XPoint *xp;
    int i, n, p;
    float a, r, c, s;

    if ((boxStyle & 2) == 0 || (boxSize == 0 && boxRatio == 0.0)) {
        if (fill)
            XFillRectangles(dpy, d, gc, rect, 1);
        else
            XDrawRectangles(dpy, d, gc, rect, 1);
    } else {
        if (boxSize>0)
	    r = MIN(boxSize, MIN(rect->width, rect->height)/2);
        else
            r = 0.5 * MIN(rect->width, rect->height) * boxRatio;
        p = 3 + 4.0 * sqrt(r);
        n = 4 * p + 1;
        xp = (XPoint *) XtMalloc(n * sizeof(XPoint));
        for (i=0; i<p; i++) {
  	    a = i*M_PI/(2*p-2);
            c = 1.0 - cos(a);
            s = 1.0 - sin(a);
            xp[i].x = rect->x + r * c + 0.5;
            xp[i].y = rect->y + r * s + 0.5;
            xp[i+p].x = rect->x + rect->width - r * s + 0.5;
            xp[i+p].y = rect->y + r * c + 0.5;
            xp[i+2*p].x = rect->x + rect->width - r * c + 0.5;
            xp[i+2*p].y = rect->y + rect->height - r * s + 0.5;
            xp[i+3*p].x = rect->x + r * s + 0.5;
            xp[i+3*p].y = rect->y + rect->height - r * c + 0.5;
	}
        xp[n-1] = xp[0];
        if (fill)
            XFillPolygon(dpy, d, gc, xp, n, Complex, CoordModeOrigin);
        else
            XDrawLines(dpy, d, gc, xp, n, CoordModeOrigin);
        XtFree((XtPointer) xp);
    }
}

static void 
motion(Widget w, LocalInfo * l,
       XMotionEvent * event, OpInfo * info)
{
    XRectangle rect;

    if (l->drawn == -1)
        return;

    if (l->drawn) {
	MKRECT(&rect, l->startX, l->startY, l->endX, l->endY, l->type);
        DrawBox(XtDisplay(w), XtWindow(w), l->gcx, &rect, 0);
    }
    l->endX = event->x;
    l->endY = event->y;

    l->type = 2 * (boxStyle & 1) + ((event->state & ShiftMask) ? 1 : 0);

    if ((l->drawn = (l->startX != l->endX || l->startY != l->endY))) {
	MKRECT(&rect, l->startX, l->startY, l->endX, l->endY, l->type);
        DrawBox(XtDisplay(w), XtWindow(w), l->gcx, &rect, 0);
    }
}

static void 
release(Widget w, LocalInfo * l,
	XButtonEvent * event, OpInfo * info)
{
    XRectangle rect;
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
	MKRECT(&rect, l->startX, l->startY, l->endX, l->endY, l->type);
        DrawBox(XtDisplay(w), XtWindow(w), l->gcx, &rect, 0);
    }

    if (info->isFat && info->surface == opWindow)
	return;

    MKRECT(&rect, l->rx, l->ry, event->x, event->y, l->type);
    
    if (Global.escape) {
       if (info->surface == opWindow) return;
       Global.escape = 0;
       l->drawn = -1;
       return;
    }

    UndoStart(w, info);

    if (l->swap) {
	fgc = info->first_gc;
	lgc = info->second_gc;
    } else {
	fgc = info->second_gc;
	lgc = info->first_gc;
    }

    if (l->fill)
	DrawBox(XtDisplay(w), info->drawable, fgc, &rect, 1);
    DrawBox(XtDisplay(w), info->drawable, lgc, &rect, 0);


    if (info->surface == opPixmap) {
	rect.width++;
	rect.height++;
	UndoSetRectangle(w, &rect);
	PwUpdate(w, &rect, False);
    }

    l->drawn = -1;
}

/*
**  Public routines
 */

void *
BoxAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));

    l->fill = False;
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
BoxRemove(Widget w, void *l)
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
FilledBoxAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));

    l->fill = True;
    l->drawn = -1;
    l->gcx = GetGCX(w);

    XtVaSetValues(w, XtNcompress, False, NULL);

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
FilledBoxRemove(Widget w, void *l)
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
BoxSetStyle(int mode)
{
    boxStyle = mode;
}

int
BoxGetStyle(void)
{
    return boxStyle;
}

void 
BoxSetParameter(int b_size, float b_ratio)
{
    boxSize = b_size;
    boxRatio = b_ratio;
}
