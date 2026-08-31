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

/* $Id: Colormap.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <math.h>
#include <X11/IntrinsicP.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include "ColormapP.h"

static XtResource resources[] =
{
#define offset(field) XtOffset(ColormapWidget, color.field)
    {XtNcallback, XtCCallback, XtRCallback, sizeof(XtPointer),
     offset(callbacks), XtRCallback, (XtPointer) None},
    {XtNcolor, XtCColor, XtRPixel, sizeof(Pixel),
     offset(pixel), XtRImmediate, (XtPointer) None},
    {XtNreadOnly, XtCReadOnly, XtRBoolean, sizeof(Boolean),
     offset(editable), XtRImmediate, (XtPointer) False},
    {XtNcellWidth, XtCCellWidth, XtRInt, sizeof(int),
     offset(cwidth), XtRImmediate, (XtPointer) 5},
    {XtNcellHeight, XtCCellHeight, XtRInt, sizeof(int),
     offset(cheight), XtRImmediate, (XtPointer) 5},
    {XtNthickness, XtCThickness, XtRInt, sizeof(int),
     offset(thickness), XtRImmediate, (XtPointer) 1},
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
     offset(foreground), XtRString, XtDefaultForeground},
#undef offset
};

static void ResizeProc(ColormapWidget);
static void ExposeProc(ColormapWidget, XExposeEvent *);
static void InitializeProc(ColormapWidget, ColormapWidget);
static Boolean SetValuesProc(Widget, Widget, Widget);

static void
drawSelection(ColormapWidget w, int flag)
{
    XGCValues values;
    int x, y;
    GC gc;
    int t = w->color.thickness, t2 = t / 2 + 1;

    values.line_width = w->color.thickness;
    if (flag) {
	values.foreground = w->color.foreground;
	values.background = w->core.background_pixel;
    } else {
	values.foreground = w->core.background_pixel;
	values.background = w->color.foreground;
    }

    gc = XtGetGC((Widget) w, GCForeground | GCBackground | GCLineWidth, &values);

    x = (w->color.pixel % w->color.ncwidth) * w->color.cwidth;
    y = (w->color.pixel / w->color.ncwidth) * w->color.cheight;

    XDrawRectangle(XtDisplay(w), XtWindow(w), gc,
		   x - t2, y - t2, w->color.cwidth, w->color.cheight);
}

static void
ColormapSelect(ColormapWidget w, XEvent * e,
	       String * str, Cardinal * n)
{
    int pixel;
    XColor col;

    if ((e->xbutton.x / w->color.cwidth) > w->color.ncwidth)
	return;
    if ((e->xbutton.y / w->color.cheight) > w->color.ncheight)
	return;


    pixel = (e->xbutton.y / w->color.cheight) * w->color.ncwidth;
    pixel += e->xbutton.x / w->color.cwidth;

    drawSelection(w, FALSE);
    w->color.pixel = pixel;
    drawSelection(w, TRUE);

    col.pixel = pixel;
    col.flags = DoRed | DoGreen | DoBlue;
    XQueryColor(XtDisplay(w), w->core.colormap, &col);

    XtCallCallbackList((Widget) w, w->color.callbacks, (XtPointer) & col);
}

static XtActionsRec actions[] =
{
  /* {name, procedure}, */
    {"Set", (XtActionProc) ColormapSelect},
};

static char translations[] =
"<BtnDown>:	Set()	\n\
";

ColormapClassRec colormapClassRec =
{
    {				/* core fields */
    /* superclass               */ (WidgetClass) & widgetClassRec,
    /* class_name               */ "Colormap",
    /* widget_size              */ sizeof(ColormapRec),
    /* class_initialize         */ NULL,
    /* class_part_initialize    */ NULL,
    /* class_inited             */ FALSE,
    /* initialize               */ (XtInitProc) InitializeProc,
    /* initialize_hook          */ NULL,
    /* realize                  */ XtInheritRealize,
    /* actions                  */ actions,
    /* num_actions              */ XtNumber(actions),
    /* resources                */ resources,
    /* num_resources            */ XtNumber(resources),
    /* xrm_class                */ NULLQUARK,
    /* compress_motion          */ TRUE,
    /* compress_exposure        */ TRUE,
    /* compress_enterleave      */ TRUE,
    /* visible_interest         */ FALSE,
    /* destroy                  */ NULL,
    /* resize                   */ (XtWidgetProc) ResizeProc,
    /* expose                   */ (XtExposeProc) ExposeProc,
    /* set_values               */ (XtSetValuesFunc) SetValuesProc,
    /* set_values_hook          */ NULL,
    /* set_values_almost        */ XtInheritSetValuesAlmost,
    /* get_values_hook          */ NULL,
    /* accept_focus             */ NULL,
    /* version                  */ XtVersion,
    /* callback_private         */ NULL,
    /* tm_table                 */ translations,
    /* query_geometry           */ XtInheritQueryGeometry,
    /* display_accelerator      */ XtInheritDisplayAccelerator,
    /* extension                */ NULL
    },
    {				/* colormap fields */
    /* empty                    */ 0
    }
};

WidgetClass colormapWidgetClass = (WidgetClass) & colormapClassRec;

static void
InitializeProc(ColormapWidget w, ColormapWidget new)
{
    Widget shell = XtParent(w);
    Visual *visual = NULL;

    while (!XtIsShell(shell))
	shell = XtParent(shell);

    XtVaGetValues(shell, XtNvisual, &visual, NULL);
    if (visual == NULL)
	visual = DefaultVisualOfScreen(XtScreen(w));

    new->color.ncel = visual->map_entries;
    new->color.ncwidth = (int) sqrt((double) new->color.ncel);
    new->color.ncheight = (new->color.ncel + new->color.ncwidth - 1) /
	new->color.ncwidth;
    if (new->core.width == 0)
	new->core.width = new->color.cwidth * new->color.ncwidth;
    else
	new->color.cwidth = (int) new->core.width / (int) new->color.ncwidth;
    if (new->core.height == 0)
	new->core.height = new->color.cheight * new->color.ncheight;
    else
	new->color.cheight = new->core.height / new->color.ncheight;
}

static void
ResizeProc(ColormapWidget w)
{
    w->color.cwidth = w->core.width / w->color.ncwidth;
    w->color.cheight = w->core.height / w->color.ncheight;
}

static void
ExposeProc(ColormapWidget w, XExposeEvent * event)
{
    Display *dpy = XtDisplay(w);
    Window win = XtWindow(w);
    GC gc = XCreateGC(dpy, win, 0, NULL);
    int color = 0;
    int x, y, X, Y;
    int nvert = w->color.ncwidth;
    int nhoriz = w->color.ncheight;
    int t = w->color.thickness;

    for (Y = y = 0; y < nhoriz; y++, Y += w->color.cheight) {
	for (X = x = 0; x < nvert; x++, X += w->color.cwidth) {
	    XSetForeground(dpy, gc, color++);
	    XFillRectangle(dpy, win, gc, X, Y,
		      w->color.cwidth - 1 - t, w->color.cheight - 1 - t);
	}
    }

    XFreeGC(dpy, gc);
    drawSelection(w, TRUE);
}

static Boolean
SetValuesProc(Widget curArg, Widget request, Widget newArg)
{
    ColormapWidget cur = (ColormapWidget) curArg;
    ColormapWidget new = (ColormapWidget) newArg;

    if (cur->color.pixel != new->color.pixel) {
	drawSelection(cur, FALSE);
	drawSelection(new, TRUE);
    }
    return False;
}


void
CwGetColor(Widget cw, XColor * col)
{
    ColormapWidget w = (ColormapWidget) cw;

    col->pixel = w->color.pixel;
    col->flags = DoRed | DoGreen | DoBlue;
    XQueryColor(XtDisplay(w), w->core.colormap, col);
}

void
CwSetColor(Widget cw, XColor * col)
{
    ColormapWidget w = (ColormapWidget) cw;

    col->pixel = w->color.pixel;
    XStoreColor(XtDisplay(w), w->core.colormap, col);
}
