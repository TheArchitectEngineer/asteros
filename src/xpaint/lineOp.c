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

/* $Id: lineOp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#endif

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <math.h>
#include <stdlib.h>
#include "xpaint.h"
#include "Paint.h"
#include "PaintP.h"
#include "misc.h"
#include "Paint.h"
#include "ops.h"

#define	GET_PW(w)	((PaintWidget)(((PaintWidget)w)->paint.paint == None ? \
				       w : ((PaintWidget)w)->paint.paint))
static Boolean multiLine = False;
static Boolean withArrow = False;
static int headType = 1;
static int headSize = 15;
static double headAngle = 25.0;

typedef struct {
    int type;
    Boolean didUndo, headArrow, tracking;
    int rx, ry;
    int startX, startY, endX, endY;
    int drawn;
    Drawable drawable;
    GC gcx;
} LocalInfo;

static void
DrawArrow (Widget w, Drawable d, GC gc, int stX,int stY, int enX, int enY,
           Boolean headArrow, Boolean dozoom)
{
  XPoint xpoints[4];
  Display *dpy; 
  int lw;
  int i, ux, uy, lx=0, ly=0, mx=0, my=0, z;
  int DeltaX, DeltaY;
  float norm, ll, mm;

  dpy = XtDisplay(w);
  XtVaGetValues(w, XtNlineWidth, &lw, XtNzoom, &z, NULL);
  ux = uy = 0;

  if (withArrow || headArrow) {
    DeltaX = enX - stX;
    DeltaY = enY - stY;
    norm = sqrt(pow(DeltaX, 2) + pow(DeltaY, 2)) + 0.001;
   
    if ( lw <= 2 || headArrow)
      ll = (float) headSize;
    else
      ll = (float) (4*(lw-1)+headSize);

    if (dozoom) {
       if (z>0) ll = ll * (float) z;
       if (z<0) ll = ll / (float)(-z);
    }

    mm = ll * tan(headAngle*M_PI/180.0) +0.5;
   
    lx = (int) (0.49 + (float) (abs(DeltaX) * ll) / norm);
    ly = (int) (0.49 + (float) (abs(DeltaY) * ll) / norm);
    mx = (int) (0.49 + (float) (abs(DeltaY) * mm) / norm);
    my = (int) (0.49 + (float) (abs(DeltaX) * mm) / norm);
    my = -my;
   
    if (DeltaX < 0) {
         lx = -lx; 
         my = -my;
    }
    if (DeltaY < 0) {
         ly = -ly; 
         mx = -mx;
    }
    if (lw>0) {
       ux = 9*lx/10;
       uy = 9*ly/10;
    }
    if (headType >= 2) {
       ux /= headType;
       uy /= headType;
    }
  }
   
  if (!headArrow)
    XDrawLine(dpy, d, gc, stX, stY, enX-ux, enY-uy);

  if (withArrow || headArrow) {
    if (headArrow) {
       xpoints[0].x = stX;
       xpoints[0].y = stY;
    } else {
       xpoints[0].x = enX;
       xpoints[0].y = enY;
    }
    if (headType == 1) {
       xpoints[1].x = xpoints[0].x - lx + mx;
       xpoints[1].y = xpoints[0].y - ly + my;
       xpoints[2].x = xpoints[0].x - lx - mx;
       xpoints[2].y = xpoints[0].y - ly - my;
       z = 3;
    } else
       z = 4;
    if (headType == 2) {
       xpoints[1].x = xpoints[0].x - lx + mx;
       xpoints[1].y = xpoints[0].y - ly + my;
       xpoints[2].x = xpoints[0].x - lx/2;
       xpoints[2].y = xpoints[0].y - ly/2;
       xpoints[3].x = xpoints[0].x - lx - mx;
       xpoints[3].y = xpoints[0].y - ly - my;
    }
    if (headType == 3) {
       xpoints[1].x = xpoints[0].x - lx + mx;
       xpoints[1].y = xpoints[0].y - ly + my;
       xpoints[2].x = xpoints[0].x - lx/3;
       xpoints[2].y = xpoints[0].y - ly/3;
       xpoints[3].x = xpoints[0].x - lx - mx;
       xpoints[3].y = xpoints[0].y - ly - my;
       XFillPolygon(dpy, d, gc, xpoints, 4, Complex, CoordModeOrigin);
       if (!dozoom)
	   for (i=0; i<4; i++) UndoGrow(w, (int)xpoints[i].x,(int)xpoints[i].y);
       lx /= 3;
       ly /= 3;
       for (i=0; i<4; i++) {
           xpoints[i].x -= lx;
           xpoints[i].y -= ly;
       }
    }
    XFillPolygon(dpy, d, gc, xpoints, z, Complex, CoordModeOrigin);
    if (!dozoom)
       for (i=0; i<z; i++) UndoGrow(w, (int)xpoints[i].x, (int)xpoints[i].y);
  }
}
 
static void 
press(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    /*
    **	Check to make sure all buttons are up, before doing this
     */
   
    Global.escape = 0;
   
    if ((event->state & AllButtonsMask) != 0)
	return;

    if (event->button >= Button4) return;
   
    if (event->button == Button1 && ((!l->tracking && multiLine) || !multiLine)) {
	l->rx = info->x;
	l->ry = info->y;
	l->startX = event->x;
	l->startY = event->y;

	l->drawable = info->drawable;
	l->drawn = False;
	l->didUndo = False;
	l->tracking = True;
    } else if (event->button == Button2) {
	if (l->drawn)
	    DrawArrow(w, info->drawable, l->gcx,
		      l->startX, l->startY, l->endX, l->endY, l->headArrow, 1);
	l->tracking = False;
	l->drawn = False;
        return;
    }
    if (l->tracking) {
	if (l->drawn)
	    DrawArrow(w, info->drawable, l->gcx,
		      l->startX, l->startY, l->endX, l->endY, l->headArrow, 1);

	l->endX = event->x;
	l->endY = event->y;

	DrawArrow(w, info->drawable, l->gcx,
		  l->startX, l->startY, l->endX, l->endY, l->headArrow, 1);
	l->drawn = True;
    }
}

static void 
motion(Widget w, LocalInfo * l, XMotionEvent * event, OpInfo * info)
{

    if (!l->tracking)
	return;

    if (l->drawn)
	DrawArrow(w, info->drawable, l->gcx,
		  l->startX, l->startY, l->endX, l->endY, l->headArrow, 1);

    l->type = (event->state & ShiftMask);

    if (l->type) {
	int ax = ABS(event->x - l->startX);
	int ay = ABS(event->y - l->startY);
	int dx = MIN(ax, ay);
	int v, v1 = dx - ax, v2 = dx - ay;
	int addX, addY;

	v = v1 * v1 + v2 * v2;

	if (ay * ay < v) {
	    addX = event->x - l->startX;
	    addY = 0;
	} else if (ax * ax < v) {
	    addX = 0;
	    addY = event->y - l->startY;
	} else {
	    if (ax < ay) {
		addX = event->x - l->startX;
		addY = SIGN(event->y - l->startY) * ax;
	    } else {
		addX = SIGN(event->x - l->startX) * ay;
		addY = event->y - l->startY;
	    }
	}

	l->endX = l->startX + addX;
	l->endY = l->startY + addY;
    } else {
	l->endX = event->x;
	l->endY = event->y;
    }

    /*
    **	Really set this flag in the if statement
     */
    if ((l->drawn = (l->startX != l->endX || l->startY != l->endY)))
	DrawArrow(w, info->drawable, l->gcx,
		  l->startX, l->startY, l->endX, l->endY, l->headArrow, 1);
}

static void 
release(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    XRectangle *undo = NULL;
    int mask;

    if (!l->tracking && ((info->surface == opWindow) || l->didUndo))
	return;

    /*
    **	Check to make sure all buttons are up, before doing this
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

    if (l->drawn && info->surface == opWindow) {
	DrawArrow(w, info->drawable, l->gcx,
		  l->startX, l->startY, l->endX, l->endY, l->headArrow, 1);
	l->drawn = False;
        if (Global.escape) return;
    }
   
    if (Global.escape && info->surface == opPixmap) {
	Global.escape = 0;
        l->tracking = False;
        return;
    }

    if (info->surface == opWindow && info->isFat)
	return;

    if (!l->didUndo && info->surface == opPixmap) {
        PaintWidget pw = (PaintWidget) GET_PW(w);
        UndoStart(w, info);
	undo = &pw->paint.undo->box;
        undo->x = l->rx;
        undo->y = l->ry;
        undo->width = pw->paint.lineWidth * 2 + 1;
        undo->height = undo->width;
	l->didUndo = True;
    }

    if (l->type) {
	int ax = ABS(event->x - l->rx);
	int ay = ABS(event->y - l->ry);
	int dx = MIN(ax, ay);
	int v, v1 = dx - ax, v2 = dx - ay;
	int addX, addY;

	v = v1 * v1 + v2 * v2;

	if (ay * ay < v) {
	    addX = event->x - l->rx;
	    addY = 0;
	} else if (ax * ax < v) {
	    addX = 0;
	    addY = event->y - l->ry;
	} else {
	    if (ax < ay) {
		addX = event->x - l->rx;
		addY = SIGN(event->y - l->ry) * ax;
	    } else {
		addX = SIGN(event->x - l->rx) * ay;
		addY = event->y - l->ry;
	    }
	}

	l->endX = l->rx + addX;
	l->endY = l->ry + addY;
    } else {
	l->endX = event->x;
	l->endY = event->y;
    }

    SetCapAndJoin(w, info->first_gc, 
                  ((Global.cap)?Global.cap-1:CapButt),
                  ((Global.join)?Global.join-1:JoinMiter));
    DrawArrow(w, info->drawable, info->first_gc,
	             l->rx, l->ry, l->endX, l->endY, l->headArrow, 0);

    if (info->surface == opPixmap) {
	UndoGrow(w, l->rx, l->ry);
	UndoGrow(w, l->endX, l->endY);
	PwUpdate(w, undo, False);
    }

    if (!multiLine || l->headArrow)
        l->tracking = False;
}

/*
**  Those public functions
 */
void *
LineAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));

    l->tracking = False;
    l->gcx = GetGCX(w);
    l->headArrow = False;

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
LineRemove(Widget w, void *p)
{
    LocalInfo *l = (LocalInfo *) p;

    OpRemoveEventHandler(w, opWindow, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);

    if (multiLine && l->tracking && l->drawn)
	DrawArrow(w, l->drawable, l->gcx,
		  l->startX, l->startY, l->endX, l->endY, l->headArrow, 1);

    XtFree((XtPointer) l);
}

void *
ArrowAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));

    l->tracking = False;
    l->gcx = GetGCX(w);
    l->headArrow = True;

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
ArrowRemove(Widget w, void *p)
{
    LocalInfo *l = (LocalInfo *) p;

    OpRemoveEventHandler(w, opWindow, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);

    if (multiLine && l->tracking && l->drawn)
	DrawArrow(w, l->drawable, l->gcx,
		  l->startX, l->startY, l->endX, l->endY, l->headArrow, 1);

    XtFree((XtPointer) l);
}

Boolean
MultiGetStyle(void)
{
    return multiLine;
}

void 
MultiSetStyle(Boolean flag)
{
    multiLine = flag;
}

Boolean
ArrowGetStyle(void)
{
    return withArrow;
}

void 
ArrowSetStyle(Boolean flag)
{
    withArrow = flag;
}

void
DashSetStyle(char *dashstyle)
{
    int i, j, len, shift;
    char c;

    len = strlen(dashstyle);
    if (len > 64) {
        dashstyle[64] = '\0';
	len = 64;
    }
    Global.dashoffset = 0;
    c = dashstyle[0];
    j = 0;
    if (c == '=')
        shift = 0;
    else
        shift = 1;
    Global.dashnumber = 0;

    for (i = 0; i <= len; i++) 
        if (dashstyle[i] == c)
            ++j;
        else {
	    Global.dashlist[Global.dashnumber+shift] = j;
	    ++Global.dashnumber;
	    c = dashstyle[i];
            j = 1;
	}

    if (shift)
        Global.dashlist[0] = Global.dashlist[Global.dashnumber];
    Global.dashlist[Global.dashnumber] = '\0';
        
    if (Global.dashnumber >= 2) {
        if (shift)
            Global.dashoffset = (int) Global.dashlist[0];
        else
            Global.dashoffset = 0;
    }
    else
        Global.dashnumber = 0;
}

/*
**  Those public functions
*/
void
ArrowHeadSetParameters(int t, int s, float a)
{
    headType = t;
    headSize = s;
    headAngle = a;
}
