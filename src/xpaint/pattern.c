/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)            | */
/* | Copyright 1995, 1996 Torsten Martinsen (bullestock@dk-online.dk)  | */
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

/* $Id: pattern.c,v 1.18 2005/03/20 20:15:32 demailly Exp $ */

#include <stdio.h>

#include <X11/IntrinsicP.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/CoreP.h>
#include <X11/cursorfont.h>

#include "xaw_incdir/Box.h"
#include "xaw_incdir/Form.h"
#include "xaw_incdir/Scrollbar.h"
#include "xaw_incdir/Viewport.h"
#include "xaw_incdir/Command.h"
#include "xaw_incdir/Paned.h"
#include "xaw_incdir/ToggleP.h"

#include "Colormap.h"
#include "Paint.h"
#include "palette.h"
#include "xpaint.h"
#include "menu.h"
#include "messages.h"
#include "image.h"
#include "misc.h"
#include "region.h"
#include "text.h"
#include "graphic.h"
#include "operation.h"
#include "rc.h"
#include "color.h"
#include "protocol.h"
#include "bitmaps/xbm/arrows.xbm"

#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#endif

#ifdef XAW3D
#define BSP 4
#define BWD 0
#define COLSP 4
#else
#ifdef XAW95
#define BSP 6
#define BWD 0
#define COLSP 4
#else
#define BSP 6
#define BWD 1
#define COLSP 4
#endif
#endif

extern int
WriteAsciiPNMfd(FILE * fd, Image * image);

extern Widget
createBrushDialog(Widget w);

static PaintMenuItem paletteMenu[] =
{
#define SAVE_CONFIG 0
    MI_SIMPLE("saveconfig"),
#define LOAD_CONFIG 1
    MI_SIMPLE("loadconfig"),
    MI_SEPARATOR(),
#define MARK_PATTERN 3
    MI_SIMPLE("markselected"),
#define UNMARK_PATTERN 4
    MI_SIMPLE("unmark"),
#define LOAD_MARKED_PATTERN 5
    MI_SIMPLE("loadmarked"),
#define DELETE_PATTERN 6
    MI_SIMPLE("delete"),
};
static PaintMenuItem canvasMenu[] =
{
    MI_SIMPLE("read"),
    MI_SIMPLE("save"),
    MI_SIMPLE("close"),
};
static PaintMenuItem editMenu[] =
{
    MI_SIMPLE("undo"),
    MI_SEPARATOR(),
    MI_SIMPLE("cut"),
    MI_SIMPLE("copy"),
    MI_SIMPLE("paste"),
    MI_SIMPLE("clear"),
    MI_SEPARATOR(),
    MI_SIMPLE("dup"),
    MI_SIMPLE("all"),
};

static PaintMenuItem lineMenu[] =
{
    MI_FLAGCB("1", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("2", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("4", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("6", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("8", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_SEPARATOR(),
#define LW_GENERAL 5
    MI_FLAGCB("linestyle", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
};

static PaintMenuItem sizeMenu[] =
{
#define SZ_N1	0
    MI_FLAG("16x16", MF_CHECK | MF_GROUP1),
#define SZ_N2	1
    MI_FLAG("24x24", MF_CHECK | MF_GROUP1),
#define SZ_N3	2
    MI_FLAG("32x32", MF_CHECK | MF_GROUP1),
#define SZ_N4	3
    MI_FLAG("48x48", MF_CHECK | MF_GROUP1),
#define SZ_N5	4
    MI_FLAG("64x64", MF_CHECK | MF_GROUP1),
    MI_SEPARATOR(),
#define SZ_N6	6
    MI_FLAG("other", MF_CHECK | MF_GROUP1),
};
static PaintMenuItem imageMenu[] =
{
#define IM_GRID 0
    MI_FLAG("grid", MF_CHECK),
#define IM_ZOOM 1
    MI_SIMPLE("zoom"),
#define IM_BGRD 2
    MI_SIMPLE("background"),
};
static PaintMenuItem helpMenu[] =
{
    MI_SIMPLECB("help", HelpDialog, "patternSelector"),
};
static PaintMenuBar menuBar[] =
{
    {None, "palette", XtNumber(paletteMenu), paletteMenu},
    {None, "canvas", XtNumber(canvasMenu), canvasMenu},
    {None, "edit", XtNumber(editMenu), editMenu},
    {None, "line", XtNumber(lineMenu), lineMenu},
    {None, "size", XtNumber(sizeMenu), sizeMenu},
    {None, "image", XtNumber(imageMenu), imageMenu},
    {None, "help", XtNumber(helpMenu), helpMenu},
};

#define	RED	1
#define	GREEN	2
#define	BLUE	3

typedef struct {
    int click, mask, added, nbrushes;
    void *iconlist;
    GC gc;
    Widget form, form1, form2;
    Widget bar, paint, box;
    Widget r_arrow, g_arrow, view1, view2, active, icon, fat;
    Widget cpick, edit;
    Widget curCheck;
    Widget sizeChecks[XtNumber(sizeMenu) - 1];
    Widget vport, vport2, patternbox, patternlist;
    Pixel marked;
    Pixmap iconpix;
    Palette *map;
    Image **brushes;
} LocalInfo;

/*
 *  Pattern creation and changing, utilities.
 */
#define PAT_MIN_WH	24

static int pattern_min_wh = 0;

typedef struct {
    Widget widget;
    int width, height;
    Pixmap pixmap, shownPixmap;
    Pixel pixel;
    LocalInfo *li;
} PatternInfo;

/*
 *  Forward references
 */
static void makePaletteArea(LocalInfo *, RCInfo *);
static void markPatternCB(Widget, Widget);
static void unmarkPatternCB(Widget, Widget);
static void loadPatternCB(Widget, Widget);
static void editPatternCB(Widget, Widget);
static void deletePatternCB(Widget, Widget);

static PaintMenuItem patternMenu[] =
{
    MI_SEPARATOR(),
    MI_SIMPLECB("mark", markPatternCB, NULL),
    MI_SIMPLECB("unmark", unmarkPatternCB, NULL),
    MI_SIMPLECB("loadpattern", loadPatternCB, NULL),
    MI_SIMPLECB("edit", editPatternCB, NULL),
    MI_SIMPLECB("remove", deletePatternCB, NULL),
    MI_SEPARATOR(),
    MI_SIMPLECB("help", HelpDialog, "patternSelector.bottomPatSel"),
};

static void
setPatternColorsIcon(LocalInfo * l)
{
    static int *x0 = NULL, *x1;
    Pixel bg, fg, lfg;
    Pixmap pix, lpix;
    Display *dpy; 
    int v_fr, v_lfr, v_lw, x, y, width, height, depth;

    if (!l) return;
    dpy = XtDisplay(l->icon);
    if (l->paint)
	XtVaGetValues(l->paint, XtNbackground, &bg, NULL);
    else
	XtVaGetValues(l->fat, XtNbackground, &bg, NULL);
    XtVaGetValues(l->fat, XtNfillRule, &v_fr, NULL);
    XtVaGetValues(l->fat, XtNforeground, &fg, NULL);
    XtVaGetValues(l->fat, XtNpattern, &pix, NULL);
    XtVaGetValues(l->fat, XtNlineFillRule, &v_lfr, NULL);
    XtVaGetValues(l->fat, XtNlineForeground, &lfg, NULL);
    XtVaGetValues(l->fat, XtNlinePattern, &lpix, NULL);
    XtVaGetValues(l->fat, XtNlineWidth, &v_lw, NULL);
    if (v_lw>10) v_lw = 10;

    if (!x0) {
        x0 = (int *) xmalloc(ICONHEIGHT*sizeof(int));
        x1 = (int *) xmalloc(ICONHEIGHT*sizeof(int));
	for (y=0; y<ICONHEIGHT; y++) {
	    if (y<4 || y>ICONHEIGHT-5) {
                x0[y] = ICONWIDTH;
	    } else
                x0[y] = 11+abs(y-20)/3;
                x1[y] = ICONWIDTH-3-abs(y-20)/3;
	}
    }
    for (y=0; y<ICONHEIGHT; y++) {
        XSetForeground(dpy, l->gc, bg);
	if (y<4 || y>ICONHEIGHT-5) {
            for (x=0; x<x0[y]; x++)
                XDrawPoint(dpy, l->iconpix, l->gc, x, y);
	    continue;
	}
        for (x=0; x<=2; x++)
                XDrawPoint(dpy, l->iconpix, l->gc, x, y);
        for (x=10; x<x0[y]; x++)
                XDrawPoint(dpy, l->iconpix, l->gc, x, y);

        for (x=x1[y]+1; x<ICONWIDTH; x++)
            XDrawPoint(dpy, l->iconpix, l->gc, x, y);

	if (v_fr == FillSolid) {
	    XSetForeground(dpy, l->gc, fg);
            for (x=3; x<=9; x++)
                XDrawPoint(dpy, l->iconpix, l->gc, x, y);
	} else {
            GetPixmapWHD(dpy, pix, &width, &height, &depth);
            for (x=3; x<=9; x++)
	        XCopyArea(dpy, pix, l->iconpix, l->gc,
		          x%width, y%height, 1, 1, x, y);
	}

	if (y<=4+v_lw || y>=ICONHEIGHT-v_lw-5) {
	    if (v_fr == FillSolid) {
	        XSetForeground(dpy, l->gc, fg);
                for (x=x0[y]; x<=x1[y]; x++)
                    XDrawPoint(dpy, l->iconpix, l->gc, x, y);
	    } else {
                GetPixmapWHD(dpy, pix, &width, &height, &depth);
                for (x=x0[y]; x<=x1[y]; x++)
	            XCopyArea(dpy, pix, l->iconpix, l->gc,
		              x%width, y%height, 1, 1, x, y);
	    }
            continue;
	}
	if (v_fr == FillSolid) {
            XSetForeground(dpy, l->gc, fg);
            for (x=x0[y]; x<=x0[y]+v_lw; x++)
                XDrawPoint(dpy, l->iconpix, l->gc, x, y);
            for (x=x1[y]-v_lw; x<=x1[y]; x++)
                XDrawPoint(dpy, l->iconpix, l->gc, x, y);
	} else {
            GetPixmapWHD(dpy, pix, &width, &height, &depth);
            for (x=x0[y]; x<=x0[y]+v_lw; x++)
	        XCopyArea(dpy, pix, l->iconpix, l->gc,
		          x%width, y%height, 1, 1, x, y);
            for (x=x1[y]-v_lw; x<=x1[y]; x++)
	        XCopyArea(dpy, pix, l->iconpix, l->gc,
		          x%width, y%height, 1, 1, x, y);
	}
	if (v_lfr == FillSolid) {
            XSetForeground(dpy, l->gc, lfg);
            for (x=x0[y]+v_lw+1; x<=x1[y]-v_lw-1; x++)
                XDrawPoint(dpy, l->iconpix, l->gc, x, y);
	} else {
            GetPixmapWHD(dpy, lpix, &width, &height, &depth);
            for (x=x0[y]+v_lw+1; x<=x1[y]-v_lw-1; x++)
	        XCopyArea(dpy, lpix, l->iconpix, l->gc,
		          x%width, y%height, 1, 1, x, y);
	}
    }

    /* Trick to force refresh of background Pixmap,
       although pointer value l->iconpix didn't change ... */
    XtVaSetValues(l->icon, XtNbackgroundPixmap, XtUnspecifiedPixmap, NULL);  
    XtVaSetValues(l->icon, XtNbackgroundPixmap, l->iconpix, NULL);  
}

static void
initViews(LocalInfo * info)
{
    Display *dpy;
    Pixel p;
    Pixmap pix, npix, opix = None, olpix = None;
    int rule=-1, orule=-1, olrule=-1;

    info->mask = 0;
    info->active = info->view1;
    info->edit = None;

    XtVaGetValues(info->fat, XtNfillRule, &orule, XtNpattern, &opix, 
		  XtNlineFillRule, &olrule, XtNlinePattern, &olpix, NULL);
 
    if (info->paint) {
        dpy = XtDisplay(info->paint);
        XtVaGetValues(info->paint, XtNfillRule, &rule, NULL);
        if (rule==FillSolid) {
            XtVaGetValues(info->paint, XtNforeground, &p, NULL);
	    XtVaSetValues(info->view1, XtNbackgroundPixmap,XtUnspecifiedPixmap,
			               XtNbackground, p, NULL);
            XtVaSetValues(info->fat, XtNfillRule, FillSolid,
                                     XtNforeground, p, NULL);
        } else {
            XtVaGetValues(info->paint, XtNpattern, &pix, NULL);
	    npix = dupPixmap(dpy, pix);
            XtVaSetValues(info->view1, XtNbackgroundPixmap, npix, NULL);
            XtVaSetValues(info->fat, XtNfillRule, FillTiled,
                                     XtNpattern, npix, NULL);
	    info->mask |= 1;
        }
        XtVaGetValues(info->paint, XtNlineFillRule, &rule, NULL);
        if (rule==FillSolid) {
            XtVaGetValues(info->paint, XtNlineForeground, &p, NULL);
	    XtVaSetValues(info->view2, XtNbackgroundPixmap,XtUnspecifiedPixmap,
                                       XtNbackground, p, NULL);
            XtVaSetValues(info->fat, XtNlineFillRule, FillSolid,
                                     XtNlineForeground, p, NULL);
        } else {
            XtVaGetValues(info->paint, XtNlinePattern, &pix, NULL);
	    npix = dupPixmap(dpy, pix);
            XtVaSetValues(info->view2, XtNbackgroundPixmap, npix, NULL);
            XtVaSetValues(info->fat, XtNlineFillRule, FillTiled,
                                     XtNlinePattern, npix, NULL);
	    info->mask |= 2;
	}
    } else {
        info->mask = 0;
	p = BlackPixelOfScreen(XtScreen(info->view1));
        XtVaSetValues(info->view1, XtNbackgroundPixmap, XtUnspecifiedPixmap,
                                   XtNbackground, p, NULL);
        XtVaSetValues(info->view2, XtNbackgroundPixmap, XtUnspecifiedPixmap,
		                   XtNbackground, p, NULL);
        XtVaSetValues(info->fat, XtNfillRule, FillSolid, 
		                 XtNforeground, p, 
                                 XtNlineFillRule, FillSolid, 
		                 XtNlineForeground, p, NULL);
    }

    dpy = XtDisplay(info->fat);
    if (orule==FillTiled && opix) XFreePixmap(dpy, opix);
    if (olrule==FillTiled && olpix) XFreePixmap(dpy, olpix);

    XtVaGetValues(info->fat, XtNfillRule, &rule, 
		             XtNforeground, &p, NULL);
    if (rule == FillSolid && p != BlackPixelOfScreen(XtScreen(info->view1)))
        ColorPickerSetPixel(info->cpick, p);
    setPatternColorsIcon(info);
    setCanvasColorsIcon(info->paint, NULL);
}

/*
 *  Save configuration 
 */
static void
saveConfigOkCallback(Widget il, LocalInfo * info, char *file)
{
    WidgetList children;
    int nchildren;
    int i, w, h;
    FILE *fd;
    Colormap cmap;

    XtVaGetValues(info->patternbox, XtNchildren, &children,
		  XtNnumChildren, &nchildren, NULL);
    XtVaGetValues(GetShell(info->patternbox), XtNcolormap, &cmap, NULL);

    if (nchildren == 0) {
	Notice(info->patternbox, msgText[NO_INFORMATION_TO_SAVE]);
	return;
    }
    StateSetBusyWatch(True);

    if ((fd = fopen(file, "w")) == NULL) {
	Notice(info->patternbox, 
               msgText[UNABLE_TO_OPEN_FILE_FOR_WRITING], file);
	return;
    }
    fprintf(fd, "reset\n\n");

    w = Global.default_width;
    h = Global.default_height;
    fprintf(fd, "DefaultSize %dx%d\n\n", w, h);

    for (i = 0; i < nchildren; i++) {
	PatternInfo *pi;

	pi = NULL;
	XtVaGetValues(children[i], XtNradioData, &pi, NULL);

	if (pi == NULL)
	    continue;

	if (pi->pixmap == None) {
	    XColor col;
	    int r, g, b;

	    col.pixel = pi->pixel;
	    col.flags = DoRed | DoGreen | DoBlue;
	    XQueryColor(XtDisplay(info->fat), cmap, &col);
	    r = (col.red >> 8) & 0xff;
	    g = (col.green >> 8) & 0xff;
	    b = (col.blue >> 8) & 0xff;
	    fprintf(fd, "solid #%02x%02x%02x\n", r, g, b);
	} else {
	    Image *image;

	    fprintf(fd, "pattern BeginData\n");
	    image = PixmapToImage(info->fat, pi->pixmap, cmap);
	    WriteAsciiPNMfd(fd, image);
	    ImageDelete(image);
	    fprintf(fd, "\nEndData\n");
	}
    }

    for (i = 0; i < info->nbrushes; i++) {
	fprintf(fd, "brush BeginData\n");
	WriteAsciiPNMfd(fd, info->brushes[i]);
	fprintf(fd, "\nEndData\n");
    }

    for (i = 0; i < Global.nbrushes; i++) {
	fprintf(fd, "brush BeginData\n");
	WriteAsciiPNMfd(fd, (Image *)(Global.brushes[i]));
	fprintf(fd, "\nEndData\n");
    }

    fclose(fd);

    StateSetBusyWatch(False);
}

void
saveConfigCallback(Widget w, LocalInfo * info, XtPointer junk)
{
    GetFileName(Global.patternshell, BROWSER_SIMPLESAVE, ".XPaintrc",
		(XtCallbackProc) saveConfigOkCallback, (XtPointer) info);
}

static void
loadConfigOkCallback(Widget il, LocalInfo * info, char *file)
{
    RCInfo *rcInfo = ReadRC(file);
    if (rcInfo == NULL) {
	Notice(Global.patternshell, msgText[UNABLE_TO_OPEN_FILE], file);
	return;
    }
    XawToggleUnsetCurrent(info->patternlist);
    XtDestroyWidget(info->patternbox);
    info->patternlist = None;
    info->patternbox = XtVaCreateManagedWidget("patternRack",
			            boxWidgetClass, info->vport,
				    XtNforceBars, True,
			            NULL);
    makePaletteArea(info, rcInfo);
    FreeRC(rcInfo);
    initViews(info);
}

void
loadConfigCallback(Widget w, LocalInfo * info, XtPointer junk)
{
    GetFileName(Global.patternshell, BROWSER_SIMPLEREAD, ".XPaintrc",
		(XtCallbackProc) loadConfigOkCallback, (XtPointer) info);
}

static void 
activePixel(LocalInfo * l, Pixel p)
{
    char *w_fr, *w_fg;

    if (!l->active) return;
    XtVaSetValues(l->active, XtNbackgroundPixmap, XtUnspecifiedPixmap, NULL);
    XtVaSetValues(l->active, XtNbackground, p, NULL);

    if (l->active == l->view1) {
	w_fr = XtNfillRule;
	w_fg = XtNforeground;
	l->mask &= 2;
    } else {
	w_fr = XtNlineFillRule;
	w_fg = XtNlineForeground;
	l->mask &= 1;
    }
    if (l->paint)
        XtVaSetValues(l->paint, w_fg, p, w_fr, FillSolid, NULL);
    XtVaSetValues(l->fat, w_fg, p, w_fr, FillSolid, NULL);

    if (l->cpick != None)
	ColorPickerSetPixel(l->cpick, p);
    PaletteSetInvalid(l->map, p);
    setPatternColorsIcon(l);
    setCanvasColorsIcon(l->paint, NULL);
}

static void
patternUpdate(PatternInfo * info)
{
    char *w_fr, *w_fg, *w_pat;
    Display *dpy;
    Pixmap pix=None, npix;
    int rule=-1;

    if (!info->li->active) return;
    if (info->li->active == info->li->view1) {
	w_fr = XtNfillRule;
	w_fg = XtNforeground;
	w_pat = XtNpattern;
	info->li->mask |= 1;
    } else {
	w_fr = XtNlineFillRule;
	w_fg = XtNlineForeground;
	w_pat = XtNlinePattern;
        info->li->mask |= 2;
    }

    if (info->pixmap == None)
	activePixel(info->li, info->pixel);
    else {
        if (info->li->paint) {
	    dpy = XtDisplay(info->li->paint);
            XtVaGetValues(info->li->paint, w_fr, &rule, w_pat, &pix, NULL);
	    XtVaSetValues(info->li->paint, w_fr, FillTiled, 
                          w_pat, dupPixmap(dpy, info->pixmap), NULL);
	    if (rule==FillTiled && pix) XFreePixmap(dpy, pix);
	}
        dpy = XtDisplay(info->li->fat);
        XtVaGetValues(info->li->fat, 
                      w_fr, &rule, w_pat, &pix, NULL);
        npix =  dupPixmap(dpy,info->pixmap);
        XtVaSetValues(info->li->fat, 
                      w_fr, FillTiled, w_pat, npix, NULL);
        XtVaSetValues(info->li->active, 
                      XtNbackgroundPixmap, npix, NULL);
        if (rule==FillTiled && pix) XFreePixmap(dpy, pix);
    }
    setPatternColorsIcon(info->li);
    setCanvasColorsIcon(info->li->paint, NULL);
}

static void
setPatternCallback(Widget icon, PatternInfo * info, XtPointer junk2)
{
    Widget rg;
    Boolean state;

    XtVaGetValues(icon, XtNstate, &state, XtNradioGroup, &rg, NULL);

    if (state == False)
	return;
    info->li->click = 1;
    patternUpdate(info);
    XawToggleSetCurrent(info->li->patternlist, (XtPointer) info);
}

static void
freePatternInfo(Widget icon, PatternInfo * info)
{
    if (info->shownPixmap != info->pixmap)
	XFreePixmap(XtDisplay(icon), info->shownPixmap);
    if (info->pixmap != None)
	XFreePixmap(XtDisplay(icon), info->pixmap);
    XtFree((XtPointer) info);
}

static void
changePattern(void *iArg, Pixmap pix)
{
    PatternInfo *info = (PatternInfo *) iArg;
    int depth, nw, nh;
    Display *dpy = XtDisplay(info->li->fat);

    if (info->shownPixmap != info->pixmap)
        XFreePixmap(dpy, info->shownPixmap);
    if (info->pixmap != None)
        XFreePixmap(dpy, info->pixmap);

    GetPixmapWHD(dpy, pix, &info->width, &info->height, &depth);

    info->pixmap = pix;

    nw = info->width;
    nh = info->height;
    if (info->width < pattern_min_wh || info->height < pattern_min_wh) {
        int x, y;

        nw = (info->width < pattern_min_wh) ? pattern_min_wh : info->width;
        nh = (info->height < pattern_min_wh) ? pattern_min_wh : info->height;

        info->shownPixmap = 
            XCreatePixmap(dpy, DefaultRootWindow(dpy), nw, nh, depth);

        for (y = 0; y < nh; y += info->height)
            for (x = 0; x < nw; x += info->width)
                XCopyArea(dpy, info->pixmap, info->shownPixmap, info->li->gc,
                          0, 0, info->width, info->height, x, y);
    } else
        info->shownPixmap = info->pixmap;

    XtVaSetValues(info->widget, XtNbitmap, info->shownPixmap, 
		                XtNwidth, nw+COLSP, XtNheight, nh, NULL);
}

static void
unmarkAll(LocalInfo *l)
{
    Pixel p;
    int width, height;

    if (l->edit) {
#if defined(XAW3D) || defined(XAW95)
        XtVaGetValues(l->patternbox, XtNborder, &p, NULL);
#else
        p = BlackPixelOfScreen(XtScreen(l->patternbox));
#endif
        XtVaGetValues(l->edit, XtNwidth, &width, XtNheight, &height, NULL);
        XtVaSetValues(l->edit, XtNborder, p, XtNborderWidth, BWD,
                               XtNwidth, width+BSP, XtNheight, height+BSP,
                               NULL);
	l->edit = 0;
    }
}

static void 
cac(LocalInfo * info, int inW, int inH)
{
    int width, height, i;
    String lbl;

    for (i = 0; i < XtNumber(info->sizeChecks); i++) {
	Widget w = info->sizeChecks[i];
	XtVaGetValues(w, XtNlabel, &lbl, NULL);
	width = -1;
	height = -1;
	sscanf(lbl, "%dx%d", &width, &height);
	if (width <= 0 || height <= 0 || (width == inW && height == inH)) {
	    MenuCheckItem(info->curCheck = w, True);
	    break;
	}
    }
}

void
patternResized(Widget w, LocalInfo * l, XConfigureEvent * event, Boolean * flg)
{
    Dimension width, height;
    Dimension height1, height2;

    XFlush(XtDisplay(l->form));   
    w = XtParent(l->form);
    if (!w) return;
    XtVaGetValues(w, XtNwidth, &width, XtNheight, &height, NULL);
    if (width<400) width = 400;
    if (height<300) height = 300;
#ifdef XAW3D
    height1 = ((height-61)*265)/480;
#ifdef XAW3DG
    if (height1<271) height1 = 271;
#else
    if (height1<268) height1 = 268;   
#endif   
    height2 = ((height-61)*208)/480;
#else   
    height1 = ((height-59)*276)/480;
    if (height1<276) height1 = 276;    
    height2 = ((height-59)*196)/480;
#endif   
    XtResizeWidget(l->form, width-8, height1, 1);
    XtMoveWidget(l->form, 4, 54);
    XtResizeWidget(l->form1, width-8, height2, 1);
    XtMoveWidget(l->form1, 4, 60 + height1);      
    XtResizeWidget(l->form2, width-14, height2-8, 0);
    XtMoveWidget(l->form2, 4, 4);   
#ifdef XAWPLAIN
    XtResizeWidget(l->vport, width-345, height1-32, 0);   
    XtResizeWidget(l->box, width-345, height1-32, 0);
#endif
#ifdef XAW3D   
    XtResizeWidget(l->vport, width-330, height1-18, 1);
    XtResizeWidget(l->box, width-330, height1-18, 0);
#endif
#ifdef XAW95
    XtResizeWidget(l->vport, width-345, height1-32, 0);
    XtResizeWidget(l->box, width-348, height1-35, 0);   
#endif
    XtResizeWidget(l->patternbox, width-16, height2-8, 0);   
    XtVaSetValues(l->patternbox, XtNwidth, width-16, XtNheight, height2-8, 
		  NULL);
    XtResizeWidget(l->vport2, width-16, height2-8, 0);   
    XtVaSetValues(l->vport2, XtNwidth, width-16, XtNheight, height2-8, 
		  NULL);
    XtMoveWidget(l->vport2, 0, 0);
}

static void
loadPattern(PatternInfo * info)
{
    LocalInfo * l = info->li;
    Display *dpy;
    int width, height, depth;

    if (info->pixmap == None) {
	activePixel(l, info->pixel);
    } else {
        dpy = XtDisplay(l->fat);
        GetPixmapWHD(dpy, info->pixmap, &width, &height, &depth);
        XtVaSetValues(l->fat, XtNdrawWidth, width, 
                              XtNdrawHeight, height, NULL);

        PwPutPixmap(l->fat, info->pixmap);
        cac(l, width, height);
    }
    patternResized(info->li->fat, info->li, NULL, 0);   
}

void
markPattern(LocalInfo *l)
{
    int width, height;
    XtVaGetValues(l->edit, XtNwidth, &width, XtNheight, &height, NULL);
    XtVaSetValues(l->edit, XtNborder, l->marked, 
		           XtNwidth, width-BSP, 
                           XtNheight, height-BSP, 
		           XtNborderWidth, BSP/2+BWD, NULL);
    patternResized(l->fat, l, NULL, 0);
}

static void
loadPatternAction(Widget w, XEvent *event)
{
    PatternInfo *info;
    LocalInfo *l;
    XtVaGetValues(w, XtNradioData, &info, NULL);
    if (!info) return;
    l = info->li;
    if (l->edit == info->widget)
        loadPattern(info);
    else {
        unmarkAll(l);
        l->edit = info->widget;
        markPattern(l);
    }
}

static void
deletePatternCB(Widget mb, Widget w)
{
    PatternInfo *info, *ainfo;
    Widget parent;
    WidgetList children;
    Pixmap pix;
    int nchild;

    XtVaGetValues(w, XtNradioData, &info, NULL);

    /*
     *	Check to see if it is active
     */
    ainfo = (PatternInfo *) XawToggleGetCurrent(info->widget);
    if (info == ainfo)
	XawToggleUnsetCurrent(info->widget);

    /*	
     *	Make sure there are enough widgets
     */
    parent = XtParent(info->widget);
    if (!parent) return;
    XtVaGetValues(parent,
		  XtNnumChildren, &nchild,
		  XtNchildren, &children,
		  NULL);
    if (nchild == 1) {
	Notice(w, msgText[YOU_MUST_HAVE_AT_LEAST_ONE_ENTRY]);
	return;
    }
    if (w == info->li->edit) info->li->edit = 0;
    XtVaGetValues(info->li->fat, XtNpattern, &pix, NULL);
    if (pix == info->pixmap) {
       XtVaSetValues(info->li->fat, XtNfillRule, FillSolid, NULL);
    }
    XtVaGetValues(info->li->fat, XtNlinePattern, &pix, NULL);
    if (pix == info->pixmap) {
       XtVaSetValues(info->li->fat, XtNlineFillRule, FillSolid, NULL);
    }   
    XtUnmanageChild(info->widget);   
    XtDestroyWidget(info->widget);
    patternResized(w, info->li, NULL, 0);
}

static void
editPatternCB(Widget mb, Widget w)
{
    PatternInfo * info;
    XtVaGetValues(w, XtNradioData, &info, NULL);
    if (!info) return;
    unmarkAll(info->li);
    info->li->edit = info->widget;
    markPattern(info->li);
    loadPatternAction(w, NULL);
}

static void
markPatternCB(Widget mb, Widget w)
{
    PatternInfo *info;
    int width, height;

    XtVaGetValues(w, XtNradioData, &info, NULL);
    unmarkAll(info->li);
    info->li->edit = w;
    XtVaGetValues(w, XtNwidth, &width, XtNheight, &height, NULL);
    XtVaSetValues(w, XtNborder, info->li->marked,
                     XtNwidth, width-BSP,
                     XtNheight, height-BSP,
                     XtNborderWidth, BSP/2+BWD, NULL);
    patternResized(w, info->li, NULL, 0);
}

static void
loadPatternCB(Widget mb, Widget w)
{
    PatternInfo *info;
    XtVaGetValues(w, XtNradioData, &info, NULL);
    loadPattern(info);
}

static void
loadMarkedPatternCallback(Widget w, LocalInfo * info, XtPointer *junk)
{
    PatternInfo *pi;
    if (!info->edit) return;
    XtVaGetValues(info->edit, XtNradioData, &pi, NULL);
    loadPattern(pi);
}

static void
markPatternCallback(Widget w, LocalInfo * info, XtPointer *junk)
{
    PatternInfo *pi;
    pi = XawToggleGetCurrent(info->patternlist);
    if (!pi || !pi->widget || pi->li->edit) return;
    markPatternCB(0, pi->widget);    
}

static void
unmarkPatternCB(Widget mb, Widget w)
{
    PatternInfo *info;
    XtVaGetValues(w, XtNradioData, &info, NULL);
    if (!info) return;
    if (w == info->li->edit)
        unmarkAll(info->li);
    if (w == info->widget)
        XawToggleUnsetCurrent(info->widget);
    patternResized(w, info->li, NULL, 0);   
}

static void
unmarkPatternCallback(Widget w, LocalInfo * info, XtPointer *junk)
{
    if (info->edit) 
        unmarkPatternCB(0, info->edit);
    else
        XawToggleUnsetCurrent(info->patternlist);
}

static void
deletePatternCallback(Widget w, LocalInfo * info, XtPointer *junk)
{
    if (info->edit) 
        deletePatternCB(0, info->edit);
}

static Widget
AddPattern(LocalInfo *info, Pixmap pix, Pixel pxl)
{
    static XtTranslations trans = None;
    Widget icon;
    Display *dpy = XtDisplay(info->fat);
    PatternInfo *pi = XtNew(PatternInfo);
    int i, depth, nw, nh, x, y;
    char *nm;
    char *str;
    XrmValue val;

    if (!Global.patternshell) return NULL;
    pi->li = info;

    if (pattern_min_wh == 0)
	if ((XrmGetResource(XtDatabase(dpy), "Xpaint.patternsize",
			    "XPaint", &str, &val) != 1) ||
	    (sscanf((char *) val.addr, "%d", &pattern_min_wh) != 1) ||
	    (pattern_min_wh < 4) || (pattern_min_wh > 64))
	    pattern_min_wh = PAT_MIN_WH;

    if (!info->added) {
	static XtActionsRec v =
	{"loadPattern", (XtActionProc) loadPatternAction};
	XtAppAddActions(XtWidgetToApplicationContext(info->fat), &v, 1);
	info->added = True;
    }

    if (trans == None)
	trans = XtParseTranslationTable
	    ("<BtnDown>,<BtnUp>: set() notify()\n<BtnDown>(2): loadPattern()");

    if (pix != None) {
	GetPixmapWHD(dpy, pix, &pi->width, &pi->height, &depth);
	/* convert 1x1 pixmap into a pixel ! */
	if (pi->width == 1 && pi->height == 1) {
	    XImage *xim = XGetImage(dpy, pix, 0, 0, 1, 1,
				    AllPlanes, ZPixmap);
	    pi->pixel = XGetPixel(xim, 0, 0);
	    XDestroyImage(xim);
	    pi->pixmap = None;
	} else
	    pi->pixmap = pix;
    } else {
	pi->pixel = pxl;
	pi->pixmap = None;
	XtVaGetValues(info->patternbox, XtNdepth, &depth, NULL);
    } 

    if (pi->pixmap == None) {
	nm = "solid";
#ifdef HAVE_COLTOG
        pi->shownPixmap = None;
#else
	pi->shownPixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
			    pattern_min_wh, pattern_min_wh, depth);
	XSetForeground(dpy, info->gc, pi->pixel);
	XFillRectangle(dpy, pi->shownPixmap, 
                       info->gc, 0, 0, pattern_min_wh, pattern_min_wh);
        nw = nh = pattern_min_wh;
#endif
    } else {
        nm = "pattern";
        nw = pi->width ;
        nh = pi->height;
        if (pi->width < pattern_min_wh || pi->height < pattern_min_wh) {
	   nw = (pi->width < pattern_min_wh) ? pattern_min_wh : pi->width;
	   nh = (pi->height < pattern_min_wh) ? pattern_min_wh : pi->height;

	pi->shownPixmap = 
            XCreatePixmap(dpy, DefaultRootWindow(dpy), nw, nh, depth);

	for (y = 0; y < nh; y += pi->height)
	    for (x = 0; x < nw; x += pi->width)
		XCopyArea(dpy, pi->pixmap, pi->shownPixmap, info->gc,
			  0, 0, pi->width, pi->height, x, y);
	} else
            pi->shownPixmap = pi->pixmap;
    }

#ifdef HAVE_COLTOG
    if (pi->pixmap == None) {
	icon = XtVaCreateManagedWidget(nm, colToggleWidgetClass,
					info->patternbox,
					XtNforeground, pi->pixel,
				        XtNradioGroup, info->patternlist,
					XtNtranslations, trans,
					XtNradioData, pi,
					XtNisSolid, True,
					NULL);
    } else {
#endif
	icon = XtVaCreateManagedWidget(nm, toggleWidgetClass,
					info->patternbox,
					XtNbitmap, pi->shownPixmap,
				        XtNradioGroup, info->patternlist,
					XtNtranslations, trans,
					XtNradioData, pi,
				        XtNwidth, nw+10+COLSP,
				        XtNheight, nh+10,
					NULL);
#ifdef HAVE_COLTOG
    }
#endif

    if (info->patternlist == None) info->patternlist = icon;

    XtAddCallback(icon, XtNcallback, (XtCallbackProc) setPatternCallback,
		  (XtPointer) pi);

    for (i=1; i<=5; i++)
         patternMenu[i].data = (void *) icon;
    MenuPopupCreate(icon, "popup-menu", XtNumber(patternMenu), patternMenu);

    pi->widget = icon;
    XtAddCallback(icon, XtNdestroyCallback, (XtCallbackProc) freePatternInfo,
		  (XtPointer) pi);   
    return icon;
}

static void 
cpickCallback(Widget w, LocalInfo * l, Pixel p)
{
    if (Global.popped_up) return;

    if (l->patternlist && !l->click && !l->edit)
        XawToggleUnsetCurrent(l->patternlist);
    l->click = 0;
    activePixel(l, p);
}

static void
changeSolid(PatternInfo * info, Pixel p)
{
    LocalInfo *l = info->li;
    Display *dpy = XtDisplay(l->fat);
    int depth;

    info->pixel = p;

#ifdef HAVE_COLTOG
    XtVaSetValues(info->widget, XtNforeground, info->pixel);
#else
    XtVaGetValues(info->widget, XtNdepth, &depth, NULL);
    /* should already be created -- just to be sure !?! */
    if (info->shownPixmap == None)
        info->shownPixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
			     pattern_min_wh, pattern_min_wh, depth);
    XSetForeground(dpy, info->li->gc, p);
    XFillRectangle(dpy, info->shownPixmap, 
                        info->li->gc, 0, 0, pattern_min_wh, pattern_min_wh);
    XtVaSetValues(info->widget, XtNbitmap, info->shownPixmap, NULL);
#endif
}

static void 
setAsBackgroundCallback(Widget w, LocalInfo * l, XPointer * junk2)
{
    Pixel p;
    XColor *color;
    XColor xc;

    if (Global.popped_up) return;

    if (l->map->isMapped) {
        color = ColorPickerGetXColor(l->cpick);
        p = PaletteAlloc(l->map, color);
    } else
        XtVaGetValues(l->active, XtNbackground, &p, NULL);

    xc.pixel = p;
    xc.flags = DoRed | DoGreen | DoBlue;
    XQueryColor(XtDisplay(w), l->map->cmap, &xc);

    Global.bg[0] = (xc.red >> 8) & 0xff;
    Global.bg[1] = (xc.green >> 8) & 0xff;
    Global.bg[2] = (xc.blue >> 8) & 0xff;
    XtVaSetValues(l->paint, XtNbackground, p, NULL);
    activePixel(l, p);
}

static void 
colorSelectCallback(Widget w, LocalInfo * l, XPointer * junk2)
{
    Widget icon;
    WidgetList children;
    PatternInfo *pi;
    Pixel p;
    XColor *color;
    XColor xc;
    int r=0, g=0, b=0;
    int i, nchildren;

    if (Global.popped_up) return;

    if (l->map->isMapped) {
        color = ColorPickerGetXColor(l->cpick);
        p = PaletteAlloc(l->map, color);
	activePixel(l, p);
	xc.pixel = p;
	xc.flags = DoRed | DoGreen | DoBlue;
	XQueryColor(XtDisplay(w), l->map->cmap, &xc);
	r = (xc.red >> 8) & 0xff;
	g = (xc.green >> 8) & 0xff;
	b = (xc.blue >> 8) & 0xff;
    } else
        XtVaGetValues(l->active, XtNbackground, &p, NULL);

    XtVaGetValues(l->patternbox, XtNchildren, &children,
		  XtNnumChildren, &nchildren, NULL);
    for (i=0; i<nchildren; i++) {
	pi = NULL;
	XtVaGetValues(children[i], XtNradioData, &pi, NULL);

	if (pi == NULL) continue;

	if (pi->pixmap == None) {
	    if (l->map->isMapped) {
	        xc.pixel = pi->pixel;
	        xc.flags = DoRed | DoGreen | DoBlue;
	        XQueryColor(XtDisplay(w), l->map->cmap, &xc);
		if (((xc.red >> 8) & 0xff) == r &&
                    ((xc.green >> 8) & 0xff) == g &&
                    ((xc.blue >> 8) & 0xff) == b) {
                    XawToggleSetCurrent(l->patternlist, (XtPointer) pi);
	            return;
		}
	    } else
	    if (pi->pixel == p) {
                XawToggleSetCurrent(l->patternlist, (XtPointer) pi);
	        return;
	    }
        }
    }

    if (l->edit) {
        XtVaGetValues(l->edit, XtNradioData, &pi, NULL);
	if (pi) {
            unmarkAll(l);
            changeSolid(pi, p);
            XawToggleSetCurrent(l->patternlist, (XtPointer) pi);
            if (l->paint) AddItemToCanvasPalette(l->paint, p, None);
	    return;
	}
    }

    unmarkAll(l);
    icon = AddPattern(l, 0, p);
    XtVaGetValues(icon, XtNradioData, &pi, NULL);
    XawToggleSetCurrent(l->patternlist, (XtPointer) pi);
    if (l->paint) AddItemToCanvasPalette(l->paint, p, None);
    patternResized(l->fat, l, NULL, 0);
}

static void 
popupHandler(Widget w, LocalInfo * l, XEvent * event, Boolean * flg)
{
    KeySym keysym;
    int len;
    char buf[21];

    if (Global.popped_up) {
        if (event->type == ButtonRelease)
            PopdownMenusGlobal();
        event->type = None;
        return;
    }

    if (event->type == KeyPress) {
        len = XLookupString((XKeyEvent *)event, 
                            buf, sizeof(buf) - 1, &keysym, NULL);
        switch (keysym) {
            case XK_Escape:
                PopdownMenusGlobal();
                event->type = None;
                break;
            default:
                break;
	}
    }
}

static void 
switchCallback(Widget w, LocalInfo * l, XEvent * event, Boolean * flg)
{
    Pixel p;
    int side = 1;

    if (Global.popped_up) {
        if (event->type == ButtonRelease)
            PopdownMenusGlobal();
        event->type = None;
        return;
    }

    if (event->type != ButtonRelease);
    if (w == l->icon) {
        if (l->paint)
           RaiseWindow(XtDisplay(l->paint), XtWindow(GetShell(l->paint)));
	return;
    }
    l->active = w;
    if (w == l->view1) {
        XMapWindow(XtDisplay(w), XtWindow(l->r_arrow));
        XUnmapWindow(XtDisplay(w), XtWindow(l->g_arrow));
	side = 1;
    }
    if (w == l->view2) {
        XUnmapWindow(XtDisplay(w), XtWindow(l->r_arrow));
        XMapWindow(XtDisplay(w), XtWindow(l->g_arrow));
	side = 2;
    }
    if (!(l->mask & side)) {
        XtVaGetValues(l->active, XtNbackground, &p, NULL);
	/* Don't activate pure black because it's perturbing... */
	if (p != BlackPixelOfScreen(XtScreen(w)))
            activePixel(l, p);
    }
    setPatternColorsIcon(l);
    setCanvasColorsIcon(l->paint, NULL);
}

static void 
grabPatternCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    LocalInfo *info = (LocalInfo *) infoArg;
    Image *image;
    Colormap cmap;
    Pixmap pix;
    int grabW, grabH;

    XtVaGetValues(info->fat, XtNdrawWidth, &grabW,
		  XtNdrawHeight, &grabH,
		  NULL);

    image = DoGrabImage(w, grabW, grabH);

    XtVaGetValues(info->fat, XtNcolormap, &cmap, NULL);
    pix = None;
    if (ImageToPixmapCmap(image, info->fat, &pix, cmap))
	PwPutPixmap(info->fat, pix);
}

void
checkPatternLink(Widget w, int mode)
{
    LocalInfo *info = (LocalInfo *) Global.patterninfo;
    Pixel p;
    int lw;

    if (!info) return;
    if (w && info->paint == w) {
        if (mode == 0) {
            info->paint = 0;
            unmarkAll(info);
            initViews(info);
	}
        if (mode == 1) {
            XtVaGetValues(w, XtNlineWidth, &lw, NULL);
            XtVaGetValues(w, XtNbackground, &p, NULL);
            XtVaSetValues(info->fat, XtNlineWidth, lw, NULL);
            XtVaSetValues(info->fat, XtNbackground, p, NULL);
            setPatternColorsIcon(info);
	    setCanvasColorsIcon(info->paint, NULL);
            MenuCheckItem(lineMenu[(lw/2<5)? (lw/2) : LW_GENERAL].widget, True);
	}
    }
}

void
setPatternLineWidth(void *ptr, int width)
{
  LocalInfo * info = (LocalInfo *) ptr;
    if (!info) return;
    XtVaSetValues(info->fat, XtNlineWidth, width, NULL);
    MenuCheckItem(lineMenu[(width/2<5)? (width/2) : LW_GENERAL].widget, True);
    if (!info->paint)
        setPatternColorsIcon(info);
}

static void 
closeShellCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    LocalInfo *info = (LocalInfo *) infoArg;
    PopdownMenusGlobal();
    if (info->iconpix)
        XFreePixmap(XtDisplay(info->icon), info->iconpix);
    info->iconpix = None;
    XFreeGC(XtDisplay(info->fat), info->gc);
    XtDestroyWidget(Global.patternshell);
    XtFree((XtPointer) info);
    Global.patternshell = 0;
    Global.patterninfo = NULL;
}

static void 
selectPatternCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    LocalInfo *info = (LocalInfo *) infoArg;
    PatternInfo *pi;
    Pixmap pix;
    Widget icon;
    int width, height;

    if (!info) return;

    PwRegionFinish(info->fat, True);
    PwGetPixmap(info->fat, &pix, NULL, NULL);

    XawToggleUnsetCurrent(info->patternlist);
    if (info->edit) {
	XtVaGetValues(info->edit, XtNradioData, &pi, NULL);
        if (pi) {
	   XtVaGetValues(info->fat, XtNdrawWidth, &width, 
                                    XtNdrawHeight, &height, NULL);
	   changePattern(pi, pix);
           patternUpdate(pi);
	}
	unmarkAll(info);
    } else {
	icon = AddPattern(info, pix, 0);
	XtVaGetValues(icon, XtNradioData, &pi, NULL);
	if (pi) patternUpdate(pi);
    }
    if (info->paint) AddItemToCanvasPalette(info->paint, None, pix);
    patternResized(info->fat, info, NULL, 0);      
}

static void 
asbrushPatternCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    LocalInfo *info = (LocalInfo *) infoArg;
    Pixmap pix;
    Image *image;
    int i;

    PwRegionFinish(info->fat, True);
    PwGetPixmap(info->fat, &pix, NULL, NULL);

    image = PixmapToImage(info->fat, 
                          dupPixmap(XtDisplay(Global.toplevel), pix),
                          info->map->cmap);

    i = Global.nbrushes;
    Global.nbrushes = i+1;
    Global.brushes = (void **) 
       realloc(Global.brushes, Global.nbrushes * sizeof(void *));
    Global.brushes[i] = (void *) image;

    /* Refresh Brush Popup */
    Global.brushpopup = createBrushDialog(Global.toplevel);
    BrushSelect(Global.toplevel);
}

static void 
readFileCallback(Widget paint, XtPointer fileArg, XtPointer imageArg)
{
    Image *image = (Image *) imageArg;
    Pixmap pix;
    Colormap cmap;
    int  width, height, depth;

    /*  XXX - allocating a new colormap! */
    if (ImageToPixmap(image, paint, &pix, &cmap)) {
        GetPixmapWHD(XtDisplay(paint), pix, &width, &height, &depth);
        XtVaSetValues(paint, XtNdrawWidth, width,
		      XtNdrawHeight, height, NULL);
	XtVaSetValues(paint, XtNcolormap, cmap, NULL);
	PwPutPixmap(paint, pix);
	return;
    }
}

static void 
readCallback(Widget w, XtPointer paint, XtPointer junk)
{
    GetFileName((Widget) paint, BROWSER_READ, NULL, readFileCallback, NULL);
}

static void 
sizeOkChoiceCallback(Widget w, LocalInfo * l, TextPromptInfo * info)
{
    int width = atoi(info->prompts[0].rstr);
    int height = atoi(info->prompts[1].rstr);

    if (width <= 0 || height <= 0) {
	Notice(w, msgText[INVALID_WIDTH_OR_HEIGHT_MUST_BE_GREATER_THAN_ZERO]);
    } else if (width > 128 || height > 128) {
	Notice(w, msgText[INVALID_WIDTH_OR_HEIGHT_MUST_BE_LESS_THAN_HUNDRED_TWENTY_NINE]);
    } else {
	XtVaSetValues(l->fat, XtNdrawWidth, width,
		      XtNdrawHeight, height,
		      NULL);
	MenuCheckItem(l->curCheck, False);
	l->curCheck = None;
	cac(l, width, height);
    }
}
static void 
sizeChoiceCallback(Widget w, XtPointer larg, XtPointer junk)
{
    static TextPromptInfo info;
    static struct textPromptInfo values[3];
    char bufA[16], bufB[16];
    int width, height;
    LocalInfo *l = (LocalInfo *) larg;

    XtVaGetValues(l->fat, XtNdrawWidth, &width,
		  XtNdrawHeight, &height,
		  NULL);

    info.prompts = values;
    info.nprompt = 2;
    info.title = msgText[ENTER_THE_DESIRED_PATTERN_SIZE];

    values[0].prompt = msgText[PATTERN_WIDTH];
    values[0].str = bufA;
    values[0].len = 4;
    values[1].prompt = msgText[PATTERN_HEIGHT];
    values[1].str = bufB;
    values[1].len = 4;

    sprintf(bufA, "%d", width);
    sprintf(bufB, "%d", height);

    TextPrompt(w, "sizeselect", &info, (XtCallbackProc) sizeOkChoiceCallback,
	       NULL, larg);
}
static void 
sizeCallback(Widget w, XtPointer larg, XtPointer junk)
{
    LocalInfo *l = (LocalInfo *) larg;
    int width, height;
    String lbl;

    if (l->curCheck == w)
	return;

    XtVaGetValues(w, XtNlabel, &lbl, NULL);
    width = -1;
    height = -1;
    sscanf(lbl, "%dx%d", &width, &height);
    if (width <= 0 || height <= 0 || width >= 256 || height >= 256) {
	Notice(w, msgText[INVALID_WIDTH_OR_HEIGHT_MUST_BE_BETWEEN_ZERO_AND_TWO_HUNDRED_FIFTY_SIX]);
	return;
    }
    MenuCheckItem(l->curCheck = w, True);

    /*
    **  Just change the size, no OK
     */
    XtVaSetValues(l->fat, XtNdrawWidth, width,
		  XtNdrawHeight, height, NULL);
}

static void 
gridCallback(Widget w, XtPointer larg, XtPointer junk)
{
    LocalInfo *l = (LocalInfo *) larg;
    Boolean v;

    XtVaGetValues(l->fat, XtNgrid, &v, NULL);
    v = !v;
    XtVaSetValues(l->fat, XtNgrid, v, NULL);

    MenuCheckItem(w, v);
}

/*
static void
changeShellSize(Widget w, LocalInfo * l, XEvent * event, Boolean * flg)
{
    static Dimension u=0, v=0, up, vp;
    Display *dpy;
    if (event->type == ConfigureNotify && l->cpick) {
        XtVaGetValues(w, XtNwidth, &up, XtNheight, &vp, NULL);
	if (u == 0 && v == 0) {
	    u = up;
	    v = vp;
	    return;
	}
	if (up == u && vp == v) return;
	dpy = XtDisplay(w);
        XUnmapWindow(dpy, XtWindow(l->r_arrow));
        XUnmapWindow(dpy, XtWindow(l->g_arrow));
	if (l->active == l->view1)
            XMapWindow(dpy, XtWindow(l->r_arrow));
	else
            XMapWindow(dpy, XtWindow(l->g_arrow));
    }
}

static void
lookupPatternCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    LocalInfo *info = (LocalInfo *) infoArg;
    int nchildren, i;
    WidgetList children;
    Widget icon, list;
    Colormap cmap;
    Pixel p;
    PatternInfo *pi;

    DoGrabPixel(w, &p, &cmap);

    if (cmap != info->map->cmap) {
	XColor col;

	col.pixel = p;
	col.flags = DoRed | DoGreen | DoBlue;
	XQueryColor(XtDisplay(w), cmap, &col);
	if (!PaletteLookupColor(info->map, &col, &p))
	    p = PaletteAlloc(info->map, &col);
    }
    list = info->patternlist;
    if (!list || !XtParent(list)) return;

    XtVaGetValues(XtParent(list),
		  XtNnumChildren, &nchildren,
		  XtNchildren, &children,
		  NULL);

    for (i = 0; i < nchildren; i++) {
	pi = NULL;
	XtVaGetValues(children[i], XtNradioData, &pi, NULL);
	if (pi == NULL || pi->pixmap != None)
	    continue;

	if (pi->pixel == p) {
	    XawToggleSetCurrent(list, (XtPointer) pi);
	    return;
	}
    }

    icon = AddPattern(info, None, p);
    XtVaGetValues(icon, XtNradioData, &pi, NULL);
    XawToggleSetCurrent(list, (XtPointer) pi);
}
*/

void LoadRCInfo(RCInfo *rcInfo, Palette *map)
{
    Display *dpy = XtDisplay(Global.toplevel);
    XColor col, rgb;
    int i, j;
    /*
     *	Allocate the color entries
     */
    rcInfo->colorFlags = (Boolean *) XtCalloc(sizeof(Boolean),
					      rcInfo->ncolors);
    rcInfo->colorPixels = (Pixel *) XtCalloc(sizeof(Pixel),
					     rcInfo->ncolors);
    for (i = 0; i < rcInfo->ncolors; i++)
	rcInfo->colorFlags[i] = False;

    for (i = 0; i < rcInfo->ncolors; i++) {
	if (XLookupColor(dpy, map->cmap,
			 rcInfo->colors[i], &col, &rgb) ||
	    XParseColor(dpy, map->cmap,
			rcInfo->colors[i], &col)) {
	    rcInfo->colorPixels[i] = PaletteAlloc(map, &col);
	    rcInfo->colorFlags[i] = True;
	    for (j = 0; j < i; j++)
		if (rcInfo->colorPixels[i] == 
                    rcInfo->colorPixels[j])
		    rcInfo->colorFlags[i] = False;
	}
    }
}

/*
 *  Convert RC file information into the pattern box info
 */
static void
makePaletteArea(LocalInfo * info, RCInfo * rcInfo)
{
    int i;

    LoadRCInfo(rcInfo, info->map);

    for (i = 0; i < rcInfo->ncolors; i++) {
	if (!rcInfo->colorFlags[i])
	    continue;
	AddPattern(info, None, rcInfo->colorPixels[i]);
    }

    for (i = 0; i < rcInfo->nimages; i++) {
	Pixmap pix;

	pix = None;
	rcInfo->images[i]->refCount++;
	ImageToPixmapCmap(rcInfo->images[i], info->fat, &pix,
                          info->map->cmap);
	AddPattern(info, pix, 0);
    }
}

static void
copyPaletteArea(LocalInfo * info, Pixel *pixels, Pixmap *patterns,
                int npixels, int npatterns)
{
    Display *dpy;
    int i;

    if (!info->paint) return;
    dpy = XtDisplay(info->paint);

    for (i = 0; i < npixels; i++)
	AddPattern(info, None, pixels[i]);

    for (i = 0; i < npatterns; i++)
	AddPattern(info, dupPixmap(dpy, patterns[i]), 0);
}

void
PatternEdit(Widget w, Pixel *pixels, Pixmap *patterns, void *brushes,
	    int npixels, int npatterns, int nbrushes)
{
    static Pixmap r_arrow_pix = 0, g_arrow_pix = 0;
    LocalInfo *info = NULL; 
    Display * dpy;
    Widget topf;
    Widget fat, norm, grabPattern, selectPattern, asbrushPattern;
    Widget cpick, cancel;
    Colormap cmap;
    Pixel white = WhitePixelOfScreen(XtScreen(w));
    Pixel black = BlackPixelOfScreen(XtScreen(w));
    Pixel bgpix;
    Palette *map;
    int width, height, depth, lw;
    Position x, y;
    Arg args[6];
    int nargs = 0;
    char *ptr;

    dpy = XtDisplay(w);
    depth = DefaultDepthOfScreen(XtScreen(w));

    if (Global.patternshell && depth<=8) {
        info = (LocalInfo *) Global.patterninfo;
	if (info->paint != w)
	    closeShellCallback(Global.patternshell, Global.patterninfo, NULL);
    }

    if (Global.patternshell) {
        info = (LocalInfo *) Global.patterninfo;
        unmarkAll(info);
	XawToggleUnsetCurrent(info->patternlist);
	if (info->paint != w) {
	    XFreeGC(dpy, info->gc);
            XtVaGetValues(GetShell(w), XtNcolormap, &cmap, NULL);
            info->gc = XCreateGC(dpy, XtWindow(w), 0, 0);
	    info->paint = w;
            info->map = map = PaletteFind(w, cmap);
	}
	initViews(info);
        XMapWindow(dpy, XtWindow(info->r_arrow));
        XUnmapWindow(dpy, XtWindow(info->g_arrow));
	RaiseWindow(dpy, XtWindow(Global.patternshell));
	return;
    }
    StateSetBusyWatch(True);
    info = (LocalInfo *) XtMalloc(sizeof(LocalInfo));
    Global.patterninfo = (void *) info;
   
    info->paint = w;
    info->gc = XCreateGC(dpy, XtWindow(w), 0, 0);
    info->iconlist = None;
    info->patternlist = None;
    info->click = 0;
    info->mask = 0;
    info->edit = 0;
    info->added = 0;
    info->nbrushes = nbrushes;
    info->brushes = (Image **)brushes;

    /* StateShellBusy(w, True); */
    XtVaGetValues(GetShell(w), XtNcolormap, &cmap, XtNx, &x, XtNy, &y, NULL);
    info->map = map = PaletteFind(w, cmap);
    if (map->isMapped) 
        info->marked = black;
    else {
        XColor xc;
        XAllocNamedColor(dpy, cmap, "red", &xc, &xc);
        info->marked = xc.pixel;
    }

    XtSetArg(args[nargs], XtNcolormap, cmap); nargs++; 
    XtSetArg(args[nargs], XtNx,  x+24);  nargs++;
    XtSetArg(args[nargs], XtNy,  y+24);  nargs++;

    Global.patternshell = 
        XtVisCreatePopupShell("pattern", topLevelShellWidgetClass,
			      Global.toplevel, args, nargs);

    PaletteAddUser(map, Global.patternshell);

    topf = XtVaCreateManagedWidget("form", formWidgetClass, 
                                   Global.patternshell, NULL);

    info->bar = MenuBarCreate(topf, XtNumber(menuBar), menuBar);
    XtVaSetValues(info->bar, XtNvertDistance, 9, NULL);

    info->r_arrow = XtVaCreateManagedWidget("r_arrow", coreWidgetClass, topf,
                                      XtNwidth, r_arrow_width,
                                      XtNheight, r_arrow_height,
				      XtNvertDistance, 15,
                                      XtNhorizDistance, 12,
                                      XtNfromHoriz, info->bar,
                                      XtNborderWidth, 0,
                                      NULL);

    info->view1 = XtVaCreateManagedWidget("view1", coreWidgetClass, topf,
                                      XtNwidth, ICONHEIGHT,
                                      XtNheight, ICONHEIGHT,
				      XtNhorizDistance, 4,
                                      XtNfromHoriz, info->r_arrow,
                                      XtNborderWidth, 1,
                                      NULL);

    info->view2 = XtVaCreateManagedWidget("view2", coreWidgetClass, topf,
                                      XtNwidth, ICONHEIGHT,
                                      XtNheight, ICONHEIGHT,
                                      XtNfromHoriz, info->view1,
                                      XtNborderWidth, 1,
                                      NULL);

    info->g_arrow = XtVaCreateManagedWidget("g_arrow", coreWidgetClass, topf,
                                      XtNwidth, g_arrow_width,
                                      XtNheight, g_arrow_height,
				      XtNvertDistance, 12,
                                      XtNhorizDistance, 5,
                                      XtNfromHoriz, info->view2,
                                      XtNborderWidth, 0,
                                      NULL);

    info->icon = XtVaCreateManagedWidget("icon", coreWidgetClass, topf,
                                      XtNwidth, ICONWIDTH,
                                      XtNheight, ICONHEIGHT,
                                      XtNfromHoriz, info->g_arrow,
				      XtNhorizDistance, 12,
                                      XtNborderWidth, 1,
                                      NULL);

    cancel = XtVaCreateManagedWidget("close", commandWidgetClass, topf,
                                      XtNfromHoriz, info->icon,
				      XtNvertDistance, 13,
				      XtNhorizDistance, 15,
                                      NULL);

    info->form = XtVaCreateManagedWidget("patternbox", formWidgetClass, topf,
				   XtNfromVert, info->view1,
				   NULL);

    cpick = None;

    if (!map->isMapped || !map->readonly_ || map->ncolors > 256)
	cpick = ColorPickerPalette(info->form, map, &white);

    grabPattern = XtVaCreateManagedWidget("grab",
					 commandWidgetClass, info->form,
					 XtNhorizDistance, 25,
					 XtNvertDistance, 4,
				         XtNleft, XtChainLeft,
				         XtNtop, XtChainTop,
				         XtNbottom, XtChainBottom,
					 NULL);

    selectPattern = XtVaCreateManagedWidget("select",
				       commandWidgetClass, info->form,
				       XtNvertDistance, 4,
				       XtNfromHoriz, grabPattern,
				       XtNright, XtChainRight,
				       XtNtop, XtChainTop,
				       XtNbottom, XtChainBottom,
				       NULL);

    asbrushPattern = XtVaCreateManagedWidget("asbrush",
				       commandWidgetClass, info->form,
				       XtNvertDistance, 4,
				       XtNfromHoriz, selectPattern,
				       XtNright, XtChainRight,
				       XtNtop, XtChainTop,
				       XtNbottom, XtChainBottom,
				       NULL);

    info->vport = XtVaCreateManagedWidget("viewport", viewportWidgetClass, 
				 info->form,
				 XtNfromVert, grabPattern,
				 XtNvertDistance, 4,
				 XtNallowVert, True,
				 XtNallowHoriz, True,
				 XtNuseBottom, True,
				 XtNuseRight, True,
				 XtNleft, XtChainLeft,
				 XtNright, XtChainRight,
			         XtNtop, XtChainTop,
				 XtNbottom, XtChainBottom,
				 XtNwidth, 286,
				 XtNheight, 290,
				 NULL);

    if (cpick != None) {
        XtVaSetValues(info->vport, XtNfromHoriz, cpick, NULL);
        XtVaSetValues(grabPattern, XtNfromHoriz, cpick, NULL);
    }

    info->box = XtVaCreateManagedWidget("paintBox", boxWidgetClass, info->vport,
			    XtNbackgroundPixmap, 
				  GetBackgroundPixmap(info->vport),
                            XtNtop, XtChainTop,
                            XtNbottom, XtChainBottom,
			    XtNorientation, XtorientVertical,
			    NULL);

    XtVaGetValues(w, XtNlineWidth, &lw, NULL);
   
    fat = XtVaCreateManagedWidget("paint", paintWidgetClass, info->box,
			    XtNbackground, white,
			    XtNdrawWidth, 24,
			    XtNdrawHeight, 24,
			    XtNlineWidth, lw,
			    XtNfillRule, FillSolid,
			    XtNlineFillRule, FillSolid,
			    NULL);
    norm = XtVaCreateManagedWidget("norm", paintWidgetClass, info->box,
			    XtNbackground, white,
                            XtNfromVert, fat,
			    XtNhorizDistance, 0,
			    XtNpaint, fat,
			    XtNfillRule, FillSolid,
			    XtNlineFillRule, FillSolid,
			    XtNzoom, 1,
			    NULL);
   
    OperationSetPaint(fat);
    ccpAddStdPopup(fat, NULL);

    info->cpick = cpick;
    info->fat = fat;

    if (cpick != None) {
        if (!map->isMapped) {
	    ColorPickerSetFunction(cpick,
		       (XtCallbackProc) cpickCallback, (XtPointer) info);
	}
	XtAddCallback(ColorPickerGetSelectWidget(cpick), XtNcallback,
	    (XtCallbackProc) colorSelectCallback, (XtPointer) info);
	XtAddCallback(ColorPickerGetAsBgWidget(cpick), XtNcallback,
	    (XtCallbackProc) setAsBackgroundCallback, (XtPointer) info);
    }

    if (cpick != None) {
        XtVaSetValues(grabPattern, XtNfromHoriz, cpick, NULL);
        XtInsertRawEventHandler(cpick, 
           ButtonPressMask | ButtonReleaseMask, False,
	   (XtEventHandler) popupHandler, (XtPointer) info,
           XtListHead);
    }

    XtAddEventHandler(topf, 
        KeyPressMask | ButtonPressMask | ButtonReleaseMask, False,
        (XtEventHandler) popupHandler, (XtPointer) info);
    XtAddEventHandler(info->form, 
        KeyPressMask | ButtonPressMask | ButtonReleaseMask, False,
        (XtEventHandler) popupHandler, (XtPointer) info);

    XtAddEventHandler(info->view1, ButtonPressMask | ButtonReleaseMask, False,
        (XtEventHandler) switchCallback, (XtPointer) info);
    XtAddEventHandler(info->view2, ButtonPressMask | ButtonReleaseMask, False,
        (XtEventHandler) switchCallback, (XtPointer) info);
    XtAddEventHandler(info->icon, ButtonPressMask | ButtonReleaseMask, False,
        (XtEventHandler) switchCallback, (XtPointer) info);
    XtAddCallback(cancel, XtNcallback, closeShellCallback, (XtPointer) info);

    XtAddCallback(paletteMenu[SAVE_CONFIG].widget, XtNcallback,
                  (XtCallbackProc) saveConfigCallback, (XtPointer) info);
    XtAddCallback(paletteMenu[LOAD_CONFIG].widget, XtNcallback,
                  (XtCallbackProc) loadConfigCallback, (XtPointer) info);
    XtAddCallback(paletteMenu[MARK_PATTERN].widget, XtNcallback,
                  (XtCallbackProc) markPatternCallback, (XtPointer) info);
    XtAddCallback(paletteMenu[UNMARK_PATTERN].widget, XtNcallback,
                  (XtCallbackProc) unmarkPatternCallback, (XtPointer) info);
    XtAddCallback(paletteMenu[LOAD_MARKED_PATTERN].widget, XtNcallback,
                  (XtCallbackProc) loadMarkedPatternCallback, (XtPointer)info);
    XtAddCallback(paletteMenu[DELETE_PATTERN].widget, XtNcallback,
                  (XtCallbackProc) deletePatternCallback, (XtPointer) info);

    XtAddCallback(XtNameToWidget(info->bar, "canvas.canvasMenu.close"),
		  XtNcallback, closeShellCallback, (XtPointer) info);
    XtAddCallback(XtNameToWidget(info->bar, "canvas.canvasMenu.save"),
		  XtNcallback, StdSaveFile, (XtPointer) fat);
    XtAddCallback(XtNameToWidget(info->bar, "canvas.canvasMenu.read"),
		  XtNcallback, readCallback, (XtPointer) fat);

    ccpAddUndo(XtNameToWidget(info->bar, "edit.editMenu.undo"), fat);
    ccpAddCut(XtNameToWidget(info->bar, "edit.editMenu.cut"), fat);
    ccpAddCopy(XtNameToWidget(info->bar, "edit.editMenu.copy"), fat);
    ccpAddPaste(XtNameToWidget(info->bar, "edit.editMenu.paste"), fat);
    ccpAddClear(XtNameToWidget(info->bar, "edit.editMenu.clear"), fat);
    ccpAddDuplicate(XtNameToWidget(info->bar, "edit.editMenu.dup"), fat);

    XtAddCallback(XtNameToWidget(info->bar, "edit.editMenu.all"),
		  XtNcallback, StdSelectAllCallback, (XtPointer) fat);

    XtAddCallback(sizeMenu[SZ_N1].widget,
		  XtNcallback, sizeCallback, (XtPointer) info);
    XtAddCallback(sizeMenu[SZ_N2].widget,
		  XtNcallback, sizeCallback, (XtPointer) info);
    XtAddCallback(sizeMenu[SZ_N3].widget,
		  XtNcallback, sizeCallback, (XtPointer) info);
    XtAddCallback(sizeMenu[SZ_N4].widget,
		  XtNcallback, sizeCallback, (XtPointer) info);
    XtAddCallback(sizeMenu[SZ_N5].widget,
		  XtNcallback, sizeCallback, (XtPointer) info);
    XtAddCallback(sizeMenu[SZ_N6].widget,
		  XtNcallback, sizeChoiceCallback, (XtPointer) info);
    XtAddCallback(imageMenu[IM_GRID].widget,
		  XtNcallback, gridCallback, (XtPointer) info);
    XtAddCallback(imageMenu[IM_ZOOM].widget,
		  XtNcallback, zoomCallback, (XtPointer) info->fat);
    XtAddCallback(imageMenu[IM_BGRD].widget,
		  XtNcallback, changeBackground, (XtPointer) info->paint);

    info->sizeChecks[0] = sizeMenu[SZ_N1].widget;
    info->sizeChecks[1] = sizeMenu[SZ_N2].widget;
    info->sizeChecks[2] = sizeMenu[SZ_N3].widget;
    info->sizeChecks[3] = sizeMenu[SZ_N4].widget;
    info->sizeChecks[4] = sizeMenu[SZ_N5].widget;
    info->sizeChecks[5] = sizeMenu[SZ_N6].widget;

    XtVaGetValues(info->fat, XtNdrawWidth, &width, XtNdrawHeight, &height, NULL);
    cac(info, width, height);

    XtAddCallback(selectPattern, XtNcallback,
		  (XtCallbackProc) selectPatternCallback, (XtPointer) info);
    XtAddCallback(asbrushPattern, XtNcallback,
		  (XtCallbackProc) asbrushPatternCallback, (XtPointer) info);
    AddDestroyCallback(Global.patternshell,
                       (DestroyCallbackFunc) closeShellCallback, info);
    XtAddCallback(grabPattern, XtNcallback,
		  (XtCallbackProc) grabPatternCallback, (XtPointer) info);

   
    GraphicAdd(fat);
    GraphicAdd(norm);
    XtVaSetValues(fat, XtNmenuwidgets, NULL, NULL);
    XtVaSetValues(norm, XtNmenuwidgets, NULL, NULL);

    info->form1 = XtVaCreateManagedWidget("form1",
				   formWidgetClass, topf,
				   XtNfromVert, info->form,
				   NULL);

    info->form2 = XtVaCreateManagedWidget("patternRackForm",
				   formWidgetClass, info->form1,
				   XtNborderWidth, 0,
				   XtNleft, XtChainLeft,
				   XtNright, XtChainRight,
				   NULL);

    info->vport2 = XtVaCreateManagedWidget("viewport2",
				    viewportWidgetClass, info->form2,
				    XtNallowVert, True,
				    XtNuseBottom, True,
				    XtNuseRight, True,
				    NULL);
    info->patternbox = XtVaCreateManagedWidget("patternRack",
			            boxWidgetClass, info->vport2,
				    XtNforceBars, False,
			            NULL);

    /*
     *	Now construct the palette area
     */

    copyPaletteArea(info, pixels, patterns, npixels, npatterns);
    XtPopup(Global.patternshell, XtGrabNone);
    XtVaGetValues(Global.patternshell, XtNtitle, &ptr, NULL);
    StoreName(Global.patternshell, ptr);
   
    XtUnmanageChild(info->bar);
    if (info->cpick)
       XtUnmanageChild(info->cpick);
    XtUnmanageChild(grabPattern);
    XtUnmanageChild(selectPattern);
    XtUnmanageChild(asbrushPattern);
    XtUnmanageChild(info->r_arrow);
    XtUnmanageChild(info->g_arrow);
    XtUnmanageChild(info->form);   
    XtUnmanageChild(info->form1);
    XtUnmanageChild(info->form2);
    XtUnmanageChild(info->vport2);
    XtUnmanageChild(info->view1);
    XtUnmanageChild(info->view2);
    XtUnmanageChild(info->icon);
    XtUnmanageChild(cancel);

    XtVaGetValues(Global.patternshell, XtNbackground, &bgpix, NULL);
    if (!r_arrow_pix) {
        r_arrow_pix = XCreatePixmapFromBitmapData(dpy, 
             DefaultRootWindow(dpy), (char *)r_arrow_data,
             r_arrow_width, r_arrow_height, info->marked, bgpix,
             DefaultDepthOfScreen(XtScreen(Global.patternshell)));
    }
    if (!g_arrow_pix) {
        g_arrow_pix = XCreatePixmapFromBitmapData(dpy, 
             DefaultRootWindow(dpy), (char*) g_arrow_data,
             g_arrow_width, g_arrow_height, info->marked, bgpix, 
	     DefaultDepthOfScreen(XtScreen(Global.patternshell)));
    }
    XtVaSetValues(info->r_arrow, XtNbackgroundPixmap, r_arrow_pix, NULL);
    XtVaSetValues(info->g_arrow, XtNbackgroundPixmap, g_arrow_pix, NULL);

    info->iconpix = XCreatePixmap(dpy, DefaultRootWindow(dpy), 
                                   ICONWIDTH, ICONHEIGHT, depth);

    XMapWindow(dpy, XtWindow(info->bar));
    if (info->cpick)
       XMapWindow(dpy, XtWindow(info->cpick));
    XMapWindow(dpy, XtWindow(grabPattern));
    XMapWindow(dpy, XtWindow(selectPattern));
    XMapWindow(dpy, XtWindow(asbrushPattern));
    XMapWindow(dpy, XtWindow(info->form));   
    XMapWindow(dpy, XtWindow(info->form1));
    XMapWindow(dpy, XtWindow(info->form2));      
    XMapWindow(dpy, XtWindow(info->view1));
    XMapWindow(dpy, XtWindow(info->view2));
    XMapWindow(dpy, XtWindow(info->vport2));
    XMapWindow(dpy, XtWindow(info->icon));
    XMapWindow(dpy, XtWindow(cancel));   
    XMapWindow(dpy, XtWindow(info->r_arrow));
    XUnmapWindow(dpy, XtWindow(info->g_arrow));
    initViews(info);

    XtVaGetValues(info->paint, XtNlineWidth, &nargs, NULL);
    MenuCheckItem(lineMenu[(nargs/2<5)? (nargs/2) : LW_GENERAL].widget, True);
    StateSetBusyWatch(False);
    XtAddEventHandler(Global.patternshell, StructureNotifyMask, False,
		      (XtEventHandler) patternResized, (XtPointer) info);

    XtSetMinSizeHints(Global.patternshell, 210, 186);
}



