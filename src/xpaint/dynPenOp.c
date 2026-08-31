/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)            | */
/* | Copyright 1995, Tor Lillqvist (tml@iki.fi)                        | */
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

/* Drawing with a dynamic pen. Adapted from Paul Haeberli's
   dynadraw.c (for Iris GL),
   see http://www.sgi.com/grafica/dyna/index.html */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtWindow XTWINDOW
#endif
   
#include <math.h>
#include <sys/param.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include "xpaint.h"
#include "misc.h"
#include "Paint.h"

#define XTIMEOUT 20

typedef struct {
	float	startx, starty;
	float	curx, cury;
	float	velx, vely, vel;
	float	accx, accy;
	float	angx, angy;
	float	lastx, lasty;
	int	mousex, mousey;
	Time	lastt;
	int	fixedangle;
	float	width;
	float	odelx, odely;
	float	mass, drag;
	int	doFinish, pressed, finishing;
	XtIntervalId tid;
	Widget  w;
	OpInfo	*opinfo;
	GC	gc;
} LocalInfo;

static Atom dummy_prop = None;

static float width = 10, mass = 600, drag = 15;
static int doFinish = False;

#define flerp(f0, f1, p) ((f0*(1.0-p))+(f1*p))

static int 
filterapply(LocalInfo *l, XMotionEvent *event)
{
	float	fx, fy, acc;

	/* If we have just started pulling the tool, and the mouse
	   has moved just a little bit, don't draw yet,
	   the first motion events are usually a bit randomish. */
	if (l->vel == 0
	    && fabs(event->x - l->startx) <= 2
	    && fabs(event->y - l->starty) <= 2)
		return 0;

	l->mousex = event->x;
	l->mousey = event->y;

	/* calculate force and acceleration */
#if 0
	fx = (l->mousex - l->curx) / (event->time - l->lastt); 
	fy = (l->mousey - l->cury) / (event->time - l->lastt);
#else
	fx = (l->mousex - l->curx);
	fy = (l->mousey - l->cury);
#endif

	acc = sqrt(fx*fx + fy*fy);
	if (acc < 0.000000001)
		return 0;
	l->accx = fx / l->mass;
	l->accy = fy / l->mass;

	/* calculate new velocity */
	l->velx += l->accx;
	l->vely += l->accy;
	l->vel = sqrt(l->velx*l->velx + l->vely*l->vely);
	if(l->vel < 0.000001) 
		return 0;

#if 0
printf("acc = (%f, %f), vel = (%f, %f)\n", l->accx, l->accy, l->velx, l->vely);
#endif
	/* calculate angle of drawing tool */
	if(l->fixedangle) {
		l->angx = 0.6;
		l->angy = 0.2;
	} else {
		l->angx = -l->vely / l->vel;
		l->angy = l->velx / l->vel;
	}

	/* apply drag */
	l->velx *= 1.0 - 0.00005*l->drag*l->drag;
	l->vely *= 1.0 - 0.00005*l->drag*l->drag;

	/* update position */
	l->lastx = l->curx;
	l->lasty = l->cury;
	l->curx = l->curx + l->velx * (event->time - l->lastt)/(float)XTIMEOUT;
	l->cury = l->cury + l->vely * (event->time - l->lastt)/(float)XTIMEOUT;
	l->lastt = event->time;

	return 1;
}

static void
press(Widget w, LocalInfo *l, XButtonEvent *event, OpInfo *info) 
{
        int save;
	if (info->surface == opWindow)
		return;

	if ((event->state & (Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask)) != 0)
		return;
        if (event->button >= Button3) return;
        if (event->type == MotionNotify) return;

	l->mass	    = mass;
	l->drag     = drag;
	l->width    = width;
	l->doFinish = doFinish;
	l->finishing = False;
	l->pressed = True;
	l->fixedangle = 0;
	l->tid	    = 0;
	l->w	    = w;
	l->opinfo   = info;

	l->startx  = l->lastx = l->curx = l->mousex = event->x;
	l->starty  = l->lasty = l->cury = l->mousey = event->y;
	l->lastt  = event->time;
	l->vel = 0;
	l->velx = l->vely = 0;
	l->accx = l->accy = 0;
	l->odelx = l->odely = 0;

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

	UndoStart(w, info);
}

static Bool
dummy_pred(Display *d, XEvent *ev, char *arg)
{
	return (ev->type == MotionNotify ||
		  (ev->type == PropertyNotify
		     && ev->xproperty.atom == dummy_prop));
}

static void motion(Widget, LocalInfo *, XMotionEvent *, OpInfo *);

static void
timeout(XtPointer client_data, XtIntervalId *iid)
{
	XEvent ev;
	XMotionEvent me;
	LocalInfo *l = (LocalInfo *)client_data;

	XChangeProperty(XtDisplay(l->w), XtWindow(l->w), dummy_prop,
			XA_ATOM, 32, PropModeReplace,
			(unsigned char *) &dummy_prop, 1);

	/* Wait for the PropertyNotify event coming back,
	   or a MotionNotify */

	XPeekIfEvent(XtDisplay(l->w), &ev, dummy_pred, NULL);

	if (ev.type == MotionNotify)
	   return;
   
	/* It's a PropertyNotify */
	l->tid = 0;
	me.x = l->mousex;
	me.y = l->mousey;
	me.time = ev.xproperty.time;

	motion(l->w, l, &me, l->opinfo);
}

static void
motion(Widget w, LocalInfo *l, XMotionEvent *event, OpInfo *info) 
{
	XRectangle	undo;

	if (!l->pressed)
		return;
	if (info->surface == opWindow)
		return;
        if (l->gc != info->first_gc && l->gc != info->second_gc) return;

	if (l->tid) {
		XtRemoveTimeOut(l->tid); 
                l->tid = 0;
	}

#if 0
printf("motion: %d: (%d, %d)\n", event->time, event->x, event->y);
#endif
	if (filterapply(l, event)) {
		float	wid, delx, dely; 
		XPoint	p[4], *pp;

		wid = (8 - l->vel)/8 * l->width;
		if (wid < 1)
			wid = 1;
		delx = l->angx * wid;
		dely = l->angy * wid;

		pp = p;

		pp->x = l->lastx + l->odelx;
		pp->y = l->lasty + l->odely;
		pp++;

		pp->x = l->lastx - l->odelx;
		pp->y = l->lasty - l->odely;
		pp++;

		pp->x = l->curx - delx;
		pp->y = l->cury - dely;
		pp++;
		
		pp->x = l->curx + delx;
		pp->y = l->cury + dely;
		pp++;

		if (!info->isFat)
			XFillPolygon(XtDisplay(w), XtWindow(w), l->gc,
				p, 4, Complex, CoordModeOrigin);
		XFillPolygon(XtDisplay(w), info->drawable, l->gc,
			p, 4, Complex, CoordModeOrigin);

		l->odelx = delx;
		l->odely = dely;

		XYtoRECT(MIN(MIN(p[0].x,p[1].x),MIN(p[2].x,p[3].x)),
			 MIN(MIN(p[0].y,p[1].y),MIN(p[2].y,p[3].y)),
			 MAX(MAX(p[0].x,p[1].x),MAX(p[2].x,p[3].x)),
			 MAX(MAX(p[0].y,p[1].y),MAX(p[2].y,p[3].y)),
			 &undo);
		PwUndoAddRectangle(w, &undo);

		l->startx = event->x;
		l->starty = event->y;

		PwUpdate(w, &undo, False);
	     
		if (!l->finishing)
			l->tid = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
						 XTIMEOUT, timeout, l);
	}
}

static void
release(Widget w, LocalInfo *l, XButtonEvent *event, OpInfo *info) 
{
	XMotionEvent me;
        int          mask;

        /*
        **  Check to make sure all buttons are up, before doing this
        */
        if (!l->pressed) return;
   
        mask = Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask;
        switch (event->button) {
        case Button1:   mask ^= Button1Mask; break;
        case Button2:   mask ^= Button2Mask; break;
        case Button3:   mask ^= Button3Mask; break;
        case Button4:   mask ^= Button4Mask; break;
        case Button5:   mask ^= Button5Mask; break;
        }
   
        if ((event->type & PointerMotionMask) == MotionNotify) return;

        if ((event->state & mask) != 0)
                return;

        if (event->button >= Button3) return;

	if (l->tid) {
	    XtRemoveTimeOut(l->tid);
            l->tid = 0;
	}

	if (!l->doFinish) {
                l->pressed = False;
		return;
	}

	l->finishing = True;

	me.x = event->x;
	me.y = event->y;
	me.time = event->time;
	while (l->vel > 0.1) {
		me.time += XTIMEOUT;
		motion(w, l, &me, info);
	}
        l->pressed = False;
}

/*
**  Those public functions
*/
void
DynPencilSetParameters(float w, float m, float d)
{
	width = w;
	mass = m;
	drag = d;
}

Boolean
DynPencilGetFinish()
{
	return doFinish;
}

void
DynPencilSetFinish(Boolean flag)
{
	doFinish = flag;
}

void
*DynPencilAdd(Widget w)
{
	LocalInfo	*l = (LocalInfo*)XtMalloc(sizeof(LocalInfo));
	XWindowAttributes wa;

	XtVaSetValues(w, XtNcompress, False, NULL);

	OpAddEventHandler(w, opPixmap, ButtonPressMask, FALSE, (OpEventProc)press, l);
	OpAddEventHandler(w, opPixmap, ButtonMotionMask, FALSE, (OpEventProc)motion, l);
	OpAddEventHandler(w, opPixmap, PointerMotionMask|ButtonReleaseMask, FALSE, (OpEventProc)release, l);
	SetPencilCursor(w);

	if (dummy_prop == None)
		dummy_prop = XInternAtom(XtDisplay(w), "XPAINT_DUMMY", FALSE);

	XGetWindowAttributes(XtDisplay(w), XtWindow(w), &wa);
	XSelectInput(XtDisplay(w), XtWindow(w),
		     wa.your_event_mask | PropertyChangeMask);

        l->pressed = 0;
	return l;
}

void
DynPencilRemove(Widget w, LocalInfo *l)
{
	OpRemoveEventHandler(w, opPixmap, ButtonPressMask, FALSE, (OpEventProc)press, l);
	OpRemoveEventHandler(w, opPixmap, ButtonMotionMask, FALSE, (OpEventProc)motion, l);
	OpRemoveEventHandler(w, opPixmap, PointerMotionMask|ButtonReleaseMask, FALSE, (OpEventProc)release, l);
	XtFree((XtPointer)l);
}
