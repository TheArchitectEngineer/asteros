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

/* $Id: arcOp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <math.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include "xpaint.h"
#include "Paint.h"
#include "PaintP.h"
#include "misc.h"
#include "ops.h"

static int arcMode = 0;

typedef struct {
    Boolean lastFlag, mode;
    int rx, ry, zoom, tracking;
    int startX, startY, midX, midY, endX, endY, a, b, cx, cy, t1, t2;
    GC lgc, gcx;
} LocalInfo;

static void 
press(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    /*
    **	Check to make sure all buttons are up, before doing this
     */
    if (event->button >= Button4) return;
    if ((event->state & AllButtonsMask) != 0)
	return;
    if (event->button == Button3) return;

    if (l->tracking == -1) {
        Global.escape = 0;
        l->mode = arcMode;
        l->endX = l->midX = l->startX = event->x;
        l->endY = l->midY = l->startY = event->y;
	l->zoom = 1;
	l->rx = 0;
	l->ry = 0;
	l->tracking = 0;
	if (event->button == Button1)
	    l->lgc = info->first_gc;
	else
	    l->lgc = info->second_gc;
    }
}

static void
DrawArc(Widget w, Drawable d, GC gc, LocalInfo * l)
{
    Display *dpy;
    XPoint *xpoint = NULL;
    int i, imax = 0;
    float x, y, xp, yp;
    float a0, b0, a1, b1, a2, b2, t, tp, u, v, coef, corr, norm;

    dpy = XtDisplay(w);

    if (l->mode >= 2) {
        if (l->tracking<=0) {
	    if (l->mode == 2) {
	        l->a = 2*abs(l->endX - l->startX);
	        l->b = 2*abs(l->endY - l->startY);
                if (l->lastFlag) {
		    if (l->b > l->a) l->a = l->b;
	            if (l->b < l->a) l->b = l->a;
		}
                l->cx = l->startX - l->a/2;
                l->cy = l->startY - l->b/2;
	    }
	    if (l->mode == 3) {
    	        if (l->endX >= l->startX) {
         	    l->cx = l->startX;
         	    l->a = l->endX - l->startX;
         	} else {
         	    l->cx = l->endX;
         	    l->a = l->startX - l->endX;
                }
         	if (l->endY >= l->startY) {
         	    l->cy = l->startY;
         	    l->b = l->endY - l->startY;
         	} else {
         	    l->cy = l->endY;
         	    l->b = l->startY - l->endY;
		}
                if (l->lastFlag) {
		    if (l->b > l->a) l->a = l->b;
	            if (l->b < l->a) l->b = l->a;
		}
	    }
	    l->t1 = 0;
	    l->t2 = 360*64;
            if (l->zoom>0)
                XDrawArc(dpy, d, gc, l->rx+l->cx/l->zoom, l->ry+l->cy/l->zoom, 
  		         l->a/l->zoom, l->b/l->zoom, l->t1, l->t2);
            else
                XDrawArc(dpy, d, gc, l->rx-l->cx*l->zoom, l->ry-l->cy*l->zoom, 
  		         -l->a*l->zoom, -l->b*l->zoom, l->t1, l->t2);
	}
        if (l->tracking>=1) {
	    x = (l->endX - l->cx - l->a/2) * l->b;
	    y = -(l->endY - l->cy - l->b/2) * l->a;
	    t = 0.0;
	    if (abs(x)>abs(y)) { 
                t = atan((float)y/(float)x);
		if (x<0) t = t+M_PI;
		if (t>M_PI) t = t-2*M_PI;
	    } else
	    if (y != 0) { 
                t = M_PI/2 - atan((float)x/(float)y);
		if (y<0) t = t-M_PI;
	    }
            t = 64.0*180.0*t/M_PI;
	    if (l->tracking == 1) {
	        l->t1 = t;
		l->t2 = 0;
	    }
	    if (l->tracking == 2) {
	        x = l->t2;
		l->t2 = t;
		if (l->t2 - x > 64*180) l->t2 -= 64*360;
		if (l->t2 - x < -64*180) l->t2 += 64*360;
	    }
            if (l->zoom>0)
                XDrawArc(dpy, d, gc, l->rx+l->cx/l->zoom, l->ry+l->cy/l->zoom, 
		         l->a/l->zoom, l->b/l->zoom, l->t1, l->t2-l->t1);
            else
                XDrawArc(dpy, d, gc, l->rx-l->cx*l->zoom, l->ry-l->cy*l->zoom, 
		         -l->a*l->zoom, -l->b*l->zoom, l->t1, l->t2-l->t1);
	}
	if (l->tracking == 20) {
	    if (l->zoom>0) {
	        UndoGrow(w, l->cx/l->zoom, l->cy/l->zoom);
	        UndoGrow(w, (l->cx+l->a)/l->zoom, (l->cy+l->b)/l->zoom);
	    } else {
	        UndoGrow(w, -l->cx*l->zoom, -l->cy*l->zoom);
	        UndoGrow(w, -(l->cx+l->a)*l->zoom, -(l->cy+l->b)*l->zoom);
	    }
	}
        return;
    }        

    if (l->tracking >= 10) {
        imax = 100;
        xpoint = (XPoint *) xmalloc((imax+1)*sizeof(XPoint));
        if (l->zoom>0) {
            xpoint[0].x = l->rx + l->startX / l->zoom;
            xpoint[0].y = l->ry + l->startY / l->zoom;
            xpoint[1].x = l->rx + l->endX / l->zoom;
            xpoint[1].y = l->ry + l->endY / l->zoom;
	} else {
            xpoint[0].x = l->rx - l->startX * l->zoom;
            xpoint[0].y = l->ry - l->startY * l->zoom;
            xpoint[1].x = l->rx - l->endX * l->zoom;
            xpoint[1].y = l->ry - l->endY * l->zoom;
	}
	if (l->tracking == 20)
	    UndoGrow(w, (int)xpoint[0].x, (int)xpoint[0].y);
    }

    if (l->tracking==1 && l->lastFlag) {
        l->midX = l->endX;
        l->midY = l->endY;
    }

    if (l->endX==l->startX && l->endY==l->startY) {
	if (xpoint) free(xpoint);
        return;
    }

    if (l->mode == 1) {
	l->a = 2 * abs(l->endX - l->startX);
	l->b = 2 * abs(l->endY - l->startY);
	l->t2 = 90*64;
	if ((!l->lastFlag && (l->endY > l->startY)) ||
	    (l->lastFlag && (l->endY < l->startY))) {
            l->cx = l->startX - l->a/2;
	    l->cy = l->endY - l->b/2;
	    if (l->lastFlag) {
	        if (l->endX < l->startX)
	            l->t1 = 180*64;
	        else
	            l->t1 = 270*64;
	    } else {
	        if (l->endX > l->startX)
	            l->t1 = 0;
	        else
	            l->t1 = 90*64;
	    }
	} else {
            l->cx = l->endX - l->a/2;
	    l->cy = l->startY - l->b/2;
	    if (l->lastFlag) {
	        if (l->endX > l->startX)
	            l->t1 = 180*64;
	        else
	            l->t1 = 270*64;
	    } else {
	        if (l->endX < l->startX)
	            l->t1 = 0;
	        else
	            l->t1 = 90*64;
	    }
	}
        if (l->zoom>0)
            XDrawArc(dpy, d, gc, l->rx+l->cx/l->zoom, l->ry+l->cy/l->zoom, 
                     l->a/l->zoom, l->b/l->zoom, l->t1, l->t2);
        else
            XDrawArc(dpy, d, gc, l->rx-l->cx*l->zoom, l->ry-l->cy*l->zoom, 
                     -l->a*l->zoom, -l->b*l->zoom, l->t1, l->t2);
        return;
    }
    if ((l->midX==l->startX && l->midY==l->startY) ||
        (l->midX==l->endX && l->midY==l->endY)) {
        if (l->tracking < 10)
            XDrawLine(dpy, d, gc, l->startX, l->startY, l->endX, l->endY);
	else
	    XDrawLines(dpy, d, gc, xpoint, 2, CoordModeOrigin);
	if (xpoint) free(xpoint);
	return;
    }

    a1 = l->endX - l->startX;
    b1 = l->endY - l->startY;
    a2 = l->midX - l->startX;
    b2 = l->midY - l->startY;
    corr = 1.0 + 120.0/(a2*a2+b2*b2);

    a0 = a1*a2 - b1*b2;
    b0 = a1*b2 + b1*a2;
    a2 = a2 - a1;
    b2 = b2 - b1;
    a1 = a1 - a2;
    b1 = b1 - b2;
    xp = l->startX;
    yp = l->startY;
    t = 0.00001;
    coef = sqrt(a0*a0 + b0*b0);
    i = 1;
    while (t <= 1.0) {
	x = xp;
	y = yp;
        u = a2 + a1*t;
        v = b2 + b1*t;
	norm = u*u+v*v;
        tp = t/norm;
        xp = l->startX + (a0*u + b0*v)*tp;
        yp = l->startY + (b0*u - a0*v)*tp;
	if (t >= 1.0) 
	    t = 2.0;
	else {
	    tp = sqrt(norm)/(corr*coef);
	    if (tp<0.0001) tp = 0.0001;
	    t = t + tp;
	    if (t>1.0) t = 1.0;
	}
	if (l->tracking < 10) {
	    XDrawLine(dpy, d, gc, rint(x), rint(y), rint(x), rint(y));
	    XDrawLine(dpy, d, gc, rint(x), rint(y), rint(xp), rint(yp));
	} else {
	    if (i>imax) {
	        imax = imax+10;
	        xpoint = (XPoint *)realloc(xpoint, (imax+1)*sizeof(XPoint));
	    }
            if (l->zoom>0) {
	        xpoint[i].x = rint(l->rx + xp / l->zoom + 0.5);
	        xpoint[i].y = rint(l->ry + yp / l->zoom + 0.5);
	    } else {
	        xpoint[i].x = rint(l->rx - xp * l->zoom + 0.5);
	        xpoint[i].y = rint(l->ry - yp * l->zoom + 0.5);
	    }
	    if (l->tracking == 20)
	        UndoGrow(w, (int)xpoint[i].x, (int)xpoint[i].y);
	    ++i;
	}
    }
    if (l->tracking >= 10) {
	XDrawLines(dpy, d, gc, xpoint, i, CoordModeOrigin);
	free(xpoint);
    }
}

static void 
motion(Widget w, LocalInfo * l,
       XMotionEvent * event, OpInfo * info)
{
    if (l->tracking == -1) return;
    if (l->tracking == 10) return;

    DrawArc(w, info->drawable, l->gcx, l);

    if (l->tracking == 0) {
        l->midX = l->endX = event->x;
        l->midY = l->endY = event->y;
    }
    if (l->tracking >= 1) {
        l->endX = event->x;
        l->endY = event->y;
    }

    /*
    **	Really set this flag in the if statement
     */
    l->lastFlag = event->state & ShiftMask;

    DrawArc(w, info->drawable, l->gcx, l);
}

static void 
release(Widget w, LocalInfo * l,
	XButtonEvent * event, OpInfo * info)
{
    XRectangle undo;
    int width, height, mask;

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

    if (l->tracking == -1) return;

    if (info->surface == opWindow) {

	if (l->mode == 1 && l->tracking == 0) {
	   l->tracking = 1;
	   return;
	}
        DrawArc(w, info->drawable, l->gcx, l);
       
        if (l->tracking == 0) {
            l->midX = l->endX = event->x;
            l->midY = l->endY = event->y;
	    if (l->endX != l->startX || l->endY != l->startY) {
	        l->tracking = 1;
  	        DrawArc(w, info->drawable, l->gcx, l);
	    }
        } else
        if (l->tracking >= 1) {
            l->endX = event->x;
            l->endY = event->y;
	    if (l->mode >= 2 && l->tracking == 1)
	        l->tracking = 2;
	    else
	        l->tracking = 10;
            if (l->tracking>=1 && l->endX == l->startX && l->endY == l->startY)
	        l->tracking = -1;
	}
	if (l->tracking == 10) {
	    if (Global.escape) {
	       l->tracking = -1;
               return;
	    }
            SetCapAndJoin(w, l->lgc,
                ((Global.cap)?Global.cap-1:CapButt),
		((Global.join)?Global.join-1:JoinRound));
	    if (!info->isFat)
                DrawArc(w, info->drawable, l->lgc, l);
	}
    } else {
        if (l->tracking == 10) {
	    if (Global.escape) {
	       Global.escape = 0;
	       l->tracking = -1;
               return;
	    }
	    l->tracking = 20;
            SetCapAndJoin(w, l->lgc,
                ((Global.cap)?Global.cap-1:CapButt),
		((Global.join)?Global.join-1:JoinRound));
	    XtVaGetValues(w, XtNzoom, &l->zoom, NULL);
            if (l->zoom>0) {
                l->rx = info->x - l->endX / l->zoom;
                l->ry = info->y - l->endY / l->zoom;
	    } else {
                l->rx = info->x + l->endX * l->zoom;
                l->ry = info->y + l->endY * l->zoom;
	    }
	    UndoStart(w, info);
            DrawArc(w, info->drawable, l->lgc, l);

            XtVaGetValues(w, XtNdrawWidth, &width, 
                             XtNdrawHeight, &height, NULL);
            undo.x = 0;
            undo.y = 0;
            undo.width = width;
            undo.height = height;

            if (info->isFat && (info->surface == opPixmap))
	        PwUpdate(w, &undo, True);
	    else
	        PwUpdate(w, &undo, False);
            l->tracking = -1;
	}
    }
}

/*
**  Those public functions
 */
void *
ArcAdd(Widget w)
{
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));

    l->gcx = GetGCX(w);
    l->tracking = -1;
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
ArcRemove(Widget w, void *l)
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
ArcSetMode(int mode)
{
    arcMode = mode;
}
