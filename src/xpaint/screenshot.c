/*
 *   code taken from xsnap.c  by:
 *
 *   Copyright 1989 Clauss Strauch
 *                  cbs@cad.cs.cmu.edu
 *
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted.
 *   This software is provided "as is", without express or implied warranty.
 *   
 */

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Intrinsic.h>
#include "Paint.h"
#include "image.h"
#include "graphic.h"
#include "messages.h"
#include "misc.h"
#include "region.h"

/* extern procedures */
extern Image * outputImage;
extern char *routine;

/*  Leave arguments as globals, since there are so many of them.
 *  They'll only be referenced in process_args and main.
 */
static Colormap private_cmap = 0;

/*
 *  createEventWindow returns the ID of a InputOnly window that covers
 *  the given window.
 */

Window createEventWindow(Display *dpy, Window win, int init_cursor)
{
    XSetWindowAttributes xswa;
    unsigned long xswvaluemask;
    Window root_win;
    Window event_window;
    unsigned int win_width, win_height;
    unsigned int  win_border_width, win_depth;
    int win_x, win_y;
    
    /* get the geometry of the window  */
    
    XGetGeometry(dpy, win, &root_win, &win_x, &win_y,
		 &win_width, &win_height, &win_border_width, &win_depth);
    
    /* make an input only window to get events from  */

    xswa.cursor = init_cursor;
    xswa.override_redirect = True;
    xswa.event_mask = ButtonPressMask | ButtonReleaseMask | Button1MotionMask;
    xswvaluemask = CWCursor | CWOverrideRedirect | CWEventMask;
    event_window = XCreateWindow(dpy, win, win_x, win_y, win_width,
				 win_height, 0, 0, InputOnly, CopyFromParent, 
				 xswvaluemask, &xswa);
    return(event_window);
}

/*
 *   draw_box draws a box on the given window, with the given GC 
 *
 */

void draw_box(Display *dpy, Window win, GC gc, int x1, int y1, int x2, int y2)
{
    XSegment segments[4];
    segments[0].x1 = (short)x1;
    segments[0].y1 = (short)y1;
    segments[0].x2 = (short)x1;
    segments[0].y2 = (short)y2;
    
    segments[1].x1 = (short)x1;
    segments[1].y1 = (short)y1;
    segments[1].x2 = (short)x2;
    segments[1].y2 = (short)y1;
    
    segments[2].x1 = (short)x2;
    segments[2].y1 = (short)y2;
    segments[2].x2 = (short)x1;
    segments[2].y2 = (short)y2;

    segments[3].x1 = (short)x2;
    segments[3].y1 = (short)y2;
    segments[3].x2 = (short)x2;
    segments[3].y2 = (short)y1;

    XDrawSegments(dpy, win, gc, segments, 4);
}

/*
 *  get_region
 * takes as input:
 *    dpy
 *    win to get region from 
 *    pointers to x1, y1, width, height
 *  
 *   returns:  the position and width and height of a
 *             user selected region via the given pointers.
 *
 */

void
find_window(Display *dpy, int flag, int x, int y,
	   int *u, int *v, int *width, int *height)
{
    XWindowAttributes wa;
    Window findW = DefaultRootWindow(dpy), stopW = 0, childW, initW;

    XTranslateCoordinates(dpy, findW, findW, x, y, &x, &y, &stopW);

    if (stopW) 
        initW = stopW;
    else
        initW = findW;

    while (stopW) {
        XTranslateCoordinates(dpy, findW, stopW, x, y, &x, &y, &childW);
	findW = stopW;
	if (childW &&
	    XGetWindowAttributes(dpy, childW, &wa) &&
	    (wa.class != InputOutput))
	    break;
	stopW = childW;
    }

    if (!flag)
      findW = initW;

    XGetWindowAttributes(dpy, findW, &wa);
    *width = wa.width;
    *height = wa.height;
    private_cmap = wa.colormap;

    XTranslateCoordinates(dpy, findW, DefaultRootWindow(dpy),
			  0, 0, u, v, &stopW);
}

void get_region(Display *dpy, Window win,
	       int *x, int *y, unsigned *width, unsigned *height)
{
    Window event_window;
    Cursor up_right_curs, up_left_curs;
    Cursor low_right_curs, low_left_curs;
    Cursor current_cursor = None;
    int done;
    int init_x=0, init_y=0;
    int last_x, last_y;
    XEvent event;
    GC xor_gc;
    XGCValues xor_gc_values;             /* for creating xor_gc */
    unsigned long xor_gc_valuemask;       /* valuemask for creating xor_gc */

    /* make the GC and cursors we'll need */

    up_right_curs = XCreateFontCursor(dpy, XC_ur_angle);

    up_left_curs = XCreateFontCursor(dpy, XC_ul_angle);

    low_right_curs = XCreateFontCursor(dpy, XC_lr_angle);
	
    low_left_curs = XCreateFontCursor(dpy, XC_ll_angle);
	
    xor_gc_valuemask = GCFunction | GCSubwindowMode  | GCForeground;
    xor_gc_values.function = GXxor;
    xor_gc_values.foreground = 0xfd;
    xor_gc_values.subwindow_mode = IncludeInferiors;
    xor_gc = XCreateGC(dpy, win, xor_gc_valuemask, &xor_gc_values);

    event_window = createEventWindow(dpy, win, up_left_curs);
    XMapRaised(dpy, event_window);

    if (XGrabPointer(dpy, event_window, True, 
		     ButtonPressMask,
		     GrabModeAsync, GrabModeAsync, None, up_left_curs, 
		     CurrentTime) != 0) {
        fprintf(stderr, "%s", msgText[CANNOT_GRAB_POINTER]);
	return;
    }

    /* get the initial button  press */
    done = 0;
    while (done == 0) {
        XNextEvent(dpy, &event);
	switch(event.type) {
	case MappingNotify:
	    XRefreshKeyboardMapping((XMappingEvent *)&event);
	    break;
	case ButtonPress:
	    if (event.xbutton.button == 1) {
	        init_x = event.xbutton.x;
		init_y = event.xbutton.y;
		done = 1;
		break;
	    }
	    if (event.xbutton.button == 2) {
	        init_x = event.xbutton.x;
		init_y = event.xbutton.y;
		done = 2;
		break;
	    }
	    if (event.xbutton.button == 3) {
	        *width = 0;
		*height = 0; 
		done = -1;
		break;
	    }
	}
    }

    /*  now we have the location of one corner of the box.   change the cursor,
     *  and have the user drag out the area.
     */
    last_x = init_x;
    last_y = init_y;
    if (done == 1) {
        current_cursor = low_right_curs;
	XChangeActivePointerGrab(dpy, ButtonReleaseMask | Button1MotionMask,
				 current_cursor, CurrentTime);
	done = 0;
	draw_box(dpy, win, xor_gc, init_x, init_y, last_x, last_y);
    }
    while (!done) {
        XNextEvent(dpy, &event);
	switch(event.type) {
	case MappingNotify:
	    XRefreshKeyboardMapping((XMappingEvent *)&event);
	    break;
	case MotionNotify:
	    draw_box(dpy, win, xor_gc, 
		    init_x, init_y, last_x, last_y);  /* erase old */
	    last_x = event.xmotion.x;
	    last_y = event.xmotion.y;
	    draw_box(dpy, win, xor_gc, 
		    init_x, init_y, last_x, last_y); /* draw new  */
	    /*  Change cursor to correspond to position of pointer */
	    if ((init_x < last_x) && (init_y < last_y)
		&& (current_cursor != low_right_curs)) {
	        current_cursor = low_right_curs;
		XChangeActivePointerGrab(dpy, 
					 ButtonReleaseMask | Button1MotionMask,
					 low_right_curs, CurrentTime);
	    }
	    else if ((last_x < init_x) && (last_y < init_y)
		     &&  (current_cursor != up_left_curs)) {
	        current_cursor = up_left_curs;
		XChangeActivePointerGrab(dpy, 
					 ButtonReleaseMask | Button1MotionMask,
					 up_left_curs, CurrentTime);
	    }
	    else if ((init_x < last_x) && (last_y < init_y)
		     && (current_cursor != up_right_curs)) {
	        current_cursor = up_right_curs;
		XChangeActivePointerGrab(dpy, 
					 ButtonReleaseMask | Button1MotionMask,
					 up_right_curs, CurrentTime);
	    }
	    else if ((last_x < init_x) && (init_y < last_y)
		     && (current_cursor != low_left_curs)) {
	        current_cursor = low_left_curs;
		XChangeActivePointerGrab(dpy, 
					 ButtonReleaseMask | Button1MotionMask,
					 low_left_curs, CurrentTime);
	    }
	    break;
	case ButtonRelease:
	    if (event.xbutton.button == 1) {
	        done = True;
		draw_box(dpy, win, xor_gc, 
			 init_x, init_y, last_x, last_y);  /* erase last box drawn */
	    }
	    break;
	}
    }
    XFlush(dpy);   /*  gets rid of last box on screen  */
    if (init_x < last_x)
      *x = init_x;
    else
      *x = last_x;
    if (init_y < last_y)
      *y = init_y;
    else
      *y = last_y;
    *width = (unsigned int)abs(last_x - init_x);
    *height = (unsigned int)abs(last_y - init_y);

    /* clean up after ourself: */

    XDestroyWindow(dpy, event_window);
    XFreeGC(dpy, xor_gc);

    /* we'll let the caller ungrab the pointer */

    if (done == 1) {
      int u;
      if (*width==0 && *height==0)
	  find_window(dpy, 0, init_x, init_y, x, y, (int*) width, (int*) height);
      else
	  find_window(dpy, 0, init_x, init_y, &u, &u, &u, &u);
    }

    if (done == 2)
        find_window(dpy, 1, init_x,init_y, x, y, (int*) width, (int*) height);
}

/* 
 *  get_pixmap_region
 *
 *       input :
 *               a dpy, a window, x, y, width, height, interactive.
 *               if interactive, the user is prompted for a region,
 *               other wise the given region is copied to a pixmap.
 *       returns : a pixmap containing a copy of a user-specified area
 *                 of the given window;
 *
 */

Pixmap get_pixmap_region(Display *dpy, int screen, Window win, GC gc,
		         int *x, int *y,
		         unsigned *width, unsigned *height, unsigned *depth)
{
    int reg_x, reg_y;
    unsigned int reg_width, reg_height;
    Pixmap pixmap_returned;
    int junk_left, junk_top, junk_width, junk_height, junk_border_width;
    Window junk_root;

    get_region(dpy, win, &reg_x, &reg_y, &reg_width, &reg_height);
    *x = reg_x;
    *y = reg_y;
    *width = reg_width;
    *height = reg_height;

    if (*width==0 || *height==0) return None;

    /* Use the depth of `win' for the depth of the pixmap */
	  
    XGetGeometry (dpy, win, &junk_root, &junk_left, &junk_top,
		  (unsigned int*) &junk_width, (unsigned int*) &junk_height,
		  (unsigned int*) &junk_border_width, depth);

    pixmap_returned = XCreatePixmap(dpy, 
				    DefaultRootWindow(dpy),
				    *width, *height, *depth);

    /*  now copy the area we specified  */

    XCopyArea(dpy, win, pixmap_returned, gc, *x, *y, 
	      *width, *height, 0, 0);
    XUngrabPointer(dpy, CurrentTime);
    return pixmap_returned;
}

void
ScreenshotImage(Widget w, XtPointer paintArg, int flag)
{
    Display *dpy;
    int screen;
    GC copy_gc;
    unsigned long copy_gc_valuemask;
    XGCValues copy_gc_values;
    Widget ww;
    Window window_to_snap;            
    Pixmap  snap_pixmap;
    int reg_x, reg_y;
    unsigned int reg_width, reg_height, reg_depth;
    Image *image;

    routine = "snapshot";
    dpy = XtDisplayOfObject(w);
    screen = DefaultScreen(dpy);
    XSetErrorHandler(privateXErrorHandler);

    /* start with root window for now */
    window_to_snap = XRootWindow(dpy, screen);

    /* make copy GC */

    copy_gc_valuemask = GCSubwindowMode;
    copy_gc_values.subwindow_mode = IncludeInferiors;
    copy_gc = XCreateGC(dpy, window_to_snap, copy_gc_valuemask, &copy_gc_values);

    XFlush(dpy);
    XGrabServer(dpy);

    snap_pixmap = get_pixmap_region(dpy, screen,
				    window_to_snap, copy_gc,
				    &reg_x, &reg_y,
				    &reg_width, &reg_height,
				    &reg_depth);

    /* ungrab the server and free GC */
    XUngrabServer(dpy);
    XFreeGC(dpy, copy_gc);

    if (!snap_pixmap || reg_width==0 || reg_height==0)
        return;

    if (paintArg) ww = (Widget)paintArg; else ww = w;
    image = PixmapToImage(ww, snap_pixmap, private_cmap);
    if (snap_pixmap) XFreePixmap(dpy, snap_pixmap);
   
    if (!image) return;

    if (flag) {
        ClipboardSetImage(ww, image);
	StdPasteCallback(ww, paintArg, (XtPointer) NULL);
        XtVaSetValues(ww, XtNdirty, True, NULL);
    }
    else {
        Widget paint;
        snap_pixmap = None;
        ImageToPixmap(image, w, &snap_pixmap, &private_cmap);
	paint = (Widget) graphicCreate(makeGraphicShell(w), 
		      reg_width, reg_height, -1, 
				       snap_pixmap, private_cmap, NULL);
        if (snap_pixmap) XFreePixmap(dpy, snap_pixmap);
        XtVaSetValues(paint, XtNdirty, True, NULL);
    }
}
