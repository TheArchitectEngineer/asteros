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

/* $Id: PaintEvent.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <X11/StringDefs.h>
#include <X11/IntrinsicP.h>
#include "PaintP.h"
#include "xpaint.h"
#include "misc.h"

extern void PopdownMenusGlobal();

typedef void (*func_t) (Widget, void *, XEvent *, OpInfo *);

typedef struct data_s {
    Widget w;
    func_t func;
    void *data;
    int mask, flag;
    int surfMask;
    struct data_s *next;
    OpInfo *info;
} data_t;

static data_t *list = NULL;

#if 0
static data_t **
widgetEvent(Widget w, data_t * look)
{
    static data_t *d[10];
    data_t *cur = list;
    int n = 0;

    while (cur != NULL) {
	if (cur->mask == look->mask &&
	    cur->flag == look->flag &&
	    cur->func == look->func &&
	    cur->w == w)
	    d[n++] = cur;
	cur = cur->next;
    }

    d[n] = NULL;

    return n == 0 ? NULL : d;
}
#endif

#define FROM_BELOW 1
#if FROM_BELOW
#define APPROX(u,m) (((u)/(m))*(m))
#else
#define APPROX(u,m) ((((u)+(m)/2)/(m))*(m))
#endif

static void 
opHandleEvent(Widget w, XtPointer dataArg, XEvent * event, Boolean * junk)
{
    data_t *data = (data_t *) dataArg;
    int snap_x, snap_y;
    PaintWidget paint = (PaintWidget) w;
    OpInfo *info = data->info;
    static Window lastWindow = None;
    static Display *lastDisplay = None;
    static int lastType;
    static int lastX, lastY;
    Boolean same;
    Position x, y;
    Dimension wd, ht;
    int zoom;

    if (Global.popped_up) {
        if (event->type == ButtonRelease) {
	    XtVaGetValues(Global.popped_up,
                          XtNx, &x, XtNy, &y, 
                          XtNwidth, &wd, XtNheight, &ht, NULL);
            x = event->xbutton.x_root - x;
            y = event->xbutton.y_root - y;
            if (x<0 || x>wd || y<0 || y>ht)
                PopdownMenusGlobal();
        }
        event->type = None;
        return;
    }

    XtVaGetValues((Widget)paint, XtNlocked, &same, NULL);
    if (same) return;
#if 0
    XEvent myEvent;

    memcpy(&myEvent, event, sizeof(XEvent));
    event = &myEvent;
#endif

    /*
    **  Figure out if there is a snap, either by
    **   choice or by zooming
     */

    if ((zoom = GET_ZOOM(paint)) == 0)
	zoom = 1;
    info->zoom = zoom;
    info->isFat = ((zoom = GET_ZOOM(paint)) != 1);
    if (paint->paint.snapOn) {
        if (zoom>0) {
	    snap_x = paint->paint.snap_x * zoom;
	    snap_y = paint->paint.snap_y * zoom;
	} else {
	    snap_x = paint->paint.snap_x / (-zoom);
	    snap_y = paint->paint.snap_y / (-zoom);
	}
    } else {
        if (zoom>0)
            snap_x = snap_y = zoom;
        else
            snap_x = snap_y = 1;
    }
    if (snap_x == 0) snap_x = 1;
    if (snap_y == 0) snap_y = 1;

    same = (event->xany.window == lastWindow) &&
	(event->xany.display == lastDisplay) &&
	(event->xany.type == lastType);
    lastWindow = event->xany.window;
    lastDisplay = event->xany.display;
    lastType = event->xany.type;

    /*
    **  Snap events to the snap
     */
    if (event->type == ButtonPress || event->type == ButtonRelease) {
	info->realX = event->xbutton.x;
	info->realY = event->xbutton.y;
    } else if (event->type == MotionNotify) {
	info->realX = event->xmotion.x;
	info->realY = event->xmotion.y;
    }
    if (((event->type == ButtonPress) ||
	 (event->type == ButtonRelease) ||
	 (event->type == MotionNotify)) && (snap_x > 1 || snap_y > 1)) {
	switch (event->type) {
	case ButtonPress:
	case ButtonRelease:
	    event->xbutton.x = APPROX(event->xbutton.x, snap_x);
	    event->xbutton.y = APPROX(event->xbutton.y, snap_y);
	    if (same && lastX == event->xbutton.x && lastY == event->xbutton.y)
		return;
	    break;
	case MotionNotify:
	    event->xmotion.x = APPROX(event->xbutton.x, snap_x);
	    event->xmotion.y = APPROX(event->xbutton.y, snap_y);
	    if (same && lastX == event->xmotion.x && lastY == event->xmotion.y)
		return;
	    break;
	}
	lastX = event->xmotion.x;
	lastY = event->xmotion.y;
    } else {
	lastX = lastY = -1;
    }

    if (event->type == MotionNotify) {
        if (zoom>0) {
	    info->x = (event->xmotion.x / zoom) + paint->paint.zoomX;
	    info->y = (event->xmotion.y / zoom) + paint->paint.zoomY;
	} else {
	    info->x = (event->xmotion.x * (-zoom) + (-zoom/2)) + paint->paint.zoomX;
	    info->y = (event->xmotion.y * (-zoom) + (-zoom/2)) + paint->paint.zoomY;
	}
    } else if (event->type == ButtonPress || event->type == ButtonRelease) {
        if (zoom>0) {
	    info->x = (event->xbutton.x / zoom) + paint->paint.zoomX;
	    info->y = (event->xbutton.y / zoom) + paint->paint.zoomY;
	} else {
	    info->x = (event->xbutton.x * (-zoom) + (-zoom/2)) + paint->paint.zoomX;
	    info->y = (event->xbutton.y * (-zoom) + (-zoom/2)) + paint->paint.zoomY;
	}
    }
    /*
    **  In fatbits we can't tell the difference 
    **   between snapping and fat bits.
     */
    if (info->isFat) {
	info->realX = info->x;
	info->realY = info->y;
    } else {
	info->realX = info->realX + paint->paint.zoomX;
	info->realY = info->realY + paint->paint.zoomY;
    }

    /*
    **  If this is button down, save it for reference
     */
    if (event->type == ButtonPress) {
	paint->paint.downX = info->realX;
	paint->paint.downY = info->realY;
    }
    if (paint->paint.paint != None) {
	PaintWidget pw = (PaintWidget) paint->paint.paint;
	info->base = pw->paint.sourcePixmap;
    } else {
	info->base = paint->paint.sourcePixmap;
    }

    if (data->surfMask & opWindow) {
#if FROM_BELOW
	int z = GET_ZOOM(paint) / 2;

	if (z > 0) {
	    if (event->type == MotionNotify) {
		event->xmotion.x += z;
		event->xmotion.y += z;
	    } else if (event->type == ButtonPress || event->type == ButtonRelease) {
		event->xbutton.x += z;
		event->xbutton.y += z;
	    }
	}
#endif
	info->surface = opWindow;
	info->drawable = event->xany.window;
	data->func(w, data->data, event, info);
    }
    if (event->type == MotionNotify) {
	event->xmotion.x = info->x;
	event->xmotion.y = info->y;
    } else if (event->type == ButtonPress || event->type == ButtonRelease) {
	event->xbutton.x = info->x;
	event->xbutton.y = info->y;
    }
    if (data->surfMask & opPixmap) {
	info->surface = opPixmap;
	info->drawable = GET_PIXMAP(paint);
	data->func(w, data->data, event, info);
    }
}

void 
OpAddEventHandler(Widget w, int surfMask, int mask, Boolean flag,
	   void (*func) (Widget, void *, XEvent *, OpInfo *), void *data)
{
    PaintWidget paint = (PaintWidget) w;
    PaintWidget pp = (PaintWidget) paint->paint.paint;
    data_t *new = (data_t *) XtMalloc(sizeof(data_t));
    OpInfo *info;

    if (new == NULL)
	return;

    Global.operation = 1 << getIndexOp();
    new->w = w;
    new->func = func;
    new->data = data;
    new->mask = mask;
    new->flag = flag;
    new->surfMask = surfMask;

    info = new->info = (OpInfo *) XtMalloc(sizeof(OpInfo));
    info->refCount = 1;
    info->realX = 0;
    info->realY = 0;

    /*
    **  Now build the approprate info structure
     */
    if (pp == None) {
	/*
	** If this is a paint widget, this is easy
	 */
	info->first_gc = paint->paint.fgc;
	info->second_gc = paint->paint.sgc;
	info->base_gc = paint->paint.igc;
    } else {
	/*
	** For a fatbits paint widget get paint information
	 */
	info->first_gc = pp->paint.fgc;
	info->second_gc = pp->paint.sgc;
	info->base_gc = pp->paint.igc;
    }

    new->next = list;
    list = new;

    XtAddEventHandler(w, mask, flag, opHandleEvent, (XtPointer) new);
}

void 
OpRemoveEventHandler(Widget w, int surfMask, int mask, Boolean flag,
	   void (*func) (Widget, void *, XEvent *, OpInfo *), void *data)
{
    data_t *cur = list;
    data_t **prev = &list;

    Global.operation = 0;

    while (cur != NULL) {
	if (cur->w == w &&
	    cur->data == data &&
	    cur->mask == mask &&
	    cur->flag == flag &&
	    cur->surfMask == surfMask &&
	    cur->func == func)
	    break;
	prev = &cur->next;
	cur = cur->next;
    }

    if (cur == NULL)
	return;

    XtRemoveEventHandler(w, mask, flag, opHandleEvent, (XtPointer) cur);

    *prev = cur->next;

    cur->info->refCount--;
    if (cur->info->refCount == 0)
	XtFree((XtPointer) cur->info);

    XtFree((XtPointer) cur);
}
