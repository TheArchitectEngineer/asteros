/* +-------------------------------------------------------------------+ */
/* | Copyright 1993, David Koblas (koblas@netcom.com)		       | */
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

/* $Id: grab.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtScreen XTSCREEN
#endif

#include <X11/Intrinsic.h>
#include <X11/cursorfont.h>
#include "misc.h"
#include "image.h"

#define GRAB_FAST

/*
**  Convenience function for doing server pointer grabs
 */
#define GRAB_INTERVAL	30

typedef struct {
    XtAppContext app;
    Display *dpy;
    GC gc;
    Window root;
    Boolean drawn;
    int x, y, ox, oy, width, height;
    XtIntervalId id;
} GrabInfo;

/*
 * Draws the box cursor used when grabbing a rectangle.
 */
static void 
grabHilite(XtPointer infoArg, XtIntervalId * id)
{
    GrabInfo *info = (GrabInfo *) infoArg;

    if (info->drawn) {
	if (info->ox == info->x && info->oy == info->y)
	    goto end;

	XDrawRectangle(info->dpy, info->root, info->gc,
		       info->ox - info->width / 2,
		       info->oy - info->height / 2,
		       info->width, info->height);
    }
    info->drawn = True;

    XDrawRectangle(info->dpy, info->root, info->gc,
		   info->x - info->width / 2,
		   info->y - info->height / 2,
		   info->width, info->height);
    info->ox = info->x;
    info->oy = info->y;

  end:
    info->id = XtAppAddTimeOut(info->app, GRAB_INTERVAL,
			       grabHilite, (XtPointer) info);
}

/*
 * Given coords x,y in the 'base' window, descend the window hierarchy
 * and find the child window of class InputOutput containing those
 * coordinates. Return coords in child window in (*nx,*ny).
 * If the child window has a colormap, return that; otherwise return
 * the default colormap for the display.
 */
static void 
xyToWindowCmap(Display * dpy, int x, int y, Window base,
	       int *nx, int *ny, Window * window, Colormap * cmap)
{
    Window twin;
    Colormap tmap;
    Window child, sub;
    XWindowAttributes attr;

    twin = base;
    tmap = None;

    sub = base;
    *nx = x;
    *ny = y;

    while (sub != None) {
	x = *nx;
	y = *ny;
	child = sub;
	XTranslateCoordinates(dpy, base, child, x, y, nx, ny, &sub);
	base = child;

	XGetWindowAttributes(dpy, child, &attr);
	if (attr.class == InputOutput && attr.colormap != None) {
	    tmap = attr.colormap;
	    twin = child;
	}
    }

    if (tmap == None)
	*cmap = DefaultColormap(dpy, DefaultScreen(dpy));
    else
	*cmap = tmap;
    *window = twin;
}

/*
 * Grab a rectangle of some window.
 * Zero width and height specifies to just grab a single pixel.
 * Returns coords of event.
 */
static void 
doGrab(Widget w, int width, int height, int *x, int *y)
{
    Display *dpy = XtDisplay(w);
    XtAppContext app = XtWidgetToApplicationContext(w);
    Window root = DefaultRootWindow(dpy);
    XEvent event;
    Cursor cursor = XCreateFontCursor(dpy, XC_crosshair);
    int count = 0;
    GrabInfo *info = NULL;

    /* Set up grab cursor */
    if (width != 0 && height != 0) {
	XGCValues gcv;
	info = XtNew(GrabInfo);
	gcv.function = GXxor;
	gcv.foreground = 1;
	gcv.subwindow_mode = IncludeInferiors;
	info->root = root;
	info->app = app;
	info->x = 0;
	info->y = 0;
	info->drawn = False;
	info->dpy = dpy;
	info->gc = XCreateGC(dpy, root,
			     GCSubwindowMode | GCForeground | GCFunction, &gcv);
	info->width = width;
	info->height = height;
	info->id = XtAppAddTimeOut(app, GRAB_INTERVAL,
				   grabHilite, (XtPointer) info);
    }
    if (XGrabPointer(dpy, root, False,
		     info == NULL ? ButtonPressMask | ButtonReleaseMask
	       : ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		     GrabModeSync, GrabModeAsync,
		     root, cursor, CurrentTime))
	return;

    do {
	XAllowEvents(dpy, SyncPointer, CurrentTime);
	XtAppNextEvent(app, &event);
	if (event.type == ButtonPress)
	    count++;
	else if (event.type == ButtonRelease) {
	    if (count == 1)
		break;
	    else
		count--;
	} else if (event.type == MotionNotify) {
	    info->x = event.xmotion.x;
	    info->y = event.xmotion.y;
	} else
	    XtDispatchEvent(&event);
    }
    while (True);

    XUngrabPointer(dpy, CurrentTime);

    if (info != NULL) {
	/* remove grab cursor */
	if (info->drawn)
	    XDrawRectangle(info->dpy, info->root, info->gc,
			   info->ox - info->width / 2,
			   info->oy - info->height / 2,
			   info->width, info->height);
	XtRemoveTimeOut(info->id);
	XFreeGC(XtDisplay(w), info->gc);
	XtFree((XtPointer) info);
    }
    *x = event.xbutton.x;
    *y = event.xbutton.y;
}

Image *
DoGrabImage(Widget w, int width, int height)
{
    Screen *screen = XtScreen(w);
    Display *dpy = XtDisplay(w);
    Window root = RootWindowOfScreen(screen);
    int x, y, nx, ny, tx, ty;
    Colormap cmap, lastCmap;
    Window window, lastWindow;
    unsigned char *ip;
    Image *image;
    XColor *xcol;
    XImage *xim;
#ifndef GRAB_FAST
    Colormap dmap = DefaultColormapOfScreen(screen);
    int fx, fy, i, count, xi, yi;
#endif

    doGrab(w, width, height, &x, &y);

    x -= width / 2;		/* center the grabbed region around event coords */
    y -= height / 2;

    /* Do some manual clipping */
    if (x < 0) {
	width += x;
	x = 0;
    }
    if (y < 0) {
	height += y;
	y = 0;
    }
    if (x + width > WidthOfScreen(screen))
	width = WidthOfScreen(screen) - x;
    if (y + height > HeightOfScreen(screen))
	height = HeightOfScreen(screen) - x;

    if (width == 0 || height == 0)
	return NULL;

    image = ImageNew(width, height);
    ip = image->data;

    xcol = (XColor *) XtCalloc(width, sizeof(XColor));

#ifdef GRAB_FAST
    /*
    **	Fast grabs, just use the colormap at the 0,0 position 
    **	  gotcha is that the XImage is from the root window
    **	  thus, with its depth.
     */

    xyToWindowCmap(dpy, x, y, root, &nx, &ny, &window, &cmap);
    xyToWindowCmap(dpy, x + width, y + height, root,
		   &tx, &ty, &lastWindow, &lastCmap);

    /*
    **	XXX Improvement:
    **	    instead of window == lastWindow
    **	    we could check for common parent, and use that.
     */
    if (window == lastWindow)
	xim = XGetImage(dpy, window, nx, ny, width, height, AllPlanes, ZPixmap);
    else
	xim = XGetImage(dpy, root, x, y, width, height, AllPlanes, ZPixmap);

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    xcol[x].pixel = XGetPixel(xim, x, y);
	    xcol[x].flags = DoRed | DoGreen | DoBlue;
	}
	XQueryColors(XtDisplay(w), cmap, xcol, width);
	for (x = 0; x < width; x++) {
	    *ip++ = (xcol[x].red >> 8) & 0xff;
	    *ip++ = (xcol[x].green >> 8) & 0xff;
	    *ip++ = (xcol[x].blue >> 8) & 0xff;
	}
    }
#else
    /*
    **	Slow grabs, get the correct color and colormap though a lot of
    **	  server calls.
     */
    StateSetBusyWatch(True);

    for (yi = 0; yi < height; yi++) {
	count = 0;
	lastCmap = None;
	lastWindow = None;
	for (xi = 0; xi < width; xi++, count++) {
	    xyToWindowCmap(dpy, xi + x, yi + y, root, &nx, &ny, &window, &cmap);
	    if ((window != lastWindow || cmap != lastCmap) && count != 0) {
		if (cmap == lastCmap && lastWindow == root)
		    continue;

		xim = XGetImage(dpy, lastWindow, fx, fy, count, 1,
				AllPlanes, ZPixmap);

		for (i = 0; i < count; i++) {
		    xcol[i].pixel = XGetPixel(xim, i, 0);
		    xcol[i].flags = DoRed | DoGreen | DoBlue;
		}
		XQueryColors(XtDisplay(w), lastCmap, xcol, count);

		for (i = 0; i < count; i++) {
		    *ip++ = (xcol[i].red >> 8) & 0xff;
		    *ip++ = (xcol[i].green >> 8) & 0xff;
		    *ip++ = (xcol[i].blue >> 8) & 0xff;
		}

		XDestroyImage(xim);

		count = 0;
	    }
	    if (count == 0) {
		fx = nx;
		fy = ny;
		if (cmap == dmap) {
		    fx = x + xi;
		    fy = y + yi;
		    lastWindow = root;
		} else {
		    lastWindow = window;
		}
		lastCmap = cmap;
	    }
	}
	if (count != 0) {
	    xim = XGetImage(dpy, lastWindow, fx, fy, count, 1,
			    AllPlanes, ZPixmap);

	    for (i = 0; i < count; i++) {
		xcol[i].pixel = XGetPixel(xim, i, 0);
		xcol[i].flags = DoRed | DoGreen | DoBlue;
	    }
	    XQueryColors(XtDisplay(w), lastCmap, xcol, count);

	    for (i = 0; i < count; i++) {
		*ip++ = (xcol[i].red >> 8) & 0xff;
		*ip++ = (xcol[i].green >> 8) & 0xff;
		*ip++ = (xcol[i].blue >> 8) & 0xff;
	    }

	    XDestroyImage(xim);
	}
    }

    StateSetBusyWatch(False);
#endif

    XtFree((XtPointer) xcol);
    XDestroyImage(xim);

    return image;
}

/*
**  Grab the pixel value from some other window
**
**  Store pixel value in *p and colormap ID in *cmap unless they are NULL.
**
**   General strategy:
**     Grab the cursor
**     Wait for the up/down button event
**     Lookup what window the event is over
**     Query the pixel value
 */
void 
DoGrabPixel(Widget w, Pixel * p, Colormap * cmap)
{
    int x, y, nx, ny;
    XImage *xim;
    Colormap amap;
    Window root = RootWindowOfScreen(XtScreen(w));
    Window window;
    Display *dpy = XtDisplay(w);

    doGrab(w, 0, 0, &x, &y);

    if (cmap == NULL)
	cmap = &amap;

    xyToWindowCmap(dpy, x, y, root, &nx, &ny, &window, cmap);

    xim = XGetImage(dpy, window, nx, ny, 1, 1, AllPlanes, ZPixmap);

    if (p != NULL)
	*p = XGetPixel(xim, 0, 0);

    XDestroyImage(xim);
}

/*
**  Grab the RGB value from some other window
**
**   General strategy:
**     Grab the cursor
**     Wait for the up/down button event
**     Lookup what window the event is over
**     Query the pixel value
**     Query the colormap of the window
**     Query the rgb pixel value
 */
XColor *
DoGrabColor(Widget w)
{
    static XColor xcol;
    Colormap cmap;
    Pixel p;

    DoGrabPixel(w, &p, &cmap);

    xcol.pixel = p;
    xcol.flags = DoRed | DoGreen | DoBlue;
    XQueryColor(XtDisplay(w), cmap, &xcol);

    return &xcol;
}
