
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

/* $Id: polyOp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtWindow XTWINDOW
#endif

#include <stdlib.h>
#include <math.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include "PaintP.h"
#include "xpaint.h"
#include "misc.h"
#include "Paint.h"
#include "ops.h"

#define	REGION		0x4
#define	FILL		0x2
#define	POLY		0x1
#define IsRegion(x)	(x & REGION)
#define IsPoly(x)	(x & POLY)
#define	IsFill(x)	(x & FILL)

typedef struct {
    int flag, zoom;
    int startX, startY, endX, endY, button;
    int drawn, first, tracking, go;
    int npoints;
    XPoint * points;
    XPoint * real;
    /*
    **  Borrowed from my info structure.
     */
    GC fgc, lgc, gcx;
    Pixmap pixmap;
    Widget widget;
    Boolean isFat;
} LocalInfo;

static int MAXPOINTS = 30;
static int polygonType = 1;
static int polygonSides = 5;
static double polygonRatio = 0.381966; /* (3-sqrt(5))/2 for a perfect star */

void Vertices(XPoint *xp, int ox, int oy, int ex, int ey)
{
    int i, num;
    double c, s;
    ex -= ox;
    ey -= oy;
    num = (polygonType-1)*polygonSides;

    for (i=0; i<=num; i++) {
        s = 2*i*M_PI/num;
        c = cos(s);
        s = sin(s);
        if (polygonType == 3 && (i&1)) {
	    c *= polygonRatio;
            s *= polygonRatio;
	}
        xp[i].x = ox + c*ex - s*ey + 0.5;
        xp[i].y = oy + s*ex + c*ey + 0.5;
    }
}

void
CreatePolygonalRegion(Widget w, XPoint *xp, int n)
{
    Display *dpy;
    XRectangle rect;
    int i, xmin, ymin, xmax, ymax, width, height;
    Pixmap mask;
    GC gc;

    dpy = XtDisplay(w);

    /* Create region */
    xmin = ymin = 32767;
    xmax = ymax = -32768;
    for (i=0; i<n; i++) {
        if (xp[i].x<xmin) xmin = xp[i].x;
        if (xp[i].y<ymin) ymin = xp[i].y;
        if (xp[i].x>xmax) xmax = xp[i].x;
        if (xp[i].y>ymax) ymax = xp[i].y;
    }
    if (xmin<0) xmin = 0;
    if (ymin<0) ymin = 0;
    XtVaGetValues(w, XtNdrawWidth, &width, 
                     XtNdrawHeight, &height, NULL);
    if (xmax>width) xmax = width;
    if (ymax>height) ymax = height;

    rect.x = xmin;
    rect.y = ymin;
    rect.width = xmax-xmin+1;
    rect.height = ymax-ymin+1;
    mask = XCreatePixmap(XtDisplay(w), XtWindow(w),
                         rect.width, rect.height, 1);
    gc = XCreateGC(XtDisplay(w), mask, 0, 0);
    XSetFunction(XtDisplay(w), gc, GXclear);
    XFillRectangle(XtDisplay(w), mask, gc, 0, 0,
    rect.width, rect.height);
    XSetFunction(XtDisplay(w), gc, GXset);
    for (i=0; i<n; i++) {
         xp[i].x -= xmin;
         xp[i].y -= ymin;
    }
    XFillPolygon(XtDisplay(w), mask, gc, xp, n,
                 Complex, CoordModeOrigin);
    XFreeGC(XtDisplay(w), gc);
    if (xmax==xmin || ymax==ymin) {
        PwRegionFinish(w, True);
        return;
    }
    if (SelectGetCutMode() != 0 && !chromaCut(w, &rect, &mask)) {
        PwRegionFinish(w, True);
        return;
    }
    PwRegionSet(w, &rect, None, mask);
}

static void 
finish(Widget w, LocalInfo * l, Boolean flag)
{
    if (!l->tracking)
	return;

    l->tracking = False;

    if (l->drawn)
	XDrawLines(XtDisplay(w), XtWindow(w), l->gcx,
		   l->points, l->npoints, CoordModeOrigin);
    if (flag && l->drawn)
	XDrawLine(XtDisplay(w), XtWindow(w), l->gcx,
		  l->points[l->npoints - 1].x,
		  l->points[l->npoints - 1].y,
		  l->endX, l->endY);

    if (l->npoints<=1) return;
   
    if (polygonType>=2) {
        Vertices(l->real, l->real[0].x,l->real[0].y, l->real[1].x,l->real[1].y);
    }

    if (IsRegion(l->flag)) {
        CreatePolygonalRegion(w, l->real, l->npoints);
        l->npoints = -1;
        return;
    }

    if (IsFill(l->flag)) {
	if (!l->isFat)
	    XFillPolygon(XtDisplay(w), XtWindow(w), l->fgc,
			 l->real, l->npoints, Complex, CoordModeOrigin);
	XFillPolygon(XtDisplay(w), l->pixmap, l->fgc,
		     l->real, l->npoints, Complex, CoordModeOrigin);
    }
    if (IsPoly(l->flag) && polygonType==1) {
	l->real[l->npoints].x = l->real[0].x;
	l->real[l->npoints].y = l->real[0].y;
	l->npoints++;
    }

    SetCapAndJoin(w, l->lgc,
                  ((Global.cap)?Global.cap-1:CapButt),
                  ((Global.join)?Global.join-1:JoinMiter));
    if (!l->isFat)
	XDrawLines(XtDisplay(w), XtWindow(w), l->lgc,
		   l->real, l->npoints, CoordModeOrigin);

    XDrawLines(XtDisplay(w), l->pixmap, l->lgc,
	       l->real, l->npoints, CoordModeOrigin);

    PwUpdate(w, NULL, False);
}

static void 
press(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    PaintWidget pw = (PaintWidget) w;

    if (event->button == Button3) return;

    if (l->npoints<0 && IsRegion(l->flag)
        && pw->paint.region.isVisible) {
	  l->npoints = -3;
          return;
    }

    if (event->button == Button2 && l->tracking) {
        if (polygonType >= 2) {
            if (l->tracking && l->npoints>=1)
	        XDrawLines(XtDisplay(w), XtWindow(w), l->gcx,
		           l->points, l->npoints, CoordModeOrigin);
  	    l->tracking = False;
            l->npoints = -2;
            l->drawn = True;
            return;
	}
        l->drawn = True;
	finish(w, l, True);
	return;
    }
}

static void 
release(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    PaintWidget pw = (PaintWidget) w;
    int i, j;

    if (event->button >= Button2) return;

    if (Global.escape) {
       finish(w, l, True);
       l->npoints = -2;
       Global.escape = 0;       
       return;
    }

    if (l->npoints == -2) {
        l->npoints = -1;
        return;
    }
    if (l->npoints == -3 && IsRegion(l->flag) && 
        pw->paint.region.isVisible) {
        PwRegionFinish(w, True);
        pw->paint.region.isVisible = False;
        l->npoints = -2;
        return;
    }

    if (l->tracking && event->button == 1 && 
        info->surface == opWindow && polygonType >= 2) {
        l->drawn = True;
        l->real[1].x = info->x;
        l->real[1].y = info->y;
        i = info->x-l->real[0].x;
        j = info->y-l->real[0].y;
        i = sqrt(i*i+j*j)+1;
        UndoGrow(w, l->real[0].x+i, l->real[0].y+i);
        UndoGrow(w, l->real[0].x-i, l->real[0].y-i);
	finish(w, l, False);
        l->npoints = -2;
	return;
    }

    if (!l->tracking && event->button == Button1) {
	l->endX = l->startX = event->x;
	l->endY = l->startY = event->y;

	l->button = event->button;

	l->points[0].x = event->x;
	l->points[0].y = event->y;
	l->real[0].x = info->x;
	l->real[0].y = info->y;

	if (polygonType == 1) {
	    l->npoints = 1;
	}
	if (polygonType >= 2) {
	    l->npoints = (polygonType-1)*polygonSides + 1;
            MAXPOINTS = l->npoints + 3;
	    l->points = 
                (XPoint *) realloc(l->points, MAXPOINTS*sizeof(XPoint));
	    l->real = 
                (XPoint *) realloc(l->real, MAXPOINTS*sizeof(XPoint));
	}

	l->drawn = False;
	l->tracking = True;
	l->first = True;

	l->isFat = info->isFat;
	l->fgc = info->second_gc;
	l->lgc = info->first_gc;
    }

    if (l->npoints >= MAXPOINTS - 2) {
        MAXPOINTS += 10;
        l->points = 
           (XPoint *) realloc(l->points, MAXPOINTS*sizeof(XPoint));
        l->real = 
           (XPoint *) realloc(l->real, MAXPOINTS*sizeof(XPoint));
    }

    if (l->first && info->surface == opPixmap) {
	UndoStartPoint(w, info, info->x, info->y);
	l->pixmap = info->drawable;
	l->widget = w;
    }

    if (info->surface == opWindow) {
	if (!l->first) {
	    l->endX = l->points[l->npoints].x = event->x;
	    l->endY = l->points[l->npoints].y = event->y;
	    l->startX = event->x;
	    l->startY = event->y;
	}
	return;
    }
    if (l->first) {
	l->first = False;
	return;
    }
    /*
    **  else on the pixmap.
     */
    l->real[l->npoints].x = info->x;
    l->real[l->npoints].y = info->y;
    l->npoints++;

    UndoGrow(w, info->x, info->y);
}

static void 
motion(Widget w, LocalInfo * l, XMotionEvent * event, OpInfo * info)
{
    /*
    **  Haven't done the first button press
     */
    if (!l->tracking || l->first)
	return;

    if (l->npoints == -1) 
        return;

    if (Global.escape) {
       finish(w, l, True);
       l->npoints = -1;
       Global.escape = 0;
       return;
    }
       
    if (l->drawn) {
        if (polygonType==1)
	    XDrawLine(XtDisplay(w), info->drawable, l->gcx,
		      l->startX, l->startY, l->endX, l->endY);
        else
	    XDrawLines(XtDisplay(w), info->drawable, l->gcx,
		      l->points, l->npoints, CoordModeOrigin);
    }

    l->endX = event->x;
    l->endY = event->y;
    if (polygonType >= 2)
        Vertices(l->points, l->startX, l->startY, l->endX, l->endY);

    if ((l->drawn = (l->startX != l->endX || l->startY != l->endY))) {
        if (polygonType==1)
            XDrawLine(XtDisplay(w), info->drawable, l->gcx,
		      l->startX, l->startY, l->endX, l->endY);
        else
	    XDrawLines(XtDisplay(w), info->drawable, l->gcx,
		      l->points, l->npoints, CoordModeOrigin);
    }
}

static
LocalInfo * createLocalInfo()
{
    LocalInfo * l;
    l = (LocalInfo *) xmalloc(sizeof(LocalInfo));
    l->points = (XPoint *) xmalloc(MAXPOINTS*sizeof(XPoint));
    l->real =   (XPoint *) xmalloc(MAXPOINTS*sizeof(XPoint));
    return l;
}

static 
void freeLocalInfo(LocalInfo *l)
{
    free((void *) l->points);
    free((void *) l->real);
    free((void *) l);
}

/*
**  Those public functions
 */
void *
PolygonAdd(Widget w)
{
    LocalInfo *l = 

    l = (LocalInfo *) createLocalInfo();

    l->flag = POLY;
    XtVaGetValues(w, XtNzoom, &l->zoom, NULL);
    l->drawn = False;
    l->first = True;
    l->tracking = False;
    l->npoints = -1;
    l->gcx = GetGCX(w);

    XtVaSetValues(w, XtNcompress, True, NULL);

    OpAddEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) release, l);
    SetCrossHairCursor(w);

    return l;
}

void 
PolygonRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);

    finish(w, (LocalInfo *) l, True);
    freeLocalInfo((LocalInfo *) l);
}

void *
FilledPolygonAdd(Widget w)
{
    LocalInfo *l;

    l = (LocalInfo *) createLocalInfo();
    l->flag = POLY | FILL;
    XtVaGetValues(w, XtNzoom, &l->zoom, NULL);
    l->drawn = False;
    l->first = True;
    l->tracking = False;
    l->npoints = -1;
    l->gcx = GetGCX(w);

    XtVaSetValues(w, XtNcompress, True, NULL);

    OpAddEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) release, l);
    SetCrossHairCursor(w);

    return l;
}

void 
FilledPolygonRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);

    finish(w, (LocalInfo *) l, True);
    freeLocalInfo((LocalInfo *) l);
}

void *
SelectPolygonAdd(Widget w)
{
    LocalInfo *l;

    l = (LocalInfo *) createLocalInfo();
    l->flag = POLY | REGION;
    XtVaGetValues(w, XtNzoom, &l->zoom, NULL);
    l->drawn = False;
    l->first = True;
    l->tracking = False;
    l->npoints = -1;
    l->gcx = GetGCX(w);

    XtVaSetValues(w, XtNcompress, True, NULL);

    OpAddEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) release, l);

    SetCrossHairCursor(w);

    return l;
}

void 
SelectPolygonRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);

    finish(w, (LocalInfo *) l, True);
    freeLocalInfo((LocalInfo *) l);
}

void *
BrokenlineAdd(Widget w)
{
    LocalInfo *l;

    l = createLocalInfo();
    l->flag = 0;
    XtVaGetValues(w, XtNzoom, &l->zoom, NULL);
    l->drawn = False;
    l->first = True;
    l->tracking = False;
    l->npoints = -1;
    l->gcx = GetGCX(w);

    XtVaSetValues(w, XtNcompress, True, NULL);

    OpAddEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
		      (OpEventProc) release, l);
    SetCrossHairCursor(w);

    return l;
}

void 
BrokenlineRemove(Widget w, void *l)
{
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonPressMask, FALSE,
			 (OpEventProc) press, l);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
			 (OpEventProc) motion, l);
    OpRemoveEventHandler(w, opWindow | opPixmap, ButtonReleaseMask, FALSE,
			 (OpEventProc) release, l);

    finish(w, (LocalInfo *) l, True);
    freeLocalInfo((LocalInfo *) l);
}

/*
**  Those public functions
*/
void
PolygonSetParameters(int t, int s, float a)
{
    if (t) 
       polygonType = t;
    else {
       polygonSides = s;
       if (a==1.0)
	   polygonRatio = 1/(2*cos(M_PI/s)+1.0);
       else
           polygonRatio = a;
    }
}

void
PolygonGetParameters(int *t, int *s, float *a)
{
  *t = polygonType;
  *s = polygonSides;
  *a = polygonRatio;
}
