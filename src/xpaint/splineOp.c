
/* +-------------------------------------------------------------------+ */
/* | Copyright 2000, J.-P. Demailly (demailly@fourier.ujf-grenoble.fr) | */
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

/* $Id: splineOp.c,v 1.15 2005/03/20 20:15:32 demailly Exp $ */

#include <math.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include "PaintP.h"
#include "xpaint.h"
#include "misc.h"
#include "Paint.h"
#include "ops.h"

#define SUBDIV 50
#define FLOAT float

enum {OPEN=0, CLOSED, CLOSEDUP};
enum {FINISH, ALLSTEPS, ERASE, DRAW};

static int Mode = 0;
static int ModeCtrl;
static int ModeShift;
static int Filled = 0;
static int MAXPOINTS = 30;

typedef struct {
    int zoom, rx, ry, mode, npoints, nprev;
    XPoint *points;
    XPoint *interm1;
    XPoint *interm2;
    Boolean *breakpt;
    /*
    **  Borrowed from my info structure.
     */
    GC gcx, fgc, sgc;
    Pixmap pixmap;
    Boolean isFat;
} LocalInfo;

void
SplineSetMode(int mode)
{
    Mode = mode;
}

static void 
XDrawContour(Widget w, Drawable d, LocalInfo *l, int flag)
{
    Display *dpy;
    GC gc;
    XPoint *xp;
    int n, i, j, imax, i0, i1=0, zoom, rx, ry, bool;
    int u, v, up, vp;
    FLOAT ax=0.0, ay=0.0, bx, by, cx, cy, r=0.0, s, t;

    dpy = XtDisplay(w);
    bool = (d==XtWindow(w));

    if (flag == FINISH && d == l->pixmap) {
        zoom = l->zoom;
        rx = l->rx;
	ry = l->ry;
    } else {
        zoom = 1;
	rx = 0;
	ry = 0;
    }

    if (flag == ERASE || flag == ALLSTEPS) {
        n = l->nprev;
    }
    else {
        n = l->npoints;
	l->mode = ModeCtrl;
    }

    if (n <= 0)
        return;

    if (flag == FINISH)
        gc = l->fgc;
    else
        gc = l->gcx;

    if (n == 1) {
         xp = (XPoint *) xmalloc(2 * sizeof(XPoint));
         if (zoom>0) {
	     xp[0].x = rx + l->points[0].x / zoom;
	     xp[0].y = ry + l->points[0].y / zoom;
	     xp[1].x = rx + l->points[1].x / zoom;
	     xp[1].y = ry + l->points[1].y / zoom;
	 } else {
	     xp[0].x = rx + l->points[0].x;
	     xp[0].y = ry + l->points[0].y;
	     xp[1].x = rx + l->points[1].x;
	     xp[1].y = ry + l->points[1].y;
	 }
         if (bool && l->zoom<0) {
	     XDrawLine(dpy, d, gc, xp[0].x/(-l->zoom), xp[0].y/(-l->zoom), 
                                   xp[0].x/(-l->zoom), xp[0].y/(-l->zoom));
             XDrawLine(dpy, d, gc, xp[0].x/(-l->zoom), xp[0].y/(-l->zoom),
                                   xp[1].x/(-l->zoom), xp[1].y/(-l->zoom));
	 } else {
             XDrawLine(dpy, d, gc, xp[0].x, xp[0].y, xp[0].x, xp[0].y);
             XDrawLine(dpy, d, gc, xp[0].x, xp[0].y, xp[1].x, xp[1].y);
	 }
         if (flag == FINISH && d == l->pixmap)
             for (i=0; i<=1; i++) UndoGrow(w, xp[i].x, xp[i].y);
         l->mode = ModeCtrl;
         l->nprev = l->npoints;
	 free(xp);
         return;
    }

    if (l->mode != OPEN) {
        j = n+2;
        l->points[n+1].x = l->points[0].x;
        l->points[n+1].y = l->points[0].y;
	l->breakpt[n+1] = l->breakpt[0];
        l->points[n+2].x = l->points[1].x;
        l->points[n+2].y = l->points[1].y;
	l->breakpt[n+2] = l->breakpt[1];
    }
    else 
        j = n;

    i0 = n-2;
    if (i0 < 0)
        i0 = 0;
    for (i = i0; i < j; i++) {
        s = r;
	bx = ax;
	by = ay;
	ax = l->points[i+1].x - l->points[i].x;
	ay = l->points[i+1].y - l->points[i].y;
	r = ax*ax + ay*ay + 1E-9;
	ax = ax/r; ay = ay/r;
	r = sqrt(r)/3.0;
	if (i > i0) { 
	    if (l->breakpt[i]) {
	        t = 3.0*r*r;
	        l->interm1[i].x = l->points[i].x + (short)(t*ax);
	        l->interm1[i].y = l->points[i].y + (short)(t*ay);
	        t = 3.0*s*s;
	        l->interm2[i-1].x = l->points[i].x - (short)(t*bx);
	        l->interm2[i-1].y = l->points[i].y - (short)(t*by);
	    } else {
	        cx = (ax+bx)/2; 
                cy = (ay+by)/2;
	        t = sqrt(cx*cx + cy*cy + 1E-9);
	        cx = cx/t; 
	        cy = cy/t;
	        l->interm1[i].x = l->points[i].x + (short)(r*cx);
	        l->interm1[i].y = l->points[i].y + (short)(r*cy);
	        l->interm2[i-1].x = l->points[i].x - (short)(s*cx);
	        l->interm2[i-1].y = l->points[i].y - (short)(s*cy);
	    }
	    if ((i == 1) && (l->mode == OPEN || l->breakpt[i])) {
	        t = 3.0*s*s;
		l->interm1[0].x = l->interm2[0].x - (short)(t*bx);
		l->interm1[0].y = l->interm2[0].y - (short)(t*by);
	    }
	    if ((i == n-1) && (l->mode == OPEN || l->breakpt[i])) {
	        t = 3.0*r*r;
		l->interm2[i].x = l->interm1[i].x + (short)(t*ax);
		l->interm2[i].y = l->interm1[i].y + (short)(t*ay);
	    }
	    if (i == n+1 && !l->breakpt[0]) {
	        l->interm1[0].x = l->interm1[i].x;
		l->interm1[0].y = l->interm1[i].y;
	    }
	}
    }
 
    imax = n;
    if (((l->mode == CLOSED) && (flag == FINISH)) || (l->mode == CLOSEDUP))
        ++imax;

    if (flag <= ALLSTEPS) {
        i0 = -1;
	i1 = 0;
    }
    if (flag == ERASE) {
        if (ModeCtrl == OPEN)
	    i0 = -1;
	else
            i0 = 0;
	i1 = l->npoints-2;
    }
    if (flag == DRAW) {
        if (ModeCtrl == OPEN)
            i0 = -1;
	else
            i0 = 0;
	i1 = n-2;
    } 
    
    if (flag == FINISH) {
        n = 0;
        xp = (XPoint *) xmalloc((imax * SUBDIV + 2) * sizeof(XPoint));
        if (zoom>0) {
	    xp[0].x = rx + l->points[0].x / zoom;
	    xp[0].y = ry + l->points[0].y / zoom;
	} else {
	    xp[0].x = rx + l->points[0].x;
	    xp[0].y = ry + l->points[0].y;
	}
	if (d == l->pixmap)
            UndoGrow(w, xp[0].x, xp[0].y);
        for (i = 0; i < imax; i++) {
	    for (j = 1; j <= SUBDIV; j++) {
		t = ((FLOAT) j)/((FLOAT) SUBDIV);
		s = 1.0-t;
		u = (int) (s*s*(s*l->points[i].x+3.0*t*l->interm1[i].x)+
			     t*t*(t*l->points[i+1].x+3.0*s*l->interm2[i].x));
		v = (int) (s*s*(s*l->points[i].y+3.0*t*l->interm1[i].y)+
		       t*t*(t*l->points[i+1].y+3.0*s*l->interm2[i].y));
                if (zoom>0) {
		    xp[++n].x = rx + u / zoom;
		    xp[n].y = ry + v / zoom;
		} else {
		    xp[++n].x = rx + u;
		    xp[n].y = ry + v;
		}
		if (d == l->pixmap)
		    UndoGrow(w, xp[n].x, xp[n].y);
	    }
	}
	if (Filled==1)
	    XFillPolygon(dpy, d, l->sgc, xp, n+1, Complex, CoordModeOrigin);
	if (Filled && l->mode == OPEN) 
	        xp[++n] = xp[0];
        if (Filled<=1)
	    XDrawLines(dpy, d, l->fgc, xp, n+1, CoordModeOrigin);
	if (Filled==2 && d!=XtWindow(w)) {
	    CreatePolygonalRegion(w, xp, n);
	    free(xp);
            return;
	}
    } else
    for (i = 0; i < imax; i++) {
        if ((i <= i0) || (i >= i1)) {
	    u = l->points[i].x;
	    v = l->points[i].y;
	    for (j = 1; j <= SUBDIV; j++) {
	        up = u;
		vp = v;
		t = ((FLOAT) j)/((FLOAT) SUBDIV);
		s = 1.0-t;
		u = (int) (s*s*(s*l->points[i].x+3.0*t*l->interm1[i].x)+
			     t*t*(t*l->points[i+1].x+3.0*s*l->interm2[i].x));
		v = (int) (s*s*(s*l->points[i].y+3.0*t*l->interm1[i].y)+
		       t*t*(t*l->points[i+1].y+3.0*s*l->interm2[i].y));
                if (bool && l->zoom<0) {
		    XDrawLine(dpy, d, gc, up/(-l->zoom), vp/(-l->zoom), 
                                          up/(-l->zoom), vp/(-l->zoom));
		    XDrawLine(dpy, d, gc, up/(-l->zoom), vp/(-l->zoom),
                                          u/(-l->zoom), v/(-l->zoom));
		} else {
		    XDrawLine(dpy, d, gc, up, vp, up, vp);
		    XDrawLine(dpy, d, gc, up, vp, u, v);
		}
	    }
	}
    }

    l->mode = ModeCtrl;
    l->nprev = l->npoints;
}

static void 
finish(Widget w, LocalInfo * l)
{
    XRectangle undo;
    int width, height;

    if (l->npoints < 0)
        return;

    XDrawContour(w, XtWindow(w), l, ALLSTEPS);
    --l->npoints;

    if (l->npoints > 0) {
        SetCapAndJoin(w, l->fgc,
           ((Global.cap)?Global.cap-1:CapButt),
           ((Global.join)?Global.join-1:JoinRound));
        if (!l->isFat)
           XDrawContour(w, XtWindow(w), l, FINISH);
        XDrawContour(w, l->pixmap, l, FINISH);
    }

    XtVaGetValues(w, XtNdrawWidth, &width, 
                     XtNdrawHeight, &height, NULL);
    undo.x = 0;
    undo.y = 0;
    undo.width = width;
    undo.height = height;

    if (l->isFat)
	PwUpdate(w, &undo, True);
    else
	PwUpdate(w, &undo, False);

    l->npoints = -1;
    l->nprev = -1;
}

static void 
check_modifiers(XButtonEvent * event)
{
    ModeCtrl = Mode;
    if (event->state & ControlMask) {
        if (Mode == CLOSED) ModeCtrl = CLOSEDUP;
        if (Mode == CLOSEDUP) ModeCtrl = CLOSED;
    }
    ModeShift = (event->state & ShiftMask)? 1 : 0;
}

static void 
pressSplineBand(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    PaintWidget pw = (PaintWidget) w;

    if (event->button == Button3) return;
    check_modifiers(event);

    if ((l->npoints < 0) && (event->button == Button1)) {
      if (Filled==2 && pw->paint.region.isVisible) {
	  l->npoints = -2;
          return;
      }
      XtVaGetValues(w, XtNzoom, &l->zoom, NULL);
      if (info->surface == opPixmap) {
	l->isFat = info->isFat;
	l->fgc = info->first_gc;
	l->sgc = info->second_gc;
        if (l->zoom>0) {
            l->rx = info->x - l->points[0].x / l->zoom;
            l->ry = info->y - l->points[0].y / l->zoom;
	} else {
	    l->rx = info->x - l->points[0].x;
	    l->ry = info->y - l->points[0].y;
	}
	l->npoints = 0;
        l->mode = ModeCtrl;
        UndoStart(w, info);
        l->pixmap = info->drawable;
      } else {
        if (l->zoom>0) {
	    l->points[0].x = event->x;
	    l->points[0].y = event->y;
	} else {
	    l->points[0].x = event->x * (-l->zoom);
	    l->points[0].y = event->y * (-l->zoom);
	}
        l->breakpt[0] = (ModeShift)? True : False;
      }
      return;        
    }
 
    if ((l->npoints >= 0) && (event->button == Button2) &&
	(info->surface == opWindow)) {
  	finish(w, l);
	return;
    }
}

void 
releaseSplineBand(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    PaintWidget pw = (PaintWidget) w;

    if (Global.escape) {
       finish(w, l);
       l->npoints = -1;
       l->nprev = -1;
       Global.escape = 0;       
       return;
    }
   
    if (l->npoints==-2 && Filled==2 && pw->paint.region.isVisible) {
        PwRegionFinish(w, True);
        pw->paint.region.isVisible = False;
	l->npoints = -1;
	l->nprev = -1;
        return;
    }

    if (event->button == Button3) return;

    if (l->npoints < 0) 
        return;

    if (l->npoints >= MAXPOINTS - 3) {
        MAXPOINTS += 10;
        l->points = 
           (XPoint *) realloc(l->points, MAXPOINTS*sizeof(XPoint));
        l->interm1 = 
           (XPoint *) realloc(l->interm1, MAXPOINTS*sizeof(XPoint));
        l->interm2 = 
           (XPoint *) realloc(l->interm2, MAXPOINTS*sizeof(XPoint));
        l->breakpt = 
           (Boolean *) realloc(l->breakpt, MAXPOINTS*sizeof(Boolean));
    }

    if ((event->button == Button1) && (info->surface == opWindow)) {
        check_modifiers(event);
        if (l->zoom>0) {
	    if (l->npoints>0 &&
                event->x==l->points[l->npoints-1].x &&
	        event->y==l->points[l->npoints-1].y) return;
            ++l->npoints;
	    l->points[l->npoints].x = event->x;
	    l->points[l->npoints].y = event->y;
	} else {
	    if (l->npoints>0 &&
                event->x==l->points[l->npoints-1].x*(-l->zoom) &&
	        event->y==l->points[l->npoints-1].y*(-l->zoom)) return;
            ++l->npoints;
	    l->points[l->npoints].x = event->x * (-l->zoom);
	    l->points[l->npoints].y = event->y * (-l->zoom);
	}
    }
}

void 
motionSplineBand(Widget w, LocalInfo * l, XMotionEvent * event, OpInfo * info)
{
    /*
    **  Haven't done the first button press
     */
    if (l->npoints < 0)
	return;

    if (Global.escape) {
       finish(w, l);
       l->npoints = -1;
       l->nprev = -1;
       Global.escape = 0;       
       return;
    }

    if (l->zoom>0) {   
        if ((event->x != l->points[l->npoints].x) ||
	    (event->y != l->points[l->npoints].y)) {
            check_modifiers((XButtonEvent *)event);
            XDrawContour(w, info->drawable, l, ERASE);
            l->points[l->npoints].x = event->x;
            l->points[l->npoints].y = event->y;
            l->breakpt[l->npoints-1] = (ModeShift)? True : False;
  	    XDrawContour(w, info->drawable, l, DRAW);
	}
    } else {
        if ((event->x != l->points[l->npoints].x*(-l->zoom)) ||
	    (event->y != l->points[l->npoints].y*(-l->zoom))) {
            check_modifiers((XButtonEvent *)event);
            XDrawContour(w, info->drawable, l, ERASE);
	    l->points[l->npoints].x = event->x * (-l->zoom);
            l->points[l->npoints].y = event->y * (-l->zoom);
            l->breakpt[l->npoints-1] = (ModeShift)? True : False;
  	    XDrawContour(w, info->drawable, l, DRAW);
	}
    }
}

static
LocalInfo * createLocalInfo()
{
    LocalInfo * l;
    l = (LocalInfo *) xmalloc(sizeof(LocalInfo));
    l->points =  (XPoint *) xmalloc(MAXPOINTS*sizeof(XPoint));
    l->interm1 = (XPoint *) xmalloc(MAXPOINTS*sizeof(XPoint));
    l->interm2 = (XPoint *) xmalloc(MAXPOINTS*sizeof(XPoint));
    l->breakpt = (Boolean *) xmalloc(MAXPOINTS*sizeof(Boolean));
    return l;
}

static 
void freeLocalInfo(LocalInfo *l)
{
    free((void *)l->points);
    free((void *) l->interm1);
    free((void *) l->interm2);
    free((void *) l->breakpt);
    free((void *) l);
}

void *
SplineAdd(Widget w)
{
    LocalInfo *l;
    
    l = createLocalInfo();
    Filled = 0;
    ModeCtrl = Mode;
    l->npoints = -1;
    l->nprev = -1;
    l->gcx = GetGCX(w);

    XtVaSetValues(w, XtNcompress, True, NULL);

    OpAddEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
		      (OpEventProc) pressSplineBand, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motionSplineBand, l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) releaseSplineBand, l);
    SetCrossHairCursor(w);

    return l;
}

void 
SplineRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) pressSplineBand, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motionSplineBand, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) releaseSplineBand, l);

    finish(w, (LocalInfo *) l);
    freeLocalInfo((LocalInfo *) l);
}

void *
FilledSplineAdd(Widget w)
{
    LocalInfo *l;

    l = createLocalInfo();
    Filled = 1;
    ModeCtrl = Mode;
    l->npoints = -1;
    l->nprev = -1;
    l->gcx = GetGCX(w);

    XtVaSetValues(w, XtNcompress, True, NULL);

    OpAddEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
		      (OpEventProc) pressSplineBand, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motionSplineBand, l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) releaseSplineBand, l);
    SetCrossHairCursor(w);

    return l;
}

void 
FilledSplineRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) pressSplineBand, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motionSplineBand, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) releaseSplineBand, l);

    finish(w, (LocalInfo *) l);
    freeLocalInfo((LocalInfo *) l);
}

void *
SelectSplineAdd(Widget w)
{
    LocalInfo *l;

    l = createLocalInfo();
    Filled = 2;
    ModeCtrl = Mode;
    l->npoints = -1;
    l->nprev = -1;
    l->gcx = GetGCX(w);

    XtVaSetValues(w, XtNcompress, True, NULL);

    OpAddEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
		      (OpEventProc) pressSplineBand, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motionSplineBand, l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) releaseSplineBand, l);

    SetCrossHairCursor(w);

    return l;
}

void 
SelectSplineRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) pressSplineBand, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motionSplineBand, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) releaseSplineBand, l);

    finish(w, (LocalInfo *) l);
    freeLocalInfo((LocalInfo *) l);
}
