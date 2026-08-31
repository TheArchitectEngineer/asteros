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

/* $Id: color.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>

#include "xaw_incdir/Form.h"
#include "xaw_incdir/Label.h"


#undef NeedFunctionPrototypes
#define NeedFunctionPrototypes 1
#undef NeedWidePrototypes
#include "xaw_incdir/Scrollbar.h"
#include "xaw_incdir/AsciiText.h"
#include "xaw_incdir/Command.h"
#include "xaw_incdir/Text.h"

#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#endif

#include "palette.h"
#include "protocol.h"
#include "color.h"

#include <math.h>

#include "xpaint.h"
#include "messages.h"
#include "misc.h"
#include "image.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

#define PIXMAP_WIDTH  120
#define PIXMAP_HEIGHT 120
#define RADIUS	      50

#define RECOMPUTE(l)					\
	if (l->isRGB)					\
	  rgbTOhsv(l->r,l->g,l->b,&l->h,&l->s,&l->v);	\
	else						\
	  hsvTOrgb(l->h,l->s,l->v,&l->r,&l->g,&l->b);

typedef void (*cbFunc_t) (Widget, XtPointer, Pixel);

typedef struct LocalInfo_s {
    Boolean initing, inCallback;
    Widget form;

    Widget valueBar, redBar, greenBar, blueBar;

    /*
    **	Color image picker widget, and delta x, y;
    **	 also a copy gc, and the image
     */
    GC gc, xgc;
    Pixmap pixmap;
    Widget picker;
    Widget select;
    Widget asbg;
    int dx, dy;
    Pixel bg;

    /*
    **	Static current view
     */
    Widget view;
    Pixel pixel;
    Colormap cmap;

    /*
    **
     */
    Widget valueText, redText, greenText, blueText;

    Boolean isRGB;
    float h, s, v;
    float r, g, b;

    Palette *map;
    cbFunc_t func;
    XtPointer data;

    struct LocalInfo_s *next;
} LocalInfo;

static LocalInfo *head = NULL;

static void 
rgbTOhsv(float r, float g, float b, float *h, float *s, float *v)
{
    float max = MAX(r, MAX(g, b));
    float min = MIN(r, MIN(g, b));
    float delta;

    *v = max;
    if (max != 0.0)
	*s = (max - min) / max;
    else
	*s = 0.0;

    if (*s == 0.0) {
	*h = 0.0;
    } else {
	delta = max - min;
	if (r == max)
	    *h = (g - b) / delta;
	else if (g == max)
	    *h = 2.0 + (b - r) / delta;
	else			/* if (b == max) */
	    *h = 4.0 + (r - g) / delta;
	*h *= 60.0;
	if (*h < 0.0)
	    *h += 360.0;
    }
}

static void 
hsvTOrgb(float h, float s, float v, float *r, float *g, float *b)
{
    int i;
    float f, p, q, t;

    if (s == 0 && h == 0) {
	*r = *g = *b = v;
    } else {
	if (h >= 360.0)
	    h = 0.0;
	h /= 60.0;

	i = h;
	f = h - i;
	p = v * (1 - s);
	q = v * (1 - (s * f));
	t = v * (1 - (s * (1 - f)));
	switch (i) {
	case 0:
	    *r = v;
	    *g = t;
	    *b = p;
	    break;
	case 1:
	    *r = q;
	    *g = v;
	    *b = p;
	    break;
	case 2:
	    *r = p;
	    *g = v;
	    *b = t;
	    break;
	case 3:
	    *r = p;
	    *g = q;
	    *b = v;
	    break;
	case 4:
	    *r = t;
	    *g = p;
	    *b = v;
	    break;
	case 5:
	    *r = v;
	    *g = p;
	    *b = q;
	    break;
	}
    }
}

/*
**  Event callback routines
 */
static void 
drawCursor(LocalInfo * l, Boolean flag)
{
    Dimension width, height;
    int sx, sy;

    if (!XtIsRealized(l->picker))
	return;

    XtVaGetValues(l->picker, XtNwidth, &width, XtNheight, &height, NULL);

    sx = -l->dx - 3 + width / 2;
    sy = -l->dy - 3 + height / 2;

    if (flag) {
        if (!l->xgc)
            l->xgc = XtGetGC(l->picker, 0, NULL);
	XSetForeground(XtDisplay(l->picker), l->xgc,
			   BlackPixelOfScreen(XtScreen(l->picker)));
	XDrawArc(XtDisplay(l->picker), XtWindow(l->picker), l->xgc,
		 sx, sy, 6, 6, 0, 360 * 64);
    } else {
	int cx, cy;
	cx = PIXMAP_WIDTH / 2;
	cy = PIXMAP_HEIGHT / 2;

	XCopyArea(XtDisplay(l->picker), l->pixmap, XtWindow(l->picker), l->gc,
		  -l->dx + cx - 4, -l->dy + cy - 4,
		  8, 8, sx - 1, sy - 1);
    }
}

static void 
update(LocalInfo * l, Boolean doSB, Boolean doText, Boolean doCursor)
{
    XColor col;
    Pixel p;

    if (doSB) {
	XawScrollbarSetThumb(l->valueBar, l->v, -1.0);
	XawScrollbarSetThumb(l->redBar, l->r, -1.0);
	XawScrollbarSetThumb(l->greenBar, l->g, -1.0);
	XawScrollbarSetThumb(l->blueBar, l->b, -1.0);
    }
    if (doText) {
	char buf[20];

	sprintf(buf, "%5.3f", l->v);
	XtVaSetValues(l->valueText, XtNstring, buf, NULL);
	sprintf(buf, "%d", (int) (l->r * 255));
	XtVaSetValues(l->redText, XtNstring, buf, NULL);
	sprintf(buf, "%d", (int) (l->g * 255));
	XtVaSetValues(l->greenText, XtNstring, buf, NULL);
	sprintf(buf, "%d", (int) (l->b * 255));
	XtVaSetValues(l->blueText, XtNstring, buf, NULL);
    }
    if (doCursor) {
	drawCursor(l, False);

	l->dx = sin(l->h * M_PI / 180.0) * l->s * RADIUS;
	l->dy = cos(l->h * M_PI / 180.0) * l->s * RADIUS;

	drawCursor(l, True);
    }
    if (l->initing)
	return;

    col.flags = DoRed | DoGreen | DoBlue;
    col.red = l->r * 0xffff;
    col.green = l->g * 0xffff;
    col.blue = l->b * 0xffff;
    col.pixel = l->pixel;
    p = l->pixel;

#if 0
    if ((l->map != NULL) &&
	(l->map->readonly_ ||
	 ((DefaultVisualOfScreen(XtScreen(l->view))->class & 1) == 0)))
#endif
	XtVaSetValues(l->view, XtNbackground,
		      p = PaletteAlloc(l->map, &col), NULL);
#if 0
    else
	XStoreColor(XtDisplay(l->view), l->cmap, &col);
#endif

    if (l->func != NULL) {
	l->inCallback = True;
	l->func(l->form, l->data, p);
	l->inCallback = False;
    }
}

static void 
expose(Widget w, LocalInfo * l, XExposeEvent * event, Boolean * flg)
{
    Dimension width, height;
    int dx, dy;

    XtVaGetValues(w, XtNwidth, &width, XtNheight, &height, NULL);

    dx = (PIXMAP_WIDTH - width) / 2;
    dy = (PIXMAP_HEIGHT - height) / 2;

    XCopyArea(XtDisplay(w), l->pixmap, XtWindow(w), l->gc,
	      event->x + dx, event->y + dy,
	      event->width, event->height,
	      event->x, event->y);
    drawCursor(l, True);
}

static void 
motion(Widget w, LocalInfo * l, XEvent * event, Boolean * flg)
{
    Dimension width, height;
    int cx, cy;
    int dx, dy, dx2, dy2;

    if (Global.popped_up) {
        if (event->type == ButtonPress)
	    PopdownMenusGlobal();
        event->type = None;
        return;
    }

    while (XCheckTypedWindowEvent(XtDisplay(w), XtWindow(w),
				  MotionNotify, (XEvent *) event));

    XtVaGetValues(w, XtNwidth, &width, XtNheight, &height, NULL);

    cx = width / 2;
    cy = height / 2;

    if (event->type == ButtonPress) {
	dx = cx - event->xbutton.x;
	dy = cy - event->xbutton.y;
    } else {
	dx = cx - event->xmotion.x;
	dy = cy - event->xmotion.y;
    }
    dx2 = dx * dx;
    dy2 = dy * dy;


    if (dx == 0 && dy == 0)
	l->h = 0;
    else if ((l->h = atan2((double) dx, (double) dy)) < 0.0)
	l->h += 2 * M_PI;
    l->s = sqrt((double) (dx2 + dy2)) / RADIUS;
    l->h *= 360.0 / (2 * M_PI);

    if (l->s > 1.0) {
	l->s = 1.0;
	dx = sin(l->h * M_PI / 180.0) * RADIUS;
	dy = cos(l->h * M_PI / 180.0) * RADIUS;
    }
    drawCursor(l, False);
    l->dx = dx;
    l->dy = dy;
    drawCursor(l, True);

    l->isRGB = False;
    RECOMPUTE(l);
    update(l, True, True, False);
}

static void 
scrollCB(Widget w, LocalInfo * l, XtPointer position)
{
    float *percent = NULL;

    
    l->isRGB = True;
    if (l->redBar == w) percent = &l->r;
    else if (l->greenBar == w) percent = &l->g;
    else if (l->blueBar == w) percent = &l->b;
    else if (l->valueBar == w) {
	percent = &l->v;
	l->isRGB = False;
    }
    if (!percent) return;
    if ((long)position>0) 
        *percent += 1.0/256.0;
    else
        *percent -= 1.0/256.0;

    if (*percent<0) *percent = 0;
    /* This should avoid values being rounded below 255, as well 
       as above 256... */
    if (*percent>0.99999) *percent = 1.00001;

    RECOMPUTE(l);
    update(l, True, True, l->isRGB);
}

static void 
barCB(Widget w, LocalInfo * l, float *percent)
{
    l->isRGB = True;

    /* This should avoid values being rounded below 255, as well 
       as above 256... */
    if (*percent >= 0.99999) *percent = 1.00001;
    if (l->redBar == w)
	l->r = *percent;
    else if (l->greenBar == w)
	l->g = *percent;
    else if (l->blueBar == w)
	l->b = *percent;
    else if (l->valueBar == w) {
	l->v = *percent;
	l->isRGB = False;
    }
    RECOMPUTE(l);
    update(l, True, True, l->isRGB);
}

static void 
textExitCB(Widget w, LocalInfo * l, XtPointer junk)
{
    String val;
    int v;


    XtVaGetValues(w, XtNstring, &val, NULL);
    if (w != l->valueText) {
	v = atoi(val);
	if (v > 255) {
	    v = 255;
	    XtVaSetValues(w, XtNstring, "255", NULL);
	}
	if (v < 0) {
	    v = 0;
	    XtVaSetValues(w, XtNstring, "0", NULL);
	}
	if (w == l->redText) {
	    if ((int) (l->r * 255.0) == v)
		return;
	    l->r = v / 255.0;
	    l->isRGB = True;
	    RECOMPUTE(l);
	}
	if (w == l->greenText) {
	    if ((int) (l->g * 255.0) == v)
		return;
	    l->g = v / 255.0;
	    l->isRGB = True;
	    RECOMPUTE(l);
	}
	if (w == l->blueText) {
	    if ((int) (l->b * 255.0) == v)
		return;
	    l->b = v / 255.0;
	    l->isRGB = True;
	    RECOMPUTE(l);
	}
    } else {
	float f = atof(val);

	/*
	**  Same to 3 decimal places?
	 */
	if ((int) (f * 1000) == (int) (l->v * 1000))
	    return;

	l->v = f;
	l->isRGB = False;
	RECOMPUTE(l);
    }

    update(l, True, True, l->isRGB);
}

static void 
textAction(Widget w, XEvent * event, String * prms, Cardinal * nprms)
{
    Widget cpick;
    LocalInfo *l = head;

    cpick = w;
    while (cpick != None && strcmp(XtName(cpick), "colorPicker") != 0)
	cpick = XtParent(cpick);

    if (cpick == None)
	return;

    for (; l != NULL && l->form != cpick; l = l->next);

    textExitCB(w, l, NULL);
}


static void 
grabCB(Widget w, LocalInfo * l, XtPointer junk)
{
    XColor *xcol = DoGrabColor(w);

    l->isRGB = True;
    l->r = (float) ((xcol->red >> 8) & 0xff) / (float) 0xff;
    l->g = (float) ((xcol->green >> 8) & 0xff) / (float) 0xff;
    l->b = (float) ((xcol->blue >> 8) & 0xff) / (float) 0xff;

    RECOMPUTE(l);
    update(l, True, True, True);
}

/*
**  Create a nice little color wheel that we can pick colors from.
**  Create it as an Image so that we can compress it down to the
**  supported number of colors, after we've allocated "perfect" ones.
**
 */
static void 
drawInPixmap(Widget w, Palette * map, Colormap cmap,
	     Pixel bg, Pixmap pix, GC gc)
{
    XImage *xim = XGetImage(XtDisplay(w), pix, 0, 0,
			    PIXMAP_WIDTH, PIXMAP_HEIGHT,
			    AllPlanes, ZPixmap);
    int offX = PIXMAP_WIDTH / 2 - RADIUS;
    int offY = PIXMAP_HEIGHT / 2 - RADIUS;
    float r, g, b;
    float h, s;
    int x, y;
    static Image *image = NULL;
    unsigned char *ip;
    int i;
    XColor xcol[64];
    Pixel pval[XtNumber(xcol)];
    int backgroundIndex = 0;
    Boolean isTrue = (map != NULL && !map->isMapped);

    StateSetBusyWatch(True);

    if (image == NULL || isTrue) {
	image = ImageNew(2 * RADIUS, 2 * RADIUS);
	ip = image->data;

	for (y = 0; y < 2 * RADIUS; y++)
	    for (x = 0; x < 2 * RADIUS; x++) {
		int dx, dy, dx2, dy2;

		dx = RADIUS - x;
		dx2 = dx * dx;
		dy = RADIUS - y;
		dy2 = dy * dy;

		if (dx2 + dy2 > RADIUS * RADIUS) {
		    *ip++ = 0;
		    *ip++ = 0;
		    *ip++ = 0;
		    continue;
		}
		if (dx == 0 && dy == 0)
		    h = 0;
		else if ((h = atan2((double) dx, (double) dy)) < 0.0)
		    h += 2 * M_PI;
		s = sqrt((double) (dx2 + dy2)) / RADIUS;

		h *= 360.0 / (2 * M_PI);

		hsvTOrgb(h, s, 1.0, &r, &g, &b);

		*ip++ = r * 255;
		*ip++ = g * 255;
		*ip++ = b * 255;
		if (isTrue) {
		    XColor c;
		    c.red = r * 0xffff;
		    c.green = g * 0xffff;
		    c.blue = b * 0xffff;
		    XPutPixel(xim, offX + x, offY + y,
			      PaletteAlloc(map, &c));
		}
	    }
	if (isTrue) {
	    ImageDelete(image);
	    image = NULL;
	} else
	    image = ImageCompress(image, XtNumber(xcol), 0);
    }
    if (!isTrue) {
	for (i = 0; i < image->cmapSize; i++) {
	    xcol[i].flags = DoRed | DoGreen | DoBlue;
	    xcol[i].red = image->cmapData[i * 3 + 0] * 256;
	    xcol[i].green = image->cmapData[i * 3 + 1] * 256;
	    xcol[i].blue = image->cmapData[i * 3 + 2] * 256;
	    if (xcol[i].green == 0 && xcol[i].blue == 0 && xcol[i].red == 0) {
		backgroundIndex = i;
		continue;
	    }
	    if (map == NULL) {
		XAllocColor(XtDisplay(w), cmap, &xcol[i]);
		pval[i] = xcol[i].pixel;
	    }
	}

	if (map != NULL)
	    PaletteAllocN(map, xcol, image->cmapSize, pval);

	for (ip = image->data, y = 0; y < 2 * RADIUS; y++)
	    for (x = 0; x < 2 * RADIUS; x++, ip++)
		if (*ip == backgroundIndex)
		    XPutPixel(xim, offX + x, offY + y, bg);
		else
		    XPutPixel(xim, offX + x, offY + y, pval[*ip]);
    }
    XPutImage(XtDisplay(w), pix, gc, xim, 0, 0, 0, 0,
	      PIXMAP_WIDTH, PIXMAP_HEIGHT);
    XDestroyImage(xim);

    StateSetBusyWatch(False);
}

static Widget
createBarText(Widget parent, Widget above, String msg,
	      Widget * bar, Widget * text)
{
    static String textTranslations =
    "#override\n\
<Key>Return: color-text-ok()\n\
<Key>Linefeed: color-text-ok()\n\
Ctrl<Key>M: color-text-ok()\n\
Ctrl<Key>J: color-text-ok()\n";
    static XtTranslations trans = None;
    Widget subform, label;

    if (trans == None) {
	static XtActionsRec act =
	{"color-text-ok", (XtActionProc) textAction};

	XtAppAddActions(XtWidgetToApplicationContext(parent), &act, 1);

	trans = XtParseTranslationTable(textTranslations);
    }
    subform = XtVaCreateManagedWidget("form", formWidgetClass, parent,
				      XtNborderWidth, 0,
				      XtNfromVert, above,
				      NULL);
    label = XtVaCreateManagedWidget("valueLabel", labelWidgetClass, subform,
				    XtNlabel, msg,
				    XtNborderWidth, 0,
				    XtNright, XtChainLeft,
				    XtNleft, XtChainLeft,
				    NULL);
    *bar = XtVaCreateManagedWidget("valueBar", scrollbarWidgetClass, subform,
				   XtNorientation, XtorientHorizontal,
				   XtNwidth, 200,
				   XtNfromHoriz, label,
				   XtNleft, XtChainLeft,
				   NULL);
    *text = XtVaCreateManagedWidget("valueText", asciiTextWidgetClass, subform,
				    XtNfromHoriz, *bar,
				    XtNeditType, XawtextEdit,
				    XtNwrap, XawtextWrapNever,
				    XtNresize, XawtextResizeWidth,
				    XtNtranslations, trans,
				    XtNwidth, 50,
				    XtNlength, 5,
				    NULL);
    XtAddEventHandler(*text, 
                      ButtonPressMask|ButtonReleaseMask, False,
                      (XtEventHandler) clickFocusCallback, NULL);
    return subform;
}

static void 
freePixel(Widget w, LocalInfo * l)
{
    XFreeColors(XtDisplay(w), l->cmap, &l->pixel, 1, 0);
}

static LocalInfo *
colorPicker(Widget parent, Colormap cmap, Pixel * pixval)
{
    Widget top = GetToplevel(parent);
    Widget form, picker, subform;
    Widget button;
    LocalInfo *l = (LocalInfo *) XtNew(LocalInfo);

    if (cmap == None)
	cmap = DefaultColormapOfScreen(XtScreen(parent));

    l->initing = True;
    l->inCallback = False;
    l->func = NULL;

    l->cmap = cmap;
    l->xgc = 0;

    form = XtVaCreateManagedWidget("colorPicker", formWidgetClass, parent, NULL);
    /*
    **	The HS space image
     */
    picker = XtVaCreateManagedWidget("palette", coreWidgetClass, form,
				     XtNwidth, PIXMAP_WIDTH,
				     XtNheight, PIXMAP_WIDTH,
				     NULL);
    l->picker = picker;

    XtAddEventHandler(picker, ExposureMask, False,
		      (XtEventHandler) expose, (XtPointer) l);
    XtAddEventHandler(picker, ButtonPressMask, False,
		      (XtEventHandler) motion, (XtPointer) l);
    XtAddEventHandler(picker, ButtonMotionMask, False,
		      (XtEventHandler) motion, (XtPointer) l);

    /*
    **	The color "view"
     */
    l->pixel = None;
    if (pixval == NULL) {
	if (XAllocColorCells(XtDisplay(top), cmap, False, NULL, 0, &l->pixel, 1)) {
	    XColor col;
	    col.pixel = l->pixel;
	    col.flags = DoRed | DoGreen | DoBlue;
	    col.red = 0xffff;
	    col.green = 0xffff;
	    col.blue = 0xffff;
	    XStoreColor(XtDisplay(top), cmap, &col);

	    XtAddCallback(form, XtNdestroyCallback,
			  (XtCallbackProc) freePixel, (XtPointer) l);
	}
    } else {
	l->pixel = *pixval;
    }
    l->view = XtVaCreateManagedWidget("view", coreWidgetClass, form,
				      XtNwidth, 40,
				      XtNheight, 40,
				      XtNfromHoriz, picker,
				      XtNbackground, l->pixel,
				      NULL);

    button = XtVaCreateManagedWidget("match", commandWidgetClass, form,
				     XtNfromVert, l->view,
				     XtNfromHoriz, picker,
				     NULL);
    l->select = XtVaCreateWidget("select", commandWidgetClass, form,
				     XtNfromVert, button,
				     XtNfromHoriz, picker,
				     NULL);
    l->asbg = XtVaCreateWidget("asbg", commandWidgetClass, form,
				     XtNfromVert, l->select,
				     XtNfromHoriz, picker,
				     NULL);
    XtAddCallback(button, XtNcallback, (XtCallbackProc)grabCB, (XtPointer) l);

    /*
    **	Now create the scroll bars
     */
    subform = createBarText(form, picker, msgText[VALUE], &l->valueBar, &l->valueText);
    subform = createBarText(form, subform, msgText[RED_COLOR], &l->redBar, &l->redText);
    subform = createBarText(form, subform, msgText[GREEN_COLOR], &l->greenBar, &l->greenText);
    subform = createBarText(form, subform, msgText[BLUE_COLOR], &l->blueBar, &l->blueText);

    XtAddCallback(l->valueBar, XtNjumpProc,
		  (XtCallbackProc) barCB, (XtPointer) l);
    XtAddCallback(l->redBar, XtNjumpProc,
		  (XtCallbackProc) barCB, (XtPointer) l);
    XtAddCallback(l->greenBar, XtNjumpProc,
		  (XtCallbackProc) barCB, (XtPointer) l);
    XtAddCallback(l->blueBar, XtNjumpProc,
		  (XtCallbackProc) barCB, (XtPointer) l);

    XtAddCallback(l->valueBar, XtNscrollProc,
		  (XtCallbackProc) scrollCB, (XtPointer) l);
    XtAddCallback(l->redBar, XtNscrollProc,
		  (XtCallbackProc) scrollCB, (XtPointer) l);
    XtAddCallback(l->greenBar, XtNscrollProc,
		  (XtCallbackProc) scrollCB, (XtPointer) l);
    XtAddCallback(l->blueBar, XtNscrollProc,
		  (XtCallbackProc) scrollCB, (XtPointer) l);

    l->isRGB = True;
    l->r = l->g = l->b = 1.0;
    RECOMPUTE(l);
    update(l, True, True, True);

    l->form = form;
    l->next = head;
    head = l;

    return l;
}

Widget
ColorPickerPalette(Widget parent, Palette * map, Pixel * pixval)
{
    LocalInfo *l = colorPicker(parent, map->cmap, pixval);
    XGCValues gcvalues;
    Cardinal depth;

    XtVaGetValues(l->picker, XtNbackground, &gcvalues.foreground,
		  XtNdepth, &depth, NULL);
    l->pixmap = XCreatePixmap(XtDisplay(l->picker),
			      DefaultRootWindow(XtDisplay(l->picker)),
			      PIXMAP_WIDTH, PIXMAP_HEIGHT, depth);
    l->gc = XtGetGC(l->picker, GCForeground, &gcvalues);
    XFillRectangle(XtDisplay(l->picker), l->pixmap, l->gc,
		   0, 0, PIXMAP_WIDTH, PIXMAP_HEIGHT);
    l->map = map;
    l->bg = gcvalues.foreground;
    drawInPixmap(l->picker, map, l->cmap, l->bg, l->pixmap, l->gc);

    l->initing = False;
    return l->form;
}

Widget
ColorPicker(Widget parent, Colormap cmap, Pixel * pixval)
{
    LocalInfo *l = colorPicker(parent, cmap, pixval);
    XGCValues gcvalues;
    Cardinal depth;

    l->map = NULL;

    XtVaGetValues(l->picker, XtNbackground, &gcvalues.foreground,
		  XtNdepth, &depth, NULL);
    l->pixmap = XCreatePixmap(XtDisplay(l->picker),
			      DefaultRootWindow(XtDisplay(l->picker)),
			      PIXMAP_WIDTH, PIXMAP_HEIGHT, depth);
    l->gc = XtGetGC(l->picker, GCForeground, &gcvalues);
    XFillRectangle(XtDisplay(l->picker), l->pixmap, l->gc,
		   0, 0, PIXMAP_WIDTH, PIXMAP_HEIGHT);

    l->bg = gcvalues.foreground;
    drawInPixmap(l->picker, NULL, l->cmap, l->bg,
		 l->pixmap, l->gc);

    l->initing = False;
    return l->form;
}

void 
ColorPickerShell(Widget w)
{
    Widget top = GetToplevel(w);
    Widget shell;
    int nargs = 0;
    Arg args[4];

    shell = XtVisCreatePopupShell("color", transientShellWidgetClass,
				  GetShell(w), args, nargs);

    ColorPicker(shell, DefaultColormapOfScreen(XtScreen(top)), NULL);

    XtPopup(shell, XtGrabNone);
}

void 
ColorPickerSetXColor(Widget w, XColor * xcol)
{
    LocalInfo *l = head;

    for (; l != NULL && l->form != w; l = l->next);

    if (l == NULL || l->inCallback)
	return;

    l->isRGB = True;
    l->r = (float) ((xcol->red >> 8) & 0xff) / (float) 0xff;
    l->g = (float) ((xcol->green >> 8) & 0xff) / (float) 0xff;
    l->b = (float) ((xcol->blue >> 8) & 0xff) / (float) 0xff;

    RECOMPUTE(l);
    update(l, True, True, True);
}

void 
ColorPickerSetPixel(Widget w, Pixel pix)
{
    LocalInfo *l = head;
    XColor xcol;

    for (; l != NULL && l->form != w; l = l->next);

    if (l == NULL || l->inCallback)
	return;

    XtVaSetValues(l->view, XtNbackground, pix, NULL);

    xcol.pixel = pix;
    XQueryColor(XtDisplay(w), l->cmap, &xcol);

    l->isRGB = True;
    l->r = (float) ((xcol.red >> 8) & 0xff) / (float) 0xff;
    l->g = (float) ((xcol.green >> 8) & 0xff) / (float) 0xff;
    l->b = (float) ((xcol.blue >> 8) & 0xff) / (float) 0xff;
    l->pixel = pix;

    RECOMPUTE(l);
    update(l, True, True, True);
}

void 
ColorPickerSetFunction(Widget w, XtCallbackProc func, XtPointer data)
{
    LocalInfo *l = head;

    for (; l != NULL && l->form != w; l = l->next);

    if (l == NULL)
	return;

    l->func = (cbFunc_t) func;
    l->data = data;
}

Widget
ColorPickerGetSelectWidget(Widget w)
{
    LocalInfo *l = head;

    for (; l != NULL && l->form != w; l = l->next);

    if (l == NULL) return NULL;

    XtManageChild(l->select);
    return l->select;
}

Widget
ColorPickerGetAsBgWidget(Widget w)
{
    LocalInfo *l = head;

    for (; l != NULL && l->form != w; l = l->next);

    if (l == NULL) return NULL;

    XtManageChild(l->asbg);
    return l->asbg;
}

Pixel
ColorPickerGetPixel(Widget w)
{
    LocalInfo *l = head;

    for (; l != NULL && l->form != w; l = l->next);

    if (l == NULL)
	return None;
    return l->pixel;
}

XColor *
ColorPickerGetXColor(Widget w)
{
    static XColor xcol;
    LocalInfo *l = head;

    for (; l != NULL && l->form != w; l = l->next);

    if (l == NULL)
	return NULL;
    xcol.pixel = l->pixel;
    xcol.red = l->r * 0xffff;
    xcol.green = l->g * 0xffff;
    xcol.blue = l->b * 0xffff;

    return &xcol;
}

/*
 * Update the colour map for the specified ColorPicker.
 */
void 
ColorPickerUpdateMap(Widget w, Palette * map)
{
    LocalInfo *l = head;


    for (; l != NULL && l->form != w; l = l->next);

    if (l == NULL)
	return;

    l->map = map;
    l->cmap = map->cmap;

    drawInPixmap(l->picker, map, l->cmap, l->bg, l->pixmap, l->gc);
    XClearArea(XtDisplay(w), XtWindow(l->picker), 0, 0, 0, 0, True);
}
