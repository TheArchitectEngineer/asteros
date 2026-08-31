
/* +-------------------------------------------------------------------+ */
/* | Copyright 1993, David Koblas (koblas@netcom.com)                  | */
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

/* $Id: protocol.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtIsRealized XTISREALIZED
#define XtParent XTPARENT
#define XtScreen XTSCREEN
#define XtWindow XTWINDOW
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "xpaint.h"
#include "messages.h"
#include "misc.h"
#include "protocol.h"


typedef struct li_s {
    /*
    **  Destroy request info
     */
    void *data;
    DestroyCallbackFunc func;

    /*
    **  Info for watch cursor
     */
    int watchCount, computingCount;
    Widget shell, parent;
    Window win;
    Display *dpy;
    struct li_s *next;
} LocalInfo;

#define	DO_MAP(l)	((l)->watchCount + (l)->computingCount > 0)

#include "bitmaps/xbm/wait1.xbm"
#include "bitmaps/xbm/wait2.xbm"
#include "bitmaps/xbm/wait3.xbm"
#include "bitmaps/xbm/wait4.xbm"

static Atom delwin = None, protocols = None;
static LocalInfo *head = NULL;

static int currentCursor = 0;
static Cursor watchCursor = None;

static struct {
    Cursor cursor;
    int width, height;
    unsigned char *bits;
} cursorInfo[] =

{
    {
	None, wait1_width, wait1_height, wait1_bits
    }
    ,
    {
	None, wait2_width, wait2_height, wait2_bits
    }
    ,
    {
	None, wait3_width, wait3_height, wait3_bits
    }
    ,
    {
	None, wait4_width, wait4_height, wait4_bits
    }
    ,
};

static void 
initCursors(Widget w)
{
    static int inited = False;
    Display *dpy = XtDisplay(w);
    Screen *screen = XtScreen(w);
    GC gc = None;
    Pixmap pix, mask;
    int i, width, height;
    XImage *src, *msk;
    int x, y;
    XColor xcols[2];

    if (inited)
	return;
    inited = True;

    xcols[0].pixel = BlackPixelOfScreen(screen);
    xcols[1].pixel = WhitePixelOfScreen(screen);

    XQueryColors(dpy, DefaultColormapOfScreen(screen), xcols, XtNumber(xcols));

    for (i = 0; i < XtNumber(cursorInfo); i++) {
	width = cursorInfo[i].width;
	height = cursorInfo[i].height;
	pix = XCreatePixmapFromBitmapData(dpy, RootWindowOfScreen(screen),
	     (char *) cursorInfo[i].bits, width, height, True, False, 1);
	src = XGetImage(dpy, pix, 0, 0, width, height, AllPlanes, ZPixmap);
	msk = NewXImage(dpy, NULL, 1, width, height);
	for (y = 0; y < height; y++) {
	    for (x = 0; x < width; x++) {
		Boolean flg = (Boolean) XGetPixel(src, x, y);

		if (!flg && x > 0)
		    flg = XGetPixel(src, x - 1, y);
		if (!flg && x < width - 1)
		    flg = XGetPixel(src, x + 1, y);
		if (!flg && y > 0)
		    flg = XGetPixel(src, x, y - 1);
		if (!flg && y < height - 1)
		    flg = XGetPixel(src, x, y + 1);
		XPutPixel(msk, x, y, flg);
	    }
	}

	mask = XCreatePixmap(dpy, pix, width, height, 1);
	if (gc == None)
	    gc = XCreateGC(dpy, mask, 0, 0);
	XPutImage(dpy, mask, gc, msk, 0, 0, 0, 0, width, height);

	XDestroyImage(src);
	XDestroyImage(msk);

	cursorInfo[i].cursor = XCreatePixmapCursor(dpy, pix, mask,
			    &xcols[0], &xcols[1], width / 2, height / 2);
	XFreePixmap(dpy, pix);
	XFreePixmap(dpy, mask);
    }
    if (gc != None)
	XFreeGC(dpy, gc);
}

static void 
destroyCallback(Widget w, XtPointer data, XtPointer junk)
{
    LocalInfo *l = (LocalInfo *) data;
    LocalInfo *cur, **pp;

    for (cur = *(pp = &head); cur != NULL && cur != l; cur = *(pp = &cur->next));

    if (cur == NULL)
	return;

    *pp = cur->next;

    XtFree((XtPointer) data);
}

static void 
handler(Widget w, XtPointer lArg, XEvent * eventArg, Boolean * junk)
{
    XClientMessageEvent *event = (XClientMessageEvent *) eventArg;
    LocalInfo *l = (LocalInfo *) lArg;
    if (event->type != ClientMessage)
	return;

    if (event->message_type == protocols && event->data.l[0] == delwin)
	l->func(w, l->data, eventArg);
}

static void 
add(Widget w, LocalInfo * l)
{
    unsigned long valuemask;
    XSetWindowAttributes attributes;
    /*
    **  Set up the shell destroy info
     */
    XtAddRawEventHandler(w, 0, True, handler, (XtPointer) l);
    XChangeProperty(XtDisplay(w), XtWindow(w), protocols,
		    XA_ATOM, 32, PropModeReplace,
		    (unsigned char *) &delwin, 1);

    /* 
    **  Now set up watch cursor information
    **
    **  Ignore device events while the busy cursor is displayed.
     */
    valuemask = CWDontPropagate | CWCursor;
    attributes.do_not_propagate_mask = (KeyPressMask | KeyReleaseMask |
		ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    if (watchCursor == None)
	watchCursor = XCreateFontCursor(XtDisplay(w), XC_watch);
    attributes.cursor = watchCursor;

    /*
    ** The window will be as big as the display screen, and clipped by
    ** its own parent window, so we never have to worry about resizing
     */
    l->win = XCreateWindow(XtDisplay(w), XtWindow(w), 0, 0,
		 WidthOfScreen(XtScreen(w)), HeightOfScreen(XtScreen(w)),
			   (unsigned int) 0, 0, InputOnly,
			   CopyFromParent, valuemask, &attributes);

    if (DO_MAP(l))
	RaiseWindow(l->dpy, l->win);
}


static void 
realize(Widget w, XtPointer ldataArg, XEvent * event, Boolean * junk)
{
    if (event->type == MapNotify) {
	XtRemoveEventHandler(w, StructureNotifyMask, False,
			     realize, ldataArg);
	add(w, (LocalInfo *) ldataArg);
    }
}

void 
AddDestroyCallback(Widget w, DestroyCallbackFunc func, void *data)
{
    LocalInfo *l;

    if (delwin == None)
	delwin = XInternAtom(XtDisplay(w), "WM_DELETE_WINDOW", FALSE);
    if (protocols == None)
	protocols = XInternAtom(XtDisplay(w), "WM_PROTOCOLS", FALSE);

    initCursors(w);

    if (delwin == None || protocols == None) {
	fprintf(stderr, "%s", msgText[ERROR_IN_XINTERNATOM]);
	exit(1);
    }

    l = XtNew(LocalInfo);
    l->data = data;
    l->func = func;
    l->win = None;
    l->dpy = XtDisplay(w);
    l->watchCount = 0;
    l->computingCount = 0;
    l->shell = w;
    l->parent = None;

    if (!XtIsRealized(w))
	XtAddEventHandler(w, StructureNotifyMask, False, realize, (XtPointer) l);
    else
	add(w, l);

    XtAddCallback(w, XtNdestroyCallback, destroyCallback, (XtPointer) l);

    l->next = head;
    head = l;
}

static Boolean
setBusy(LocalInfo * cur, Boolean isWatch, Boolean flag)
{
    Boolean doFlush = False;

    if (isWatch) {
	if (flag) {
	    cur->watchCount++;
	} else {
	    if (cur->watchCount == 0)
		return False;
	    cur->watchCount--;
	}
    } else {
	if (flag) {
	    cur->computingCount++;
	} else {
	    if (cur->computingCount == 0)
		return False;
	    cur->computingCount--;
	}
    }

    if (cur->win == None)
	return False;

    if (cur->watchCount == 0 && cur->computingCount == 0) {
	XUnmapWindow(cur->dpy, cur->win);
	return False;
    }
    if (cur->watchCount == 1) {
	if (isWatch) {
	    RaiseWindow(cur->dpy, cur->win);
	    XDefineCursor(cur->dpy, cur->win, watchCursor);
	    doFlush = True;
	}
    } else if (cur->computingCount == 1) {
	if (!isWatch) {
	    RaiseWindow(cur->dpy, cur->win);
	    XDefineCursor(cur->dpy, cur->win, cursorInfo[currentCursor].cursor);
	    doFlush = True;
	}
    }
    return doFlush;
}

/*
 * Turn watch cursor on (True) or off (False).
 */
void 
StateSetBusyWatch(Boolean flag)
{
    LocalInfo *cur;
    Boolean doFlush = False;

    for (cur = head; cur != NULL; cur = cur->next)
	doFlush |= setBusy(cur, True, flag);

    if (doFlush)
	XFlush(head->dpy);
}

/*
 * Turn busy cursor on (True) or off (False).
 */
void 
StateSetBusy(Boolean flag)
{
    LocalInfo *cur;
    Boolean doFlush = False;

    for (cur = head; cur != NULL; cur = cur->next)
	doFlush |= setBusy(cur, False, flag);

    if (doFlush)
	XFlush(head->dpy);
}

void 
StateShellBusy(Widget w, Boolean flag)
{
    LocalInfo *cur;
    Boolean doFlush = False;

    while (w && !XtIsShell(w))
	w = XtParent(w);

    if (w == None)
	return;

    for (cur = head; cur != NULL; cur = cur->next) {
	if (cur->shell == w) {
	    doFlush |= setBusy(cur, True, flag);
	} else if (cur->parent == w) {
	    StateShellBusy(cur->shell, flag);
	}
    }
    if (doFlush)
	XFlush(head->dpy);
}

void 
StateAddParent(Widget w, Widget parent)
{
    LocalInfo *cur;

    while (parent && !XtIsShell(parent))
	parent = XtParent(parent);
    if (!parent) return;

    while (w && !XtIsShell(w))
	w = XtParent(w);
   if (!w) return;

    for (cur = head; cur != NULL && cur->shell != w; cur = cur->next);
    if (cur == NULL)
	return;

    cur->parent = parent;
}

void 
StateTimeStep(void)
{
    static Boolean inited = False;
    static struct timeval lastTime;
    struct timeval nowTime;
    Boolean done = False, need = False;
    Cursor c;
    LocalInfo *cur;

    for (cur = head; cur != NULL; cur = cur->next)
	if (cur->watchCount == 0 && cur->computingCount != 0 && cur->win != None)
	    need = True;

    if (!need)
	return;

    gettimeofday(&nowTime, NULL);
    if (inited) {
	long ds, dus;

	dus = nowTime.tv_usec - lastTime.tv_usec;
	ds = nowTime.tv_sec - lastTime.tv_sec;
	if (dus < 0) {
	    ds--;
	    dus += 1000000;
	}
	if (ds == 0 && dus < 200000)
	    return;
    } else {
	inited = True;
    }
    lastTime = nowTime;


    if (++currentCursor == XtNumber(cursorInfo))
	currentCursor = 0;

    c = cursorInfo[currentCursor].cursor;

    for (cur = head; cur != NULL; cur = cur->next) {
	if (cur->watchCount == 0 && cur->computingCount != 0 && cur->win != None) {
	    XDefineCursor(cur->dpy, cur->win, c);
	    done = True;
	}
    }

    if (done)
	XFlush(head->dpy);
}
