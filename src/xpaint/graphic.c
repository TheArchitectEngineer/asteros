/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)	       | */
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

/* $Id: graphic.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#include <X11/Intrinsic.h>
#include <X11/Xft/Xft.h>
#ifdef XAW3D
#include <X11/IntrinsicP.h>
#endif
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/xpm.h>

#include "xaw_incdir/Form.h"
#include "xaw_incdir/Viewport.h"
#ifdef XAW3D
#include "xaw_incdir/ViewportP.h"
#endif
#include "xaw_incdir/Box.h"
#include "xaw_incdir/Label.h"
#include "xaw_incdir/Toggle.h"
#include "xaw_incdir/SmeBSB.h"
#include "xaw_incdir/SmeLine.h"
#include "xaw_incdir/Paned.h"
#include "xaw_incdir/MenuButton.h"
#include "xaw_incdir/SimpleMenu.h"


#ifdef HAVE_COLTOG
#include "ColToggle.h"
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#endif

/*
 * Undefine this if you want the Lookup button to be
 * inside the palette area (not recommended)
 */
#define LOOKUP_OUTSIDE

#include "Paint.h"
#include "PaintP.h"
#include "xpaint.h"
#include "palette.h"
#include "misc.h"
#include "menu.h"
#include "messages.h"
#include "text.h"
#include "graphic.h"
#include "image.h"
#include "region.h"
#include "operation.h"
#include "rc.h"
#include "protocol.h"
#include "color.h"
#include "ops.h"
#include "crc32.h"

#include "rw/rwTable.h"

#include "bitmaps/xpm/eye.xpm"
#include "bitmaps/xpm/fill_b1.xpm"
#include "bitmaps/xpm/fill_b2.xpm"
#include "bitmaps/xpm/fill_b3.xpm"
#include "bitmaps/xpm/fill_r1.xpm"
#include "bitmaps/xpm/fill_r2.xpm"
#include "bitmaps/xpm/fill_r3.xpm"

#define POS_WIDTH 62
#define PALETTE_SIZE 30
#define CELL_WIDTH 12
#define MEMORY_HSTEPS 6
#define MEMSLOT_SIZE 6
#define LINESTYLE_HEIGHT 20
static int LINESTYLE_WIDTH = 180;

extern int file_isSpecialImage;
extern int file_transparent;
extern int file_numpages;

extern Boolean * setDrawable(Widget w, Drawable d, XImage * s);
extern int WritePNM(char *file, Image * image);
extern void fill(int x, int y, int width, int height);

#define NUMBER_LINEWIDTHS 5 /* 6 items in Line selector */

#define NUMBER_PREDEF_FONTS 12
char *fontNames[NUMBER_PREDEF_FONTS] = {
  "times-12",
  "times-18",
  "times-24",
  "times-18:bold",
  "times-18:italic",
  "times-18:bold:italic",
  "dejavu.sans.mono-12",
  "dejavu.sans.mono-18",
  "dejavu.sans.mono-24",
  "dejavu.sans.mono-18:bold",
  "dejavu.sans.mono-18:italic",
  "dejavu.sans.mono-18:bold:italic"
};

enum {W_ICON_PALETTE=0,
      W_ICON_COLORS,
      W_ICON_COLPIXMAP,
      W_ICON_TOOL,
      W_ICON_BRUSH,
      W_ICON_FONT,
      W_ICON_LINESTYLE,
      W_ICON_LINEPIXMAP,
      W_INFO_DATA,
      W_FILE_REVERT,
      W_EDIT_PASTE,
      W_FONT_WRITE,
      W_REGION_LAST, 
      W_REGION_UNDO, 
      W_SELECTOR_TRANSPARENT,
      W_SELECTOR_INTERPOLATION,
      W_SELECTOR_GRID,
      W_SELECTOR_SNAP,
      W_ALPHA_MODES,
      W_MEMORY_STACK = W_ALPHA_MODES+11,
      W_MEMORY_RECALL,
      W_MEMORY_DISCARD,
      W_MEMORY_SCROLL,
      W_MEMORY_ERASE,
      W_LINE_WIDTHS, 
      W_FONT_DESCR = W_LINE_WIDTHS + NUMBER_LINEWIDTHS + 1, 
      W_TOPMENU = W_FONT_DESCR + NUMBER_PREDEF_FONTS - W_FILE_REVERT + 1,
      W_NWIDGETS = 2*W_TOPMENU + W_FILE_REVERT + 1
};

int w_edit_paste = W_EDIT_PASTE;

static Pixmap ImgProcessPix;
static Colormap ImgProcessCmap;
static int ImgProcessFlag;

static char dashStyleStr[72] = "=";

/* forward references */
static void fontSet(Widget w, void *ptr);
static void StdCloneCanvasCallback(Widget w, XtPointer paintArg, XtPointer junk2);

/* Global data */
struct imageprocessinfo ImgProcessInfo =
{
    7,				/* oilArea		*/
    20,				/* noiseDelta		*/
    2,				/* spreadDistance	*/
    4,				/* pixelizeXSize	*/
    4,				/* pixelizeYSize	*/
    3,				/* despeckleMask	*/
    3,				/* smoothMaskSize	*/
    20,				/* tilt			*/
    50,				/* solarizeThreshold	*/
    99,				/* contrastW		*/
    2,				/* contrastB		*/
    0,				/* redshift		*/
    0,				/* greenshift		*/
    0,				/* blueshift		*/
    16,				/* quantizeColors	*/
    4.0,                        /* rescale_x            */
    4.0,                        /* rescale_y            */     
    1,                          /* interpolate          */
    0, 0, 0, 0,                 /* tilt values          */
    1.0, 0.0, 0.0, 1.0,         /* matrix for linear transform */
    NULL                        /* background color pointer */
};

struct paintWindows {
    Widget paint;
    struct paintWindows *next;
    void *ldata;
    Boolean done;
};

static struct paintWindows *head = NULL;

typedef struct {
    Widget shell, pane, form, bar, zoom, viewport, paint, paintbox;
    Widget palette, position, resize, memory, mempopup,
           colors, tool, brush, font, linestyle;
    Pixmap palette_pixmap, mem_pixmap;
    Palette *map;
    RCInfo *rcInfo;
    Pixel *pixels;
    Pixmap *patterns;
    Dimension barheight; /* height of menu bar */
    int npatterns, npixels;
    int boolcount, channel, vsteps, boolpos, mem_index;
    int resize_state, key_handler;
} LocalInfo;

static Boolean selectionOwner = False;

static Image *(*lastFilter) (Image *);

/*
 *  Begin menus
 */

static PaintMenuItem fileMenu[] =
{
#define FILE_OPEN 0
    MI_SIMPLE("open"),
#define FILE_SAVE 1
    MI_SIMPLE("save"),
#define FILE_SAVEAS 2
    MI_SIMPLE("saveas"),
#define FILE_SAVE_REGION 3
    MI_SIMPLE("saveregion"),
#define FILE_LOAD_MEMORY 4
    MI_SIMPLE("load-mem"),
#define FILE_REVERT 5
    MI_SIMPLE("revert"),
#define FILE_LOADED 6
    MI_SIMPLE("loaded"),
#define FILE_PRINT 7
    MI_SIMPLE("print"),
#define FILE_EXTERN 8
    MI_SIMPLE("extern"),
#define FILE_CLOSE 9
    MI_SIMPLE("close"),
};

static PaintMenuItem editMenu[] =
{
#define EDIT_UNDO	0
    MI_SIMPLE("undo"),
#define EDIT_REDO	1
    MI_SIMPLE("redo"),
#define EDIT_UNDO_SIZE	2
    MI_SIMPLE("undosize"),
    MI_SEPARATOR(),     /* 3 */
#define EDIT_REFRESH	4
    MI_SIMPLE("refresh"),
    MI_SEPARATOR(),     /* 5 */
#define EDIT_CUT	6
    MI_SIMPLE("cut"),
#define EDIT_COPY	7
    MI_SIMPLE("copy"),
#define EDIT_PASTE	8
    MI_SIMPLE("paste"),
#define EDIT_CLEAR	9
    MI_SIMPLE("clear"),
    MI_SEPARATOR(),     /* 10 */
#define EDIT_SELECT_ALL	11
    MI_SIMPLE("all"),
#define EDIT_UNSELECT	12
    MI_SIMPLE("unselect"),
    MI_SEPARATOR(),     /* 13 */   
#define EDIT_DUP	14
    MI_SIMPLE("dup"),
#define EDIT_ERASE_ALL	15
    MI_SIMPLE("erase"),
    MI_SEPARATOR(),     /* 16 */
#define EDIT_CLONE_CANVAS	17
    MI_SIMPLE("clone_canvas"),
#define EDIT_CLONE_CANVAS1	18
    MI_SIMPLE("clone_canvas1"),
    MI_SEPARATOR(),     /* 19 */
#define EDIT_SNAPSHOT	20
    MI_SIMPLE("screenshot"),
};

static PaintMenuItem lineMenu[] =
{
    MI_FLAGCB("1", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("2", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("4", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("6", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("8", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_SEPARATOR(),
    MI_FLAGCB("linestyle", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
};

static PaintMenuItem textMenu[] =
{
    MI_FLAGCB("times 12", MF_CHECK | MF_GROUP1, fontSet, 0),
    MI_FLAGCB("times 18", MF_CHECK | MF_GROUP1, fontSet, 1),
    MI_FLAGCB("times 24", MF_CHECK | MF_GROUP1, fontSet, 2),
    MI_FLAGCB("times 18 bold", MF_CHECK | MF_GROUP1, fontSet, 3),
    MI_FLAGCB("times 18 italic", MF_CHECK | MF_GROUP1, fontSet, 4),
    MI_FLAGCB("times 18 bold italic", MF_CHECK | MF_GROUP1, fontSet, 5),
    MI_FLAGCB("dejavu sans mono 12", MF_CHECK | MF_GROUP1, fontSet, 6),
    MI_FLAGCB("dejavu sans mono 18", MF_CHECK | MF_GROUP1, fontSet, 7),
    MI_FLAGCB("dejavu sans mono 24", MF_CHECK | MF_GROUP1, fontSet, 8),
    MI_FLAGCB("dejavu sans mono 18 bold", MF_CHECK | MF_GROUP1, fontSet, 9),
    MI_FLAGCB("dejavu sans mono 18 italic", MF_CHECK | MF_GROUP1, fontSet, 10),
    MI_FLAGCB("dejavu sans mono 18 bold italic", MF_CHECK | MF_GROUP1, fontSet, 11),
    MI_SEPARATOR(),
#define FONT_SELECT	13
    MI_SIMPLE("select"),
    MI_SEPARATOR(),
#define FONT_WRITE	15
    MI_SIMPLE("write"),
};

static PaintMenuItem rotateMenu[] =
{
    MI_SEPARATOR(),
    MI_SIMPLE("rotate1"),
    MI_SIMPLE("rotate2"),
    MI_SIMPLE("rotate3"),
    MI_SIMPLE("rotate4"),
    MI_SIMPLE("rotate5"),
};

static PaintMenuItem regionMenu[] =
{
#define REGION_FLIPX	0
    MI_SIMPLE("flipX"),
#define REGION_FLIPY	1
    MI_SIMPLE("flipY"),
#define REGION_ROTATETO	2
    MI_RIGHT("rotateTo", XtNumber(rotateMenu), rotateMenu),
#define REGION_ROTATE	3
    MI_SIMPLE("rotate"),
#define REGION_LINEAR	4
    MI_SIMPLE("linear"),
#define REGION_RESET	5
    MI_SIMPLE("reset"),
    MI_SEPARATOR(),	/* 6 */
#define REGION_EXPAND 7
    MI_SIMPLE("expand"),
#define REGION_DOWNSCALE 8
    MI_SIMPLE("downscale"),
    MI_SEPARATOR(),	/* 9 */
#define REGION_CLONE    10
    MI_SIMPLE("clone"),
#define REGION_CROP	11
    MI_SIMPLE("crop"),
    MI_SEPARATOR(),	/* 12 */
#define	REGION_AUTOCROP	13
    MI_SIMPLE("autocrop"),
#define REGION_DELIMIT	14
    MI_SIMPLE("delimit"),
#define REGION_COMPLEMENT	15
    MI_SIMPLE("complement"),
    MI_SEPARATOR(),	/* 16 */
#define REGION_OCR	17
    MI_SIMPLE("ocr"),
};

static PaintMenuItem filterMenu[] =
{
#define FILTER_INVERT	0
    MI_SIMPLE("invert"),
#define FILTER_SHARPEN	1
    MI_SIMPLE("sharpen"),
  /* remove noise */
    MI_SEPARATOR(),		/* 2 */
#define FILTER_SMOOTH	3
    MI_SIMPLE("smooth"),
#define FILTER_DIRFILT 4
    MI_SIMPLE("dirfilt"),
#define FILTER_DESPECKLE 5
    MI_SIMPLE("despeckle"),
    MI_SEPARATOR(),		/* 6 */
#define FILTER_EDGE	7
    MI_SIMPLE("edge"),
#define FILTER_EMBOSS	8
    MI_SIMPLE("emboss"),
#define FILTER_OIL	9
    MI_SIMPLE("oil"),
#define FILTER_NOISE	10
    MI_SIMPLE("noise"),
#define FILTER_SPREAD	11
    MI_SIMPLE("spread"),
#define FILTER_PIXELIZE  12
    MI_SIMPLE("pixelize"),
  /* special fx */
#define FILTER_TILT	13
    MI_SIMPLE("tilt"),
#define FILTER_BLEND	14
    MI_SIMPLE("blend"),
#define FILTER_SOLARIZE 15
    MI_SIMPLE("solarize"),
  /* colour manipulation */
    MI_SEPARATOR(),		/* 16 */
#define FILTER_TOGREY 17
    MI_SIMPLE("togrey"),
#define FILTER_TOMASK 18
    MI_SIMPLE("tomask"),
#define FILTER_CONTRAST 19
    MI_SIMPLE("contrast"),
#define FILTER_MODIFY_RGB 20
    MI_SIMPLE("modify_rgb"),
#define FILTER_QUANTIZE 21
    MI_SIMPLE("quantize"),
    MI_SEPARATOR(),		/* 22 */
#define FILTER_USERDEF 23
    MI_SIMPLE("userdef"),
    MI_SEPARATOR(),		/* 24 */
#define FILTER_LAST	25
    MI_SIMPLE("last"),
#define FILTER_UNDO	26
    MI_SIMPLE("undo"),
};

static PaintMenuItem selectorMenu[] =
{
#define	SELECTOR_FATBITS	0
    MI_SIMPLE("fatbits"),
#define	SELECTOR_PATTERNS	1
    MI_SIMPLE("patterns"),
#define	SELECTOR_CHROMA	2
    MI_SIMPLE("chroma"),
#define SELECTOR_BACKGROUND	3
    MI_SIMPLE("background"),
#define	SELECTOR_TOOLS		4
    MI_SIMPLE("tools"),
#define	SELECTOR_BRUSH		5
    MI_SIMPLE("brush"),
#define	SELECTOR_FONT		6
    MI_SIMPLE("font"),
#define	SELECTOR_LUPE		7
    MI_SIMPLE("magnifier"),
#define	SELECTOR_SCRIPT		8
    MI_SIMPLE("c_script"),
    MI_SEPARATOR(),             /* 9 */
#define	SELECTOR_CHANGE_SIZE	10
    MI_SIMPLE("size"),
#define	SELECTOR_CHANGE_ZOOM		11
    MI_SIMPLE("zoom"),
#define	SELECTOR_SIZE_ZOOM_DEFS	12
    MI_SIMPLE("size_zoom_defs"),
    MI_SEPARATOR(),             /* 13 */
#define	SELECTOR_TRANSPARENT  14
    MI_FLAG("transparent", MF_CHECK),
#define	SELECTOR_INTERPOLATION  15
    MI_FLAG("interpolation", MF_CHECK),
    MI_SEPARATOR(),             /* 16 */
#define	SELECTOR_SNAP		17
    MI_FLAG("snap", MF_CHECK),
#define	SELECTOR_SNAP_SPACING		18
    MI_SIMPLE("snap_spacing"),
    MI_SEPARATOR(),             /* 19 */
#define	SELECTOR_GRID		20
    MI_FLAG("grid", MF_CHECK),
#define	SELECTOR_GRID_PARAM		21
    MI_SIMPLE("grid_param"),
    MI_SEPARATOR(),             /* 22 */
#define	SELECTOR_SIMPLE		23
    MI_FLAG("simple", MF_CHECK),
#define	SELECTOR_HIDE_MENUBAR	24
    MI_FLAG("hide_menubar", MF_CHECK),
    MI_SEPARATOR(),		/* 25 */
#define	SELECTOR_HELP		26
    MI_SIMPLECB("help", HelpDialog, "introduction"),
};

#define ZOOMMENU_VALUES 25
static PaintMenuItem zoomMenu[ZOOMMENU_VALUES];

static void
alphaModeCallback(Widget w, XtPointer wlArg, XtPointer junk);

static PaintMenuItem alphaMenu[] =
{
#define ALPHA_MODES	0
    MI_FLAG("mode0", MF_CHECK | MF_GROUP5),
    MI_FLAG("mode1", MF_CHECK | MF_GROUP5),
    MI_FLAG("mode2", MF_CHECK | MF_GROUP5),
    MI_FLAG("mode3", MF_CHECK | MF_GROUP5),
    MI_SEPARATOR(),	/* 4 */
#define ALPHA_PARAMS	5
    MI_SIMPLE("params"),
    MI_SEPARATOR(),	/* 6 */
#define ALPHA_CREATE	7
    MI_SIMPLE("create"),
#define ALPHA_SAVE	8
    MI_SIMPLE("save"),
#define ALPHA_EDIT	9
    MI_SIMPLE("edit"),
#define ALPHA_DELETE 10
    MI_SIMPLE("delete"),
};

static PaintMenuBar menuBar[] =
{
    {None, "file", XtNumber(fileMenu), fileMenu},
    {None, "edit", XtNumber(editMenu), editMenu},
    {None, "line", XtNumber(lineMenu), lineMenu},
    {None, "text", XtNumber(textMenu), textMenu},
    {None, "region", XtNumber(regionMenu), regionMenu},
    {None, "filter", XtNumber(filterMenu), filterMenu},
    {None, "selector", XtNumber(selectorMenu), selectorMenu},
    {None, "zoom", 0, zoomMenu},
    {None, "alpha", XtNumber(alphaMenu), alphaMenu}
};

static PaintMenuItem memoryMenu[] =
{
#define	MEMORY_NOOP	0
    MI_SEPARATOR(),
#define	MEMORY_STACK	1
    MI_SIMPLE("stack"),
#define	MEMORY_RECALL	2
    MI_SIMPLE("recall"),
#define	MEMORY_DISCARD	3
    MI_SIMPLE("discard"),
#define	MEMORY_SCROLL	4
    MI_SIMPLE("scroll"),
    MI_SEPARATOR(),  /* 5 */
#define	MEMORY_ERASE	6
    MI_SIMPLE("erase")
};

static PaintMenuItem popupFileMenu[] =
{
    MI_SEPARATOR(),
#define P_FILE_OPEN 1
    MI_SIMPLE("open"),
#define P_FILE_SAVE 2
    MI_SIMPLE("save"),
#define P_FILE_SAVEAS 3
    MI_SIMPLE("saveas"),
#define P_FILE_SAVE_REGION 4
    MI_SIMPLE("saveregion"),
#define P_FILE_LOAD_MEMORY 5
    MI_SIMPLE("load-mem"),
#define P_FILE_REVERT 6
    MI_SIMPLE("revert"),
#define P_FILE_LOADED 7
    MI_SIMPLE("loaded"),
#define P_FILE_PRINT 8
    MI_SIMPLE("print"),
#define P_FILE_EXTERN 9
    MI_SIMPLE("extern"),
#define P_FILE_CLOSE 10
    MI_SIMPLE("close"),
};

static PaintMenuItem popupEditMenu[] =
{
    MI_SEPARATOR(),     /* 0 */
#define P_EDIT_UNDO	1
    MI_SIMPLE("undo"),
#define P_EDIT_REDO	2
    MI_SIMPLE("redo"),
#define P_EDIT_UNDO_SIZE	        3
    MI_SIMPLE("undosize"),
    MI_SEPARATOR(),             /* 4 */
#define P_EDIT_REFRESH	5
    MI_SIMPLE("refresh"),
    MI_SEPARATOR(),  /* 6 */
#define P_EDIT_CUT	7
    MI_SIMPLE("cut"),
#define P_EDIT_COPY	8
    MI_SIMPLE("copy"),
#define P_EDIT_PASTE	9
    MI_SIMPLE("paste"),
#define P_EDIT_CLEAR	10
    MI_SIMPLE("clear"),
    MI_SEPARATOR(),  /* 11 */
#define P_EDIT_SELECT_ALL 12
    MI_SIMPLE("all"),
#define P_EDIT_UNSELECT	13
    MI_SIMPLE("unselect"),   
    MI_SEPARATOR(),		/* 14 */
#define P_EDIT_DUP	15
    MI_SIMPLE("dup"),
#define P_EDIT_ERASE_ALL	16
    MI_SIMPLE("erase"),
    MI_SEPARATOR(),     /* 17 */
#define P_EDIT_CLONE_CANVAS	18
    MI_SIMPLE("clone_canvas"),
#define P_EDIT_CLONE_CANVAS1	19
    MI_SIMPLE("clone_canvas1"),
    MI_SEPARATOR(),  /* 20 */
#define P_EDIT_SNAPSHOT	21
    MI_SIMPLE("screenshot"),
};

static PaintMenuItem popupLineMenu[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("1", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("2", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("4", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("6", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_FLAGCB("8", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
    MI_SEPARATOR(),
    MI_FLAGCB("linestyle", MF_CHECK | MF_GROUP3, lineStyleCallback, NULL),
};

static PaintMenuItem popupTextMenu[] =
{
    MI_SEPARATOR(),
    MI_FLAGCB("times 12", MF_CHECK | MF_GROUP1, fontSet, 0),
    MI_FLAGCB("times 18", MF_CHECK | MF_GROUP1, fontSet, 1),
    MI_FLAGCB("times 24", MF_CHECK | MF_GROUP1, fontSet, 2),
    MI_FLAGCB("times 18 bold", MF_CHECK | MF_GROUP1, fontSet, 3),
    MI_FLAGCB("times 18 italic", MF_CHECK | MF_GROUP1, fontSet, 4),
    MI_FLAGCB("times 18 bold italic", MF_CHECK | MF_GROUP1, fontSet, 5),
    MI_FLAGCB("dejavu sans mono 12", MF_CHECK | MF_GROUP1, fontSet, 6),
    MI_FLAGCB("dejavu sans mono 18", MF_CHECK | MF_GROUP1, fontSet, 7),
    MI_FLAGCB("dejavu sans mono 24", MF_CHECK | MF_GROUP1, fontSet, 8),
    MI_FLAGCB("dejavu sans mono 18 bold", MF_CHECK | MF_GROUP1, fontSet, 9),
    MI_FLAGCB("dejavu sans mono 18 italic", MF_CHECK | MF_GROUP1, fontSet, 10),
    MI_FLAGCB("dejavu sans mono 18 bold italic", MF_CHECK | MF_GROUP1, fontSet, 11),
    MI_SEPARATOR(),
#define P_FONT_SELECT	14
    MI_SIMPLE("select"),
    MI_SEPARATOR(),
#define P_FONT_WRITE	16
    MI_SIMPLE("write"),
};

static PaintMenuItem popupRotateMenu[] =
{
    MI_SEPARATOR(),
    MI_SIMPLE("rotate1"),
    MI_SIMPLE("rotate2"),
    MI_SIMPLE("rotate3"),
    MI_SIMPLE("rotate4"),
    MI_SIMPLE("rotate5"),
};

static PaintMenuItem popupRegionMenu[] =
{
    MI_SEPARATOR(),	/* 0 */
#define P_REGION_FLIPX	1
    MI_SIMPLE("flipX"),
#define P_REGION_FLIPY	2
    MI_SIMPLE("flipY"),
#define P_REGION_ROTATETO	3
    MI_RIGHT("rotateTo", XtNumber(popupRotateMenu), popupRotateMenu),
#define P_REGION_ROTATE	4
    MI_SIMPLE("rotate"),
#define P_REGION_LINEAR	5
    MI_SIMPLE("linear"),
#define P_REGION_RESET	6
    MI_SIMPLE("reset"),
    MI_SEPARATOR(),		/* 7 */
#define P_REGION_EXPAND 8
    MI_SIMPLE("expand"),
#define P_REGION_DOWNSCALE 9
    MI_SIMPLE("downscale"),
    MI_SEPARATOR(),		/* 10 */
#define P_REGION_CLONE		11
    MI_SIMPLE("clone"),
#define P_REGION_CROP		12
    MI_SIMPLE("crop"),
    MI_SEPARATOR(),		/* 13 */
#define	P_REGION_AUTOCROP	14
    MI_SIMPLE("autocrop"),
#define P_REGION_DELIMIT	15
    MI_SIMPLE("delimit"),
#define P_REGION_COMPLEMENT	16
    MI_SIMPLE("complement"),
    MI_SEPARATOR(),		/* 17 */
#define P_REGION_OCR	18
    MI_SIMPLE("ocr"),
};

static PaintMenuItem popupFilterMenu[] =
{
    MI_SEPARATOR(),             /* 0 */
#define P_FILTER_INVERT	1
    MI_SIMPLE("invert"),
#define P_FILTER_SHARPEN	2
    MI_SIMPLE("sharpen"),
  /* remove noise */
    MI_SEPARATOR(),		/* 3 */
#define P_FILTER_SMOOTH	4
    MI_SIMPLE("smooth"),
#define P_FILTER_DIRFILT 5
    MI_SIMPLE("dirfilt"),
#define P_FILTER_DESPECKLE 6
    MI_SIMPLE("despeckle"),
    MI_SEPARATOR(),		/* 7 */
#define P_FILTER_EDGE	8
    MI_SIMPLE("edge"),
#define P_FILTER_EMBOSS	9
    MI_SIMPLE("emboss"),
#define P_FILTER_OIL	10
    MI_SIMPLE("oil"),
#define P_FILTER_NOISE	11
    MI_SIMPLE("noise"),
#define P_FILTER_SPREAD	12
    MI_SIMPLE("spread"),
#define P_FILTER_PIXELIZE  13
    MI_SIMPLE("pixelize"),
  /* special fx */
#define P_FILTER_TILT	14
    MI_SIMPLE("tilt"),
#define P_FILTER_BLEND	15
    MI_SIMPLE("blend"),
#define P_FILTER_SOLARIZE  16
    MI_SIMPLE("solarize"),
  /* color manipulation */
    MI_SEPARATOR(),		/* 17 */
#define P_FILTER_TOGREY   18
    MI_SIMPLE("togrey"),
#define P_FILTER_TOMASK   19
    MI_SIMPLE("tomask"),
#define P_FILTER_CONTRAST 20
    MI_SIMPLE("contrast"),
#define P_FILTER_MODIFY_RGB   21
    MI_SIMPLE("modify_rgb"),
#define P_FILTER_QUANTIZE 22
    MI_SIMPLE("quantize"),
    MI_SEPARATOR(),		/* 23 */
#define P_FILTER_USERDEF 24
    MI_SIMPLE("userdef"),
    MI_SEPARATOR(),		/* 25 */
#define P_FILTER_LAST	26
    MI_SIMPLE("last"),
#define P_FILTER_UNDO	27
    MI_SIMPLE("undo"),
};

static PaintMenuItem popupSelectorMenu[] =
{
    MI_SEPARATOR(),             /* 0 */
#define	P_SELECTOR_FATBITS	1
    MI_SIMPLE("fatbits"),
#define	P_SELECTOR_PATTERNS	2
    MI_SIMPLE("patterns"),
#define	P_SELECTOR_CHROMA	3
    MI_SIMPLE("chroma"),
#define P_SELECTOR_BACKGROUND	4
    MI_SIMPLE("background"),
#define	P_SELECTOR_TOOLS	5
    MI_SIMPLE("tools"),
#define	P_SELECTOR_BRUSH	6
    MI_SIMPLE("brush"),
#define	P_SELECTOR_FONT		7
    MI_SIMPLE("font"),
#define	P_SELECTOR_LUPE		8
    MI_SIMPLE("magnifier"),
#define	P_SELECTOR_SCRIPT	9
    MI_SIMPLE("c_script"),
    MI_SEPARATOR(),             /* 10 */
#define	P_SELECTOR_CHANGE_SIZE	11
    MI_SIMPLE("size"),
#define	P_SELECTOR_CHANGE_ZOOM	12
    MI_SIMPLE("zoom"),
#define	P_SELECTOR_SIZE_ZOOM_DEFS	13
    MI_SIMPLE("size_zoom_defs"),
    MI_SEPARATOR(),             /* 14 */
#define	P_SELECTOR_TRANSPARENT        15
    MI_FLAG("transparent", MF_CHECK),
#define	P_SELECTOR_INTERPOLATION        16
    MI_FLAG("interpolation", MF_CHECK),
    MI_SEPARATOR(),             /* 17 */
#define	P_SELECTOR_SNAP	18
    MI_FLAG("snap", MF_CHECK),
#define	P_SELECTOR_SNAP_SPACING	19
    MI_SIMPLE("snap_spacing"),
    MI_SEPARATOR(),             /* 20 */
#define	P_SELECTOR_GRID		21
    MI_FLAG("grid", MF_CHECK),
#define	P_SELECTOR_GRID_PARAM		22
    MI_SIMPLE("grid_param"),
    MI_SEPARATOR(),             /* 23 */
#define	P_SELECTOR_HIDE_MENUBAR	24
    MI_SIMPLE("hide_menubar"),
#define	P_SELECTOR_SHOW_MENUBAR	25
    MI_SIMPLE("show_menubar"),
    MI_SEPARATOR(),             /* 26 */
#define P_SELECTOR_HELP		27
    MI_SIMPLECB("help", HelpDialog, "introduction"),
};

static PaintMenuItem popupAlphaMenu[] =
{
    MI_SEPARATOR(),	/* 0 */
#define P_ALPHA_MODES 1
    MI_FLAG("mode0", MF_CHECK | MF_GROUP5),
    MI_FLAG("mode1", MF_CHECK | MF_GROUP5),
    MI_FLAG("mode2", MF_CHECK | MF_GROUP5),
    MI_FLAG("mode3", MF_CHECK | MF_GROUP5),
    MI_SEPARATOR(),	/* 5 */
#define P_ALPHA_PARAMS	6
    MI_SIMPLE("params"),
    MI_SEPARATOR(),	/* 7 */
#define P_ALPHA_CREATE	8
    MI_SIMPLE("create"),
#define P_ALPHA_SAVE	9
    MI_SIMPLE("save"),
#define P_ALPHA_EDIT	10
    MI_SIMPLE("edit"),
#define P_ALPHA_DELETE 11
    MI_SIMPLE("delete"),
};

static PaintMenuItem popupMemoryMenu[] =
{
    MI_SEPARATOR(),  /* 0 */
#define P_MEMORY_STACK	1
    MI_SIMPLE("stack"),
#define P_MEMORY_RECALL	2
    MI_SIMPLE("recall"),
#define P_MEMORY_DISCARD	3
    MI_SIMPLE("discard"),
    MI_SEPARATOR(),  /* 4 */
#define P_MEMORY_ERASE	5
    MI_SIMPLE("erase")
};

static PaintMenuItem popupMenu[] =
{     
    MI_SEPARATOR(),
    MI_RIGHT("File", XtNumber(popupFileMenu), popupFileMenu),
    MI_RIGHT("Edit", XtNumber(popupEditMenu), popupEditMenu),
    MI_RIGHT("Line", XtNumber(popupLineMenu), popupLineMenu),
    MI_RIGHT("Text", XtNumber(popupTextMenu), popupTextMenu),
    MI_RIGHT("Region", XtNumber(popupRegionMenu), popupRegionMenu),
    MI_RIGHT("Filters", XtNumber(popupFilterMenu), popupFilterMenu),
    MI_RIGHT("Selectors", XtNumber(popupSelectorMenu), popupSelectorMenu),
    MI_RIGHT("Alpha", XtNumber(popupAlphaMenu), popupAlphaMenu),
    MI_RIGHT("Memory", XtNumber(popupMemoryMenu), popupMemoryMenu)
};

/*
**  This really should be a "local" or malloced variable
 */
typedef struct {
    Pixmap pixmap;
    int count;
    int width, height, depth;
} selectInfo;

/*
 *  End of menus
 */

static void
prCallback(Widget paint, Widget w, Boolean flag)
{
    XtVaSetValues(w, XtNsensitive, flag, NULL);
}

void
GraphicRemove(Widget paint, XtPointer junk, XtPointer junk2)
{
    struct paintWindows *cur = head, **prev = &head;

    while (cur != NULL && cur->paint != paint) {
	prev = &cur->next;
	cur = cur->next;
    }

    if (cur == NULL)
	return;

    *prev = cur->next;

    if (cur->done)
	CurrentOp->remove(cur->paint, cur->ldata);
    XtFree((XtPointer) cur);
}

static void
realize(Widget paint, XtPointer ldataArg, XEvent * event, Boolean * junk)
{
    struct paintWindows *cur = (struct paintWindows *) ldataArg;

    if (event->type == MapNotify) {
	XtRemoveEventHandler(paint, StructureNotifyMask, False, realize, ldataArg);
	if (CurrentOp != NULL && CurrentOp->add != NULL)
	    cur->ldata = CurrentOp->add(paint);
	cur->done = True;
    }
}

void
GraphicAdd(Widget paint)
{
    struct paintWindows *new = XtNew(struct paintWindows);

    new->next = head;
    head = new;
    new->paint = paint;
    new->ldata = NULL;
    new->done = False;

    if (XtIsRealized(paint)) {
	if (CurrentOp != NULL && CurrentOp->add != NULL)
	    new->ldata = CurrentOp->add(paint);
	new->done = True;
    } else
	XtAddEventHandler(paint, StructureNotifyMask, False,
			  realize, (XtPointer) new);

    XtAddCallback(paint, XtNdestroyCallback, GraphicRemove, NULL);
}

Widget
GetNonDirtyCanvas()
{
    struct paintWindows *cur;
    PaintWidget pw;
    Widget paint = None;
    WidgetList wlist;
    Boolean flag;

    for (cur = head; cur != NULL; cur = cur->next) {
        pw = (PaintWidget) cur->paint;
        if (pw->paint.paint) continue; /* maybe fatbits ... */
        XtVaGetValues(cur->paint, XtNdirty,&flag, XtNmenuwidgets,&wlist, NULL);
	if (!flag && wlist) {
	    paint = cur->paint;
	    break;
	}
    }
    return paint;
}

void
GraphicAll(GraphicAllProc func, void *data)
{
    struct paintWindows *cur;

    for (cur = head; cur != NULL; cur = cur->next) {
	if (!cur->done)
	    continue;
	func(cur->paint, data);
    }
}

void
GraphicSetOp(void (*stop) (Widget, void *), void *(*start) (Widget))
{
    struct paintWindows *cur;

    for (cur = head; cur != NULL; cur = cur->next) {
	if (stop != NULL)
	    stop(cur->paint, cur->ldata);
	if (start != NULL)
	    cur->ldata = start(cur->paint);
    }
}

void *
GraphicGetData(Widget w)
{
    struct paintWindows *cur;

    for (cur = head; cur != NULL; cur = cur->next)
	if (cur->paint == w)
	    return cur->ldata;
    return NULL;
}

/*
 *  First level menu callbacks.
 */

static void
closeOkCallback(Widget shell, XtPointer paintArg, XtPointer junk2)
{
    LocalInfo * info = (LocalInfo *) paintArg;
    Display *dpy = XtDisplay(info->paint);
    Pixmap pix;
    WidgetList wlist;
    int i, rule;

    PopdownMenusGlobal();
    if (info->palette_pixmap) XFreePixmap(dpy, info->palette_pixmap);
    if (info->mem_pixmap) XFreePixmap(dpy, info->mem_pixmap);
    for (i=0; i<info->npatterns; i++) XFreePixmap(dpy, info->patterns[i]);
    free(info->pixels);
    free(info->patterns);
    XtVaGetValues(info->paint, XtNmenuwidgets, &wlist, NULL);
    if (wlist) {
        if (wlist[W_ICON_COLPIXMAP]) {
	    XFreePixmap(dpy, (Pixmap) wlist[W_ICON_COLPIXMAP]);
        wlist[W_ICON_COLPIXMAP] = 0;
	}
        if (wlist[W_ICON_LINEPIXMAP]) {
	    XFreePixmap(dpy, (Pixmap) wlist[W_ICON_LINEPIXMAP]);
        wlist[W_ICON_LINEPIXMAP] = 0;
	}
    }
    XtVaGetValues(info->paint, 
		  XtNfillRule, &rule, XtNpattern, &pix, NULL);
    if (rule==FillTiled && pix) XFreePixmap(dpy, pix);
    XtVaGetValues(info->paint, 
		  XtNlineFillRule, &rule, XtNlinePattern, &pix, NULL);
    if (rule==FillTiled && pix) XFreePixmap(dpy, pix);
    checkPatternLink(info->paint, 0);
    checkExternalLink(info->paint);
    if (Global.canvas == shell) {
	XtVaSetValues(Global.back, XtNsensitive, False, NULL);
        Global.canvas = None;
    }
    XtDestroyWidget(shell);
}

static void
genericCancelCallback(Widget shell, XtPointer junk, XtPointer junk2)
{
}

static void
closeCallback(Widget w, XtPointer wlArg, XtPointer junk2)
{
    LocalInfo *info = (LocalInfo *) wlArg;
    Widget shell = GetShell(info->paint);
    Boolean flag;
    XtVaGetValues(info->paint, XtNdirty, &flag, NULL);

    if (flag)
	AlertBox(shell, msgText[UNSAVED_CHANGES_WISH_TO_CLOSE],
		 closeOkCallback, genericCancelCallback, info);
    else
        closeOkCallback(shell, info, NULL);
}

/*
 * Reload the image from the last saved file.
 */

/*
 * We need to use a work procedure, otherwise X gets confused.
 * Simplify a bit by assuming that there will never be more than
 * one revert process going on at any one time.
 */

#if 0 /* remove this clumsy stuff !! */

static XtWorkProcId workProcId;
static int workProcDone;

static Boolean
workProc(XtPointer w)
{
    if (workProcDone)
	return True;

    /* this is a kluge, but it is necessary */
    workProcDone = 1;

    XtRemoveWorkProc(workProcId);

    XtDestroyWidget(GetShell((Widget) w));
    return True;
}
#endif

static void
revertOkCallback(Widget shell, XtPointer arg, XtPointer junk2)
{
    Widget paint = (Widget) arg;
    char *file;
    XtVaGetValues(paint, XtNfilename, &file, NULL);
    loadPrescribedFile(paint, file);
}

static void
revertCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    Boolean flag;

    XtVaGetValues(paint, XtNdirty, &flag, NULL);
    if (flag)
	AlertBox(GetShell(paint), msgText[UNSAVED_CHANGES_WISH_TO_REVERT],
		 revertOkCallback, genericCancelCallback, paint);
}

static void
loadedCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    GetFileName(Global.toplevel, BROWSER_LOADED, NULL, GraphicOpenFile, NULL);
}

static void
printCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PrintPopup(GetToplevel(w), paintArg);
}

static void
externCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ExternPopup(GetToplevel(w), paintArg);
}

/*
 * Pop up the fatbits window.
 */
static void
fatCallback(Widget w, XtPointer paint, XtPointer junk2)
{
    FatbitsEdit((Widget) paint);
}

void RefreshWidget(Widget w)
{
    XtUnmapWidget(w);
    XtMapWidget(w);
    XFlush(XtDisplay(w));
}

void RedrawPaintWidget(Widget w)
{
    XRectangle rect;
    PaintWidget pw = (PaintWidget) w;
    rect.x = 0;
    rect.y = 0;
    rect.width = pw->paint.drawWidth;
    rect.height = pw->paint.drawHeight;
    zoomUpdate(pw, True, &rect);
}

void RefreshPaintWidget(Widget w)
{
    RefreshWidget(w);
    RedrawPaintWidget(w);
}

void 
StdRefreshCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    RefreshPaintWidget((Widget) paintArg);
}

/*
 * Toggle the 'transparent' and 'interpolation' menu item.
 */

static void
transparencyCallback(Widget w, XtPointer paintArg, XtPointer junk)
{
    Widget paint = (Widget) paintArg;
    WidgetList wlist;
    int v;

    Global.transparent = !Global.transparent;
    v = (Global.transparent)?1:2;
    XtVaSetValues(paint, XtNtransparent, v, NULL);
    MenuCheckItem(w, Global.transparent);
    RefreshPaintWidget((Widget) paintArg);
    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    MenuCheckItem(wlist[W_SELECTOR_TRANSPARENT], Global.transparent);
    MenuCheckItem(wlist[W_TOPMENU+W_SELECTOR_TRANSPARENT], Global.transparent);
}

static void
interpolationCallback(Widget w, XtPointer paintArg, XtPointer junk)
{
    Widget paint = (Widget) paintArg;
    WidgetList wlist;

    int v;
    XtVaGetValues(paint, XtNinterpolation, &v, NULL);
    v = !v;
    XtVaSetValues(paint, XtNinterpolation, v, NULL);
    MenuCheckItem(w, v);

    RefreshPaintWidget((Widget) paintArg);

    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    MenuCheckItem(wlist[W_SELECTOR_INTERPOLATION], v);
    MenuCheckItem(wlist[W_TOPMENU+W_SELECTOR_INTERPOLATION], v);
}

/*
 * Toggle the 'grid' menu item.
 */

static void 
gridParamOkCallback(Widget w, XtPointer paintArg, XtPointer infoArg)
{
    PaintWidget pw = (PaintWidget) w;
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    Display *dpy = XtDisplay(w);
    Colormap cmap = None;
    int gridmode;
    Pixel gridcolor = 0;
    XColor color;

    gridmode = (atoi(info->prompts[0].rstr)&3)|
               ((atoi(info->prompts[1].rstr)>0)<<31);
    XtVaSetValues(w, XtNgridmode, gridmode, NULL);

    if (DefaultDepthOfScreen(XtScreen(w)) > 8) {
        cmap = DefaultColormapOfScreen(XtScreen(w));
        if (XAllocNamedColor(dpy, cmap, info->prompts[2].rstr,
                             &color, &color)!=0)
            gridcolor = color.pixel;
        XtVaSetValues(w, XtNgridcolor, gridcolor, NULL);
    }
    RefreshPaintWidget((Widget)pw);
}

static void 
gridParamCallback(Widget w, XtPointer paintArg, XtPointer junk)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[3];
    static char buf[10];
    char *num[4] = { "0", "1", "2", "3" };
    int gridmode;
    Display *dpy = XtDisplay(paint);
    Colormap cmap = None;
    Pixel gridcolor;
    XColor color;

    XtVaGetValues(paint, 
                  XtNgridmode, &gridmode, XtNgridcolor, &gridcolor, NULL);

    info.prompts = value;
    info.title = msgText[GRID_DRAWING_MODE];
    info.nprompt = 3;
    value[0].prompt = msgText[GRID_LINES_OR_OTHER];
    value[0].str = num[gridmode&3];
    value[1].prompt = msgText[GRID_USES_SNAP];
    value[1].str = num[(gridmode>>31)&1];
    value[2].prompt = msgText[GRID_COLOR];

    if (DefaultDepthOfScreen(XtScreen(paint)) > 8) {
        cmap = DefaultColormapOfScreen(XtScreen(paint));
        XtVaGetValues(GetShell(paint), XtNcolormap, &cmap, NULL);
        color.pixel = gridcolor;
        XQueryColor(dpy, cmap, &color);
        color.red = color.green = color.blue = 0;
        sprintf(buf, "#%02x%02x%02x", 
                color.red>>8, color.green>>8, color.blue>>8);
    } else
        sprintf(buf, "#000000");
    value[2].str = buf;
    value[0].len = 4;
    value[1].len = 4;
    value[2].len = 10;

    TextPrompt(paint, "gridselect", &info, gridParamOkCallback, NULL, NULL);
}

static void 
setGridMenu(Widget paint, void *ptr)
{
    WidgetList wlist;
    Boolean v;
    v = (ptr)? True : False;
    XtVaSetValues(paint, XtNgrid, v, NULL);
    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    MenuCheckItem(wlist[W_SELECTOR_GRID], v);
    MenuCheckItem(wlist[W_TOPMENU+W_SELECTOR_GRID], v);
    RefreshPaintWidget(paint);
}

static void
gridCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    Boolean v;

    XtVaGetValues(paint, XtNgrid, &v, NULL);
    v = !v;
    setGridMenu(paint, (void *)((long) v));
}

/*
**  The set line width callback pair
 */
static char currentLineWidth[10] = "1";

static void 
setLineWidth(Widget w, void *width)
{
    int i = (int)(long)width;
    XtVaSetValues(w, XtNlineWidth, i, NULL);
    if (Global.patternshell) {
        setPatternLineWidth(Global.patterninfo, i);
        checkPatternLink(w, 1);
    } else
      setCanvasColorsIcon(w, NULL);
    if (i==0) i = 1;
    sprintf(currentLineWidth, "%d", i);
}

static void 
setLineWidthMenu(Widget w, void *ptr)
{
    WidgetList wlist;
    XtVaGetValues(w, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    MenuCheckItem(wlist[W_LINE_WIDTHS+(long)ptr], True);
    MenuCheckItem(wlist[W_TOPMENU+W_LINE_WIDTHS+(long)ptr], True);
}

void
setZoomButtonLabel(Widget paint, int zoom)
{
    WidgetList wlist;
    LocalInfo *info;
    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    if (!wlist[W_ICON_TOOL]) return;
    info = (LocalInfo *)wlist[W_INFO_DATA];
    if (!info) return;  
    XtVaSetValues(info->zoom, XtNlabel, ZoomToStr(zoom), NULL);
    XtVaSetValues(info->zoom, XtNwidth, 21, XtNheight, 22, NULL);
}

void 
setToolIconPixmap(Widget paint, void *ptr) 
{
    WidgetList wlist;
    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    if (!wlist[W_ICON_TOOL]) return;
    setToolIconOnWidget(wlist[W_ICON_TOOL]);
    XtVaSetValues(wlist[W_ICON_TOOL],
                  XtNwidth, ICONWIDTH, XtNheight, ICONHEIGHT, NULL);
}

void
setCanvasColorsIcon(Widget paint, void *data)
{
    WidgetList wlist;
    static int *x0 = NULL, *x1;
    Pixel bg, fg, lfg;
    Pixmap pix, lpix, colpixmap, linepixmap;
    Display *dpy;
    GC gc;
    int v_fr, v_lfr, v_lw, x, y, width, height, depth;

    if (!paint) return;
    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist || !wlist[W_ICON_COLORS]) return;
    colpixmap = (Pixmap) wlist[W_ICON_COLPIXMAP];
    if (!colpixmap) return;
    dpy = XtDisplay(paint);
    gc = XtGetGC(paint, 0, 0);    
    depth = DefaultDepthOfScreen(XtScreen(paint));

    XtVaGetValues(paint, XtNbackground, &bg, NULL);
    XtVaGetValues(paint, XtNfillRule, &v_fr, NULL);
    XtVaGetValues(paint, XtNforeground, &fg, NULL);
    XtVaGetValues(paint, XtNpattern, &pix, NULL);
    XtVaGetValues(paint, XtNlineFillRule, &v_lfr, NULL);
    XtVaGetValues(paint, XtNlineForeground, &lfg, NULL);
    XtVaGetValues(paint, XtNlinePattern, &lpix, NULL);
    XtVaGetValues(paint, XtNlineWidth, &v_lw, NULL);

    if (v_lw>10) v_lw = 10;

    if (!x0) {
        x0 = (int *) xmalloc(ICONHEIGHT*sizeof(int));
        x1 = (int *) xmalloc(ICONHEIGHT*sizeof(int));
	for (y=0; y<ICONHEIGHT; y++) {
	    if (y<4 || y>ICONHEIGHT-5) {
                x0[y] = ICONWIDTH;
	    } else
                x0[y] = 11+abs(y-ICONHEIGHT/2)/3;
                x1[y] = ICONWIDTH-3-abs(y-ICONHEIGHT/2)/3;
	}
    }
    for (y=0; y<ICONHEIGHT; y++) {
        XSetForeground(dpy, gc, bg);
	if (y<4 || y>ICONHEIGHT-5) {
            for (x=0; x<x0[y]; x++)
                XDrawPoint(dpy, colpixmap, gc, x, y);
	    continue;
	}
        for (x=0; x<=2; x++)
                XDrawPoint(dpy, colpixmap, gc, x, y);
        for (x=10; x<x0[y]; x++)
                XDrawPoint(dpy, colpixmap, gc, x, y);

        for (x=x1[y]+1; x<ICONWIDTH; x++)
            XDrawPoint(dpy, colpixmap, gc, x, y);

	if (v_fr == FillSolid) {
	    XSetForeground(dpy, gc, fg);
            for (x=3; x<=9; x++)
                XDrawPoint(dpy, colpixmap, gc, x, y);
	} else if (pix) {
            GetPixmapWHD(dpy, pix, &width, &height, &depth);
            for (x=3; x<=9; x++)
	        XCopyArea(dpy, pix, colpixmap, gc,
		          x%width, y%height, 1, 1, x, y);
	}

	if (y<=4+v_lw || y>=ICONHEIGHT-v_lw-5) {
	    if (v_fr == FillSolid) {
	        XSetForeground(dpy, gc, fg);
                for (x=x0[y]; x<=x1[y]; x++)
                    XDrawPoint(dpy, colpixmap, gc, x, y);
	    } else {
                GetPixmapWHD(dpy, pix, &width, &height, &depth);
                for (x=x0[y]; x<=x1[y]; x++)
	            XCopyArea(dpy, pix, colpixmap, gc,
		              x%width, y%height, 1, 1, x, y);
	    }
            continue;
	}
	if (v_fr == FillSolid) {
            XSetForeground(dpy, gc, fg);
            for (x=x0[y]; x<=x0[y]+v_lw; x++)
                XDrawPoint(dpy, colpixmap, gc, x, y);
            for (x=x1[y]-v_lw; x<=x1[y]; x++)
                XDrawPoint(dpy, colpixmap, gc, x, y);
	} else {
            GetPixmapWHD(dpy, pix, &width, &height, &depth);
            for (x=x0[y]; x<=x0[y]+v_lw; x++)
	        XCopyArea(dpy, pix, colpixmap, gc,
		          x%width, y%height, 1, 1, x, y);
            for (x=x1[y]-v_lw; x<=x1[y]; x++)
	        XCopyArea(dpy, pix, colpixmap, gc,
		          x%width, y%height, 1, 1, x, y);
	}
	if (v_lfr == FillSolid) {
            XSetForeground(dpy, gc, lfg);
            for (x=x0[y]+v_lw+1; x<=x1[y]-v_lw-1; x++)
                XDrawPoint(dpy, colpixmap, gc, x, y);
	} else if (lpix) {
            GetPixmapWHD(dpy, lpix, &width, &height, &depth);
            for (x=x0[y]+v_lw+1; x<=x1[y]-v_lw-1; x++)
	        XCopyArea(dpy, lpix, colpixmap, gc,
		          x%width, y%height, 1, 1, x, y);
	}
    }

    /* Trick to force refresh of background Pixmap,
       although pointer value colpixmap didn't change ... */
    XtVaSetValues(wlist[W_ICON_COLORS],
                  XtNbackgroundPixmap, XtUnspecifiedPixmap, NULL);
    XtVaSetValues(wlist[W_ICON_COLORS],
                  XtNbackgroundPixmap, colpixmap, NULL);

    XtReleaseGC(paint, gc);

    if (!wlist[W_ICON_LINESTYLE]) return;
    linepixmap = (Pixmap) wlist[W_ICON_LINEPIXMAP];
    if (linepixmap) {
        gc = XCreateGC(dpy, XtWindow(paint), 0, 0);
        XSetForeground(dpy, gc, bg);
        XFillRectangle(dpy, linepixmap, gc, 0, 0, LINESTYLE_WIDTH, LINESTYLE_HEIGHT);
        if (v_fr == FillSolid)
	    XSetForeground(dpy, gc, fg);
        else {
	    XSetFillStyle(dpy, gc, v_fr);
	    XSetTile(dpy, gc, pix);
	}
        SetCapAndJoin(paint, gc, CapButt, JoinBevel);
        y = LINESTYLE_HEIGHT/2;
        XDrawLine(dpy, linepixmap, gc, 10, y, LINESTYLE_WIDTH-10, y);
        XtVaSetValues(wlist[W_ICON_LINESTYLE],
                      XtNbackgroundPixmap, linepixmap, NULL);
        XCopyArea(dpy, linepixmap, XtWindow(wlist[W_ICON_LINESTYLE]), gc,
                  0, 0, LINESTYLE_WIDTH, LINESTYLE_HEIGHT, 0, 0);
        XFreeGC(dpy, gc);
    }
}

void 
setBrushIconPixmap(Widget paint, void *ptr) 
{
    WidgetList wlist;
    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    if (!wlist[W_ICON_BRUSH]) return;
    setBrushIconOnWidget(wlist[W_ICON_BRUSH]);
    XtResizeWidget(wlist[W_ICON_BRUSH], ICONWIDTH, ICONHEIGHT,     
#ifdef XAWPLAIN
    1);
#else
    0);
#endif
}

extern void LoadRCInfo(RCInfo *rcInfo, Palette *map);

static void 
setPalettePixmap(Widget paint, LocalInfo *info, int mode)
{
    Display *dpy = XtDisplay(paint);
    Pixel black = BlackPixelOfScreen(XtScreen(paint)),
          white = WhitePixelOfScreen(XtScreen(paint));
    static char ** fill_xpm[7] = { 
           eye_xpm, 
           fill_b1_xpm, fill_b2_xpm, fill_b3_xpm,
           fill_r1_xpm, fill_r2_xpm, fill_r3_xpm};
    static Pixmap fill_pix[7];
    GC gc;
    int i, j, k, l, x, y;
    int width, height, depth;
    XpmAttributes attributes;
    WidgetList wlist;

    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    if (!wlist[W_ICON_PALETTE]) return;

    if (!info->rcInfo) {
        info->rcInfo = ReadDefaultRC();
        LoadRCInfo(info->rcInfo, info->map);
        info->npixels = 0;	
        for (i=0; i<info->rcInfo->ncolors; i++)
            if (info->rcInfo->colorFlags[i]) ++info->npixels;
        info->pixels = (Pixel *)xmalloc(info->npixels * sizeof(Pixel));
	j = 0;
        for (i=0; i<info->rcInfo->ncolors; i++) {
            if (info->rcInfo->colorFlags[i]) 
	        info->pixels[j++] = info->rcInfo->colorPixels[i];
	}
	info->npatterns = info->rcInfo->nimages;
	info->patterns = (Pixmap *) 
            xmalloc(info->npatterns * sizeof(Pixmap));
	for (i=0; i<info->npatterns; i++) {
	    info->patterns[i] = None;
            info->rcInfo->images[i]->refCount++;
	    ImageToPixmapCmap(info->rcInfo->images[i], 
			      info->paint, &info->patterns[i], 
                              info->map->cmap);
	}
    }

    if (!info->palette_pixmap) {
        info->palette_pixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
	                PALETTE_SIZE*CELL_WIDTH+32, CELL_WIDTH*info->vsteps-1, 
			DefaultDepthOfScreen(XtScreen(paint)));
	attributes.valuemask = XpmCloseness;
        attributes.closeness =  40000;
        for (k=0; k<=6; k++)
            XpmCreatePixmapFromData(dpy, DefaultRootWindow(dpy),
                                fill_xpm[k], &fill_pix[k], NULL, &attributes);
    }

    gc = XCreateGC(dpy, XtWindow(paint), 0, 0);
    for (k=0; k<=3; k++) {
        XCopyArea(dpy, fill_pix[k+((k==info->channel)?3:0)], 
                  info->palette_pixmap, gc, 0, 0, 16, 17, 
                  16*(k/2), 17*(k&1));
    }
    if (mode) goto finish;

    for (k=0; k<=1; k++)
    for (y = 32; y<CELL_WIDTH*info->vsteps-1; y++)
        XCopyArea(dpy, fill_pix[0], info->palette_pixmap, gc, 
		  0, 0, 16, 1, 16*k, y);

    XSetForeground(dpy, gc, white);
    for (x = 33; x<PALETTE_SIZE*CELL_WIDTH+32; x++)
    for (y = 0; y<CELL_WIDTH*info->vsteps-1; y++)
        XDrawPoint(dpy, info->palette_pixmap, gc, x, y);

    i = -1;
    for (x = 32; x<PALETTE_SIZE*CELL_WIDTH+32; x++) {
        if ((x-32)%CELL_WIDTH == 0) {
	    XSetForeground(dpy, gc, black);
	    for (y = 0; y<CELL_WIDTH*info->vsteps-1; y++) 
	        XDrawPoint(dpy, info->palette_pixmap, gc, x, y);
	    for (k = 1; k<info->vsteps; k++) 
	    for (l = 0; l<CELL_WIDTH; l++) 
	        XDrawPoint(dpy, info->palette_pixmap, gc, x+l, CELL_WIDTH*k-1);
	    ++x;
            ++i;
	}
        for (k = 0; k<info->vsteps; k++) {
	    j = i+k*PALETTE_SIZE;
	    if (j<info->npixels) {
                XSetForeground(dpy, gc, info->pixels[j]);
	        for (y = 0; y<=CELL_WIDTH-2; y++)
                    XDrawPoint(dpy, info->palette_pixmap, gc, x, y+CELL_WIDTH*k);
	    } else {
	        /* number of rows of pixels */
	        y = 1 + (info->npixels-1)/PALETTE_SIZE; 
                if (info->npatterns>(info->vsteps-y)*PALETTE_SIZE)
		    j -= info->npixels;
                else {
	            j -= y*PALETTE_SIZE;
		}
		if (j>=0 && j<info->npatterns) {
		    GetPixmapWHD(dpy, info->patterns[j], 
                                      &width, &height, &depth);
		    for (y = 0; y<=CELL_WIDTH-2; y++) {
                        XCopyArea(dpy, info->patterns[j], 
                            info->palette_pixmap, gc, 
                            ((x-32)%CELL_WIDTH)%width, y%height, 
                            1, 1, x, y+CELL_WIDTH*k);
		    }
		}
	    }
	}
    }

 finish:
    XtVaSetValues(wlist[W_ICON_PALETTE], 
                      XtNbackgroundPixmap, XtUnspecifiedPixmap, NULL);
    XtVaSetValues(wlist[W_ICON_PALETTE], 
                      XtNbackgroundPixmap, info->palette_pixmap, NULL);
    XtReleaseGC(paint, gc);
}

void 
AddItemToCanvasPalette(Widget paint, Pixel p, Pixmap pix)
{
    Display *dpy = XtDisplay(paint);
    LocalInfo *info;
    WidgetList wlist;
    int i;

    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    info = (LocalInfo *)wlist[W_INFO_DATA];
    if (!info) return;
    
    if (p != None) {
         i = info->npixels;
         info->npixels = i+1;
	 info->pixels = realloc(info->pixels, info->npixels * sizeof(Pixel));
	 info->pixels[i] = p;
    }
    if (pix != None) {
         i = info->npatterns;
         info->npatterns = i+1;
	 info->patterns = 
               realloc(info->patterns, info->npixels * sizeof(Pixel));
         info->patterns[i] = dupPixmap(dpy, pix);
    }

    setPalettePixmap(paint, info, 0);
}

static void
paletteHandler(Widget w, LocalInfo * info, XEvent * event, Boolean * flag)
{
    Display *dpy = XtDisplay(w);
    Pixmap pix;
    Pixel p;
    int i, j, rule;

    if (!event) return;

    if (event->type == ButtonRelease) {
        if (Global.popped_up) {
           PopdownMenusGlobal();
           event->type = None;
           return;
        }
        Global.popped_parent = None;

        if (event->xbutton.x>=0 && event->xbutton.x<32 && event->xbutton.y<30) {
	    i = 2*(event->xbutton.x/16) + event->xbutton.y/17;
            if (i>0) {
	        info->channel = i;
                setPalettePixmap(info->paint, info, 1);
	    } else {
	        DoGrabPixel(info->paint, &p, &info->map->cmap);
		j = 1;
		for (i=0; i<info->npixels; i++)
		    if (p == info->pixels[i]) {
		        j = 0;
			break;
		    }
		if (j) {
		    i = info->npixels;
                    info->npixels = i+1;
                    info->pixels = 
                    realloc(info->pixels, info->npixels * sizeof(Pixel));
                    info->pixels[i] = p;
                    setPalettePixmap(info->paint, info, 0);
		}
	        if (info->channel & 1) {
		    XtVaGetValues(info->paint, 
		       XtNfillRule, &rule, XtNpattern, &pix, NULL);
                    XtVaSetValues(info->paint, 
	                      XtNfillRule, FillSolid,
                              XtNforeground, p, NULL);
		    if (rule==FillTiled && pix) XFreePixmap(dpy, pix);
		}
                if (info->channel & 2) {
		    XtVaGetValues(info->paint, 
		       XtNlineFillRule, &rule, XtNlinePattern, &pix, NULL);
                    XtVaSetValues(info->paint, 
			      XtNlineFillRule, FillSolid,
			      XtNlineForeground, p, NULL);
		    if (rule==FillTiled && pix) XFreePixmap(dpy, pix);
		}
                setCanvasColorsIcon(info->paint, NULL);
	    }
        } else
	if (event->xbutton.x>=32) {
            i = info->vsteps*((event->xbutton.x-32)/CELL_WIDTH) + 
                event->xbutton.y/CELL_WIDTH;
            i = (event->xbutton.x-32)/CELL_WIDTH +
	        (event->xbutton.y/CELL_WIDTH)*PALETTE_SIZE;
	    if (i<info->npixels) {
	        p = info->pixels[i];
	        setcolor:
	        if (info->channel & 1) {
		    XtVaGetValues(info->paint, 
		       XtNfillRule, &rule, XtNpattern, &pix, NULL);
                    XtVaSetValues(info->paint, 
			 	  XtNfillRule, FillSolid,
                                  XtNforeground, p, 
				  NULL);
		    if (rule==FillTiled && pix) XFreePixmap(dpy, pix);
		}
	        if (info->channel & 2) {
		    XtVaGetValues(info->paint, 
		       XtNlineFillRule, &rule, XtNlinePattern, &pix, NULL);
                    XtVaSetValues(info->paint, 
				  XtNlineFillRule, FillSolid,
                                  XtNlineForeground, p,
				  NULL);
		    if (rule==FillTiled && pix) XFreePixmap(dpy, pix);
		}
	    } else {
	        j = 1 + (info->npixels-1)/PALETTE_SIZE; 
                if (info->npatterns>(info->vsteps-j)*PALETTE_SIZE)
		    i -= info->npixels;
                else {
	            i -= j*PALETTE_SIZE;
		}
                if (i<0 || i>=info->npatterns) {
		   p = WhitePixelOfScreen(XtScreen(info->paint));
                   goto setcolor;
		} else
	        if (info->channel&1) {
		   XtVaGetValues(info->paint, 
                       XtNfillRule, &rule, XtNpattern, &pix, NULL);
		   XtVaSetValues(info->paint, 
				 XtNfillRule, FillTiled,
                                 XtNpattern,
				 dupPixmap(dpy,info->patterns[i]), 
                                 NULL);
		   if (rule==FillTiled && pix) XFreePixmap(dpy, pix);
		}
	        if (info->channel&2) {
		   XtVaGetValues(info->paint, 
                       XtNlineFillRule, &rule, XtNlinePattern, &pix, NULL);
		   XtVaSetValues(info->paint, 
				 XtNlineFillRule, FillTiled,
                                 XtNlinePattern, 
				 dupPixmap(dpy,info->patterns[i]), 
                                 NULL);
		   if (rule==FillTiled && pix) XFreePixmap(dpy, pix);
		}
	    }
            setCanvasColorsIcon(info->paint, NULL);
	}
    }
}

void
setMemoryIcon(Widget w, LocalInfo * info)
{
    Display *dpy = XtDisplay(w);
    WidgetList wlist;
    Widget paint;
    Pixel pixel, black, white;
    GC gc;
    int i, j, k, l;
    int memw, memh, width, vsteps;
    char buf[40];

    if (!info) {
       XtVaGetValues(w, XtNmenuwidgets, &wlist, NULL);
       if (!wlist) return;
       info = (LocalInfo *)wlist[W_INFO_DATA];
    }
    if (!info) return;
    paint = info->paint;
    black = BlackPixelOfScreen(XtScreen(paint));
    white = WhitePixelOfScreen(XtScreen(paint));
    gc = XCreateGC(dpy, XtWindow(paint), 0, 0);
    width = 2+MEMSLOT_SIZE;
    memw = width*MEMORY_HSTEPS+3;
    memh = CELL_WIDTH*info->vsteps+1;
    vsteps = (memh-1)/width;
    
    XtVaGetValues(info->memory, XtNbackground, &pixel, NULL);

    if (info->mem_pixmap == None) {
         info->mem_pixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
                        memw, memh, DefaultDepthOfScreen(XtScreen(paint)));
       XSetForeground(dpy, gc, pixel);
       for (j=0; j<memh;j++)
          for (i=0; i<memw;i++)
	     XDrawPoint(dpy, info->mem_pixmap, gc, i, j);
       XSetForeground(dpy, gc, black);
       for (j=0; j<=memh-1; j+=memh-1)
          for (i=0; i<memw;i++) if ((i+j)&1)
	     XDrawPoint(dpy, info->mem_pixmap, gc, i, j);
       for (i=0; i<=memw-1; i+=memw-1)
          for (j=0; j<memh;j++) if ((i+j)&1)
	     XDrawPoint(dpy, info->mem_pixmap, gc, i, j);
    }
    

    if (info->mem_index == -1 || Global.numregions == 0)
       l = -1;
    else
       l = info->mem_index;

    if (l>=0) {
       sprintf(buf, "%d / %d", 
               info->mem_index+1, Global.numregions);
       XtVaSetValues(info->position, XtNlabel, buf, NULL);
       XtVaSetValues(info->position, XtNwidth, POS_WIDTH, NULL);
    }

    for (k=0; k<=Global.numregions; k++) {
       i = k%MEMORY_HSTEPS;
       j = k/MEMORY_HSTEPS;
       if (1+width*(j+1) >= memh) break;
       XSetForeground(dpy, gc, white);
       XFillRectangle(dpy, info->mem_pixmap, gc,
                      2+width*i, 2+width*j, 
                      MEMSLOT_SIZE, MEMSLOT_SIZE);
       XSetForeground(dpy, gc, (k==Global.numregions)?pixel:black);
       if (k == Global.numregions)
          XFillRectangle(dpy, info->mem_pixmap, gc, 2+width*i, 2+width*j, 
                         MEMSLOT_SIZE+1, MEMSLOT_SIZE+1);
       else
       if (k == l)
          XFillRectangle(dpy, info->mem_pixmap, gc, 2+width*i, 2+width*j, 
                         MEMSLOT_SIZE, MEMSLOT_SIZE);
       else
          XDrawRectangle(dpy, info->mem_pixmap, gc, 2+width*i, 2+width*j,
                         MEMSLOT_SIZE, MEMSLOT_SIZE);
    }

    XtVaSetValues(info->memory, XtNbackgroundPixmap, 
                                XtUnspecifiedPixmap, NULL);
    XtVaSetValues(info->memory, XtNbackgroundPixmap, 
                                info->mem_pixmap, NULL);
    XFreeGC(dpy, gc);

    XtVaGetValues(info->paint, XtNmenuwidgets, &wlist, NULL);
    if (wlist) {
       l = (Global.numregions>0);
       if (wlist[W_MEMORY_RECALL])
          XtVaSetValues(wlist[W_MEMORY_RECALL], XtNsensitive, l, NULL);
       if (wlist[W_TOPMENU+W_MEMORY_RECALL])
          XtVaSetValues(wlist[W_TOPMENU+W_MEMORY_RECALL], XtNsensitive, l, NULL);
       if (wlist[W_MEMORY_DISCARD])
          XtVaSetValues(wlist[W_MEMORY_DISCARD], XtNsensitive, l, NULL);
       if (wlist[W_TOPMENU+W_MEMORY_DISCARD])
          XtVaSetValues(wlist[W_TOPMENU+W_MEMORY_DISCARD], XtNsensitive, l, NULL);
       if (wlist[W_MEMORY_ERASE])
          XtVaSetValues(wlist[W_MEMORY_ERASE], XtNsensitive, l, NULL);
       if (wlist[W_TOPMENU+W_MEMORY_ERASE])
          XtVaSetValues(wlist[W_TOPMENU+W_MEMORY_ERASE], XtNsensitive, l, NULL);

       if (wlist[W_TOPMENU+W_MEMORY_SCROLL])
          XtVaSetValues(wlist[W_TOPMENU+W_MEMORY_SCROLL], XtNsensitive, 
			(Global.numregions>vsteps*MEMORY_HSTEPS), NULL);
    }
}

static void
removeFromMemory(Display *dpy, int num)
{
    Pixmap pix, mask;
    unsigned char * alpha;
    int i;
    if (num<0 || num>=Global.numregions) return;

    pix = Global.regiondata[num].pix;
    mask = Global.regiondata[num].mask;
    alpha = Global.regiondata[num].alpha;
    if (pix!=None) XFreePixmap(dpy, pix);
    if (mask!=None) XFreePixmap(dpy, mask);
    if (alpha!=NULL) XtFree((char *)alpha);
    --Global.numregions;
    for (i=num; i<Global.numregions; i++) {
       Global.regiondata[i].pix = Global.regiondata[i+1].pix;
       Global.regiondata[i].mask = Global.regiondata[i+1].mask;
       Global.regiondata[i].alpha = Global.regiondata[i+1].alpha;
    }
}

void 
memoryRecallCallback(Widget w, XtPointer paintArg,  XtPointer junk2)
{
    LocalInfo * info = (LocalInfo *) paintArg;
    Display * dpy = XtDisplay(info->paint);
    unsigned char *alpha;
    int width, height, depth;
    GC gc;
    XRectangle rect;
    Pixmap pix, mask;
    PaintWidget pw = (PaintWidget) info->paint;

    if (info->mem_index<0) return;
    if (info->mem_index>=Global.numregions) return;

    GetPixmapWHD(dpy, Global.regiondata[info->mem_index].pix,
		 &width, &height, &depth);
    rect.x = Global.regiondata[info->mem_index].x;
    rect.y = Global.regiondata[info->mem_index].y;

    rect.width = width;
    rect.height = height;
    pix = XCreatePixmap(dpy, DefaultRootWindow(dpy), width, height, depth);
    gc = XCreateGC(dpy, pix, 0, 0);
    XCopyArea(dpy, Global.regiondata[info->mem_index].pix, pix, gc, 0, 0, 
        width, height, 0, 0);
    XFreeGC(dpy, gc);

    mask = Global.regiondata[info->mem_index].mask;
    if (mask) {
        mask = XCreatePixmap(dpy, DefaultRootWindow(dpy), width, height, 1);
        gc = XCreateGC(dpy, mask, 0, 0);
        XCopyArea(dpy, Global.regiondata[info->mem_index].mask, 
		  mask, gc, 0, 0, 
                  width, height, 0, 0);
        XFreeGC(dpy, gc);
    }

    pw->paint.zoomX = 0;
    pw->paint.zoomY = 0;
    pw->paint.downX = 0;
    pw->paint.downY = 0;
    PwRegionSet(info->paint, &rect, pix, mask);
    RegionMove(pw, rect.x, rect.y);
    alpha = Global.regiondata[info->mem_index].alpha;
    if (alpha) {
        pw->paint.region.alpha = (unsigned char *)
            XtMalloc(rect.width*rect.height);
        memcpy(pw->paint.region.alpha, alpha, rect.width*rect.height);
    }
}

void 
memoryStackCallback(Widget w, XtPointer infoArg,  XtPointer junk)
{
    LocalInfo * info = (LocalInfo *) infoArg;
    StdMemorySetCallback(info->paint, info, NULL);
    GraphicAll((GraphicAllProc) setMemoryIcon, NULL);
}

void 
memoryDiscardCallback(Widget w, XtPointer infoArg,  XtPointer junk)
{
    LocalInfo * info = (LocalInfo *) infoArg;
    removeFromMemory(XtDisplay(info->paint), info->mem_index);
    info->mem_index = Global.numregions-1;
    GraphicAll((GraphicAllProc) setMemoryIcon, NULL);
}

void 
memoryScrollCallback(Widget w, XtPointer infoArg,  XtPointer junk)
{
    LocalInfo * info = (LocalInfo *) infoArg;
    RegionData r;
    int i, j;
    int vsteps = (CELL_WIDTH*info->vsteps)/(2+MEMSLOT_SIZE);
    int shift = ((vsteps+1)/2)*MEMORY_HSTEPS;

    if (Global.numregions<=vsteps*MEMORY_HSTEPS) return;

    for (j=1; j<=shift; j++) {
       r = Global.regiondata[0];
       for (i=0; i<=Global.numregions-2; i++) {
           Global.regiondata[i] = Global.regiondata[i+1];
       Global.regiondata[Global.numregions-1] = r;
       }
    }
    if (info->mem_index >= 0)
       info->mem_index = 
          (info->mem_index+Global.numregions-shift) % Global.numregions;
    GraphicAll((GraphicAllProc) setMemoryIcon, NULL);
}

void 
memoryEraseCallback(Widget w, XtPointer infoArg,  XtPointer junk)
{
    LocalInfo * info = (LocalInfo *) infoArg;
    int i;

    if (!info) return;
    for (i=Global.numregions-1; i>=0; i--) {
        removeFromMemory(XtDisplay(info->paint), i);
        GraphicAll((GraphicAllProc) setMemoryIcon, NULL);
    }
}

static void
memoryHandler(Widget w, LocalInfo * info, XEvent * event, Boolean * flag)
{
    int i, j, x, y, num, width;

    if (Global.popped_up) {
        if (event->type == ButtonPress)
            PopdownMenusGlobal();
        return;
    }

    width = MEMSLOT_SIZE+2;
    num = -1;

    if (event->xbutton.button == 1 &&
        (event->type == ButtonPress || event->type == ButtonRelease)) {
       i = (event->xbutton.x-2)/width;
       j = (event->xbutton.y-2)/width;
       x = 2+width*i;
       y = 2+width*j;
       if (event->xbutton.x>=x && event->xbutton.x<=x+MEMSLOT_SIZE &&
	  event->xbutton.y>=y && event->xbutton.y<=y+MEMSLOT_SIZE) {
          num = i+MEMORY_HSTEPS*j;
          if (num>=0 && num<Global.numregions) {
	     if (event->xbutton.button == 1 && event->type == ButtonRelease) {
	        info->mem_index = num;
	        PwRegionOff(info->paint, True);
	        memoryRecallCallback(w, info, NULL);
                GraphicAll((GraphicAllProc) setMemoryIcon, NULL);
	     }
	  }
       } 
    }

    if ( event->xbutton.button == 2 && event->type == ButtonRelease)
       StdMemorySetCallback(w, info, NULL);

    if (event->xbutton.button == 3) {
       if (event->type == ButtonPress)
           XtPopup(info->mempopup, XtGrabNone);
       else
       if (event->type == ButtonRelease)
           Global.popped_up = info->mempopup;
    }
}

/* This manages Window resize of main canvas */

static void
shellHandler(Widget w, LocalInfo * l, XEvent * event, Boolean * flag)
{
    Dimension u, v, up, vp, ub, vb;

    if (event->type == ConfigureNotify) {

        XtVaGetValues(l->paint, XtNwidth, &u, XtNheight, &v, NULL);
        XtVaGetValues(l->viewport, XtNwidth, &up, XtNheight, &vp, NULL);

#if defined(XAW3D) || defined(XAW95)
	if (up>=u+14) ub = up-4; else ub = u+10;
	if (vp>=v+14) vb = vp-4; else vb = v+10;
#else
	if (up>=u+14) ub = up; else ub = u+14;
	if (vp>=v+14) vb = vp; else vb = v+14;
#endif
        XtVaSetValues(l->paintbox, XtNwidth, ub, XtNheight, vb, NULL);
        XtResizeWidget(l->paintbox, ub, vb, 0);

#if defined(XAW3D) || defined(XAW95)
	if (up>=u+14 && vp>=v+14)
            XtVaSetValues(l->viewport, XtNallowVert, False,
                                       XtNallowHoriz, False, NULL);
	else 
        if (vp>=v+30)
            XtVaSetValues(l->viewport, XtNallowVert, False,
                                       XtNallowHoriz, True, NULL);
	else
        if (up>=u+30)
            XtVaSetValues(l->viewport, XtNallowVert, True,
                                       XtNallowHoriz, False, NULL);
	else
            XtVaSetValues(l->viewport, XtNallowVert, True,
                                       XtNallowHoriz, True, NULL);
#else
	if (up>=u+10 && vp>=v+10) {
            XtVaSetValues(l->viewport, XtNallowVert, False,
                                       XtNallowHoriz, False, NULL);
	} else 
        if (up>=u+26) {
            XtVaSetValues(l->viewport, XtNallowVert, True,
                                       XtNallowHoriz, False, NULL);
	} else
        if (vp>=v+26) {
            XtVaSetValues(l->viewport, XtNallowVert, False,
                                       XtNallowHoriz, True, NULL);
	} else {
            XtVaSetValues(l->viewport, XtNallowVert, True,
                                       XtNallowHoriz, True, NULL);
	}
#endif
    }
}

static Pixel
GetPixelByName(Widget w, char *name)
{
    XColor color;
    Colormap cmap;
    cmap = DefaultColormapOfScreen(XtScreen(w));
    if (XAllocNamedColor(XtDisplay(w), cmap, name, &color, &color)!=0)
       return color.pixel;
    else
       return BlackPixelOfScreen(XtScreen(w));
}

void StoreName(Widget w, char *name)
{
    Display *dpy = XtDisplay(w);
    char *cp;

    XStoreName(dpy, XtWindow(w), name);

    if ((cp = strrchr(name, '/')) == NULL)
	cp = name;
    else
	cp++;
    XtVaSetValues(w, XtNtitle, name, XtNiconName, cp, NULL);

    XChangeProperty(dpy, XtWindow(w),
        XInternAtom(dpy, "_NET_WM_NAME", False),
        XInternAtom(dpy, "UTF8_STRING", False),
        8, PropModeReplace, (unsigned char *) name,
	strlen(name));
}

void 
SetAlphaMode(Widget paint, int mode)
{
    PaintWidget pw = (PaintWidget) paint;
    WidgetList wlist;
    int i;

    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
   
    if (pw->paint.current.alpha) {
        if (mode == -1) mode = 2;
    } else
        mode = -1;

    if (mode>=0) {
        pw->paint.alpha_mode = mode%4;
        for (i=1; i<=ALPHA_DELETE; i++) {
            prCallback(paint, wlist[W_ALPHA_MODES+i], True);
            prCallback(paint, wlist[W_TOPMENU+W_ALPHA_MODES+i], True);
	}
        XtVaSetValues(paint, XtNborderColor, 
                      GetPixelByName(paint, "blue"), NULL);
    } else {
        pw->paint.alpha_mode = 0;
        for (i=1; i<=ALPHA_DELETE; i++) 
        if (i!=ALPHA_PARAMS && i!=ALPHA_CREATE && i!=ALPHA_SAVE) {
            prCallback(paint, wlist[W_ALPHA_MODES+i], False);
            prCallback(paint, wlist[W_TOPMENU+W_ALPHA_MODES+i], False);
        }
        XtVaSetValues(paint, XtNborderColor,
                      BlackPixelOfScreen(XtScreen(paint)), NULL);
    }
    MenuCheckItem(wlist[W_ALPHA_MODES+pw->paint.alpha_mode], True);
    MenuCheckItem(wlist[W_TOPMENU+W_ALPHA_MODES+pw->paint.alpha_mode], True);
}

void
loadPrescribedFile(Widget w, char *file)
{
    Display *dpy;
    Image * image;
    WidgetList wlist = NULL;
    LocalInfo * info = NULL;
    unsigned char *alpha;
    XRectangle rect;
    Colormap  cmap;
    Pixmap pix;
    PaintWidget pw = (PaintWidget) w;
    XEvent event;
    int width, height, widthp, heightp, oldw, oldh, zoom;

    if ((file == NULL) || (*file == 0)) return;
    XtVaGetValues(w, XtNzoom, &zoom, XtNcolormap, &cmap, 
		  XtNmenuwidgets, &wlist, NULL);
    if (wlist)
        info = (LocalInfo *)wlist[W_INFO_DATA];

    StateSetBusy(True);

    if ((image = (Image *) ReadMagic(file)) != NULL) {
        width = image->width;
        height = image->height;
        XtVaSetValues(w, XtNdrawWidth, width, XtNdrawHeight, height, NULL);
        if (zoom>0) {
	    widthp = width * zoom;
	    heightp = height * zoom;
	} else {
	    widthp = (width-zoom-1) / (-zoom);
	    heightp = (height-zoom-1) / (-zoom);
	}
        XtVaGetValues(w, XtNwidth, &oldw, XtNheight, &oldh, NULL);
        XtVaSetValues(w, XtNwidth, widthp, XtNheight, heightp, NULL);
        rect.x = 0;
        rect.y = 0;
        rect.width = width;
        rect.height = height;
        PwRegionSet(w, NULL, None, None);
        alpha = image->alpha;
        image->alpha = NULL;
        ImageToPixmap(image, w, &pix, &cmap);
        pw->paint.zoomX = 0;
        pw->paint.zoomY = 0;
        pw->paint.downX = 0;
        pw->paint.downY = 0;
        PwRegionSet(w, &rect, pix, None);
        PwRegionSet(w, NULL, None, None);
        XtVaSetValues(w, XtNdirty, False, NULL);
        pw->paint.current.alpha = alpha;
        event.type = ConfigureNotify;
        if (info && (widthp!=oldw || heightp != oldh)) {
	    dpy = XtDisplay(info->shell);
            XtResizeWidget(w, widthp, heightp, 1);
            shellHandler(info->shell, info, &event, NULL);
            StoreName(info->shell, file);
	}
        SetAlphaMode(w, -1);
        StateSetBusy(False);
    } else {
        StateSetBusy(False);
	Notice(Global.toplevel, msgText[UNABLE_TO_OPEN_INPUT_FILE], file, RWGetMsg());
    }
}

static int xorig = 0;
static int yorig = 0;

static void 
resizeCanvas(LocalInfo *info, int w, int h, int mode)
{
    Dimension u, v;
    Widget parent;
    int wp, hp, w1, h1, w2, h2, d, z;
    static int n=-1;
    char buf[16];

    XtVaGetValues(info->paint, XtNzoom, &z, 
                  XtNdrawWidth, &w1, XtNdrawHeight, &h1, NULL);
    parent = XtParent(XtParent(info->paint));
    XtVaGetValues(parent, XtNwidth, &u, XtNheight, &v, NULL);

    XtUnmanageChild(info->paint);
    if (mode) {
       if (w - w1> h - h1) h = h1 ; else w = w1;
    }
    if (w<=0) w = 1;
    if (h<=0) h = 1;
    
    if (info->resize_state != 1 && (w != w1 || h != h1)) {
        n = Global.numregions;
        StdMemorySetCallback(info->paint, info, NULL);
        if (Global.numregions == n) n = -1;
    }

    /* Popdown fatbit window, if any, to avoid problems */
    /* FatbitsEditDestroy(info->paint); */

    if (z==0 || z==-1) z = 1;
    if (z>0) {
        wp = w*z; hp = h*z; 
    } else {
        z = -z;
        wp = (w+z-1)/z; hp = (h+z-1)/z; 
    }

    /* This has the effect of resizing pixmap and alpha channel buffer */
    XtVaSetValues(info->paint, XtNdrawWidth, w, XtNdrawHeight, h,
                  XtNwidth, wp, XtNheight, hp, XtNdirty, True, NULL);

#ifdef XAW95
    if (u >= wp+14 && v >= hp+14)
#else
    if (u >= wp+10 && v >= hp+10)
#endif
        XtVaSetValues(parent, XtNallowVert, False, XtNallowHoriz, False, NULL);
    else
        XtVaSetValues(parent, XtNallowVert, True, XtNallowHoriz, True, NULL);
    FatbitsUpdate(info->paint, 0);
    XtManageChild(info->paint);

    if (info->resize_state == 1 && Global.numregions && 
        (n==Global.numregions-1) && (w>w1 || h>h1)) {
        Pixmap pix = Global.regiondata[n].pix;
        w2 = h2 = 0;
        if (pix) GetPixmapWHD(XtDisplay(info->paint), pix, &w2, &h2, &d);
        if (w2>w1 || h2>h1) {
	    d = info->mem_index;
            info->mem_index = n;
            memoryRecallCallback(info->paint, info, NULL);
            PwRegionSet(info->paint, NULL, None, None);
            info->mem_index = d;
	}
    }
    if (info->resize_state == -1) info->resize_state = 1;

    sprintf(buf, "%d %d", w, h);
    XtVaSetValues(info->position, XtNlabel, buf, NULL);
    XtVaSetValues(info->position, XtNwidth, POS_WIDTH, NULL);
}

static void
popupHandler(Widget w, LocalInfo * info, XEvent * event, Boolean * flag)
{
    if (Global.popped_up) {
        if (event->type == ButtonRelease || event->type == ButtonPress)
            PopdownMenusGlobal();
        event->type = None;       
    }
}

static void
canvasHandler(Widget w, LocalInfo * info, XEvent * event, Boolean * flag)
{
    static int x, y, z;

    /*
    printf("%d (viewport=%d paint=%d box=%d)\n", w, 
           info->viewport, info->paint, info->paintbox);
    */

    if (w==info->paint) {
        xorig = event->xbutton.x;
        yorig = event->xbutton.y;
    } else
    if (Global.popped_up) {
        if (event->type == ButtonPress || event->type == ButtonRelease)
            PopdownMenusGlobal();
        event->type = None;
        return;
    } 

    if (info->resize_state) {
        if (event->type == ButtonPress) {
           if (w==info->paint) {
	      x = event->xbutton.x; 
              y = event->xbutton.y;
           } else
           if (w==info->paintbox) {
 	      x = event->xbutton.x-4; 
              y = event->xbutton.y-4;
	   }
	} else
        if (event->type == MotionNotify) {
           if (w==info->paint) {
	      x = event->xmotion.x; 
              y = event->xmotion.y;
           } else
           if (w==info->paintbox) {
 	      x = event->xmotion.x-4; 
              y = event->xmotion.y-4;
	   }
	} else
	   return;
        XtVaGetValues(info->paint, XtNzoom, &z, NULL);
        if (z>0)
            resizeCanvas(info, x/z, y/z, 1);
        else
            resizeCanvas(info, -x*z, -y*z, 1);
    }
}

static void
motionHandler(Widget w, LocalInfo * info, XEvent * event, Boolean * flag)
{
    char buf[30];
    int zoom, h;

    if (info->boolpos) {
        XtVaGetValues(w, XtNzoom, &zoom, XtNheight, &h, NULL);
        if (((XButtonEvent *) event)->state & ControlMask) {
	    if (zoom>0)
                sprintf(buf, "%d %d", 
	            (event->xbutton.x - xorig)/zoom, 
                    (event->xbutton.y - yorig)/zoom);
            else
                sprintf(buf, "%d %d", 
	            -(event->xbutton.x - xorig)*zoom, 
                    -(event->xbutton.y - yorig)*zoom);
	} else
	    if (zoom>0)
                sprintf(buf, "%d %d", 
	            event->xbutton.x/zoom, event->xbutton.y/zoom);
            else
                sprintf(buf, "%d %d", 
	            -event->xbutton.x*zoom, -event->xbutton.y*zoom);
        XtVaSetValues(info->position, XtNlabel, buf, NULL);
        XtVaSetValues(info->position, 
                      XtNwidth, POS_WIDTH, XtNheight, 20, NULL);
    }
}

void 
motionExtern(Widget paint, XEvent * event, int x, int y, int flag)
{
    LocalInfo *info;
    WidgetList wlist;
    char buf[30];
    int h;

    XtVaGetValues(paint, XtNmenuwidgets, &wlist, 
                         XtNdrawHeight, &h, NULL);
    if (!wlist) return;
    info = (LocalInfo *)wlist[W_INFO_DATA];
    if (!info) return;

    if (flag) {
        xorig = x;
        yorig = y;
    }
    if (info->boolpos) {
        if (((XButtonEvent *) event)->state & ControlMask) {
            sprintf(buf, "%4d %4d", x - xorig, y - yorig);
	} else
            sprintf(buf, "%4d %4d", x, y);
        XtVaSetValues(info->position, XtNlabel, buf, NULL);
        XtVaSetValues(info->position, XtNwidth, POS_WIDTH, NULL);
    }
}

static void
positionHandler(Widget w, LocalInfo * info, XEvent * event, Boolean * flag)
{
    if (Global.popped_up) {
        if (event->type == ButtonRelease)
            PopdownMenusGlobal();
        event->type = None;
        return;
    }

    info->boolpos = !info->boolpos;
    XtVaSetValues(info->position, XtNlabel, 
	 (info->boolpos)? "???  ???" : msgText[POSITION], NULL);
    XtVaSetValues(info->position, XtNwidth, POS_WIDTH, NULL);
    xorig = 0;
    yorig = 0;
}

static void
formHandler(Widget w, LocalInfo * info, XEvent * event, Boolean * flag)
{
    Display *dpy = XtDisplay(w);
    Dimension v;
    WidgetList wlist;
    int i;

    XtVaGetValues(info->paint, XtNmenuwidgets, &wlist, NULL);
    XtVaGetValues(w, XtNheight, &v, NULL);
    i = (v - 32)/CELL_WIDTH;
    if (i<3) i = 3;

    if (wlist && i != info->vsteps) {
         info->vsteps = i;
	 XFreePixmap(XtDisplay(w), info->palette_pixmap);
	 info->palette_pixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
	                PALETTE_SIZE*CELL_WIDTH+32, CELL_WIDTH*info->vsteps-1, 
			DefaultDepthOfScreen(XtScreen(info->paint)));
	 if (info->mem_pixmap) {
            XFreePixmap(XtDisplay(w), info->mem_pixmap);
	    info->mem_pixmap = None;
            GraphicAll((GraphicAllProc) setMemoryIcon, NULL);
	    XtVaSetValues(info->memory, XtNheight, CELL_WIDTH*info->vsteps+1, NULL);
	 }
         if (wlist[W_ICON_PALETTE])
	     XtVaSetValues(wlist[W_ICON_PALETTE], 
                           XtNheight, CELL_WIDTH*info->vsteps-1, NULL);
	 setPalettePixmap(info->paint, info, 0);
	 shellHandler(GetToplevel(info->paint), info, event, flag);
    }
}

static void 
lineStyleOkCallback(Widget w, XtPointer junk, XtPointer infoArg)
{
    Arg arg;
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int i, width, length, valid, cap, join;
#define MAXLENGTH 64

    width = atoi(info->prompts[0].rstr);

    if (width < 1 || width > 1000) {
	Notice(w, msgText[INVALID_WIDTH_MUST_BE_GREATER_THAN_ZERO]);
	return;
    }

    length = strlen(info->prompts[1].rstr);
    if (length > MAXLENGTH) valid = 0; else {
        valid = 1;
        for (i = 0; i<length; i++)
	  if (info->prompts[1].rstr[i] != '-' &&
              info->prompts[1].rstr[i] != '=') {
	      valid = 0;
	      break;
	  }
    }

    if (!valid) {
        Notice(w, msgText[DASH_IS_GIVEN_BY_ALTERNATING_THAT_MANY_CHARACTERS], 
               MAXLENGTH);
	return;
    }

    cap = atoi(info->prompts[2].rstr);
    if (cap<0 || cap>4) {
        Notice(w, msgText[DASH_IS_GIVEN_BY_ALTERNATING_THAT_MANY_CHARACTERS], 
               0);
	return;
    }

    join = atoi(info->prompts[3].rstr);
    if (join<0 || join>3) {
        Notice(w, msgText[DASH_IS_GIVEN_BY_ALTERNATING_THAT_MANY_CHARACTERS], 
               0);
	return;
    }

    sprintf(currentLineWidth, "%d", width);
    if (width == 1) width = 0;
    GraphicAll(setLineWidth, (void *)(long)width);
    XtSetArg(arg, XtNlineWidth, width);
    OperationAddArg(arg);
    strncpy(dashStyleStr, info->prompts[1].rstr, 64);
    DashSetStyle(dashStyleStr);
    Global.cap = cap;
    Global.join = join;
    GraphicAll(setCanvasColorsIcon, NULL);
}

void 
lineStyleAction(Widget w, int width)
{
    if (width <= 0) {
	static TextPromptInfo info;
	static struct textPromptInfo value[4];
        static char cap_buf[80];
        static char join_buf[80];
        Dimension h;

	info.prompts = value;
	info.title = msgText[WIDTH_AND_DESIRED_LINE_STYLE];
	info.nprompt = 4;
	value[0].prompt = msgText[LINE_WIDTH];
	value[0].str = currentLineWidth;
	value[0].len = 5;
        value[1].prompt = msgText[DASH_STYLE];    
        value[1].str = dashStyleStr;
        value[1].len = 66;
        value[2].prompt = msgText[CAP_STYLE];    
        sprintf(cap_buf, "%d  (%s)", Global.cap, msgText[CAP_HINT]);
        value[2].str = cap_buf;
        value[2].len = 5;
        value[3].prompt = msgText[JOIN_STYLE];    
        sprintf(join_buf, "%d  (%s)", Global.join, msgText[JOIN_HINT]);
        value[3].str = join_buf;
        value[3].len = 5;

        GraphicAll(setLineWidthMenu, (void *)NUMBER_LINEWIDTHS);
	w = TextPrompt(GetToplevel(w), 
            "linewidth", &info, lineStyleOkCallback, NULL, NULL);
        if (w) {
            XtVaGetValues(w, XtNheight, &h, NULL);
            XtResizeWidget(w, 600, h, 0);
	}
    } else {
        Arg arg;
        if (width == 1) width = 0;
	GraphicAll(setLineWidth, (void *)(long)width);
	GraphicAll(setLineWidthMenu, (void *)(long)(width/2));
	XtSetArg(arg, XtNlineWidth, width);
	OperationAddArg(arg);
    }
}

void 
lineStyleCallback(Widget w, XEvent * event)
{
    String lbl;
    int width;
    XtVaGetValues(w, XtNlabel, &lbl, NULL);
    width = atoi(lbl);
    lineStyleAction(GetToplevel(w), width);
}

static void
lineStyleHandler(Widget w, LocalInfo * info, XEvent * event, Boolean * flag)
{
    lineStyleAction(GetToplevel(w), 0);
}

static void 
PopupMenuOkCallback(Widget w, XtPointer junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    void (* proc)(TextPromptInfo *);

    proc = junk;
    if (proc) proc(info);
}

void *
PopupMenu(char * title, int n, void *values, void * proc)
{
    static TextPromptInfo info;
    Widget w = (Global.curpaint)? Global.curpaint : Global.toplevel;

    PopdownMenusGlobal();

    if (!values || n<=0) return NULL;
    info.prompts = (struct textPromptInfo *) values;
    info.title = title;
    info.nprompt = n;

    TextPrompt(w, "genericselect", &info, PopupMenuOkCallback, NULL, proc);

#if DEBUG
    fprintf(stderr, "graphic.c/PopupMenu : %ld (%ld, %ld, %ld):\n",
                    PopupMenuOkCallback, w, proc, &info);
#endif
    return &info;
}

/*
**  Font menu callbacks.
 */
void
setFontIcon(Widget paint)
{
    WidgetList wlist;
    char buf[10];
    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    sprintf(buf, "Abc\n:%g", Global.xft_size);
    if (wlist && wlist[W_ICON_FONT]) {
        XtVaSetValues(wlist[W_ICON_FONT], XtNlabel, buf, NULL);
        XtVaSetValues(wlist[W_ICON_FONT], 
                      XtNwidth, ICONWIDTH, XtNheight, ICONHEIGHT,
                      NULL);
        /* XFlush(XtDisplay(paint)); */
        XtResizeWidget(wlist[W_ICON_FONT], ICONWIDTH, ICONHEIGHT, 
#ifdef XAWPLAIN
        1);
#else 
        0);
#endif
    }
}

static void 
fontSetCallback(Widget paint, void *junk)
{
    FontChanged(paint);
}

static void 
fontMenuCallback(Widget paint, void *ptr)
{
    WidgetList wlist;
    static int i[2] = {-1 , -1};
    int k;

    i[1] = (int)(long)ptr;
    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    for (k=0; k<=1; k++)
    if (i[k]>=0 && i[k]<NUMBER_PREDEF_FONTS) {
        MenuCheckItem(wlist[W_FONT_DESCR+i[k]], k);
        MenuCheckItem(wlist[W_TOPMENU+W_FONT_DESCR+i[k]], k);
    }
    i[0] = i[1];
}

static void 
fontSet(Widget w, void *ptr)
{
    Display * dpy;

    if (ptr == (void *)NUMBER_PREDEF_FONTS) {
        GraphicAll(fontMenuCallback, ptr);
	FontSelect(Global.toplevel, None);
    } else {
        dpy = XtDisplay(GetShell(w));
	if (setDefaultGlobalFont(dpy, fontNames[(long)ptr])) {
	    GraphicAll(fontSetCallback, NULL);
            GraphicAll(fontMenuCallback, ptr);
	} else
  	    XtVaSetValues(w, XtNsensitive, False, NULL);
    }
}

static void 
StdFontSet(Widget w, XtPointer infoArg, XtPointer junk)
{
    fontSet(w, (void *)NUMBER_PREDEF_FONTS);
}

/*
 * Toggle the 'snap' menu item.
 */
static void 
setSnapMenu(Widget paint, void *ptr)
{
    WidgetList wlist;
    Boolean v;

    v = (ptr)? True : False;
    XtVaSetValues(paint, XtNsnapOn, v, NULL);
    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    RefreshPaintWidget(paint);
    if (!wlist) return;
    MenuCheckItem(wlist[W_SELECTOR_SNAP], v);
    MenuCheckItem(wlist[W_TOPMENU+W_SELECTOR_SNAP], v);
}
static void
snapCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    Boolean v;

    XtVaGetValues(paint, XtNsnapOn, &v, NULL);
    v = !v;
    setSnapMenu(paint, (void *)((long) v));
}

/*
 *  Callbacks for setting snap spacing.
 */
static void
snapSpacingOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int snap_x = atoi(info->prompts[0].rstr);
    int snap_y = atoi(info->prompts[1].rstr);

    if (snap_x < 1 || snap_x > 100 || snap_y < 1 || snap_y > 100) {
	Notice(paint, msgText[BAD_SNAP_SPACING]);
	return;
    }
    XtVaSetValues(paint, XtNsnapX, snap_x, NULL);
    XtVaSetValues(paint, XtNsnapY, snap_y, NULL);
    RefreshPaintWidget(paint);
}

static void
snapSpacingCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[2];
    static char bufX[10];
    static char bufY[10];
    int snap_x, snap_y;

    XtVaGetValues(paint, XtNsnapX, &snap_x, NULL);
    XtVaGetValues(paint, XtNsnapY, &snap_y, NULL);
    sprintf(bufX, "%4d", snap_x);
    sprintf(bufY, "%4d", snap_y);

    info.prompts = value;
    info.title = msgText[ENTER_DESIRED_SNAP_SPACING];
    info.nprompt = 2;
    value[0].prompt = msgText[SNAP_SPACING_X];
    value[0].str = bufX;
    value[0].len = 4;
    value[1].prompt = msgText[SNAP_SPACING_Y];
    value[1].str = bufY;
    value[1].len = 4;

    TextPrompt(paint, "linewidth", &info, snapSpacingOkCallback, NULL, NULL);
}

/*
 *  Routines for changing the image size.
 */
void
cancelWHZSizeCallback(Widget w, XtPointer arg, XtPointer junk)
{
    /* XtFree((XtPointer) arg); */
}

static void 
okWHZSizeCallback(Widget w, XtPointer argArg, XtPointer infoArg)
{
    LocalInfo *localinfo = (LocalInfo *) argArg;
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int width, height, w1, h1, z1, canvas;

    w1 = atoi(info->prompts[0].rstr);
    h1 = atoi(info->prompts[1].rstr);
    z1 = Global.default_zoom;
    canvas = info->prompts[3].len;
    width = w1;
    height = h1;

    if (localinfo == NULL || localinfo->paint == None)
        z1 = StrToZoom(info->prompts[2].rstr);
    else
	XtVaGetValues(localinfo->paint, XtNdrawWidth, &width,
		      XtNdrawHeight, &height, XtNzoom, &z1, NULL);

    if (w1 <= 0) {
	Notice(w, msgText[INVALID_WIDTH]);
        return;
    }
    if (h1 <= 0) {
	Notice(w, msgText[INVALID_HEIGHT]);
        return;
    }
    if (z1==0 || z1==-1 || z1<-16 || z1>32) {
	Notice(w, msgText[INVALID_ZOOM]);
        return;
    }

    if (localinfo && (w1 != width || h1 != height)) {
        resizeCanvas(localinfo, w1, h1, 0);
        return;
    }
    if (!localinfo) {
        if (canvas)
            /* Call from toolbox. Create new canvas */
            CreateCanvas(w, w1, h1, z1);
        else
            SetDefaultWHZ(w1, h1, z1);
        return;
    }
}

void 
WHZSizeSelect(Widget w, XtPointer arg, int canvas)
{
    LocalInfo * localinfo = (LocalInfo *) arg;
    static TextPromptInfo info;
    static struct textPromptInfo values[4];
    int width, height, zoom;
    char bufA[16], bufB[16], bufC[16];

    info.prompts = values;
    info.nprompt = (localinfo) ? 2 : 3;
    info.title = msgText[ENTER_DESIRED_IMAGE_SIZE];

    values[0].prompt = msgText[SIZE_WIDTH];
    values[0].str = bufA;
    values[0].len = 5;
    values[1].prompt = msgText[SIZE_HEIGHT];
    values[1].str = bufB;
    values[1].len = 5;
    values[2].prompt = msgText[SIZE_ZOOM];
    values[2].str = bufC;
    values[2].len = 5;
    /* ugly hack : pass argument 'new' through  values[3].len */
    values[3].len = canvas;

    if (localinfo) {
	XtVaGetValues(localinfo->paint, XtNdrawWidth, &width,
		      XtNdrawHeight, &height,
		      XtNzoom, &zoom,
		      NULL);
    } else {
        width = Global.default_width;
        height = Global.default_height;
	zoom = Global.default_zoom;
    }

    sprintf(bufA, "%d", width);
    sprintf(bufB, "%d", height);
    strcpy(bufC, ZoomToStr(zoom));
    TextPrompt(w, "WHZsize", &info, okWHZSizeCallback, cancelWHZSizeCallback, 
               localinfo);
}

static void
WHZSizeCallback(Widget w, XtPointer arg, XtPointer junk2)
{
    LocalInfo * info = (LocalInfo *) arg;
    Widget paint = (Widget) info->paint;

    WHZSizeSelect(GetShell(paint), info, 0);
}

static void
defaultWHZSizeCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;

    WHZSizeSelect(GetShell(paint), NULL, 0);
}

static void
autocropOkCallback(Widget w, XtPointer argArg, XtPointer junk)
{
    AutoCrop((Widget) argArg);
}

static void
undosizeOkCallback(Widget paint, void * junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int n = atoi(info->prompts[0].rstr);

    if (n < 0 || n > 20) {
	Notice(paint, msgText[BAD_NUMBER_OF_UNDO_LEVELS]);
	return;
    }
    XtVaSetValues(paint, XtNundoSize, n, NULL);
}

static void
undosizeCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf[5];
    int undosize;

    XtVaGetValues(paint, XtNundoSize, &undosize, NULL);
    sprintf(buf, "%d", undosize);

    value[0].prompt = msgText[LEVELS];
    value[0].str = buf;
    value[0].len = 3;
    info.prompts = value;
    info.title = msgText[NUMBER_OF_UNDO_LEVELS];
    info.nprompt = 1;

    TextPrompt(paint, "undolevels", &info, undosizeOkCallback, NULL, NULL);
}


static void
autocropCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    AlertBox(GetShell(paintArg),
       msgText[AUTOCROP_WARNING_CANNOT_BE_UNDONE],
	     autocropOkCallback, genericCancelCallback, paintArg);
}

/*
 * Callback functions for changing zoom
 */
static void
zoomAddChild(Widget paint, int zoom)
{
    Cardinal nc;
    Widget t, box;
    WidgetList children;
    int dw, dh;

    if (!paint) return;
    box = XtParent(paint);
    if (!box) return;

    /*
     *	1 child == just paint widget
     *	2 children paint widget + normal size view
     */
    XtVaGetValues(box, XtNchildren, &children, XtNnumChildren, &nc, NULL);
    XtVaGetValues(paint, XtNdrawWidth, &dw, XtNdrawHeight, &dh, NULL);
    if (nc == 1 && zoom > 1 && dw < 256 && dh < 256) {
	/*
	 * Add child
	 */
	t = XtVaCreateManagedWidget("norm", paintWidgetClass, box,
				    XtNpaint, paint,
				    XtNzoom, 1,
				    NULL);
	GraphicAdd(t);
    } else if (nc != 1 && zoom <= 1) {
	/*
	 * Remove child
	 */
	t = children[(children[0] == paint) ? 1 : 0];
	XtDestroyWidget(t);
    }
}

static
void setZoom(Widget paint, int zoom)
{
    Widget w;
    int oldzoom, dw, dh, wp, hp;
    Dimension u, v;

    XtVaGetValues(paint, XtNzoom, &oldzoom, 
                     XtNdrawWidth, &dw, XtNdrawHeight, &dh, NULL);
    if (oldzoom == zoom) return;

    StateSetBusy(True);
    XtUnmanageChild(paint);
    if (zoom>0) {
       wp = dw*zoom;
       hp = dh*zoom;
    } else {
       wp = (dw-zoom-1)/(-zoom);
       hp = (dh-zoom-1)/(-zoom);
    }

    XtVaSetValues(paint, XtNwidth, wp, XtNheight, hp, XtNzoom, zoom, NULL);
    zoomAddChild(paint, zoom);
    setStandardCursor(paint);
    FatbitsUpdate(paint, zoom);
  
    w = XtParent(paint);
    if (w) w = XtParent(w);
    if (w) {
        XtVaGetValues(w, XtNwidth, &u, XtNheight, &v, NULL);
#if defined(XAW3D) || defined(XAW95)
        if (u >= wp+14 && v >= hp+14)
#else
        if (u >= wp+10 && v >= hp+10)
#endif
        XtVaSetValues(w, 
               XtNallowVert, False, XtNallowHoriz, False, NULL);
        else
        XtVaSetValues(w, 
               XtNallowVert, True, XtNallowHoriz, True, NULL);
    }

    XtManageChild(paint);
    RefreshWidget(paint);
    StateSetBusy(False);
}

static void
zoomOkCallback(Widget w, XtPointer arg, XtPointer data)
{
    LocalInfo * localinfo = (LocalInfo *) arg;
    TextPromptInfo *info = (TextPromptInfo *) data;
    int zoom;

    zoom = StrToZoom(info->prompts[0].rstr);

    if (zoom == 0 || zoom < -9 || zoom > 32) {
	Notice(localinfo->paint, msgText[INVALID_ZOOM]);
    } else {
        setZoom(localinfo->paint, zoom);
        XtVaSetValues(localinfo->zoom, XtNlabel, ZoomToStr(zoom), NULL);
        XtVaSetValues(localinfo->zoom, XtNwidth, 21, XtNheight, 22, NULL);
    }
}

void
zoomCallback(Widget w, XtPointer arg, XtPointer junk2)
{
    static TextPromptInfo info;
    static struct textPromptInfo values[2];
    static char buf[80];
    int zoom;
    LocalInfo * localinfo = (LocalInfo *) arg;

    info.nprompt = 1;
    info.prompts = values;
    info.title = msgText[CHANGE_ZOOM_FACTOR_FOR_IMAGE];
    values[0].prompt = msgText[ZOOM];
    values[0].len = 4;
    values[0].str = buf;

    XtVaGetValues(localinfo->paint, XtNzoom, &zoom, NULL);
    strcpy(buf, ZoomToStr(zoom));

    TextPrompt(GetShell(localinfo->paint), "zoomselect", &info,
	       zoomOkCallback, NULL, localinfo);
}

/*
 * Callback functions for Region menu
 */
static void
rotateTo(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    float t;
    pwMatrix m;
    String lbl;

    XtVaGetValues(w, XtNlabel, &lbl, NULL);
    t = atof(lbl);
    if (t == 0.0)
	return;

    t *= M_PI / 180.0;

    m[0][0] = cos(t);
    m[0][1] = sin(t);
    m[1][0] = -sin(t);
    m[1][1] = cos(t);
    PwRegionAppendMatrix(paint, m);
}

static int rotateAngle = 0;

static void
rotateOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    float t = atof(info->prompts[0].rstr) * M_PI / 180.0;
    pwMatrix m;

    m[0][0] = cos(t);
    m[0][1] = sin(t);
    m[1][0] = -sin(t);
    m[1][1] = cos(t);
    PwRegionAppendMatrix(paint, m);
    rotateAngle = (int) t;
}

static void
rotate(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf[10];

    sprintf(buf, "%d", rotateAngle);

    value[0].prompt = msgText[ANGLE_IN_DEGREES];
    value[0].str = buf;
    value[0].len = 4;
    info.prompts = value;
    info.title = msgText[ENTER_DESIRED_ROTATION];
    info.nprompt = 1;

    TextPrompt(paint, "rotation", &info, rotateOkCallback, NULL, NULL);
}

static void
resetMat(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PwRegionReset((Widget) paintArg, True);
}

static void
cropToRegionOkCallback(Widget w, PaintWidget paint, XtPointer infoArg)
{
    RegionCrop(paint);
}

static void
cropToRegion(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;

    AlertBox(GetShell(paint),
    msgText[ARE_YOU_SURE_YOU_WANT_TO_CROP_THE_IMAGE_TO_THE_SIZE_OF_THE_REGION],
	     (XtCallbackProc) cropToRegionOkCallback,
	     genericCancelCallback, paint);
}

static void
linearRegionOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    pwMatrix m;

    ImgProcessInfo.linearA = atof(info->prompts[0].rstr);
    ImgProcessInfo.linearB= atof(info->prompts[1].rstr);
    ImgProcessInfo.linearC = atof(info->prompts[2].rstr);
    ImgProcessInfo.linearD = atof(info->prompts[3].rstr);
    m[0][0] = atof(info->prompts[0].rstr);
    m[0][1] = -atof(info->prompts[1].rstr);
    m[1][0] = -atof(info->prompts[2].rstr);
    m[1][1] = atof(info->prompts[3].rstr);
    PwRegionAppendMatrix(paint, m);
}

static void
linearRegion(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[4];
    static char buf1[10], buf2[10], buf3[10], buf4[10];

    sprintf(buf1, "%g", ImgProcessInfo.linearA);
    sprintf(buf2, "%g", ImgProcessInfo.linearB);
    sprintf(buf3, "%g", ImgProcessInfo.linearC);
    sprintf(buf4, "%g", ImgProcessInfo.linearD);

    value[0].prompt = "a11 =";
    value[0].str = buf1;
    value[0].len = 4;
    value[1].prompt = "a12 =";
    value[1].str = buf2;
    value[1].len = 4;
    value[2].prompt = "a21 =";
    value[2].str = buf3;
    value[2].len = 4;
    value[3].prompt = "a22 =";
    value[3].str = buf4;
    value[3].len = 4;
    info.prompts = value;
    info.title = msgText[ENTER_TWOBYTWO_MATRIX_ENTRIES];
    info.nprompt = 4;

    TextPrompt(paint, "linear", &info, linearRegionOkCallback, NULL, NULL);
}

static void
tiltRegionOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;

    ImgProcessInfo.tiltX1 = atoi(info->prompts[0].rstr);
    ImgProcessInfo.tiltY1 = atoi(info->prompts[1].rstr);
    ImgProcessInfo.tiltX2 = atoi(info->prompts[2].rstr);
    ImgProcessInfo.tiltY2 = atoi(info->prompts[3].rstr);
    StdRegionTilt(paint, paint, NULL);
}

static void
tiltRegion(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[4];
    static char buf1[10], buf2[10], buf3[10], buf4[10];

    sprintf(buf1, "%d", ImgProcessInfo.tiltX1);
    sprintf(buf2, "%d", ImgProcessInfo.tiltY1);
    sprintf(buf3, "%d", ImgProcessInfo.tiltX2);
    sprintf(buf4, "%d", ImgProcessInfo.tiltY2);

    value[0].prompt = "X1:";
    value[0].str = buf1;
    value[0].len = 4;
    value[1].prompt = "Y1:";
    value[1].str = buf2;
    value[1].len = 4;
    value[2].prompt = "X2:";
    value[2].str = buf3;
    value[2].len = 4;
    value[3].prompt = "Y2:";
    value[3].str = buf4;
    value[3].len = 4;
    info.prompts = value;
    info.title = msgText[ENTER_POINTS];
    info.nprompt = 4;

    TextPrompt(paint, "tilt", &info, tiltRegionOkCallback, NULL, NULL);
}

static void
unselectRegion(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    PwRegionSet(paint, NULL, None, None);
}

/*
 * Callback functions for Filter menu
 */

static void
oilPaintOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int t;

    t = atoi(info->prompts[0].rstr);
    if ((t < 3) || ((t & 1) == 0)) {
	Notice(paint, msgText[INVALID_MASK_SIZE]);
	return;
    }
    ImgProcessInfo.oilArea = t;
    StdRegionOilPaint(paint, paint, NULL);
}

static void
oilPaint(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf[10];

    sprintf(buf, "%d", ImgProcessInfo.oilArea);

    info.prompts = value;
    info.title = msgText[ENTER_MASK_SIZE_FOR_OIL_PAINT_EFFECT];
    info.nprompt = 1;
    value[0].prompt = msgText[MUST_BE_ODD];
    value[0].str = buf;
    value[0].len = 3;

    TextPrompt(paint, "mask", &info, oilPaintOkCallback, NULL, NULL);
}

static void
SmoothOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int t;

    t = atoi(info->prompts[0].rstr);
    if ((t < 3) || ((t & 1) == 0)) {
	Notice(paint, msgText[INVALID_MASK_SIZE]);
	return;
    }
    ImgProcessInfo.smoothMaskSize = t;
    StdRegionSmooth(paint, paint, NULL);
}

static void
doSmooth(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf[10];


    sprintf(buf, "%d", ImgProcessInfo.smoothMaskSize);

    info.prompts = value;
    info.title = msgText[ENTER_MASK_SIZE_FOR_SMOOTHING_EFFECT];
    info.nprompt = 1;
    value[0].prompt = msgText[MUST_BE_ODD];
    value[0].str = buf;
    value[0].len = 3;

    TextPrompt(paint, "mask", &info, SmoothOkCallback, NULL, NULL);
}

static void
addNoiseOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int t;

    t = atoi(info->prompts[0].rstr);
    if (t < 1) {
	Notice(paint, msgText[INVALID_NOISE_VARIANCE]);
	return;
    }
    ImgProcessInfo.noiseDelta = t;
    StdRegionAddNoise(paint, paint, NULL);
}

static void
addNoise(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf[10];


    sprintf(buf, "%d", ImgProcessInfo.noiseDelta);

    value[0].prompt = "(0-255):";
    value[0].str = buf;
    value[0].len = 3;
    info.prompts = value;
    info.title = msgText[ENTER_DESIRED_NOISE_VARIANCE];
    info.nprompt = 1;

    TextPrompt(paint, "delta", &info, addNoiseOkCallback, NULL, NULL);
}

static void
doSpreadOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int t;

    t = atoi(info->prompts[0].rstr);
    if (t < 1) {
	Notice(paint, msgText[INVALID_SPREAD_DISTANCE]);
	return;
    }
    ImgProcessInfo.spreadDistance = t;
    StdRegionSpread(paint, paint, NULL);
}

static void
doSpread(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf1[10];


    sprintf(buf1, "%d", ImgProcessInfo.spreadDistance);

    value[0].prompt = msgText[DISTANCE_PIXELS];
    value[0].str = buf1;
    value[0].len = 3;
    info.prompts = value;
    info.title = msgText[ENTER_THE_DESIRED_SPREAD_DISTANCE];
    info.nprompt = 1;

    TextPrompt(paint, "distance", &info, doSpreadOkCallback, NULL, NULL);
}

static void
doPixelizeOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    char *s = info->prompts[0].rstr;
    int e = 0, tx, ty;


    if (strchr(s, 'x')) {
	if (sscanf(s, "%d x %d", &tx, &ty) != 2)
	    ++e;
    } else {
	if (sscanf(s, "%d", &tx) != 1)
	    ++e;
	ty = tx;
    }

    if (e || (tx < 1) || (ty < 1)) {
	Notice(paint, msgText[INVALID_PIXEL_SIZE]);
	return;
    }
    ImgProcessInfo.pixelizeXSize = tx;
    ImgProcessInfo.pixelizeYSize = ty;
    StdRegionPixelize(paint, paint, NULL);
}

static void
doPixelize(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf[10];


    if (ImgProcessInfo.pixelizeXSize != ImgProcessInfo.pixelizeYSize)
	sprintf(buf, "%dx%d", ImgProcessInfo.pixelizeXSize,
		ImgProcessInfo.pixelizeYSize);
    else
	sprintf(buf, "%d", ImgProcessInfo.pixelizeXSize);

    value[0].prompt = msgText[WIDTH_X_HEIGHT_OR_SINGLE_NUMBER];
    value[0].str = buf;
    value[0].len = 3;
    info.prompts = value;
    info.title = msgText[ENTER_DESIRED_MEGAPIXEL_SIZE];
    info.nprompt = 1;

    TextPrompt(paint, "size", &info, doPixelizeOkCallback, NULL, NULL);
}

static void
despeckleOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int t;

    t = atoi(info->prompts[0].rstr);
    if ((t < 3) || ((t & 1) == 0)) {
	Notice(paint, msgText[INVALID_MASK_SIZE]);
	return;
    }
    ImgProcessInfo.despeckleMask = t;
    StdRegionDespeckle(paint, paint, NULL);
}

static void
doDespeckle(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf[10];


    sprintf(buf, "%d", ImgProcessInfo.despeckleMask);

    info.prompts = value;
    info.title = msgText[ENTER_MASK_SIZE_FOR_DESPECKLE_FILTER];
    info.nprompt = 1;
    value[0].prompt = msgText[MUST_BE_ODD];
    value[0].str = buf;
    value[0].len = 3;

    TextPrompt(paint, "despeckle", &info, despeckleOkCallback, NULL, NULL);
}

static void
contrastOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int t1, t2;

    t1 = atoi(info->prompts[0].rstr);
    if ((t1 < 0) || (t1 > 100)) {
	Notice(paint, msgText[INVALID_WHITE_LEVEL]);
	return;
    }
    t2 = atoi(info->prompts[1].rstr);
    if ((t2 < 0) || (t2 > 100)) {
	Notice(paint, msgText[INVALID_BLACK_LEVEL]);
	return;
    }
    ImgProcessInfo.contrastB = t1;
    ImgProcessInfo.contrastW = t2;
    StdRegionNormContrast(paint, paint, NULL);
}


static void
doContrast(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[2];
    static char buf1[10], buf2[10];


    sprintf(buf1, "%d", ImgProcessInfo.contrastB);
    sprintf(buf2, "%d", ImgProcessInfo.contrastW);

    info.prompts = value;
    info.title = msgText[ENTER_LEVELS_FOR_CONTRAST_ADJUSTMENT];
    info.nprompt = 2;
    value[0].prompt = msgText[BLACK_LEVEL];
    value[0].str = buf1;
    value[0].len = 3;
    value[1].prompt = msgText[WHITE_LEVEL];
    value[1].str = buf2;
    value[1].len = 3;
    TextPrompt(paint, "contrast", &info, contrastOkCallback, NULL, NULL);
}

static void
solarizeOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int t;

    t = atoi(info->prompts[0].rstr);
    if ((t < 1) || (t > 99)) {
	Notice(paint, msgText[INVALID_SOLARIZATION_THRESHOLD]);
	return;
    }
    ImgProcessInfo.solarizeThreshold = t;
    StdRegionSolarize(paint, paint, NULL);
}

static void
doSolarize(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf[10];


    sprintf(buf, "%d", ImgProcessInfo.solarizeThreshold);

    info.prompts = value;
    info.title = msgText[ENTER_THRESHOLD_FOR_SOLARIZE_FILTER];
    info.nprompt = 1;
    value[0].prompt = "(%):";
    value[0].str = buf;
    value[0].len = 3;

    TextPrompt(paint, "mask", &info, solarizeOkCallback, NULL, NULL);
}

static void
quantizeOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int t;

    t = atoi(info->prompts[0].rstr);
    if ((t < 2) || (t > 256)) {
	Notice(paint, msgText[INVALID_NUMBER_OF_COLORS]);
	return;
    }
    ImgProcessInfo.quantizeColors = t;
    StdRegionQuantize(paint, paint, NULL);
}

static void
expandOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
#define EXPAND_MAX  64.0
    PaintWidget pw = (PaintWidget) paint;
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    static Pixmap pix, mask, pixp, maskp;
    static XRectangle rect;
    XImage *pixImg, *pixpImg = NULL, *maskImg;
    Display *dpy = XtDisplay(paint);
    GC gc;
    Pixel p = 0;
    unsigned char *c_buffer, *col0, *col1;
    unsigned char *alpha;
    unsigned char col[3], colp[3];
    double xf, yf;
    int i=0, j, x, y, u, v, up, vp, r, g ,b, gray, m;
    int width, height, depth, widthp, heightp, interp;

    xf = atof(info->prompts[0].rstr);
    yf = atof(info->prompts[1].rstr);   
    if ((xf < 1.0) || (xf > EXPAND_MAX) || 
        (yf < 1.0) || (yf > EXPAND_MAX)) {
      invalid:
	Notice(paint, msgText[INVALID_SCALING_FACTOR]);
	return;
    }
    ImgProcessInfo.rescale_x = xf;
    ImgProcessInfo.rescale_y = yf;  
    
    if (info->nprompt>=3) 
        ImgProcessInfo.interpolate = (atoi(info->prompts[2].rstr)>0);
    else
        ImgProcessInfo.interpolate = 0;

    PwRegionGet(paint, &pix, &mask);
    if (!pix) return;

    GetPixmapWHD(dpy, pix, &width, &height, &depth);

    if (Global.depth>=15) 
        interp = ImgProcessInfo.interpolate;
    else
        interp = 0;

    x = (int) (xf-0.000001);
    y = (int) (yf-0.000001);

    if (junk) {
        widthp = (int) (width / xf + 0.5);
        heightp = (int) (height / yf + 0.5);
    } else {
        widthp = (int) (width * xf + 0.5);
        heightp = (int) (height * yf + 0.5);
    }    

    if (widthp==0 || heightp==0) goto invalid;

    pixp = XCreatePixmap(dpy, DefaultRootWindow(dpy),
                         widthp, heightp, depth);
    if (!pixp) return;
    gc = XCreateGC(dpy, pixp, 0, 0);
    pixImg = NewXImage(dpy, NULL, depth, width, height);
    if (!pixImg) {
    cancel:
        XDestroyImage(pixImg);
        return;
    }

    if (pw->paint.region.alpha)
        alpha = (unsigned char *) XtMalloc(widthp*heightp);
    else
        alpha = NULL;

    XGetSubImage(dpy, pix, 0, 0, width, height,
		 AllPlanes, ZPixmap, pixImg, 0, 0);

    if (interp==0 && junk==NULL) {
      no_interp_expand:
        pixpImg = NewXImage(dpy, NULL, depth, widthp, heightp);
        if (!pixpImg) goto cancel;
        for (v=0; v<height; v++)
        for (u=0; u<width; u++) {
	   p = XGetPixel(pixImg, u, v);
           for (vp=0; vp<=y; vp++)
	   for (up=0; up<=x; up++) {
	       i = (int)(xf*u+up);
               j = (int)(yf*v+vp);
	       XPutPixel(pixpImg, i, j, p);
               if (alpha) 
		   alpha[i+j*widthp] = pw->paint.region.alpha[u+v*width];
	   }
	}
        XDestroyImage(pixImg);
    }

    if (interp==1 && junk==NULL) {
        c_buffer = (unsigned char *)xmalloc(3*(width+1)*(height+1));
        if (!c_buffer) {
	    interp = 0;
            goto no_interp_expand;
	}
        for (v=0; v<height; v++) {
            for (u=0; u<width; u++) {
	        p = XGetPixel(pixImg, u, v);
	        get_color_components(p, c_buffer + 3*(v*(width+1)+u));
	    }
	    get_color_components(p, c_buffer + 3*(v*(width+1)+width));
	}
        memcpy(c_buffer+3*height*(width+1), c_buffer+3*(height-1)*(width+1),
               3*(width+1));
        XDestroyImage(pixImg);
        pixpImg = NewXImage(dpy, NULL, depth, widthp, heightp);
        if (!pixpImg) goto cancel;
        for (v=0; v<height; v++)
        for (u=0; u<width; u++) {
            for (vp=0; vp<=y; vp++)
	    for (up=0; up<=x; up++) {
	        col0 = c_buffer + 3*(v*(width+1)+u);
		col1 = c_buffer + 3*((v+1)*(width+1)+u);
		col[0] = ((col0[0]*(xf-up)+col0[3]*up)/xf);
		col[1] = ((col0[1]*(xf-up)+col0[4]*up)/xf);
		col[2] = ((col0[2]*(xf-up)+col0[5]*up)/xf);
		colp[0] = ((col1[0]*(xf-up)+col1[3]*up)/xf);
		colp[1] = ((col1[1]*(xf-up)+col1[4]*up)/xf);
		colp[2] = ((col1[2]*(xf-up)+col1[5]*up)/xf);
		col[0] = ((col[0]*(yf-vp)+colp[0]*vp)/yf);
		col[1] = ((col[1]*(yf-vp)+colp[1]*vp)/yf);
		col[2] = ((col[2]*(yf-vp)+colp[2]*vp)/yf);
	        p = get_pixel_from_colors(col);
	        XPutPixel(pixpImg, (int)(xf*u+up), (int)(yf*v+vp), p);
	    }
	}
        if (alpha) {
	    for (v=0; v<height; v++) {
	        i = v*(width+1);
	        memcpy(c_buffer+i, 
                       pw->paint.region.alpha+v*width, width);
                c_buffer[i+width] = c_buffer[i+width-1];
	    }
            memcpy(c_buffer+i+width+1, c_buffer+i, width+1);
            for (v=0; v<height; v++)
            for (u=0; u<width; u++) {
                for (vp=0; vp<=y; vp++)
	        for (up=0; up<=x; up++) {
	            col0 = c_buffer + (v*(width+1)+u);
		    col1 = c_buffer + ((v+1)*(width+1)+u);
		    col[0] = ((col0[0]*(xf-up)+col0[1]*up)/xf);
		    colp[0] = ((col1[0]*(xf-up)+col1[1]*up)/xf);
                    i = (int)(xf*u+up);
                    j =  (int)(yf*v+vp);
                    alpha[i+j*widthp] = ((col[0]*(yf-vp)+colp[0]*vp)/yf);
		}
	    }
	}
	free(c_buffer);
    }

    if (interp==0 && junk) {
        pixpImg = NewXImage(dpy, NULL, depth, widthp, heightp);
        if (!pixpImg) goto cancel;
        for (v=0; v<heightp; v++) {
	    vp = (int)(v*yf); if (vp>=height) vp = height-1;
            for (u=0; u<widthp; u++) {
 	        up = (int)(u*xf); if (up>=width) up = width-1;
 	        p = XGetPixel(pixImg, up, vp);
	        XPutPixel(pixpImg, u, v, p);
                if (alpha)
		    alpha[u+v*widthp] = pw->paint.region.alpha[up+vp*width];
 
	    }
	}
        XDestroyImage(pixImg);
    }

    if (interp==1 && junk) {
        pixpImg = NewXImage(dpy, NULL, depth, widthp, heightp);
        if (!pixpImg) goto cancel;
        m = (x+1)*(y+1);
        for (v=0; v<heightp; v++)
	for (u=0; u<widthp; u++) {
	    r = g = b = gray = 0;
	    for (j=0; j<=y; j++) {
	        vp = (int)(v*yf+j); if (vp>=height) vp = height-1;
	        for (i=0; i<=x; i++) {
	            up = (int)(u*xf+i); if (up>=width) up = width-1;
 	            p = XGetPixel(pixImg, up, vp);
                    get_color_components(p, col);
                    r += col[0];
                    g += col[1];
                    b += col[2];
                    if (alpha)
                        gray += (int) pw->paint.region.alpha[up+vp*width];
	        }
	    }
            col[0] = r/m;
            col[1] = g/m;
            col[2] = b/m;
	    p = get_pixel_from_colors(col);
	    XPutPixel(pixpImg, u, v, p);
            if (alpha) alpha[u+v*widthp] = (unsigned char)(gray/m);
	}
        XDestroyImage(pixImg);
    }

    XPutImage(dpy, pixp, gc, pixpImg, 0, 0, 0, 0, widthp, heightp);
    XFreeGC(dpy, gc);
    XDestroyImage(pixpImg);
                         
    if (mask) {
       maskp = XCreatePixmap(dpy, DefaultRootWindow(dpy), 
                             widthp, heightp, 1);
       gc = XCreateGC(dpy, maskp, 0, 0);
       maskImg = NewXImage(dpy, NULL, 1, width, height);
       XGetSubImage(dpy, mask, 0, 0, width, height,
		    AllPlanes, ZPixmap, maskImg, 0, 0);
       XSetForeground(dpy, gc, 0);
       XFillRectangle(dpy, maskp, gc, 0, 0, widthp, heightp);
       for (v=0; v<heightp; v++)
	   for (u=0; u<widthp; u++) {
	       p = XGetPixel(maskImg, (u*width)/widthp, (v*height)/heightp);
               if (p) {
                   XSetForeground(dpy, gc, p);
                   XDrawPoint(dpy, maskp, gc, u, v);
	       }
       }
       XDestroyImage(maskImg);
    } else
       maskp = None;

    rect.width = widthp;
    rect.height = heightp;
    rect.x = 0;
    rect.y = 0;
    if (alpha) {
        XtFree((char *)pw->paint.region.alpha);
        pw->paint.region.alpha = NULL;
    }
    PwRegionClear(paint);
    PwRegionSet(paint, &rect, pixp, maskp);
    pw->paint.region.alpha = alpha;
}

static void
modifyRGBOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int r, g, b;

    r = atoi(info->prompts[0].rstr);
    g = atoi(info->prompts[1].rstr);
    b = atoi(info->prompts[2].rstr);

    if ((r < -100) || (r > 100) || 
        (g < -100) || (g > 100) ||
        (b < -100) || (b > 100)) {
	Notice(paint, msgText[INVALID_RGB_PERCENTAGES]);
	return;
    }
    ImgProcessInfo.redshift = r;
    ImgProcessInfo.greenshift = g;
    ImgProcessInfo.blueshift = b;
    StdRegionModifyRGB(paint, paint, NULL);
}

static void
doModifyRGB(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[3];
    static char buf_r[10], buf_g[10], buf_b[10];


    sprintf(buf_r, "%d", ImgProcessInfo.redshift);
    sprintf(buf_g, "%d", ImgProcessInfo.greenshift);
    sprintf(buf_b, "%d", ImgProcessInfo.blueshift);

    info.prompts = value;
    info.title = msgText[ENTER_DESIRED_SHIFT_FOR_RGB_COMPONENTS];
    info.nprompt = 3;
    value[0].prompt = msgText[RED_SHIFT];
    value[0].str = buf_r;
    value[0].len = 4;
    value[1].prompt = msgText[GREEN_SHIFT];
    value[1].str = buf_g;
    value[1].len = 4;
    value[2].prompt = msgText[BLUE_SHIFT];
    value[2].str = buf_b;
    value[2].len = 4;

    TextPrompt(paint, "mask", &info, modifyRGBOkCallback, NULL, NULL);
}

static void
doQuantize(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf[10];


    sprintf(buf, "%d", ImgProcessInfo.quantizeColors);

    info.prompts = value;
    info.title = msgText[ENTER_DESIRED_NUMBER_OF_COLORS];
    info.nprompt = 1;
    value[0].prompt = "(2-256):";
    value[0].str = buf;
    value[0].len = 3;

    TextPrompt(paint, "mask", &info, quantizeOkCallback, NULL, NULL);
}

static void
expandRegion(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[3];
    static char buf_scale_x[10], buf_scale_y[10];
    static char buf_interp[10], buf_x[256], buf_y[256];

    sprintf(buf_x, "%s (x)", msgText[EXPANSION_FACTOR]);
    sprintf(buf_y, "%s (y)", msgText[EXPANSION_FACTOR]);   
    sprintf(buf_scale_x, "%g", ImgProcessInfo.rescale_x);
    sprintf(buf_scale_y, "%g", ImgProcessInfo.rescale_y);
    sprintf(buf_interp, "%d", ImgProcessInfo.interpolate);

    info.prompts = value;
    info.title = msgText[EXPANSION_PARAMETERS];
    info.nprompt = 3;
    value[0].prompt = buf_x;
    value[0].str = buf_scale_x;
    value[0].len = 3;
    value[1].prompt = buf_y;
    value[1].str = buf_scale_y;
    value[1].len = 3;   
    value[2].prompt = msgText[DO_INTERPOLATION];
    value[2].str = buf_interp;
    value[2].len = 3;   

    TextPrompt(paint, "mask", &info, expandOkCallback, NULL, NULL);
}

static void
downscaleRegion(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[3];
    static char buf_scale_x[10], buf_scale_y[10];
    static char buf_interp[10], buf_x[256], buf_y[256];

    sprintf(buf_x, "%s (x)", msgText[DOWNSCALING_FACTOR]);
    sprintf(buf_y, "%s (y)", msgText[DOWNSCALING_FACTOR]);   
    sprintf(buf_scale_x, "%g", ImgProcessInfo.rescale_x);
    sprintf(buf_scale_y, "%g", ImgProcessInfo.rescale_y);
    sprintf(buf_interp, "%d", ImgProcessInfo.interpolate);

    info.prompts = value;
    info.title = msgText[DOWNSCALING_PARAMETERS];
    info.nprompt = 3;
    value[0].prompt = buf_x;
    value[0].str = buf_scale_x;
    value[0].len = 3;
    value[1].prompt = buf_y;
    value[1].str = buf_scale_y;
    value[1].len = 3;   
    value[2].prompt = msgText[DO_INTERPOLATION];
    value[2].str = buf_interp;
    value[2].len = 3;   

    TextPrompt(paint, "mask", &info, expandOkCallback, NULL, (XtPointer)1);
}

static void
doLast(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;

    StdLastImgProcess(paint);
}

void
EnableRevert(Widget paint)
{
    WidgetList wlist;

    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    XtVaSetValues(wlist[W_FILE_REVERT], XtNsensitive, True, NULL);
    XtVaSetValues(wlist[W_TOPMENU+W_FILE_REVERT], 
                  XtNsensitive, True, NULL);
}

void
EnableLast(Widget paint)
{
    WidgetList wlist;

    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    XtVaSetValues(wlist[W_REGION_LAST], XtNsensitive, True, NULL);
    XtVaSetValues(wlist[W_REGION_UNDO], XtNsensitive, True, NULL);
    XtVaSetValues(wlist[W_TOPMENU+W_REGION_LAST], 
                  XtNsensitive, True, NULL);
    XtVaSetValues(wlist[W_TOPMENU+W_REGION_UNDO], 
                  XtNsensitive, True, NULL);
}

/*
 *  Background changer
 */
static void
changeBgOk(Widget w, Palette * map, XColor * col)
{
    StateShellBusy(w, False);

    if (col != NULL) {
	Pixel pix = PaletteAlloc(map, col);
        Global.bg[0] = (unsigned char) col->red>>8;
        Global.bg[1] = (unsigned char) col->green>>8;
        Global.bg[2] = (unsigned char) col->blue>>8;
	XtVaSetValues(w, XtNbackground, pix, NULL);
	if (Global.patternshell)
            checkPatternLink(w, 1);
	else
	  setCanvasColorsIcon(w, NULL);
    }
}

void
changeBackground(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    Colormap cmap;
    Pixel bg;
    Palette *map;

    /* StateShellBusy(paint, True); */

    XtVaGetValues(GetShell(paint), XtNcolormap, &cmap, NULL);
    XtVaGetValues(paint, XtNbackground, &bg, NULL);
    map = PaletteFind(paint, cmap);

    ColorEditor(paint, bg, map,
		False, (XtCallbackProc) changeBgOk, (XtPointer) map);
}

static void
alphaParametersOkCallback(Widget paint, void *junk, XtPointer infoArg)
{
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    unsigned int alpha_bg, alpha_fg, alpha_threshold;

    alpha_bg = atoi(info->prompts[0].rstr);
    alpha_fg = atoi(info->prompts[1].rstr);
    alpha_threshold = atoi(info->prompts[2].rstr);

    if ((alpha_bg < 0) || (alpha_bg > 255) ||
        (alpha_fg < 0) || (alpha_fg > 255) ||
        (alpha_threshold < 0) || (alpha_threshold > 255)) {
	Notice(paint, msgText[INVALID_ALPHA_CHANNEL_PARAMETERS]);
	return;
    }
    Global.alpha_bg = alpha_bg;
    Global.alpha_fg = alpha_fg;
    Global.alpha_threshold = alpha_threshold;
}

static void
alphaParametersCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[3];
    static char alpha_bg[10], alpha_fg[10], alpha_threshold[10];

    sprintf(alpha_bg, "%d", Global.alpha_bg);
    sprintf(alpha_fg, "%d", Global.alpha_fg);
    sprintf(alpha_threshold, "%d", Global.alpha_threshold);

    info.prompts = value;
    info.title = msgText[ENTER_DESIRED_ALPHA_CHANNEL_PARAMETERS];
    info.nprompt = 3;
    value[0].prompt = msgText[ALPHA_BACKGROUND_VALUE];
    value[0].str = alpha_bg;
    value[0].len = 4;
    value[1].prompt = msgText[ALPHA_FOREGROUND_VALUE];
    value[1].str = alpha_fg;
    value[1].len = 4;
    value[2].prompt = msgText[ALPHA_THRESHOLD_VALUE];
    value[2].str = alpha_threshold;
    value[2].len = 4;

    TextPrompt(paint, "alphaparam", &info, alphaParametersOkCallback, NULL, NULL);
}

static void
selectColorRange(Widget w, XtPointer paintArg, XtPointer junk)
{
    Widget paint = (Widget) paintArg;
    Colormap cmap;
    Palette *map;

    /* StateShellBusy(paint, True); */

    XtVaGetValues(GetShell(paint), XtNcolormap, &cmap, NULL);
    map = PaletteFind(paint, cmap);

    ChromaDialog(paint, map);
}

/*
 *  Start of graphic window creation routines
 */

Widget
makeGraphicShell(Widget wid)
{
    Arg args[8];
    int nargs = 0;
    Widget shell;

    XtSetArg(args[nargs], XtNtitle, msgText[DEFAULT_TITLE]); nargs++;
    XtSetArg(args[nargs], XtNiconName, msgText[DEFAULT_TITLE]); nargs++;
    XtSetArg(args[nargs], XtNdepth, Global.vis.depth); nargs++;
    XtSetArg(args[nargs], XtNvisual, Global.vis.visual); nargs++;
    
    shell = XtAppCreateShell("Canvas", "Canvas",
		   topLevelShellWidgetClass, XtDisplay(GetToplevel(wid)),
			     args, nargs);

    return shell;
}

#define ADDCALLBACK(menu, item, pw, func) \
  XtAddCallback(menu[item].widget, XtNcallback, (XtCallbackProc) func, \
		(XtPointer) pw);

static void
simpleMenuPopup(Widget w, XtPointer infoArg, XtPointer junk)
{
    Boolean v;
    LocalInfo *info = (LocalInfo *) infoArg;

    v = IsFullMenuSet(info->paint);
    SetFullMenu(info->paint, !v);
    MenuCheckItem(w, v);
}

void
ManageQuirk(LocalInfo * info)
{
    Display * dpy = XtDisplay(info->shell);
    Position x, x1, x2, y;
    Dimension w2;

    XtUnmanageChild(info->bar);
    XtUnmanageChild(info->resize);
    XtUnmanageChild(info->position);
    XtUnmanageChild(info->palette);
    XtUnmanageChild(info->memory);
    XtUnmanageChild(info->colors);
    XtUnmanageChild(info->tool);
    XtUnmanageChild(info->brush);
    XtUnmanageChild(info->font);
    XtUnmanageChild(info->linestyle);
    XFlush(dpy);
    usleep(30000);
    XtVaSetValues(info->form, XtNshowGrip, True, NULL);
    XMapWindow(dpy, XtWindow(info->bar)); 
    XMapWindow(dpy, XtWindow(info->resize)); 
    XMapWindow(dpy, XtWindow(info->position)); 
    XMapWindow(dpy, XtWindow(info->palette));
    XMapWindow(dpy, XtWindow(info->memory));  
    XMapWindow(dpy, XtWindow(info->colors)); 
    XMapWindow(dpy, XtWindow(info->tool)); 
    XMapWindow(dpy, XtWindow(info->brush)); 
    XMapWindow(dpy, XtWindow(info->font));

    XtVaGetValues(info->colors, XtNx, &x1, NULL);
    XtVaGetValues(info->linestyle, XtNy, &y, NULL);
    XtVaGetValues(info->position, XtNx, &x2, XtNwidth, &w2, NULL);
    if ((x=x2+w2+7)<x1) x = x1-1;
    XtMoveWidget(info->linestyle, x, y);
    XtVaGetValues(info->font, XtNx, &x2, XtNwidth, &w2, NULL);
    LINESTYLE_WIDTH = x2+w2 - x - 2;
    XtResizeWidget(info->linestyle, LINESTYLE_WIDTH, LINESTYLE_HEIGHT, 1);
    XMapWindow(dpy, XtWindow(info->linestyle));
    XMapWindow(dpy, XtWindow(info->shell));
}

static void
hideMenuBar(Widget w, XtPointer infoArg, XtPointer junk)
{
    LocalInfo * info = (LocalInfo *) infoArg;
    SetMenuBarVisibility(info->paint, False);
    SetFullMenu(info->paint, True);
    XtVaGetValues(info->form, XtNheight, &info->barheight, NULL);
    XtUnmanageChild(info->form);
}

static void
showMenuBar(Widget w, XtPointer infoArg, XtPointer junk)
{
    LocalInfo * info = (LocalInfo *) infoArg;
    XEvent event;
    Dimension width, height, widthp, heightp;
    int diff = 0;

    XtVaGetValues(info->shell, XtNwidth, &width, XtNheight, &height, NULL);
    XtVaGetValues(info->paint, XtNwidth, &widthp, XtNheight, &heightp, NULL);
    if (!IsMenuBarGlobal() && heightp+76>height) {
         XtResizeWidget(info->shell, width, heightp+76, 0);
         info->barheight = 75;
         height = heightp+76;
         diff = 6;
    }
    SetMenuBarVisibility(info->paint, True);
    SetFullMenu(info->paint, IsFullMenuGlobal());
    XtUnmanageChild(info->viewport);
    XtManageChild(info->form);
    XtManageChild(info->viewport);
    XtMoveWidget(info->viewport, 0, info->barheight);
    heightp = height - info->barheight;
    if (heightp < 10) heightp = 10;
    XtResizeWidget(info->viewport, width, heightp, 0);
    XtResizeWidget(info->shell, width, height-10, 0);
    XFlush(XtDisplay(info->shell));
    XtResizeWidget(info->shell, width, height-diff, 0);
    event.type = ConfigureNotify;
    shellHandler(info->shell, info, &event, NULL);
}

void 
setPasteItemMenu(Widget paint, void *ptr )
{
    WidgetList wlist;
    
    XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    if (Global.clipboard.pix || Global.clipboard.image) {
        XtVaSetValues(wlist[W_EDIT_PASTE], XtNsensitive, True, NULL);
        XtVaSetValues(wlist[W_TOPMENU+W_EDIT_PASTE], 
                      XtNsensitive, True, NULL);
    }
}

static void
alphaModeCallback(Widget w, XtPointer wlArg, XtPointer junk)
{
    LocalInfo *info = (LocalInfo *) wlArg;
    char *file, *name;
    PaintWidget pw = (PaintWidget) info->paint;
    WidgetList wlist;

    name = XtName(w);
    if (!pw->paint.current.alpha) return;
    XtVaGetValues(info->paint, XtNfilename, &file, NULL);
    pw->paint.alpha_mode = (name[4]-'0')%4;
    RefreshPaintWidget((Widget)pw);
    MenuCheckItem(w, True);
    XtVaGetValues(info->paint, XtNmenuwidgets, &wlist, NULL);
    if (!wlist) return;
    MenuCheckItem(wlist[W_ALPHA_MODES+pw->paint.alpha_mode], True);
    MenuCheckItem(wlist[W_TOPMENU+W_ALPHA_MODES+pw->paint.alpha_mode], True);
}

static void
canvasResizeToggleCallback(Widget w, XtPointer wlArg, XtPointer junk)
{
    LocalInfo *info = (LocalInfo *) wlArg;
    Pixel green, color;
    int w1, h1;
    char buf[16];
    Boolean state;

    if (Global.popped_up) {
        PopdownMenusGlobal();
        XtVaSetValues(w, XtNstate, info->resize_state, NULL);
        return;
    }

    if (((PaintWidget)info->paint)->paint.current.alpha)
        color = GetPixelByName(w, "blue");
    else
        color = BlackPixelOfScreen(XtScreen(w));
    green = GetPixelByName(w, "green");
    XtVaGetValues(info->resize, XtNstate, &state, NULL);
    XtVaSetValues(info->paint, XtNborderColor, (state)? green : color, NULL);
    XtVaSetValues(info->paint, XtNlocked, state, NULL);
    info->resize_state = (state)? -1:0;
    XtVaGetValues(info->paint, XtNdrawWidth, &w1, XtNdrawHeight, &h1, NULL);
    sprintf(buf, "%d %d", w1, h1);
    XtVaSetValues(info->position, XtNlabel, buf, NULL);
    XtVaSetValues(info->position, XtNwidth, POS_WIDTH, NULL);
    XDefineCursor(XtDisplay(info->paint), XtWindow(info->paint), (state)?
                  XCreateFontCursor(XtDisplay(info->paint), 
                                            XC_sizing) :
                  ((PaintWidget)info->paint)->paint.cursor);
    XDefineCursor(XtDisplay(info->viewport), XtWindow(info->viewport), (state)?
                  XCreateFontCursor(XtDisplay(info->viewport), 
                                            XC_sizing) :
                  XCreateFontCursor(XtDisplay(info->viewport), XC_left_ptr));
}

/*
**
 */

int
CRC_exists(RegionData *r)
{
    int i;
    for (i=0; i<Global.numregions; i++)
        if (!memcmp(&r->crcdata,&Global.regiondata[i].crcdata, sizeof(CRCdata)))
            return 1;
    return 0;
}

void
CRC_checksum(RegionData *r)
{
    unsigned int s1 = 0, s2 = 0;
    unsigned char a;
    int width, height, depth, x, y;
    Display *dpy = XtDisplay(Global.toplevel);
    XImage *xim;
    unsigned long p;

    GetPixmapWHD(dpy, r->pix, &width, &height, &depth);

    p = width;
    while (p) { s1 = CRC32(s1, p&0xff); p = p>>8; }
    p = height;
    while (p) { s1 = CRC32(s1, p&0xff); p = p>>8; }
    p = width^255;
    while (p) { s2 = CRC32(s2, p&0xff); p = p>>8; }
    p = height^255;
    while (p) { s2 = CRC32(s2, p&0xff); p = p>>8; }

    xim = XGetImage(dpy, r->pix, 0, 0, width, height, AllPlanes, ZPixmap);
    for (y=0; y<height; y++)
        for (x=0; x<width; x++) {
	    p = XGetPixel(xim, x, y);
            while (p) { 
	        a = p&0xff;
                s1 = CRC32(s1, a);
                s2 = CRC32(s2, a^(0xff));
                p = p>>8;
	    }
        }
    XDestroyImage(xim);

    if (r->mask) {
        xim = XGetImage(dpy, r->mask, 0, 0, width, height, AllPlanes, ZPixmap);
        for (y=0; y<height; y++)
	    for (x=0; x<width; x++) {
	        /* random 8 bit values here ... */
	        a = (XGetPixel(xim, x, y))? 141:27;
	        s1 = CRC32(s1, a);
	        s2 = CRC32(s2, a^(0xff));
	    }
        XDestroyImage(xim);
    }
    if (r->alpha) {
        for (y=0; y<height; y++)
	    for (x=0; x<width; x++) {
	        a = r->alpha[x+y*width];
	        s1 = CRC32(s1, a);
	        s2 = CRC32(s2, a^(0xff));
	    }
    }
    r->crcdata.s1 = s1;
    r->crcdata.s2 = s2;
}

int 
ImageToMemory(Image * image)
{
    Pixmap pix, mask;
    Colormap cmap;
    Widget w = Global.toplevel;
    Display * dpy= XtDisplay(w);
    RegionData r;
    int i;

    if (!image) return 0;
    r.alpha = image->alpha;
    image->alpha = NULL;
    XtVaGetValues(w, XtNcolormap, &cmap, NULL);
    mask = ImageMaskToPixmap(w, image);
    if (!ImageToPixmap(image, w, &pix, &cmap)) {
       XFreePixmap(dpy, mask);
       return -1;
    }

    r.x = 0;
    r.y = 0;
    r.pix = pix;
    r.mask = mask;
    CRC_checksum(&r);

    if (CRC_exists(&r)) {
        XFreePixmap(dpy, pix);
        if (mask) XFreePixmap(dpy, mask);
        if (r.alpha) XtFree((char *)r.alpha);
        return -1;
    }

    i = Global.numregions;
    ++Global.numregions;
    Global.regiondata = (RegionData *) 
        realloc(Global.regiondata, Global.numregions*sizeof(RegionData));
    Global.regiondata[i] = r;
    GraphicAll((GraphicAllProc) setMemoryIcon, NULL);
    return i;
}

static void 
loadMemoryCallback(Widget w, char *file, void *image)
{
    ClipboardSetImage(w, image);
    /* This was (and still is) the way of loading the Copy/Paste clipboard */
    /* GraphicAll(setPasteItemMenu, NULL); */
    /* Put instead image into the more user friendly memory manager */
    ImageToMemory(image);
}

void 
loadMemory(Widget w, XtPointer junk, XtPointer junk2)
{
    GetFileName(GetShell(w), BROWSER_READ, NULL, 
                (XtCallbackProc) loadMemoryCallback, NULL);
}

static void
createAlphaCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PaintWidget pw = (PaintWidget) paintArg;
    Widget paint = (Widget) paintArg;
    int i;

    if (pw->paint.current.alpha) return;
    i = pw->paint.drawWidth * pw->paint.drawHeight;
    pw->paint.current.alpha = (unsigned char *) xmalloc(i);
    memset(pw->paint.current.alpha, (unsigned char)Global.alpha_fg, i);
    SetAlphaMode(paint, -1);
    RefreshPaintWidget(paint);
}

static void
editAlphaCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PaintWidget pw = (PaintWidget) paintArg;
    Widget paint = (Widget) paintArg;
    Image * image;
    Pixmap pix;
    int width, height, zoom;
    Colormap cmap;

    if (!pw->paint.current.alpha) return;
    XtVaGetValues(paint, XtNcolormap, &cmap, XtNzoom, &zoom, NULL); 

    width = pw->paint.drawWidth;    
    height = pw->paint.drawHeight;    
    image = ImageNewGrey(width, height);
    memcpy(image->data, pw->paint.current.alpha, width*height);
    ImageToPixmap(image, paint, &pix, &cmap);
    graphicCreate(makeGraphicShell(Global.toplevel), 
		  0, 0, zoom, pix, cmap, NULL);
}

static void
saveAlphaCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PaintWidget pw = (PaintWidget) paintArg;
    Widget paint = (Widget) paintArg;
    Display *dpy;
    Pixel p, bg;
    Pixmap pix, mask, newpix;
    XImage *maskImg = NULL;
    int x, y, width, height, depth, zoom;
    Colormap cmap;
    Image *image;
    GC gc;
    unsigned char *src, *dst;

    dpy = XtDisplay(paint);
    if (PwRegionGet(paint, &pix, &mask)) {
         XtVaGetValues(paint, XtNcolormap, &cmap, XtNzoom, &zoom, NULL); 
         if (pw->paint.region.source) {
	    pix = pw->paint.region.source;
	    mask = pw->paint.region.mask;
            if (zoom>0) {
	        width = pw->paint.region.rect.width/zoom;
	        height = pw->paint.region.rect.height/zoom;
	    } else {
	        width = pw->paint.region.rect.width * (-zoom);
	        height = pw->paint.region.rect.height * (-zoom);
	    }
	 } else {
            if (zoom>0) {
	        width = pw->paint.region.orig.width/zoom;
	        height = pw->paint.region.orig.height/zoom;
	    }  else {
	        width = pw->paint.region.orig.width * (-zoom);
	        height = pw->paint.region.orig.height * (-zoom);
	    }
	 }
         if (width != pw->paint.drawWidth) return;
         if (height != pw->paint.drawHeight) return;
         gc = XtGetGC(paint, 0, 0);
	 depth = DefaultDepthOfScreen(XtScreen(paint));
	 /* Create newpix, since original pixmap may be destroyed ... */
	 newpix = XCreatePixmap(dpy,
			XtWindow(Global.toplevel), width, height, depth);
	 XCopyArea(dpy, pix, newpix, gc, 0, 0, width, height, 0, 0);
	 if (mask) {
	     maskImg = NewXImage(dpy, NULL, pw->core.depth, width, height);
	     XGetSubImage(dpy, mask, 0, 0, width, height,
		 AllPlanes, ZPixmap, maskImg, 0, 0);
	     XtVaGetValues(paint, XtNbackground, &bg, NULL);
	     XSetForeground(dpy, gc, bg);
	     for (y=0; y<height; y++)
	       for (x=0; x<width; x++) {
		   p = XGetPixel(maskImg, x, y);
		   if (!p) XDrawPoint(dpy, newpix, gc, x, y);
	       }
	     XDestroyImage(maskImg);
	 }
         image = PixmapToImage(paint, newpix, cmap);
         if (!pw->paint.current.alpha)
             pw->paint.current.alpha = (unsigned char *)XtMalloc(width*height);
         src = image->data;
         dst = pw->paint.current.alpha;
         if (image->cmapData) {
	     for (y=0; y<height; y++)
	         for (x=0; x<width; x++) {
		     *dst++ = (32*image->cmapData[3*(*src)]+
		               50*image->cmapData[3*(*src)+1]+
		               18*image->cmapData[3*(*src)+2])/100;
                     ++src;
	         }
	 } else {
	     for (y=0; y<height; y++)
	         for (x=0; x<width; x++) {
		     *dst++ = (32*src[0] + 50*src[1] + 18*src[2])/100;
	             src += 3;
	         }
	 }
         ImageDelete(image);
         XtReleaseGC(paint, gc);
         XtVaSetValues(paint, XtNdirty, True, NULL);
         pw->paint.alpha_mode = 2;
         PwRegionOff(paint, True);
         SetAlphaMode(paint, -1);
         RefreshPaintWidget(paint);
    } else {
         Notice(paint, msgText[ALPHA_CHANNEL_SHOULD_BE_CREATED]);
    }
}

static void
deleteAlphaCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PaintWidget pw = (PaintWidget) paintArg;
    Image * image;

    if (pw->paint.current.alpha) {
        /* put alpha channel in memory as grey image */
        image = ImageNewGrey(0, 0);
        image->width = pw->paint.drawWidth;
        image->height = pw->paint.drawHeight;
	image->data = pw->paint.current.alpha;
        pw->paint.current.alpha = NULL;
        ImageToMemory(image);
        XtVaSetValues((Widget)pw, XtNdirty, True, NULL);
        SetAlphaMode((Widget)pw, -1);
        RefreshPaintWidget((Widget)pw);
    }
}

void
FontSelectCallback(Widget w, XtPointer junk, XtPointer junk2)
{
    FontSelect(GetShell(w), NULL);
}

void
LupeCallback(Widget w, XtPointer junk, XtPointer junk2)
{
    StartMagnifier(w);
}

void
BrushSelectCallback(Widget w, XtPointer junk, XtPointer junk2)
{
    BrushSelect(Global.toplevel);
}

static void
ToolSelectCallback(Widget w, XtPointer shell, XtPointer junk2)
{
    Display *dpy = XtDisplay(Global.toplevel);
    Global.canvas = (Widget) shell;
    XtVaSetValues(Global.back, XtNsensitive, True, NULL);
    RaiseWindow(dpy, XtWindow(Global.toplevel));
}

static void
PatternSelectCallback(Widget w, XtPointer wlArg, XtPointer junk)
{
    LocalInfo *info = (LocalInfo *) wlArg;
    PatternEdit(info->paint, 
                info->pixels, info->patterns, (void *)info->rcInfo->brushes,
		info->npixels, info->npatterns, info->rcInfo->nbrushes);
    if (Global.patternshell)
        RaiseWindow(XtDisplay(Global.patternshell), 
                    XtWindow(Global.patternshell));
}

#if 0
void 
checkPaintDimension(Widget shell)
{
    Dimension dw, dh;
    int u, v, up, vp, uset=0, vset=0;

    GetPaintWH(&u, &v);
    XtVaGetValues(shell, XtNwidth, &dw, XtNheight, &dh, NULL);
    if (u>50) {
        up = WidthOfScreen(XtScreen(shell));
	if (u > up-14) u = up-14;
#if defined(XAW3D) || defined(XAW95)
        u -= 4;
#endif
        XtVaSetValues(shell, XtNwidth, u, NULL);
	uset = 1;
	
    }
    if (v>30) {
        vp = HeightOfScreen(XtScreen(shell));
	if (v > vp-34) v = vp-34;
#if defined(XAW3D) || defined(XAW95)
        v -= 4;
#endif
        XtVaSetValues(shell, XtNheight, v, NULL);
	vset = 1;
    }
    if (!uset) u = dw;
    if (!vset) v = dh;
    if (uset || vset)
        XResizeWindow(XtDisplay(shell), XtWindow(shell), u, v);
}
#endif

/*
 *  Key actions on selected regions
 */

void 
selectKeyPress(Widget w, void * arg, XKeyEvent * event, void * junk)
{
    WidgetList wlist;
    LocalInfo * info = (LocalInfo *)arg;
    KeySym keysym;
    Boolean locked = False;
    int len, dx, dy;
    char buf[21];
  
    /* Here we are in text operation - don't interfere with key shortcuts */
    if (getIndexOp() == 10) return;

    len = XLookupString(event, buf, sizeof(buf) - 1, &keysym, NULL);
    dx = dy = 0;

    switch (keysym) {
    case XK_Delete:
    case XK_BackSpace:
        PwRegionClear(w);
        return;
    case XK_numbersign:
        if (info) {
            XtVaGetValues(info->paint, XtNlocked, &locked, NULL);
            XtVaSetValues(info->paint, XtNlocked, !locked, NULL);
            XtVaSetValues(info->resize, XtNstate, !locked, NULL);
            canvasResizeToggleCallback(w, info, NULL);
	}
        break;
    case XK_Up:
	dy = -1;
	break;
    case XK_Down:
	dy = 1;
	break;
    case XK_Left:
	dx = -1;
	break;
    case XK_Right:
	dx = 1;
	break;
    case XK_Escape:
        PopdownMenusGlobal();
        PwRegionFinish(w, True);
        Global.escape = True;
        return;
    case XK_space:
        if (event->state & ControlMask) {
	    Global.transparent = !Global.transparent;
            XtVaGetValues(w, XtNmenuwidgets, &wlist, NULL);
            if (wlist) {
                MenuCheckItem(wlist[W_SELECTOR_TRANSPARENT],
                              Global.transparent);
                MenuCheckItem(wlist[W_TOPMENU+W_SELECTOR_TRANSPARENT],
                              Global.transparent);
	    }
            if (!Global.transparent)
                XtVaSetValues(w, XtNtransparent, 2, NULL);
        } else {
	    int value;
            XtVaGetValues(w, XtNtransparent, &value, NULL);
            value = 3-(value&1);
            XtVaSetValues(w, XtNtransparent, value, NULL);
	}
        PwRegionTear(w);
	RegionTransparency((PaintWidget) w);
	break;
    default:
        return;
    }
    
    if (event->state & ControlMask) {
	dx *= 5;
	dy *= 5;
    }

    if (info) {
        XtVaGetValues(info->paint, XtNlocked, &locked, NULL);
        if (locked) {
            int x, y, z;
            XtVaGetValues(info->paint, XtNzoom, &z, NULL);
            XtVaGetValues(info->paint, XtNdrawWidth, &x, NULL);
            XtVaGetValues(info->paint, XtNdrawHeight, &y, NULL);
            x += dx;
            y += dy;
            resizeCanvas(info, x, y, 0);
            return;
	}
    }

    if (dx || dy) {
	RegionMove((PaintWidget) w, dx, dy);
	RegionTransparency((PaintWidget) w);
	return;
    }
}

void 
selectKeyRelease(Widget w, void * l, XKeyEvent * event, void * info)
{
    KeySym keysym;
    int len;
    char buf[21];

    len = XLookupString(event, buf, sizeof(buf) - 1, &keysym, NULL);

    /* Noop at this time */
}

/*
 * This assumes that you either
 *  - specify a pixmap, in which case width and height are taken from that, or
 *  - specify width and height, in which case the pixmap is not needed.
 * Returns the created PaintWidget.
 */

WidgetList InitializedWidgetList()
{
    int i, j;
    WidgetList wlist;

    /* Store menu widgets for later reference */
    wlist = (WidgetList) XtMalloc(W_NWIDGETS * sizeof(Widget));
    for (i=0; i<W_NWIDGETS; i++) wlist[i] = None;
    wlist[W_FILE_REVERT] = popupFileMenu[P_FILE_REVERT].widget;
    wlist[W_EDIT_PASTE] = popupEditMenu[P_EDIT_PASTE].widget;
    wlist[W_FONT_WRITE] = popupTextMenu[P_FONT_WRITE].widget;
    wlist[W_REGION_LAST] = popupFilterMenu[P_FILTER_LAST].widget;
    wlist[W_REGION_UNDO] = popupFilterMenu[P_FILTER_UNDO].widget;
    wlist[W_SELECTOR_TRANSPARENT] = popupSelectorMenu[P_SELECTOR_TRANSPARENT].widget;
    wlist[W_SELECTOR_INTERPOLATION] = popupSelectorMenu[P_SELECTOR_INTERPOLATION].widget;
    wlist[W_SELECTOR_GRID] = popupSelectorMenu[P_SELECTOR_GRID].widget;
    wlist[W_SELECTOR_SNAP] = popupSelectorMenu[P_SELECTOR_SNAP].widget;
    wlist[W_MEMORY_RECALL] = popupMemoryMenu[P_MEMORY_RECALL].widget;
    wlist[W_MEMORY_DISCARD] = popupMemoryMenu[P_MEMORY_DISCARD].widget;
    wlist[W_MEMORY_ERASE] = popupMemoryMenu[P_MEMORY_ERASE].widget;

    wlist[W_TOPMENU+W_FILE_REVERT] = fileMenu[FILE_REVERT].widget;
    wlist[W_TOPMENU+W_EDIT_PASTE] = editMenu[EDIT_PASTE].widget;
    wlist[W_TOPMENU+W_FONT_WRITE] = textMenu[FONT_WRITE].widget;
    wlist[W_TOPMENU+W_REGION_LAST] = filterMenu[FILTER_LAST].widget;
    wlist[W_TOPMENU+W_REGION_UNDO] = filterMenu[FILTER_UNDO].widget;
    wlist[W_TOPMENU+W_SELECTOR_TRANSPARENT] = selectorMenu[SELECTOR_TRANSPARENT].widget;
    wlist[W_TOPMENU+W_SELECTOR_INTERPOLATION] = selectorMenu[SELECTOR_INTERPOLATION].widget;
    wlist[W_TOPMENU+W_SELECTOR_GRID] = selectorMenu[SELECTOR_GRID].widget;
    wlist[W_TOPMENU+W_SELECTOR_SNAP] = selectorMenu[SELECTOR_SNAP].widget;
    wlist[W_TOPMENU+W_MEMORY_STACK] = memoryMenu[MEMORY_STACK].widget;
    wlist[W_TOPMENU+W_MEMORY_RECALL] = memoryMenu[MEMORY_RECALL].widget;
    wlist[W_TOPMENU+W_MEMORY_DISCARD] = memoryMenu[MEMORY_DISCARD].widget;
    wlist[W_TOPMENU+W_MEMORY_SCROLL] = memoryMenu[MEMORY_SCROLL].widget;
    wlist[W_TOPMENU+W_MEMORY_ERASE] = memoryMenu[MEMORY_ERASE].widget;

    for (i=0; i<=ALPHA_DELETE; i++) {
        wlist[W_ALPHA_MODES+i] = popupAlphaMenu[P_ALPHA_MODES+i].widget;
        wlist[W_TOPMENU+W_ALPHA_MODES+i] = alphaMenu[ALPHA_MODES+i].widget;
    }

    for (i=0; i<=NUMBER_LINEWIDTHS; i++) {
        j = i+(i==NUMBER_LINEWIDTHS);
        wlist[W_LINE_WIDTHS+i] = popupLineMenu[j+1].widget;
        wlist[W_TOPMENU+W_LINE_WIDTHS+i] = lineMenu[j].widget;
    }
    for (i=0; i<=NUMBER_PREDEF_FONTS; i++) {
        j = i+(i==NUMBER_PREDEF_FONTS);
        wlist[W_FONT_DESCR+i] = popupTextMenu[j+1].widget;
        wlist[W_TOPMENU+W_FONT_DESCR+i] = textMenu[j].widget;
    }
    return wlist;
}

void
CheckMarkItems(Widget paint, WidgetList wlist)
{
  int i, j, k, l, m, u;

    if (head) {
        struct paintWindows *cur;
        wlist = NULL;
	cur = head;
	while (!wlist && cur) {
            XtVaGetValues(cur->paint, XtNmenuwidgets, &wlist, NULL);
	    cur = cur->next;
	}
    }
    if (head && wlist) {
        i = 0;
        XtVaGetValues(head->paint, XtNlineWidth, &i, NULL);
        for (j=0; j<=5; j++) 
	    if (IsItemChecked(wlist[W_LINE_WIDTHS+j])) break;
        for (k=0; k<NUMBER_PREDEF_FONTS; k++) 
	    if (IsItemChecked(wlist[W_FONT_DESCR+k])) break;
	l = IsItemChecked(wlist[W_SELECTOR_GRID]);
	m = IsItemChecked(wlist[W_SELECTOR_SNAP]);
    } else {
        i = 0;
	j = 0;
        k = -1;
        for (u=0; u<NUMBER_PREDEF_FONTS; u++) {
	    if (!strcasecmp(fontNames[u], Global.xft_name)) {
	        k = u;
                break;
	    }
	}
	l = 0;
	m = 0;
    }

    GraphicAdd(paint);

    GraphicAll(setLineWidth, (void *)(long)i);
    GraphicAll(setLineWidthMenu, (void *)(long)j);
    GraphicAll(fontMenuCallback, (void *)(long)k);
    GraphicAll(setPasteItemMenu, NULL);
}

static void
zoomMenuSelect(Widget w, XtPointer arg, XtPointer garbage)
{
     LocalInfo * info = (LocalInfo *)arg;
     char *val, *ptr;
     int zoom;
     XtVaGetValues(w, XtNlabel, &val, NULL);
     if (!val) return;
     ptr = strchr(val,'=');
     if (!ptr) return;
     zoom =StrToZoom(ptr+1);
     if (zoom<-9) zoom = -9;
     if (zoom>32) zoom = 32;
     if (zoom==0) zoom = 1;
     setZoom(info->paint, zoom);

     w = XtParent(w);
     if (w) w = XtParent(w);
     if (w) {
       XtVaSetValues(w, XtNlabel, ZoomToStr(zoom), NULL);
       XtVaSetValues(w, XtNwidth, 21, XtNheight, 22, NULL);
     }
}

void
AdjustCanvasSize(LocalInfo *info)
{
  Dimension w, h, wp, hp, ws, hs, dim;
    XEvent event;

    XtVaGetValues(info->paint, XtNwidth, &w, XtNheight, &h, NULL);
    ws = (Dimension) WidthOfScreen(XtScreen(info->paint));
    hs = (Dimension) HeightOfScreen(XtScreen(info->paint));
    dim = info->vsteps*CELL_WIDTH+51;

#if defined(XAW3D) || defined(XAW95)
    if (w > ws-24)  wp = ws-24;  else wp = w;
    if (h > hs-117) hp = hs-117; else hp = h;
    if (wp<640) wp = 640; if (wp > ws-24) wp = ws-24;
    if (hp<50) hp = 50;

    if (wp >= w && hp >= h && wp <= ws-14 && hp <= hs-34) {
        XtVaSetValues(info->viewport, XtNwidth, wp, XtNheight, hp, NULL);
        XtVaSetValues(info->shell, XtNwidth, wp+14, XtNheight, hp+dim, NULL);
    } else {
        if (wp <= ws-40)  wp = wp+16; else wp = ws - 24;
        if (hp <= hs-133) hp = hp+16; else hp = hs - 117;
        XtVaSetValues(info->viewport, XtNwidth, wp, XtNheight, hp, NULL);
        XtVaSetValues(info->shell, XtNwidth, wp+14, XtNheight, hp+dim, NULL);
        XtVaSetValues(info->viewport, XtNallowHoriz, True,
                      XtNallowVert, True, NULL);
    }
#else
    if (w > ws-20)  wp = ws-20;  else wp = w;
    if (h > hs-113) hp = hs-113; else hp = h;
    if (wp<651) wp = 651; if (wp > ws-20) wp = ws-20;
    if (hp<50) hp = 50;

    if (wp >= w && hp >= h && wp <= ws-10 && h <= hs-30) {
        XtVaSetValues(info->viewport, XtNwidth, wp, XtNheight, hp, NULL);
        XtVaSetValues(info->shell, XtNwidth, wp+10, XtNheight, hp+dim, NULL);
        XtVaSetValues(info->viewport, XtNallowHoriz, False,
                      XtNallowVert, False, NULL);
    } else {
        if (wp <= ws-36) wp = wp+16; else wp = ws - 20;
        if (hp <= hs-129) hp = hp+16; else hp = hs - 113;
        XtVaSetValues(info->viewport, XtNwidth, wp, XtNheight, hp, NULL);
        XtVaSetValues(info->shell, XtNwidth, wp+10, XtNheight, hp+dim, NULL);
        XtVaSetValues(info->viewport, XtNallowHoriz, True,
                      XtNallowVert, True, NULL);
    }
#endif

    event.type = ConfigureNotify;
    shellHandler(info->shell, info, &event, NULL);

    XtSetMinSizeHints(info->shell, 120, 120);
}


Widget
graphicCreate(Widget shell, int width, int height, int zoom,
	      Pixmap pix, Colormap cmap, unsigned char *alpha)
{
    Display *dpy = XtDisplay(shell);
    Widget form, viewport;
    Widget paint;
    Widget pane, paintbox;
    Widget bar, position, resize, palette, memory, mempopup;
    Widget colors, tool, brush, font, linestyle;
    WidgetList wlist;
    PaintWidget pw;
    Palette *map;
    XColor xc;
    Pixel gridcolor, white, black;
    int depth;
    int i;
    LocalInfo *info = XtNew(LocalInfo);
    char name[80];

    PopdownMenusGlobal();
    info->shell = shell;

    if (cmap == None) {
	map = PaletteCreate(shell);
	cmap = map->cmap;
    } else {
	map = PaletteFind(shell, cmap);
    }
    depth = map->depth;
    XtVaSetValues(shell, XtNcolormap, cmap, NULL);
    Global.canvas = shell;
    XtVaSetValues(Global.back, XtNsensitive, True, NULL);

    PaletteAddUser(map, shell);

    info->map = map;
    info->channel = 3;
    info->vsteps = 3;
    info->palette_pixmap = None;
    info->mem_pixmap = None;
    info->rcInfo = NULL;
    info->mem_index = Global.numregions-1;
    info->barheight = 75;

    pane = XtVaCreateManagedWidget("pane",
				   panedWidgetClass, shell,
				   NULL);
    info -> pane = pane;
    /*
     *	Menu area
     */
    form = XtVaCreateManagedWidget("form",
				   formWidgetClass, pane, 
				   NULL);
    info -> form = form;

    /*
     *	Menu Bar
     */
   
    if (menuBar[7].nitems == 0) {
        /* create zoom menu on first invocation */
        menuBar[7].nitems = ZOOMMENU_VALUES;
        for (i = 0; i<ZOOMMENU_VALUES; i++) {
	    if (i<=15)
 	        sprintf(name, "zoom = %d", i+1);
            else
	    if (i==16)
	        *name = '\0';
            else
 	        sprintf(name, "zoom = 1:%d", i-15);
            memset(&zoomMenu[i], 0, sizeof(PaintMenuItem));
            zoomMenu[i].name = strdup(name);
            zoomMenu[i].callback = (PaintMenuCallback)zoomMenuSelect;
	}
    }

    for (i = 0; i<ZOOMMENU_VALUES; i++) zoomMenu[i].data = info;
    bar = MenuBarCreate(form, XtNumber(menuBar), menuBar);
    info->bar = bar;
    info->zoom = XtParent(menuBar[7].widget);
    strcpy(name, ZoomToStr(zoom));
    XtVaSetValues(info->zoom, 
                  XtNlabel, name, XtNwidth, 21, XtNheight, 22, NULL);
    XtVaSetValues(XtParent(menuBar[8].widget), 
                  XtNwidth, 21, XtNheight, 22, NULL);

    XtVaSetValues(bar, XtNhorizDistance, -1, XtNvertDistance, 0, NULL);
    XtVaSetValues(form, XtNshowGrip, True, NULL);

    resize = XtVaCreateManagedWidget("resize",
				  toggleWidgetClass, form,
				  XtNlabel, "#",
				  XtNwidth, 21,
				  XtNheight, 22,
#ifdef XAWPLAIN
				  XtNborderWidth, 1,
#else
				  XtNborderWidth, 0,
#endif
				  XtNstate, False,
				  XtNfromHoriz, bar,
				  XtNhorizDistance, 0,
				  XtNvertDistance, 4,
				  NULL);
    info->resize = resize;
    info->resize_state = 0;

    XtAddCallback(resize, XtNcallback, 
                   (XtCallbackProc) canvasResizeToggleCallback, (XtPointer) info);
    
    white = WhitePixelOfScreen(XtScreen(shell));
    black = BlackPixelOfScreen(XtScreen(shell));

    position = XtVaCreateManagedWidget(msgText[POSITION],
				  labelWidgetClass, form,
				  XtNbackground, white,   
				  XtNborderWidth, 1,
                                  XtNwidth, POS_WIDTH,
				  XtNfromHoriz, resize,
				  XtNvertDistance, 4,
				  XtNhorizDistance, 3,
				  NULL);
    info->position = position;
    info->boolpos = True;
    XtInsertRawEventHandler(position, 
			    ButtonPressMask | ButtonReleaseMask,
                            False, (XtEventHandler) popupHandler, info, 
                            XtListHead);

    linestyle = XtVaCreateManagedWidget("linestyle",
				        coreWidgetClass, form,
					XtNwidth, LINESTYLE_WIDTH,
					XtNheight, LINESTYLE_HEIGHT,
					XtNfromHoriz, position,
					XtNvertDistance, 4,
					XtNhorizDistance, 10,
					NULL);
    info->linestyle = linestyle;
    XtInsertRawEventHandler(linestyle, 
			    ButtonPressMask | ButtonReleaseMask,
                            False, (XtEventHandler) popupHandler, info, 
                            XtListHead);


    palette = XtVaCreateManagedWidget("palette",
				  coreWidgetClass, form,
                                  XtNwidth, PALETTE_SIZE*CELL_WIDTH+32, 
				  XtNheight, CELL_WIDTH*info->vsteps-1,
				  XtNfromVert, bar,
				  XtNvertDistance, 0,
				  XtNhorizDistance, 2,
				  NULL);
    info->palette = palette;

    memory = XtVaCreateManagedWidget("memory",
				  coreWidgetClass, form,
				  XtNwidth, (2+MEMSLOT_SIZE)*MEMORY_HSTEPS+3, 
				  XtNheight, CELL_WIDTH*info->vsteps+1,
				  XtNborderWidth, 0, 
				  XtNfromVert, bar,
				  XtNfromHoriz, palette,
				  XtNvertDistance, 0,
				  XtNhorizDistance, 3,
				  NULL);
    info->memory = memory;
    mempopup = MenuPopupCreate(memory, "popup-menu", 
                               XtNumber(memoryMenu), memoryMenu);
    info->mempopup = mempopup;

    colors = XtVaCreateManagedWidget("",
				  commandWidgetClass, form,
                                  XtNwidth, ICONWIDTH, XtNheight, ICONHEIGHT,
				  XtNfromVert, bar,
				  XtNfromHoriz, info->memory,
				  XtNvertDistance, -2,
				  XtNhorizDistance, 4,
				  NULL);
    info->colors = colors;
    XtAddCallback(colors, XtNcallback, 
                   (XtCallbackProc) PatternSelectCallback, (XtPointer) info);
    XtInsertRawEventHandler(colors, 
			    ButtonPressMask | ButtonReleaseMask,
                            False, (XtEventHandler) popupHandler, info, 
                            XtListHead);

    tool = XtVaCreateManagedWidget("tool",
				  commandWidgetClass, form,
                                  XtNwidth, ICONWIDTH, XtNheight, ICONHEIGHT,
				  XtNfromVert, bar,
				  XtNfromHoriz, colors,
				  XtNvertDistance, -2,
				  XtNhorizDistance, 2,
				  NULL);
    info->tool = tool;
    XtAddCallback(tool, XtNcallback, 
                  (XtCallbackProc) ToolSelectCallback, (XtPointer) shell);
    XtInsertRawEventHandler(tool, 
			    ButtonPressMask | ButtonReleaseMask,
                            False, (XtEventHandler) popupHandler, info, 
                            XtListHead);

    brush = XtVaCreateManagedWidget("brush",
 				  commandWidgetClass, form,
				  XtNforeground, black,
                                  XtNwidth, ICONWIDTH, XtNheight, ICONHEIGHT,
				  XtNfromVert, bar,
  				  XtNfromHoriz, tool,
				  XtNvertDistance, -2,
  				  XtNhorizDistance, 2,
				  NULL);
    info->brush = brush;
    XtAddCallback(brush, XtNcallback,
                   (XtCallbackProc) BrushSelectCallback, (XtPointer) NULL);
    XtInsertRawEventHandler(brush, 
			    ButtonPressMask | ButtonReleaseMask,
                            False, (XtEventHandler) popupHandler, info, 
                            XtListHead);

    sprintf(name, "Abc\n:%g", Global.xft_size);
    font = XtVaCreateManagedWidget(name,
 				  commandWidgetClass, form,
                                  XtNwidth, ICONWIDTH, XtNheight, ICONHEIGHT,
				  XtNfromVert, bar,
  				  XtNfromHoriz, brush,
				  XtNvertDistance, -2,
  				  XtNhorizDistance, 2,
				  NULL);
    info->font = font;
    XtAddCallback(font, XtNcallback,
        	   (XtCallbackProc) FontSelectCallback, (XtPointer) NULL);
    XtInsertRawEventHandler(font, 
			    ButtonPressMask | ButtonReleaseMask,
                            False, (XtEventHandler) popupHandler, info, 
                            XtListHead);

    /*
     *	Drawing Area
     */
    viewport = XtVaCreateWidget("viewport",
				viewportWidgetClass, pane,
				XtNfromVert, form,
				XtNuseBottom, True,
				XtNuseRight, True,
				XtNtop, XtChainTop,
				NULL);
    info->viewport = viewport;
    XtInsertRawEventHandler(viewport, 
		      ButtonPressMask | ButtonReleaseMask | 
                      KeyPressMask | KeyReleaseMask,
                      False, (XtEventHandler) canvasHandler, info, XtListHead);

    /*
     *	Custom Drawing Widget here
     */
    paintbox = XtVaCreateWidget("paintBox",
			     boxWidgetClass, viewport,
		             XtNbackgroundPixmap,GetBackgroundPixmap(viewport),
			     NULL);
    info->paintbox = paintbox;
    XtInsertRawEventHandler(paintbox,
                      ButtonPressMask | ButtonReleaseMask | 
                      KeyPressMask | KeyReleaseMask,
                      False, (XtEventHandler) canvasHandler, info, XtListHead);

    info->boolcount = 0;

    /*
     *	Try and do something nice for the user
     */
    if (pix != None)
	GetPixmapWHD(dpy, pix, &width, &height, NULL);
    if (zoom == 0 && width <= 64 && height <= 64)
	zoom = 6;
    else
    if (zoom==0) 
        zoom = Global.default_zoom;

    if (depth>8) {
        if (XAllocNamedColor(dpy, cmap, "#cccccc", &xc, &xc) != 0)
            gridcolor = xc.pixel;
        else
	    gridcolor = black;
    } else
	gridcolor = BlackPixelOfScreen(XtScreen(shell));

    paint = XtVaCreateManagedWidget("paint",
				    paintWidgetClass, paintbox,
				    XtNdrawWidth, width,
				    XtNdrawHeight, height,
				    XtNzoom, zoom,
				    XtNcolormap, cmap,
				    XtNallowResize, True,
			            XtNinterpolation, 1,
			            XtNtransparent, 0,
                                    XtNgridcolor, gridcolor,
				    XtNundoSize, GetDefaultUndosize(),
            		            XtNfillRule, FillSolid,
			            XtNlineFillRule, FillSolid,
			            XtNpattern, None,
			            XtNlinePattern, None,
				    NULL);
    info->paint = paint;

    /* Hack to get a correct display of the menu bar */
    XtVaSetValues(viewport, XtNwidth, 651, NULL);

    XtSetKeyboardFocus(pane, paint);
    zoomAddChild(paint, zoom);

    OperationSetPaint(paint);
    SetMenuBarVisibility(paint, IsMenuBarGlobal());
    SetFullMenu(paint, IsFullMenuGlobal());

    ccpAddStdPopup(paint, (void *)info);
    prCallback(paint, popupFileMenu[P_FILE_PRINT].widget, True);
    prCallback(paint, popupFileMenu[P_FILE_EXTERN].widget, True);
    prCallback(paint, popupEditMenu[P_EDIT_PASTE].widget, False);
    prCallback(paint, popupSelectorMenu[P_SELECTOR_FATBITS].widget, True);

    XtInsertRawEventHandler(paint, ButtonPressMask,
                      False, (XtEventHandler) canvasHandler, info, XtListHead);
    XtInsertRawEventHandler(paint, PointerMotionMask,
                      False, (XtEventHandler) motionHandler, info, XtListHead);
    XtAddEventHandler(shell, StructureNotifyMask,
                      False, (XtEventHandler) shellHandler, info);
    XtAddEventHandler(form, StructureNotifyMask,
                      False, (XtEventHandler) formHandler, info);
    XtAddEventHandler(position, ButtonPressMask | ButtonReleaseMask,
                      False, (XtEventHandler) positionHandler, info);
    XtInsertRawEventHandler(memory, ButtonPressMask | ButtonReleaseMask,
			    False, (XtEventHandler) memoryHandler, 
                            info, XtListHead);
    XtAddEventHandler(palette, ButtonPressMask | ButtonReleaseMask,
                      False, (XtEventHandler) paletteHandler, info);
    XtAddEventHandler(linestyle, ButtonPressMask | ButtonReleaseMask,
                      False, (XtEventHandler) lineStyleHandler, info);
    XtAddEventHandler(paint, ButtonPressMask,
                      False, (XtEventHandler) mousewheelScroll, (XtPointer) 2);
    XtAddEventHandler(paintbox, ButtonPressMask,
                      False, (XtEventHandler) mousewheelScroll, (XtPointer) 1);
    XtAddEventHandler(paint, KeyPressMask,
		      False, (XtEventHandler) selectKeyPress, info);
    XtAddEventHandler(paint, KeyReleaseMask,
		      False, (XtEventHandler) selectKeyRelease, info);   

    XtManageChild(paintbox);
    XtManageChild(viewport);

    ADDCALLBACK(fileMenu, FILE_OPEN, paint, StdOpenFile);
    ADDCALLBACK(fileMenu, FILE_SAVE, paint, StdSaveFile);
    ADDCALLBACK(fileMenu, FILE_SAVEAS, paint, StdSaveAsFile);
    ccpAddSaveRegion(fileMenu[FILE_SAVE_REGION].widget, paint);
    ADDCALLBACK(fileMenu, FILE_LOAD_MEMORY, info, loadMemory);
    ADDCALLBACK(fileMenu, FILE_REVERT, paint, revertCallback);
    prCallback(paint, fileMenu[FILE_REVERT].widget, False);
    ADDCALLBACK(fileMenu, FILE_LOADED, paint, loadedCallback);
    ADDCALLBACK(fileMenu, FILE_PRINT, paint, printCallback);
    ADDCALLBACK(fileMenu, FILE_EXTERN, paint, externCallback);
    XtAddCallback(fileMenu[FILE_CLOSE].widget, XtNcallback, 
		     (XtCallbackProc) closeCallback, (XtPointer) info);

    ccpAddUndo(editMenu[EDIT_UNDO].widget, paint);
    ccpAddRedo(editMenu[EDIT_REDO].widget, paint);
    ADDCALLBACK(editMenu, EDIT_UNDO_SIZE, paint, undosizeCallback);
    ccpAddRefresh(editMenu[EDIT_REFRESH].widget, paint);
    ccpAddCut(editMenu[EDIT_CUT].widget, paint);
    ccpAddCopy(editMenu[EDIT_COPY].widget, paint);
    ccpAddPaste(editMenu[EDIT_PASTE].widget, paint);
    ccpAddClear(editMenu[EDIT_CLEAR].widget, paint);
    ADDCALLBACK(editMenu, EDIT_SELECT_ALL, paint, StdSelectAllCallback);
    ADDCALLBACK(editMenu, EDIT_UNSELECT, paint, unselectRegion);   
    ccpAddDuplicate(editMenu[EDIT_DUP].widget, paint);
    ADDCALLBACK(editMenu, EDIT_ERASE_ALL, paint, StdEraseAllCallback);
    ADDCALLBACK(editMenu, EDIT_CLONE_CANVAS, paint, StdCloneCanvasCallback);
    ADDCALLBACK(editMenu, EDIT_CLONE_CANVAS1, paint, StdCloneCanvasCallback);
    ADDCALLBACK(editMenu, EDIT_SNAPSHOT, info, StdScreenshotCallback);

    ADDCALLBACK(textMenu, FONT_SELECT, info, StdFontSet);
    ADDCALLBACK(textMenu, FONT_WRITE, info, StdWriteText);
    prCallback(paint, textMenu[FONT_WRITE].widget, False);

    ADDCALLBACK(regionMenu, REGION_FLIPX, paint, StdRegionFlipX);
    ADDCALLBACK(regionMenu, REGION_FLIPY, paint, StdRegionFlipY);
    for (i = 0; i < XtNumber(rotateMenu); i++) {
	if (rotateMenu[i].name[0] == '\0')
	    continue;

	ADDCALLBACK(rotateMenu, i, paint, rotateTo);
        XtAddCallback(paint, XtNregionCallback, (XtCallbackProc) prCallback,
		      (XtPointer) rotateMenu[i].widget);
	prCallback(paint, rotateMenu[i].widget, False);
    }


    ADDCALLBACK(regionMenu, REGION_ROTATE, paint, rotate);
    XtAddCallback(paint, XtNregionCallback, (XtCallbackProc) prCallback,
		  (XtPointer) regionMenu[REGION_ROTATE].widget);
    prCallback(paint, regionMenu[REGION_ROTATE].widget, False);

    ADDCALLBACK(regionMenu, REGION_EXPAND, paint, expandRegion);
    ADDCALLBACK(regionMenu, REGION_DOWNSCALE, paint, downscaleRegion);   
    ADDCALLBACK(regionMenu, REGION_LINEAR, paint, linearRegion);
    ADDCALLBACK(regionMenu, REGION_RESET, paint, resetMat);
    XtAddCallback(paint, XtNregionCallback, (XtCallbackProc) prCallback,
		  (XtPointer) regionMenu[REGION_RESET].widget);
    prCallback(paint, regionMenu[REGION_RESET].widget, False);

    ADDCALLBACK(regionMenu, REGION_CROP, paint, cropToRegion);
    ADDCALLBACK(regionMenu, REGION_DELIMIT, paint, StdRegionDelimit);
    ADDCALLBACK(regionMenu, REGION_COMPLEMENT, paint, StdRegionComplement);
    ccpAddCloneRegion(regionMenu[REGION_CLONE].widget, paint);
    ccpAddOCRRegion(regionMenu[REGION_OCR].widget, paint);
    XtAddCallback(paint, XtNregionCallback, (XtCallbackProc) prCallback,
		  (XtPointer) regionMenu[REGION_CROP].widget);
    prCallback(paint, regionMenu[REGION_CROP].widget, False);
    ADDCALLBACK(regionMenu, REGION_AUTOCROP, paint, autocropCallback);

    ADDCALLBACK(filterMenu, FILTER_INVERT, paint, StdRegionInvert);
    ADDCALLBACK(filterMenu, FILTER_SHARPEN, paint, StdRegionSharpen);
    ADDCALLBACK(filterMenu, FILTER_SMOOTH, paint, doSmooth);
    ADDCALLBACK(filterMenu, FILTER_DIRFILT, paint, StdRegionDirFilt);
    ADDCALLBACK(filterMenu, FILTER_EDGE, paint, StdRegionEdge);
    ADDCALLBACK(filterMenu, FILTER_EMBOSS, paint, StdRegionEmboss);
    ADDCALLBACK(filterMenu, FILTER_OIL, paint, oilPaint);
    ADDCALLBACK(filterMenu, FILTER_NOISE, paint, addNoise);
    ADDCALLBACK(filterMenu, FILTER_SPREAD, paint, doSpread);
    ADDCALLBACK(filterMenu, FILTER_PIXELIZE, paint, doPixelize);
    ADDCALLBACK(filterMenu, FILTER_DESPECKLE, paint, doDespeckle);
    ADDCALLBACK(filterMenu, FILTER_TILT, paint, tiltRegion);
    ADDCALLBACK(filterMenu, FILTER_BLEND, paint, StdRegionBlend);
    ADDCALLBACK(filterMenu, FILTER_SOLARIZE, paint, doSolarize);
    ADDCALLBACK(filterMenu, FILTER_TOGREY, paint, StdRegionGrey);
    ADDCALLBACK(filterMenu, FILTER_TOMASK, paint, StdRegionBWMask);
    ADDCALLBACK(filterMenu, FILTER_CONTRAST, paint, doContrast);
    ADDCALLBACK(filterMenu, FILTER_MODIFY_RGB, paint, doModifyRGB);
    ADDCALLBACK(filterMenu, FILTER_QUANTIZE, paint, doQuantize);
    ADDCALLBACK(filterMenu, FILTER_USERDEF, paint, StdRegionUserDefined);
    ADDCALLBACK(filterMenu, FILTER_LAST, paint, doLast);
    prCallback(paint, filterMenu[FILTER_LAST].widget, False);
    ADDCALLBACK(filterMenu, FILTER_UNDO, paint, StdRegionUndo);
    prCallback(paint, filterMenu[FILTER_UNDO].widget, False);

    ADDCALLBACK(selectorMenu, SELECTOR_PATTERNS, info, PatternSelectCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_CHROMA, paint, selectColorRange);
    ADDCALLBACK(selectorMenu, SELECTOR_BACKGROUND, paint, changeBackground);
    ADDCALLBACK(selectorMenu, SELECTOR_FATBITS, paint, fatCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_TOOLS, GetShell(paint), ToolSelectCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_BRUSH, NULL, BrushSelectCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_FONT, NULL, FontSelectCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_LUPE, NULL, LupeCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_SCRIPT, NULL, ScriptEditor);
    ADDCALLBACK(selectorMenu, SELECTOR_CHANGE_SIZE, info, WHZSizeCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_SIZE_ZOOM_DEFS, paint, defaultWHZSizeCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_CHANGE_ZOOM, info, zoomCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_SNAP, paint, snapCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_SNAP_SPACING, paint, snapSpacingCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_TRANSPARENT, paint, transparencyCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_INTERPOLATION, paint, interpolationCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_GRID, paint, gridCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_GRID_PARAM, paint, gridParamCallback);
    ADDCALLBACK(selectorMenu, SELECTOR_SIMPLE, info, simpleMenuPopup);
    ADDCALLBACK(selectorMenu, SELECTOR_HIDE_MENUBAR, info, hideMenuBar);

    MenuCheckItem(selectorMenu[SELECTOR_TRANSPARENT].widget, True);
    MenuCheckItem(selectorMenu[SELECTOR_INTERPOLATION].widget, True);
    MenuCheckItem(selectorMenu[SELECTOR_SIMPLE].widget, !IsFullMenuSet(paint));

    for (i=0; i<4; i++)
    ADDCALLBACK(alphaMenu, ALPHA_MODES+i, info, alphaModeCallback);
    ADDCALLBACK(alphaMenu, ALPHA_PARAMS, paint, alphaParametersCallback);
    ADDCALLBACK(alphaMenu, ALPHA_CREATE, paint, createAlphaCallback);
    ADDCALLBACK(alphaMenu, ALPHA_EDIT, paint, editAlphaCallback);
    ADDCALLBACK(alphaMenu, ALPHA_SAVE, paint, saveAlphaCallback);
    ADDCALLBACK(alphaMenu, ALPHA_DELETE, paint, deleteAlphaCallback);
    XtVaSetValues(info->zoom, XtNwidth, 21, XtNheight, 22, NULL);

    ADDCALLBACK(memoryMenu, MEMORY_STACK, info, memoryStackCallback);
    ADDCALLBACK(memoryMenu, MEMORY_RECALL, info, memoryRecallCallback);
    ADDCALLBACK(memoryMenu, MEMORY_DISCARD, info, memoryDiscardCallback);
    ADDCALLBACK(memoryMenu, MEMORY_SCROLL, info, memoryScrollCallback);
    ADDCALLBACK(memoryMenu, MEMORY_ERASE, info, memoryEraseCallback);

    wlist = InitializedWidgetList();
    wlist[W_ICON_PALETTE] = palette;
    wlist[W_ICON_COLORS] = colors;
    wlist[W_ICON_TOOL] = tool;
    wlist[W_ICON_BRUSH] = brush;
    wlist[W_ICON_FONT] = font;
    wlist[W_ICON_LINESTYLE] = linestyle;
    wlist[W_ICON_COLPIXMAP] = (Widget)
      XCreatePixmap(dpy, DefaultRootWindow(dpy), ICONWIDTH, ICONHEIGHT, depth);
    wlist[W_INFO_DATA] = (Widget) info;
    XtVaSetValues(paint, XtNmenuwidgets, wlist, NULL);

    if (!IsMenuBarVisible(paint)) XtUnmanageChild(form);   

    AddDestroyCallback(shell,
		       (DestroyCallbackFunc) closeCallback, (void *) info);

    XtRealizeWidget(shell);

    ManageQuirk(info);
    wlist[W_ICON_LINEPIXMAP] = (Widget)
      XCreatePixmap(dpy, DefaultRootWindow(dpy), LINESTYLE_WIDTH, LINESTYLE_HEIGHT, depth);
    XDefineCursor(dpy, XtWindow(viewport), XCreateFontCursor(dpy, XC_left_ptr));
                    
    XFlush(dpy);

    XtVaSetValues(colors, XtNbackgroundPixmap, 
                          (Pixmap) wlist[W_ICON_COLPIXMAP], 
		          XtNwidth, ICONWIDTH, XtNheight, ICONHEIGHT, NULL);
    XtVaSetValues(paint, XtNdirty, False, NULL);

    SetIconImage(shell);
    setPalettePixmap(paint, info, 0);
    setToolIconPixmap(paint, NULL);
    setCanvasColorsIcon(paint, NULL);
    setBrushIconPixmap(paint, NULL);
    setMemoryIcon(paint, info);
    CheckMarkItems(paint, wlist);
    setStandardCursor(paint);

    if (pix) {
        XRectangle rect;
        rect.width = width;
        rect.height = height;
        PwRegionSet(paint, &rect, pix, None);
        PwRegionSet(paint, NULL, None, None);
    }

    pw = (PaintWidget) paint;
    if (alpha) {
	pw->paint.current.alpha = alpha;
        pw->paint.alpha_mode = 2;
    } else {
        pw->paint.current.alpha = NULL;
        pw->paint.alpha_mode = 0;
        for (i=1; i<=ALPHA_DELETE; i++)
        if (i!=ALPHA_PARAMS && i!=ALPHA_CREATE && i!=ALPHA_SAVE) {
            prCallback(paint, alphaMenu[ALPHA_MODES+i].widget, False);
            prCallback(paint, popupAlphaMenu[P_ALPHA_MODES+i].widget, False);
	}
    }

    MenuCheckItem(alphaMenu[ALPHA_MODES+pw->paint.alpha_mode].widget, True);
    MenuCheckItem(popupAlphaMenu[P_ALPHA_MODES+pw->paint.alpha_mode].widget, True);

    /* Avoid unnecessary scrollbars */
    AdjustCanvasSize(info);

    return paint;
}

void
CreateCanvas(Widget w, int width, int height, int zoom)
{
    graphicCreate(makeGraphicShell(w), width, height, zoom, None, None, NULL);
}


typedef struct cwi_s {
    Widget paint;
    void *id;
    struct cwi_s *next;
} CanvasWriteInfo;

static CanvasWriteInfo *cwiHead = NULL;

static void
removeCWI(Widget w, CanvasWriteInfo * ci, XtPointer junk)
{
    CanvasWriteInfo *cur = cwiHead, **pp = &cwiHead;

    while (cur != NULL && cur != ci) {
	pp = &cur->next;
	cur = cur->next;
    }

    if (cur == NULL)
	return;
    *pp = cur->next;
    XtFree((XtPointer) ci);
}

void *
GraphicGetReaderId(Widget paint)
{
    CanvasWriteInfo *cur;

    paint = GetShell(paint);

    for (cur = cwiHead; cur != NULL && cur->paint != paint; cur = cur->next);

    if (cur == NULL)
	return NULL;

    return cur->id;
}

void
AddFileToGlobalList(char * file)
{
    int i;
    char buf[256];
    char *path;

    if (!file) return;
    if (!*file) return;
    if (*file == '/')
        path = strdup(file);
    else {
        if (getcwd(buf, 256)) {
	    path = (char *)xmalloc(strlen(buf)+strlen(file)+3);
            sprintf(path, "%s/%s", buf, file);
	} else
            path = strdup(file);
    }
    for (i=0; i<Global.numfiles; i++)
        if (!strcmp(path, Global.loadedfiles[i])) {
	    free(path);
            return;
        }
    i = Global.numfiles;
    ++Global.numfiles;
    Global.loadedfiles = (char **) 
	realloc(Global.loadedfiles, Global.numfiles*sizeof(char*));
    Global.loadedfiles[i] = path;
}

void
RemoveFileFromGlobalList(char * file)
{
    int i, j;
    char buf[256];
    char *path;

    if (!file) return;
    if (!*file) return;
    if (*file == '/')
        path = strdup(file);
    else {
        if (getcwd(buf, 256)) {
	    path = (char *)xmalloc(strlen(buf)+strlen(file)+3);
            sprintf(path, "%s/%s", buf, file);
	} else
            path = strdup(file);
    }
    for (i=0; i<Global.numfiles; i++)
        if (!strcmp(path, Global.loadedfiles[i])) {
	    free(path);
            free(Global.loadedfiles[i]);
            --Global.numfiles;
            for (j=i; j<Global.numfiles; j++)
                Global.loadedfiles[j] = Global.loadedfiles[j+1];
	    return;
	}
}

Widget
GraphicOpenFileZoom(Widget w, char *file, XtPointer imageArg, int zoom)
{
    Image *image = (Image *) imageArg;
    unsigned char *alpha;
    Colormap cmap;
    Pixmap pix = None;
    Widget shell = makeGraphicShell(w);
    CanvasWriteInfo *ci = XtNew(CanvasWriteInfo);
    PaintWidget paint;

    ci->next = cwiHead;
    cwiHead = ci;
    ci->paint = shell;
    ci->id = GetFileNameGetLastId();

    XtAddCallback(shell, XtNdestroyCallback,
		  (XtCallbackProc) removeCWI, (XtPointer) ci);

    if (image && image->alpha) {
         alpha = image->alpha;
         image->alpha = NULL;
    } else
         alpha = NULL;

    if (ImageToPixmap(image, shell, &pix, &cmap)) {
	/*
	 * If mask != None, set the mask region color to the BG color of the Canvas
	 */
        AddFileToGlobalList(file);
	if ((paint = (PaintWidget)
	     graphicCreate(shell, 0, 0, zoom, pix, cmap, alpha))!=None) {
	    char *cp = strrchr(file, '/');
	    if (cp == NULL)
		cp = file;
	    else
		cp++;
            StoreName(shell, file);
	    cp = (char *)xmalloc(strlen(file) + 1);
	    strcpy(cp, file);
	    paint->paint.filename = cp;
	    EnableRevert((Widget) paint);
            XtVaSetValues((Widget)paint, XtNdirty, False, NULL);
            if (file_isSpecialImage)
                GetFileName(Global.toplevel, BROWSER_LOADED, NULL, NULL, NULL);
            SetAlphaMode((Widget)paint, -1);
	    return (Widget) paint;
	} else {
	    XtDestroyWidget(shell);
	}
    } else {
	Notice(w, msgText[UNABLE_TO_CREATE_PAINT_WINDOW_WITH_IMAGE]);
	XtDestroyWidget(shell);
    }
    return NULL;
}

void
GraphicOpenFile(Widget w, XtPointer fileArg, XtPointer imageArg)
{
    GraphicOpenFileZoom(w, (char *) fileArg, imageArg, Global.default_zoom);
}

/*
 * 0: Create new (blank) canvas
 * 1: Open a file
 * 2: Create new (blank) canvas, querying for size
 */
void
GraphicCreate(Widget w, int value)
{
    switch (value) {
    case 0:
	graphicCreate(makeGraphicShell(w), 
                      Global.default_width, Global.default_height, 
                      Global.default_zoom, None, None, NULL);
	break;
    case 1:
        WHZSizeSelect(w, NULL, 1);
	break;
    case 2:
	GetFileName(GetToplevel(w), BROWSER_MULTIREAD, 
                    NULL, GraphicOpenFile, NULL);
	break;
    case 3:
	GetFileName(GetToplevel(w), BROWSER_LOADED, 
                    NULL, GraphicOpenFile, NULL);
        break;
    case 4:
        StartMagnifier(w);
        break;
    }
}


/* This starts what used to be cutCopyPaste.c */
/*
static void 
setToSelectOp(void)
{
    static String names[2] = {"selectBox", "selectArea"};
    OperationSet(names, 2);
}
*/

static void 
selectionLost(Widget w, Atom * selection)
{
    selectionOwner = False;
}
static void 
selectionDone(Widget w, Atom * selection, Atom * target)
{
    /* empty */
}
static Boolean
selectionConvert(Widget w, Atom * selection, Atom * target,
		 Atom * type, XtPointer * value, unsigned long *len,
		 int *format)
{
    Pixmap src = Global.clipboard.pix;
    Pixmap *pix, np;
    GC gc;
    int wth, hth, dth;
    static Atom A_TARGETS = 0;

    if (src == None)
	return False;

    if (A_TARGETS == 0)
      A_TARGETS = XInternAtom (XtDisplay(w), "TARGETS", False);

    if (*target == A_TARGETS)
      {
	Atom *targets;
	int num_targets = 3;

	targets = (Atom *) XtMalloc (sizeof (Atom) * num_targets);
	targets[0] = XA_PIXMAP;
	targets[1] = XA_BITMAP;
	targets[2] = A_TARGETS;
	*type = XA_ATOM;
	*value = (XtPointer) targets;
	*len = num_targets;
	*format = 32;
	return True;
      }
    

    if (*target != XA_PIXMAP && *target != XA_BITMAP)
	return False;

    pix = XtNew(Pixmap);

    GetPixmapWHD(XtDisplay(w), src, &wth, &hth, &dth);
    np = XCreatePixmap(XtDisplay(w), XtWindow(w), wth, hth, dth);
    gc = XCreateGC(XtDisplay(w), np, 0, 0);
    XCopyArea(XtDisplay(w), src, np, gc, 0, 0, wth, hth, 0, 0);
    XFreeGC(XtDisplay(w), gc);

    *pix = np;

    *type = XA_PIXMAP;
    *len = 1;
    *format = 32;
    *value = (XtPointer) pix;

    return True;
}

static void 
setMemoryRegion(Widget w, int x, int y,
                Pixmap pix, Pixmap mask, unsigned char *alpha)
{
    RegionData r;
    Display *dpy = XtDisplay(w);
    GC gc;
    int i, m, width, height, depth;

    r.x = x;
    r.y = y;
    r.pix = pix;
    r.mask = mask;
    r.alpha = alpha;
    CRC_checksum(&r);
    if (CRC_exists(&r)) return;

    /* Duplicate data into region */
    i = Global.numregions;
    ++Global.numregions;
    Global.regiondata = (RegionData *) 
        realloc(Global.regiondata, Global.numregions*sizeof(RegionData));
    Global.regiondata[i] = r;

    GetPixmapWHD(dpy, pix, &width, &height, &depth);
    Global.regiondata[i].pix = XCreatePixmap(dpy, DefaultRootWindow(dpy),
                                             width, height, depth);
    gc = XCreateGC(dpy, pix, 0, 0);
    XCopyArea(dpy, pix, Global.regiondata[i].pix, gc, 0, 0,
	      width, height, 0, 0);
    XFreeGC(dpy, gc);
    Global.regiondata[i].mask = None;
    if (mask) {
        Global.regiondata[i].mask = 
            XCreatePixmap(dpy, DefaultRootWindow(dpy), width, height, 1);
        gc = XCreateGC(dpy, Global.regiondata[i].mask, 0, 0);
        XCopyArea(dpy, mask, Global.regiondata[i].mask, gc, 0, 0,
	          width, height, 0, 0);
        XFreeGC(dpy, gc);
    }
    if (alpha) {
        m = width * height;
        Global.regiondata[i].alpha = (unsigned char*) XtMalloc(m);
        memcpy(Global.regiondata[i].alpha, alpha, m);
    }
}

static void 
StdCloneCanvasCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    PaintWidget pw  = (PaintWidget) paint;
    Display *dpy;
    Pixmap pix, newpix;
    Pixel p;
    XImage *src, *dst;
    int width, height, widthp, heightp, depth, zoom, bool;
    int i, j, k, l, m, zx, zy, x, y, dy, z;
    int interpolation, bytes_per_pixel, zoom_bytes_per_pixel;
    unsigned int u;
    unsigned char *alpha;
    GC gc;

    if (strchr(XtName(w), '1')) bool = 0; else bool = 1;
    dpy = XtDisplay(paint);
    XtVaGetValues(paint, XtNzoom, &zoom, NULL);
    depth = DefaultDepthOfScreen(XtScreen(paint));
    width = pw->paint.drawWidth;
    height = pw->paint.drawHeight;
    pix = GET_PIXMAP(pw);
    gc = XtGetGC(paint, 0, 0);
    StateSetBusy(True);

    if (pw->paint.current.alpha) {
        i = pw->paint.drawWidth * pw->paint.drawHeight;
        alpha = (unsigned char *)xmalloc(i);
        memcpy(alpha, pw->paint.current.alpha, i);
    } else
        alpha = NULL;

    /* Create newpix, since original pixmap may be destroyed ... */
    if (zoom>0 || bool) {
        newpix = XCreatePixmap(dpy,
		    XtWindow(Global.toplevel), width, height, depth);
        XCopyArea(dpy, pix, newpix, gc, 0, 0, width, height, 0, 0);
        graphicCreate(makeGraphicShell(Global.toplevel),
                      width, height, ((bool)?zoom:1), 
                      newpix, None, alpha);
    } else {
        z = -zoom;
        XtVaGetValues(paint, XtNinterpolation, &interpolation, NULL);
        src = XGetImage(dpy, pix, 0, 0, width, height, AllPlanes, ZPixmap);
        widthp = (width+z-1)/z;
        heightp = (height+z-1)/z;
        dst = XCreateImage(dpy, pw->paint.visual, src->depth, src->format,
		           0, NULL, widthp, heightp, 32, 0);
        dst->data = (char *) XtMalloc(heightp * dst->bytes_per_line);
        bytes_per_pixel = src->bits_per_pixel/8;
        zoom_bytes_per_pixel = z*bytes_per_pixel;
	if (src->bits_per_pixel != 8*bytes_per_pixel) {
            /* slow procedure */
	    for (y = 0; y < heightp; y++) {
	        dy = y * z ;
	        for (x = 0; x < widthp; x++) {
		    p = XGetPixel(src, x*z, dy);
		    XPutPixel(dst, x, y, p);
		}
	    }
	} else
        for (y = 0; y < heightp; y++) {
            j = y * z * src->bytes_per_line;
            i = y * dst->bytes_per_line;
            if (interpolation) {
                if (y<heightp-1) zy = z; else zy = height-y*z;
	        for (x = 0; x < widthp; x++) {
                    if (x<widthp-1) zx = z; else zx = width-x*z;
		    for (k=0; k<bytes_per_pixel; k++) { 
		         u = 0;
		         for (l=0; l<zx; l++)
		         for (m=0; m<zy; m++)
			     u += (unsigned char)src->data[j+k+l*bytes_per_pixel+m*src->bytes_per_line];
                             dst->data[i+k]= (u/(zx*zy))&255;
		    }
		    i += bytes_per_pixel;
                    j += zoom_bytes_per_pixel;
		}
	    } else
	    for (x = 0; x < widthp; x++) {
		memcpy(&dst->data[i], &src->data[j], bytes_per_pixel);
		i += bytes_per_pixel;
                j += zoom_bytes_per_pixel;
	    }
	}
        XDestroyImage(src);
        newpix = XCreatePixmap(dpy,
		    XtWindow(Global.toplevel), widthp, heightp, depth);
	XPutImage(dpy, newpix, gc, dst, 0, 0, 0, 0, widthp, heightp);
        XDestroyImage(dst);
        graphicCreate(makeGraphicShell(Global.toplevel), 
               widthp, heightp, 1, newpix, None, alpha);
    }
    XtReleaseGC(paint, gc);
    StateSetBusy(False);
}

void 
StdMemorySetCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    LocalInfo * info = (LocalInfo *) paintArg;
    Widget paint = info->paint;
    Pixmap pix, mask;
    XRectangle rect;
    int width, height, all = 0;
    PaintWidget pw = (PaintWidget) paint;

    if (!PwRegionGet(paint, &pix, None)) {
	XtVaGetValues(paint, XtNdrawWidth, &width, 
                             XtNdrawHeight, &height, NULL);
	rect.x = 0;
        rect.y = 0;
        rect.width = width;
        rect.height = height;
        PwRegionSet(paint, &rect, None, None);
	all = 1;
    }
     
    if (!PwRegionGet(paint, &pix, &mask)) return;

    setMemoryRegion(paint, pw->paint.region.orig.x, pw->paint.region.orig.y,
		    pix, mask, pw->paint.region.alpha);
    info->mem_index = Global.numregions-1;
    GraphicAll((GraphicAllProc) setMemoryIcon, NULL);

    if (all) PwRegionSet(paint, NULL, None, None);
}

static void
memoryRecallOkCallback(Widget paint, XtPointer paintArg, XtPointer infoArg)
{
    LocalInfo *localinfo = (LocalInfo *) paintArg;
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int n = atoi(info->prompts[0].rstr) - 1;

    if (n < 0 || n >= Global.numregions) {
	Notice(localinfo->paint, msgText[IMAGE_NUMBER_OUT_OF_RANGE]);
	return;
    }
    localinfo->mem_index = n;
    memoryRecallCallback(localinfo->paint, localinfo, NULL);
    GraphicAll((GraphicAllProc) setMemoryIcon, NULL);
}

void
StdMemoryRecallCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    LocalInfo *localinfo = (LocalInfo *) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf[5];
    static char title[80];

    if (Global.numregions == 0) return;
    if (Global.numregions == 1) {
       localinfo->mem_index = 0;
       memoryRecallCallback(localinfo->paint, localinfo, NULL);
       GraphicAll((GraphicAllProc) setMemoryIcon, NULL);
       return;   
    }

    if (localinfo->mem_index>=0)
       sprintf(buf, "%d", localinfo->mem_index+1);
    else
       *buf = '\0';

    value[0].prompt = msgText[IMAGE_NUMBER];
    value[0].str = buf;
    value[0].len = 3;
    info.prompts = value;
    sprintf(title, "%s (n = 1...%d)",
            msgText[RECALL_AN_IMAGE], Global.numregions);
    info.title = title;
    info.nprompt = 1;

    TextPrompt(localinfo->paint, "mem_recall", 
               &info, memoryRecallOkCallback, NULL, localinfo);
}

void 
memoryRemoveOkCallback(Widget w, XtPointer paintArg,  XtPointer infoArg)
{
    LocalInfo *localinfo = (LocalInfo *) paintArg;
    TextPromptInfo *info = (TextPromptInfo *) infoArg;
    int n = atoi(info->prompts[0].rstr) - 1;

    if (n < 0 || n >= Global.numregions) {
	Notice(localinfo->paint, msgText[IMAGE_NUMBER_OUT_OF_RANGE]);
	return;
    }
    removeFromMemory(XtDisplay(localinfo->paint), n);
    localinfo->mem_index = Global.numregions-1;
    GraphicAll((GraphicAllProc) setMemoryIcon, NULL);
}

void 
StdMemoryRemoveCallback(Widget w, XtPointer paintArg,  XtPointer junk2)
{
    LocalInfo *localinfo = (LocalInfo *) paintArg;
    static TextPromptInfo info;
    static struct textPromptInfo value[1];
    static char buf[5];
    static char title[80];

    if (!Global.numregions) return;

    if (localinfo->mem_index>=0)
       sprintf(buf, "%d", localinfo->mem_index+1);
    else
       *buf = '\0';

    value[0].prompt = msgText[IMAGE_NUMBER];
    value[0].str = buf;
    value[0].len = 3;
    info.prompts = value;
    sprintf(title, "%s (n = 1...%d)",
            msgText[REMOVE_AN_IMAGE], Global.numregions);
    info.title = title;
    info.nprompt = 1;

    TextPrompt(localinfo->paint, "mem_remove", 
               &info, memoryRemoveOkCallback, NULL, localinfo);
}

void 
StdCopyCallback(Widget w, XtPointer paintArg, String * nm, XEvent * event)
{
    Widget paint = (Widget) paintArg;
    PaintWidget pw = (PaintWidget) paint;
    Pixmap pix, mask;
    int m, width, height;

    if (!PwRegionGet(paint, &pix, &mask))
	return;

    GetPixmapWHD(XtDisplay(paint), pix, &width, &height, NULL);

    if (Global.clipboard.pix != None)
	XFreePixmap(XtDisplay(paint), Global.clipboard.pix);
    if (Global.clipboard.mask != None)
	XFreePixmap(XtDisplay(paint), Global.clipboard.mask);
    if (Global.clipboard.alpha != NULL) {
        XtFree((char *)Global.clipboard.alpha);
        Global.clipboard.alpha = NULL;
    }

    Global.clipboard.pix = pix;
    Global.clipboard.mask = mask;
    Global.clipboard.width = width;
    Global.clipboard.height = height;

    XtVaGetValues(paint, XtNcolormap, &Global.clipboard.cmap, NULL);
    if (pw->paint.region.alpha) {
        m = width * height;
        Global.clipboard.alpha = (unsigned char *)XtMalloc(m);
        memcpy(Global.clipboard.alpha, pw->paint.region.alpha, m);
    }        

    selectionOwner = XtOwnSelection(paint, XA_PRIMARY, Global.currentTime,
			 selectionConvert, selectionLost, selectionDone);

    GraphicAll(setPasteItemMenu, NULL);
}

static void 
StdPasteCB(Widget paint, XtPointer infoArg, Atom * selection, Atom * type,
	   XtPointer value, unsigned long *len, int *format)
{
    selectInfo *info = (selectInfo *) infoArg;
    Display *dpy = XtDisplay(paint);
    PaintWidget pw = (PaintWidget) paint;
    XRectangle rect;
    Pixmap pix;
    Colormap cmap;
    Pixmap newMask = None;
    unsigned char *alpha = NULL;
    int m;
    GC gc;

    if (type != NULL) {
	info->count--;
	if (*type == XA_BITMAP) {
	    int wth, hth, dth = 0;
	    Pixmap pix = *(Pixmap *) value;

            if (pix)
	        GetPixmapWHD(dpy, pix, &wth, &hth, &dth);
	    if (info->pixmap == None ||
		info->depth < dth) {
		info->pixmap = pix;
		info->width = wth;
		info->height = hth;
		info->depth = dth;
	    }
	} else if (*type == XA_PIXMAP) {
	    int wth, hth, dth = 0;
	    Pixmap pix = *(Pixmap *) value;

            if (pix)
	        GetPixmapWHD(dpy, pix, &wth, &hth, &dth);
	    if (info->pixmap == None ||
		info->depth < dth) {
		info->pixmap = pix;
		info->width = wth;
		info->height = hth;
		info->depth = dth;
	    }
	}
       
	/*
	**  Are there more possible selections coming?
	 */
	if (info->count != 0)
	    return;

	/*
	**  Now that we have gotten all of the selections
	**    use the best one.
	 */
	if (info->pixmap != None) {
	    Pixmap np;
	    GC gc;

	    if (Global.clipboard.pix != None)
		XFreePixmap(dpy, Global.clipboard.pix);
	    if (Global.clipboard.mask != None)
		XFreePixmap(dpy, Global.clipboard.mask);
	    if (Global.clipboard.alpha != NULL)
	        XtFree((char *)Global.clipboard.alpha);
	    if (Global.clipboard.image != NULL)
		ImageDelete((Image *) Global.clipboard.image);

	    Global.clipboard.pix = None;
	    Global.clipboard.mask = None;
            Global.clipboard.alpha = NULL;
	    Global.clipboard.image = NULL;

	    np = XCreatePixmap(dpy, XtWindow(paint),
			       info->width, info->height, info->depth);
	    gc = XCreateGC(dpy, np, 0, 0);
	    XCopyArea(dpy, info->pixmap, np, gc,
		      0, 0,
		      info->width, info->height,
		      0, 0);
	    XFreeGC(dpy, gc);

	    Global.clipboard.width = info->width;
	    Global.clipboard.height = info->height;
	    Global.clipboard.pix = np;

	    if (info->depth == 1)
		Global.clipboard.cmap = -1;
	    else
		Global.clipboard.cmap = DefaultColormapOfScreen(XtScreen(paint));
	}
	XtFree((XtPointer) info);
    }
    /*
    **	No valid selections anywhere or we own the selection.
     */
   
    if (Global.clipboard.pix == None && Global.clipboard.image == NULL)
	return;

    rect.x = 0;
    rect.y = 0;
    rect.width = Global.clipboard.width;
    rect.height = Global.clipboard.height;
    alpha = Global.clipboard.alpha;

    if (Global.clipboard.mask != None) {
	newMask = XCreatePixmap(dpy, XtWindow(paint), rect.width, rect.height, 1);
	gc = XCreateGC(dpy, newMask, 0, 0);
	XCopyArea(dpy, Global.clipboard.mask, newMask, gc,
		  0, 0, rect.width, rect.height, 0, 0);
	XFreeGC(dpy, gc);
    } else if (Global.clipboard.image != NULL &&
	       ((Image *) Global.clipboard.image)->alpha != NULL) {
        newMask = ImageMaskToPixmap(paint, (Image *) Global.clipboard.image);
    }

    XtVaGetValues(paint, XtNcolormap, &cmap, NULL);
    if (cmap != Global.clipboard.cmap) {
	Image *image;
	Pixmap npix = None;

	if (rect.width * rect.height > 1024)
	    StateSetBusy(True);

	if (Global.clipboard.pix == None) {
	    image = (Image *) Global.clipboard.image;
	    image->refCount++;
            alpha = image->alpha;
            image->alpha = NULL;
	} else
	    image = PixmapToImage(paint, Global.clipboard.pix, Global.clipboard.cmap);

	ImageToPixmapCmap(image, paint, &npix, cmap);

	PwRegionSet(paint, &rect, npix, newMask);

	if (rect.width * rect.height > 1024)
	    StateSetBusy(False);
    } else {
	int depth;

	XtVaGetValues(paint, XtNdepth, &depth, NULL);
	pix = XCreatePixmap(dpy, XtWindow(paint), rect.width, rect.height, depth);
	gc = XtGetGC(paint, 0, 0);
	XCopyArea(dpy, Global.clipboard.pix, pix, gc,
		  0, 0, rect.width, rect.height, 0, 0);
	XtReleaseGC(paint, gc);
        PwRegionSet(paint, &rect, pix, newMask);
    }

    if (alpha) {
        m = rect.width * rect.height;
        if (!pw->paint.region.alpha) 
	    pw->paint.region.alpha = (unsigned char *)XtMalloc(m);
        memcpy(pw->paint.region.alpha, alpha, m);
    }
}

void 
StdPasteCallback(Widget w, XtPointer paintArg, XtPointer junk)
{
    Widget paint = (Widget) paintArg;

    if (!selectionOwner) {
	static Atom targets[2] =
	{XA_PIXMAP, XA_BITMAP};
	XtPointer data[2];
	selectInfo *info = XtNew(selectInfo);

	info->count = XtNumber(targets);
	info->pixmap = None;

	data[0] = (XtPointer) info;
	data[1] = (XtPointer) info;

	XtGetSelectionValues(paint, XA_PRIMARY, targets, 2,
			     StdPasteCB, data, Global.currentTime);
    } else {
	StdPasteCB(paint, NULL, NULL, NULL, 0, NULL, NULL);
    }
}

void 
StdScreenshotCallback(Widget w, XtPointer paintArg, XtPointer junk)
{
     LocalInfo * info = (LocalInfo *) paintArg; 
     Widget paint;
     PaintWidget pw;
     Boolean selection;
     XRectangle rect;
     Position x1, y1, x2, y2;
     Dimension width, height;
     WidgetList wlist;
#ifdef XAW3D
     Pixel pixel;
#endif

     /* Disable screenshot from FatBits !! */
     if (!info) return;

     paint = info->paint;
     pw = (PaintWidget) paint;

     /* First unselect selected region, if any */
     PwRegionSet(paint, NULL, None, None);

     /* Calculate menu position */
     XtVaGetValues(paint, XtNmenuwidgets, &wlist, NULL);

     if (wlist && wlist[0]) {
        Widget ww;
        ww = wlist[W_TOPMENU+W_EDIT_PASTE];
        if (ww && !IsFullMenuSet(paint)) {
            XtVaGetValues(ww, XtNwidth, &width, XtNheight, &height, NULL);
            XtTranslateCoords(paint, 0, 0, &x1, &y1);
            XtTranslateCoords(ww, 0, 0, &x2, &y2);
            y2 += height;
            /* Popdown menu */
	    if ((ww=XtParent(ww)) && XtIsRealized(ww)) XtPopdown(ww);
            /* Redraw area erased by menu popup */
            rect.x = x2-x1 - 3;
            rect.y = 0;
            rect.width = 5 + width;
            if (rect.width>pw->paint.drawWidth) 
            rect.width = pw->paint.drawWidth;
            rect.height = y2-y1 + 1;
            if (rect.height>pw->paint.drawHeight) 
                rect.height = pw->paint.drawHeight;
	} else {
            rect.x = 0;
            rect.y = 0;
            rect.width = pw->paint.drawWidth;
            rect.height = pw->paint.drawHeight;
	}
        zoomUpdate(pw, True, &rect);
#if defined XAW3D && !defined NEXTAW
        /* redraw border of viewport window with Xaw3d widgets ... */
        XtVaGetValues((Widget)
               ((ViewportWidget)(info->viewport))->viewport.threeD, 
                   XtNbottomShadowPixel, &pixel, 
		      XtNshadowWidth, &rect.y, NULL);
        XSetWindowBackground(XtDisplay(info->viewport),
			     XtWindow(((ViewportWidget)(info->viewport))->viewport.threeD), pixel);
        XClearArea(XtDisplay(info->viewport),
		       XtWindow(((ViewportWidget)(info->viewport))->viewport.threeD),
		   rect.x, 0, rect.width+4, rect.y, True);
#endif
        XFlush(XtDisplay(paint));
     }
   
     /* Clear selection issues */
     selection = selectionOwner;
     selectionOwner = True;

     /* Now, really start screenshot ! */
     ScreenshotImage(paint, pw, 1);
     selectionOwner = selection;
}

void 
StdClearCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;

    PwRegionClear(paint);
}

void 
StdCutCallback(Widget w, XtPointer paintArg, String * nm, XEvent * event)
{
    StdCopyCallback(w, paintArg, nm, event);
    StdClearCallback(w, paintArg, NULL);
}

void 
StdDuplicateCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    XRectangle rect;
    Widget paint = (Widget) paintArg;
    Pixmap pix, mask;
    int width, height;

    if (!PwRegionGet(paint, &pix, &mask))
	return;

    GetPixmapWHD(XtDisplay(paint), pix, &width, &height, NULL);

    rect.x = 0;
    rect.y = 0;
    rect.width = width;
    rect.height = height;

    PwRegionSet(paint, &rect, pix, mask);
}

void 
StdSelectAllCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    XRectangle rect;
    int dw, dh;

    XtVaGetValues(paint, XtNdrawWidth, &dw, XtNdrawHeight, &dh, NULL);

    rect.x = 0;
    rect.y = 0;
    rect.width = dw;
    rect.height = dh;

    PwRegionSet(paint, &rect, None, None);
}

void 
StdEraseAllCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    XRectangle rect;
    int dw, dh;

    XtVaGetValues(paint, XtNdrawWidth, &dw, XtNdrawHeight, &dh, NULL);

    rect.x = 0;
    rect.y = 0;
    rect.width = dw;
    rect.height = dh;

    PwRegionSet(paint, &rect, None, None);
    PwRegionClear(paint);
}

void 
StdUndoCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PaintWidget pw = (PaintWidget) paintArg;
    int haveRegion = 0;
    XRectangle *o, *r;

    /* Only fiddle with the region if it has not been moved or resized */
    o = &pw->paint.region.orig;
    r = &pw->paint.region.rect;
    if ((o->x == r->x) && (o->y == r->y) &&
	(o->width == r->width) && (o->height == r->height))
	haveRegion = PwRegionOff((Widget) pw, True);

    Undo((Widget) pw);
    if (haveRegion)
	PwRegionSet((Widget) pw, &pw->paint.region.rect, None, None);
    StdRefreshCallback(w, paintArg, junk2);
    SetAlphaMode((Widget)paintArg, -1);
}

void 
StdRedoCallback(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PaintWidget pw = (PaintWidget) paintArg;
    int haveRegion = 0;
    XRectangle *o, *r;

    /* Only fiddle with the region if it has not been moved or resized */
    o = &pw->paint.region.orig;
    r = &pw->paint.region.rect;
    if ((o->x == r->x) && (o->y == r->y) &&
	(o->width == r->width) && (o->height == r->height))
	haveRegion = PwRegionOff((Widget) pw, True);

    Redo((Widget) pw);
    if (haveRegion)
	PwRegionSet((Widget) pw, &pw->paint.region.rect, None, None);
    RefreshWidget((Widget) pw);
    SetAlphaMode((Widget)paintArg, -1);
}

void 
ClipboardSetImage(Widget w, Image * image)
{
    if (image==NULL) return;

    if (Global.clipboard.pix != None)
	XFreePixmap(XtDisplay(Global.toplevel), Global.clipboard.pix);
    if (Global.clipboard.mask != None)
	XFreePixmap(XtDisplay(Global.toplevel), Global.clipboard.mask);
    if (Global.clipboard.alpha != NULL)
        XtFree((char *)Global.clipboard.alpha);
    if (Global.clipboard.image != NULL)
	ImageDelete(Global.clipboard.image);

    Global.clipboard.pix = None;
    Global.clipboard.mask = None;
    Global.clipboard.alpha = NULL;
    Global.clipboard.cmap = None;
    Global.clipboard.image = (void *) image;
    Global.clipboard.width = image->width;
    Global.clipboard.height = image->height;
}

void 
MemorySetImage(Widget w, Image * image)
{
    if (image==NULL) return;
}

/*
**  End of "edit" menu functions
**
 */

/*
**  "Font" menu function : WriteText
**
 */

void setWriteTextSensitive(Widget w, Boolean bool)
{
    WidgetList wlist;
    Widget ww;
    XtVaGetValues(w, XtNmenuwidgets, &wlist, NULL);
    if (wlist) {
        if ((ww=wlist[W_FONT_WRITE])) 
            XtVaSetValues(ww, XtNsensitive, bool, NULL);
        if ((ww=wlist[W_TOPMENU+W_FONT_WRITE]))
            XtVaSetValues(ww, XtNsensitive, bool, NULL);
    }
}

void 
StdOpenFile(Widget w, XtPointer paintArg, XtPointer junk)
{
    GetFileName(GetToplevel(w), BROWSER_READ, 
                NULL, GraphicOpenFile, NULL);
}

void 
StdWriteText(Widget w, XtPointer infoArg, XtPointer junk)
{
    LocalInfo * info = (LocalInfo *) infoArg; 
    WriteText(info->paint, NULL, Global.xft_text);
}

/*
**  Start "Region" menu functions
**
 */

void 
StdRegionFlipX(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    float v = -1.0;

    PwRegionAddScale(paint, &v, NULL);
}

void 
StdRegionFlipY(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    float v = -1.0;

    PwRegionAddScale(paint, NULL, &v);
}

void 
StdRegionClone(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PaintWidget pw  = (PaintWidget) paintArg;
    Widget paint = (Widget) paintArg;
    Display *dpy;
    Pixel p, bg;
    Pixmap pix, mask, newpix;
    XImage *maskImg = NULL;
    unsigned char *alpha;
    int x, y, width, height, depth, zoom;
    Colormap cmap;
    GC gc;

    dpy = XtDisplay(paint);

    if (PwRegionGet(paint, &pix, &mask)) {
         XtVaGetValues(paint, XtNcolormap, &cmap, XtNzoom, &zoom, NULL); 
         if (pw->paint.region.source) {
	    pix = pw->paint.region.source;
	    mask = pw->paint.region.mask;
            if (zoom>0) {
	        width = pw->paint.region.rect.width/zoom;
	        height = pw->paint.region.rect.height/zoom;
	    } else {
	        width = pw->paint.region.rect.width * (-zoom);
	        height = pw->paint.region.rect.height * (-zoom);
	    }
	 } else {
            if (zoom>0) {
	        width = pw->paint.region.orig.width/zoom;
	        height = pw->paint.region.orig.height/zoom;
	    }  else {
	        width = pw->paint.region.orig.width * (-zoom);
	        height = pw->paint.region.orig.height * (-zoom);
	    }
	 }
         gc = XtGetGC(paint, 0, 0);
	 depth = DefaultDepthOfScreen(XtScreen(paint));
	 /* Create newpix, since original pixmap may be destroyed ... */
	 newpix = XCreatePixmap(dpy,
			XtWindow(Global.toplevel), width, height, depth);
	 XCopyArea(dpy, pix, newpix, gc, 0, 0, width, height, 0, 0);
	 if (mask) {
	     maskImg = NewXImage(dpy, NULL, pw->core.depth, width, height);
	     XGetSubImage(dpy, mask, 0, 0, width, height,
		 AllPlanes, ZPixmap, maskImg, 0, 0);
	     XtVaGetValues(paint, XtNbackground, &bg, NULL);
	     XSetForeground(dpy, gc, bg);
	     for (y=0; y<height; y++)
	       for (x=0; x<width; x++) {
		   p = XGetPixel(maskImg, x, y);
		   if (!p) XDrawPoint(dpy, newpix, gc, x, y);
	       }
	     XDestroyImage(maskImg);
	 }

         if (pw->paint.region.alpha) {
	     x = pw->paint.region.orig.width * pw->paint.region.orig.height;
	     alpha = (unsigned char*) xmalloc(x);
             if (alpha) memcpy(alpha, pw->paint.region.alpha, x);
	 } else
	     alpha = NULL;
         pw = (PaintWidget) 
              graphicCreate(makeGraphicShell(Global.toplevel), 
		            0, 0, zoom, newpix, cmap, alpha);
         XtReleaseGC(paint, gc);
         XtVaSetValues((Widget)pw, XtNdirty, True, NULL);
    }
}

void 
StdRegionOCR(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PaintWidget pw  = (PaintWidget) paintArg;
    Widget paint = (Widget) paintArg;
    Display *dpy;
    Pixel p, bg;
    Pixmap pix, mask, newpix;
    XImage *maskImg = NULL;
    Image * image;
    int x, y, width, height, depth, zoom;
    char tmpfile[512];
    Colormap cmap;
    GC gc;

    dpy = XtDisplay(paint);
    if (PwRegionGet(paint, &pix, &mask)) {
         XtVaGetValues(paint, XtNcolormap, &cmap, XtNzoom, &zoom, NULL); 
         if (pw->paint.region.source) {
	    pix = pw->paint.region.source;
	    mask = pw->paint.region.mask;
	    width = pw->paint.region.rect.width/zoom;
	    height = pw->paint.region.rect.height/zoom;
	 } else {
	    width = pw->paint.region.orig.width/zoom;
	    height = pw->paint.region.orig.height/zoom;
	 }
         gc = XtGetGC(paint, 0, 0);
	 depth = DefaultDepthOfScreen(XtScreen(paint));
	 /* Create newpix, since original pixmap may be destroyed ... */
	 newpix = XCreatePixmap(dpy,
			XtWindow(Global.toplevel), width, height, depth);
	 XCopyArea(dpy, pix, newpix, gc, 0, 0, width, height, 0, 0);
	 if (mask) {
	     maskImg = NewXImage(dpy, NULL, pw->core.depth, width, height);
	     XGetSubImage(dpy, mask, 0, 0, width, height,
		 AllPlanes, ZPixmap, maskImg, 0, 0);
	     XtVaGetValues(paint, XtNbackground, &bg, NULL);
	     XSetForeground(dpy, gc, bg);
	     for (y=0; y<height; y++)
	       for (x=0; x<width; x++) {
		   p = XGetPixel(maskImg, x, y);
		   if (!p) XDrawPoint(dpy, newpix, gc, x, y);
	       }
	     XDestroyImage(maskImg);
	 }

         image = PixmapToImage(paint, newpix, cmap);
         sprintf(tmpfile, "%s/%s",
		 getenv("HOME")?getenv("HOME"):"/tmp", "xpaint_output.ppm");
	 WritePNM(tmpfile, image);
         sprintf(tmpfile, "%s/%s", GetShareDir(), "bin/xpaint_ocr");
         x = system(tmpfile);
	 ImageDelete(image);
    }
}

void
createRegionFromMaskData(XtPointer paintArg, Boolean *maskdata, Boolean target)
{
    PaintWidget pw = (PaintWidget) paintArg;
    Widget paint = (Widget) paintArg;
    Display *dpy = XtDisplay(paint);
    XRectangle rect;
    Pixmap pix, mask;
    Pixel bg, black, white;
    GC gc = None, gcb = None, gcw = None;
    int u, x, y, dw, dh, depth, zoom;

    white = WhitePixelOfScreen(XtScreen(paint));
    black = BlackPixelOfScreen(XtScreen(paint));
    XtVaGetValues(paint, XtNdrawWidth, &dw, XtNdrawHeight, &dh,
		  XtNbackground, &bg, XtNzoom, &zoom, NULL);
    rect.x = dw;
    rect.y = dh;
    rect.width = 0;
    rect.height = 0;
    for (y=0; y<dh; y++)
      for (x=0; x<dw; x++) {
	u = y * dw + x;
	if (maskdata[u] == target) {
	    if (x<rect.x) rect.x = x;
	    if (y<rect.y) rect.y = y;
	    if (x>rect.width) rect.width = x;
	    if (y>rect.height) rect.height = y;
	}
      }
    if (rect.width<rect.x || rect.height<rect.y) return;
    rect.width = rect.width - rect.x + 1;
    rect.height = rect.height - rect.y + 1;

    depth = pw->core.depth;
    pix = XCreatePixmap(dpy, XtWindow(paint), rect.width, rect.height, depth);
    mask = XCreatePixmap(dpy, XtWindow(paint), rect.width, rect.height, 1);

    gc = XtGetGC(paint, 0, 0);
    XSetForeground(dpy, gc, bg);
    gcw = XCreateGC(dpy, mask, 0, NULL);
    XSetForeground(dpy, gcw, white);
    gcb = XCreateGC(dpy, mask, 0, NULL);
    XSetForeground(dpy, gcb, black);
    XCopyArea(dpy, pw->paint.current.pixmap, pix, gc,
	           rect.x, rect.y, rect.width, rect.height, 0, 0);
    for (y = 0; y < rect.height; y++)
      for (x = 0; x < rect.width; x++) {
          u = (y + rect.y) * dw + x + rect.x;
	  if (maskdata[u] == target) {
	      XDrawPoint(dpy, mask, gcw, x, y);
              XDrawPoint(dpy, pw->paint.current.pixmap, 
                         gc, x+rect.x, y+rect.y);
          } else {
	      XDrawPoint(dpy, mask, gcb, x, y);
	  }
      }

    PwRegionSet(paint, &rect, pix, mask);
    XtMoveWidget(pw->paint.region.child, rect.x*zoom, rect.y*zoom);
    pw->paint.region.rect.x = rect.x;
    pw->paint.region.rect.y = rect.y;
    XFreeGC(dpy, gc);
    XFreeGC(dpy, gcb);
    XFreeGC(dpy, gcw);
}

void 
StdRegionDelimit(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PaintWidget pw = (PaintWidget) paintArg;
    Widget paint = (Widget) paintArg;
    Display *dpy = XtDisplay(paint);
    XImage * source;
    Boolean *maskdata;
    Pixel corner[4];
    Pixmap pix, mask;
    int u, x0, y0, x, y, dw, dh, depth;

    if (PwRegionGet(paint, &pix, &mask)) return;

    StateSetBusy(True);
    XtVaGetValues(paint, XtNdrawWidth, &dw, XtNdrawHeight, &dh, NULL);

    depth = pw->core.depth;

    source = NewXImage(dpy, NULL, depth, dw, dh);
    XGetSubImage(dpy, pw->paint.current.pixmap, 0, 0, dw, dh,
		 AllPlanes, ZPixmap, source, 0, 0);
    for (y = 0, u = 0; y < dh; y += dh - 1)
	for (x = 0; x < dw; x += dw - 1)
	    corner[u++] = XGetPixel(source, x, y);

    if ((corner[0] == corner[1]) || (corner[0] == corner[2]) ||
	(corner[0] == corner[3])) {
	x0 = 0;
	y0 = 0;
    }
    else if ((corner[1] == corner[2]) || (corner[1] == corner[3])) {
	x0 = dw-1;
	y0 = 0;
    }
    else if (corner[2] == corner[3]) {
	x0 = 0;
	y0 = dh-1;
    }
    else {
	XDestroyImage(source);
	StateSetBusy(False);
	return;			/* No two corners have the same colour */
    }

    maskdata = setDrawable(paint, pw->paint.current.pixmap, source);
    if (!maskdata) {
        XDestroyImage(source);
	return;
    }
    fill(x0, y0, dw, dh);
    XDestroyImage(source);
    createRegionFromMaskData(paint, maskdata, False);
    XtFree((char *)maskdata);
    StateSetBusy(False);
}

void 
StdRegionComplement(Widget w, XtPointer paintArg, XtPointer junk2)
{
    PaintWidget pw = (PaintWidget) paintArg;
    Widget paint = (Widget) paintArg;
    Display *dpy = XtDisplay(paint);
    unsigned char * alpha;
    XImage * source;
    Boolean *maskdata;
    Pixmap pix, mask;
    Pixel bg, p;
    XRectangle rect;
    int i, x, y, dw, dh, depth;

    if (!PwRegionGet(paint, &pix, &mask)) return;    

    StateSetBusy(True);
    XtVaGetValues(paint, XtNdrawWidth, &dw, XtNdrawHeight, &dh, 
                         XtNbackground, &bg, NULL);
    depth = pw->core.depth;

    rect = pw->paint.region.rect;
    maskdata = (Boolean *)xmalloc(dw * dh * sizeof(Boolean));
    memset(maskdata, False, dw * dh * sizeof(Boolean));

    if (pw->paint.current.alpha) {
        i = dw * dh;
        alpha = pw->paint.current.alpha;
        pw->paint.current.alpha = (unsigned char *) XtMalloc(i);
        memset(pw->paint.current.alpha, (unsigned char)Global.alpha_bg, i);
    } else
        alpha = NULL;

    if (mask) {
        source = NewXImage(dpy, NULL, 1, rect.width, rect.height);
        XGetSubImage(dpy, mask, 0, 0, rect.width, rect.height,
		 AllPlanes, ZPixmap, source, 0, 0);
        for (y=0; y<rect.height; y++) {
	    i = (y+rect.y) * dw + rect.x;
            for (x=0; x<rect.width; x++, i++) {
	        p = XGetPixel(source, x, y);
	        if (p) {
                    maskdata[i] = True;
                    if (alpha) {
		        pw->paint.current.alpha[i] = alpha[i];
                        alpha[i] = (unsigned char)Global.alpha_bg;
		    }
	        }
	    }
	}
        XDestroyImage(source);
    } else {
        for (y=0; y<rect.height; y++) {
	    i = (y+rect.y) * dw + rect.x;
	    for (x=0; x<rect.width; x++, i++) {
	        maskdata[i] = True;
                if (alpha) {
		     pw->paint.current.alpha[i] = alpha[i];
                     alpha[i] = (unsigned char)Global.alpha_bg;
	        }
	    }
	}
    }

    PwRegionSet(paint, NULL, None, None);    
    createRegionFromMaskData(paint, maskdata, False);
    pw->paint.region.alpha = alpha;
    XtFree((char *)maskdata);
    StateSetBusy(False);
}

/*
** This is the function that calls the actual image processing functions.
 */
Image *
ImgProcessSetup(Widget paint)
{
    Display *dpy = XtDisplay(paint);
    GC gc;
    Image *in;
    Pixel bg;
    Palette *pal;
    extern struct imageprocessinfo ImgProcessInfo;
    PaintWidget pw = (PaintWidget) paint;
    int dw, dh, depth;

    StateSetBusy(True);

    ImgProcessFlag = 0;

    /* Select full canvas if no region has been selected  */
    if (!PwRegionGet(paint, &ImgProcessPix, None))
        StdSelectAllCallback(paint, (XtPointer) paint, NULL);

    if (!PwRegionGet(paint, &ImgProcessPix, None)) {
	StateSetBusy(False);
	return NULL;	 /* Region selection probably failed */
    }

    GetPixmapWHD(dpy, ImgProcessPix, &dw, &dh, &depth);
    /* free previous unfilterPixmap */
    if (pw->paint.region.unfilterPixmap != None)
       XFreePixmap(dpy, pw->paint.region.unfilterPixmap);

    /* create new unfilterPixmap and copy selected area to it */
    pw->paint.region.unfilterPixmap = XCreatePixmap(dpy, XtWindow(paint),
                       dw, dh, depth);
    gc = XCreateGC(dpy, ImgProcessPix, 0, 0);
    XCopyArea(dpy, ImgProcessPix, pw->paint.region.unfilterPixmap, gc,
	      0, 0, dw, dh, 0, 0);
    XFreeGC(dpy, gc);

    PwRegionTear(paint);
    XtVaGetValues(paint, XtNcolormap, &ImgProcessCmap, NULL);

    XtVaGetValues(paint, XtNbackground, &bg, NULL);
    pal = PaletteFind(paint, ImgProcessCmap);
    ImgProcessInfo.background = PaletteLookup(pal, bg);

    in = PixmapToImage(paint, ImgProcessPix, ImgProcessCmap);
    ImgProcessPix = None;

    StateSetBusy(False);
    return in;
}

Image *
ImgProcessFinish(Widget paint, Image * in, Image * (*func) (Image *))
{
    Image *out;

    StateSetBusy(True);
    Global.curpaint = paint;
    out = func(in);
    if (out != in)		/* delete input unless done in place */
	ImageDelete(in);

    ImageToPixmapCmap(out, paint, &ImgProcessPix, ImgProcessCmap);

    if (!ImgProcessFlag)
	PwRegionSetRawPixmap(paint, ImgProcessPix);
    else {
	PwUpdate(paint, NULL, True);
	XtVaSetValues(paint, XtNdirty, True, NULL);
    }

    StateSetBusy(False);

    return out;
}

/* 
 * Function should be : Image * (*func) (Image *)
 */
void 
ImageRegionProcess(Widget paint, void * func)
{
    Image *in, *out;

    lastFilter = func;
    EnableLast(paint);

    if ((in = ImgProcessSetup(paint)) == NULL)
	return;
    out = ImgProcessFinish(paint, in, func);
    RefreshWidget(paint);
}

void 
StdLastImgProcess(Widget paint)
{
    if (lastFilter != NULL)
	ImageRegionProcess(paint, lastFilter);
}

void 
StdRegionInvert(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;

    ImageRegionProcess(paint, ImageInvert);
}

void 
StdRegionSharpen(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;

    ImageRegionProcess(paint, ImageSharpen);
}

void 
StdRegionSmooth(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;

    ImageRegionProcess(paint, ImageSmooth);
}

void 
StdRegionEdge(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;

    ImageRegionProcess(paint, ImageEdge);
}

void 
StdRegionEmboss(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;

    ImageRegionProcess(paint, ImageEmboss);
}

void 
StdRegionOilPaint(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;

    ImageRegionProcess(paint, ImageOilPaint);
}

void 
StdRegionAddNoise(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageAddNoise);
}

void 
StdRegionSpread(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageSpread);
}

void 
StdRegionBlend(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageBlend);
}

void 
StdRegionPixelize(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImagePixelize);
}

void 
StdRegionDespeckle(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageDespeckle);
}

void 
StdRegionNormContrast(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageNormContrast);
}

void 
StdRegionSolarize(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageSolarize);
}

void 
StdRegionModifyRGB(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageModifyRGB);
}

void 
StdRegionGrey(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageGrey);
}

void 
StdRegionBWMask(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageBWMask);
}

void 
StdRegionQuantize(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageQuantize);
}
   
void 
StdRegionUserDefined(Widget w, XtPointer paintArg, XtPointer junk2)
{
    if (!isFilterDefined()) {
       Notice(w, msgText[YOU_MUST_FIRST_COMPILE_A_C_SCRIPT]);
       return;
    }
    ImageRegionProcess((Widget) paintArg, ImageUserDefined);
}

void 
StdRegionDirFilt(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageDirectionalFilter);
}

void 
StdRegionUndo(Widget w, XtPointer paintArg, XtPointer junk2)
{
    Widget paint = (Widget) paintArg;
    PaintWidget pw = (PaintWidget) paintArg;
    Display * dpy = XtDisplay(paint);
    Pixmap np;
    GC gc;
    int width, height, depth;

    if (!pw->paint.region.unfilterPixmap) return;
    if (!PwRegionGet(paint, &ImgProcessPix, None)) return;
    GetPixmapWHD(dpy, pw->paint.region.unfilterPixmap, 
                 &width, &height, &depth);
    np = XCreatePixmap(dpy, XtWindow(paint), width, height, depth);
    gc = XCreateGC(dpy, np, 0, 0);
    XCopyArea(dpy, pw->paint.region.unfilterPixmap, np, gc,
                      0, 0, width, height, 0, 0);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, ImgProcessPix);
    XFreePixmap(dpy, pw->paint.region.unfilterPixmap);
    pw->paint.region.unfilterPixmap = None;
    PwRegionSetRawPixmap(paint, np);
}

void 
StdRegionTilt(Widget w, XtPointer paintArg, XtPointer junk2)
{
    ImageRegionProcess((Widget) paintArg, ImageTilt);
}

/*
**  Start callback functions
 */
static void 
addFunction(Widget item, Widget paint, XtCallbackProc func)
{
    XtAddCallback(item, XtNcallback, func, (XtPointer) paint);
    XtAddCallback(paint, XtNregionCallback, (XtCallbackProc) prCallback,
		  (XtPointer) item);
    XtVaSetValues(item, XtNsensitive, (XtPointer) False, NULL);
}

void 
ccpAddUndo(Widget w, Widget paint)
{
    XtAddCallback(w, XtNcallback, StdUndoCallback, (XtPointer) paint);
}

void 
ccpAddRedo(Widget w, Widget paint)
{
    XtAddCallback(w, XtNcallback, StdRedoCallback, (XtPointer) paint);
}

void 
ccpAddRefresh(Widget w, Widget paint)
{
    XtAddCallback(w, XtNcallback, StdRefreshCallback, (XtPointer) paint);
}

void 
ccpAddCut(Widget w, Widget paint)
{
    addFunction(w, paint, (XtCallbackProc) StdCutCallback);
}

void 
ccpAddCopy(Widget w, Widget paint)
{
    addFunction(w, paint, (XtCallbackProc) StdCopyCallback);
}

void 
ccpAddPaste(Widget w, Widget paint)
{
    XtVaSetValues(w, XtNsensitive, 
        (Global.clipboard.pix || Global.clipboard.image)?True:False, NULL);
    XtAddCallback(w, XtNcallback, 
                  (XtCallbackProc) StdPasteCallback, (XtPointer) paint);
}

void 
ccpAddClear(Widget w, Widget paint)
{
    addFunction(w, paint, (XtCallbackProc) StdClearCallback);
}

void 
ccpAddDuplicate(Widget w, Widget paint)
{
    addFunction(w, paint, (XtCallbackProc) StdDuplicateCallback);
}

void 
ccpAddSelectAll(Widget w, Widget paint)
{
    XtAddCallback(w, XtNcallback, (XtCallbackProc) StdSelectAllCallback, 
                  (XtPointer) paint);
}

void 
ccpAddEraseAll(Widget w, Widget paint)
{
    XtAddCallback(w, XtNcallback, (XtCallbackProc) StdEraseAllCallback, 
                  (XtPointer) paint);
}

/*
**  Region functions
 */
void 
ccpAddSaveRegion(Widget w, Widget paint)
{
    addFunction(w, paint, (XtCallbackProc) StdSaveRegionFile);
}

void 
ccpAddCloneRegion(Widget w, Widget paint)
{
    addFunction(w, paint, (XtCallbackProc) StdRegionClone);
}

void 
ccpAddOCRRegion(Widget w, Widget paint)
{
    addFunction(w, paint, (XtCallbackProc) StdRegionOCR);
}

#define ADDCALLBACK(menu, item, pw, func) \
  XtAddCallback(menu[item].widget, XtNcallback, (XtCallbackProc) func, \
		(XtPointer) pw);

static void 
generalPopupHandler(Widget w, void * arg, XKeyEvent * event, void * data)
{
    Global.popped_up = w;
}

void 
ccpAddStdPopup(Widget paint, void *info)
{
    int i;
    Widget w;

    w = MenuPopupCreate(paint, "popup-menu", 
                               XtNumber(popupMenu), popupMenu);

    XtAddEventHandler(w, ButtonPressMask,
		         False, (XtEventHandler) generalPopupHandler, paint);

    if (!info)
        XtAddEventHandler(paint, KeyPressMask,
		          False, (XtEventHandler) selectKeyPress, NULL);

    ADDCALLBACK(popupFileMenu, P_FILE_OPEN, paint, StdOpenFile);
    ADDCALLBACK(popupFileMenu, P_FILE_SAVE, paint, StdSaveFile);
    ADDCALLBACK(popupFileMenu, P_FILE_SAVEAS, paint, StdSaveAsFile);
    ccpAddSaveRegion(popupFileMenu[P_FILE_SAVE_REGION].widget, paint);
    ADDCALLBACK(popupFileMenu, P_FILE_LOAD_MEMORY, info, loadMemory);
    ADDCALLBACK(popupFileMenu, P_FILE_REVERT, paint, revertCallback);
    prCallback(paint, popupFileMenu[P_FILE_REVERT].widget, False);
    ADDCALLBACK(popupFileMenu, P_FILE_LOADED, paint, loadedCallback);
    ADDCALLBACK(popupFileMenu, P_FILE_PRINT, paint, printCallback);
    ADDCALLBACK(popupFileMenu, P_FILE_EXTERN, paint, externCallback);
    if (info)
        XtAddCallback(popupFileMenu[P_FILE_CLOSE].widget, XtNcallback, 
		     (XtCallbackProc) closeCallback, (XtPointer) info);
    else
        prCallback(paint, popupFileMenu[P_FILE_CLOSE].widget, False);

    ccpAddUndo(popupEditMenu[P_EDIT_UNDO].widget, paint);
    ccpAddRedo(popupEditMenu[P_EDIT_REDO].widget, paint);
    ADDCALLBACK(popupEditMenu, P_EDIT_UNDO_SIZE, paint, undosizeCallback);
    ccpAddRefresh(popupEditMenu[P_EDIT_REFRESH].widget, paint);
    ccpAddCut(popupEditMenu[P_EDIT_CUT].widget, paint);
    ccpAddCopy(popupEditMenu[P_EDIT_COPY].widget, paint);
    ccpAddPaste(popupEditMenu[P_EDIT_PASTE].widget, paint);
    ccpAddClear(popupEditMenu[P_EDIT_CLEAR].widget, paint);
    ccpAddSelectAll(popupEditMenu[P_EDIT_SELECT_ALL].widget, paint);
    prCallback(paint, popupEditMenu[P_EDIT_SELECT_ALL].widget, True);
    ADDCALLBACK(popupEditMenu, P_EDIT_UNSELECT, paint, unselectRegion);   
    ccpAddDuplicate(popupEditMenu[P_EDIT_DUP].widget, paint);
    ccpAddEraseAll(popupEditMenu[P_EDIT_ERASE_ALL].widget, paint);
    prCallback(paint, popupEditMenu[P_EDIT_ERASE_ALL].widget, True);
    ADDCALLBACK(popupEditMenu, P_EDIT_CLONE_CANVAS, paint, StdCloneCanvasCallback);
    ADDCALLBACK(popupEditMenu, P_EDIT_CLONE_CANVAS1, paint, StdCloneCanvasCallback);
    ADDCALLBACK(popupEditMenu, P_EDIT_SNAPSHOT, info, StdScreenshotCallback);
   
    ADDCALLBACK(popupTextMenu, P_FONT_SELECT, info, StdFontSet);
    ADDCALLBACK(popupTextMenu, P_FONT_WRITE, info, StdWriteText);
    prCallback(paint, popupTextMenu[P_FONT_WRITE].widget, False);

    ADDCALLBACK(popupRegionMenu, P_REGION_FLIPX, paint, StdRegionFlipX);
    ADDCALLBACK(popupRegionMenu, P_REGION_FLIPY, paint, StdRegionFlipY);
    ADDCALLBACK(popupRegionMenu, P_REGION_ROTATE, paint, rotate);
    for (i = 0; i < XtNumber(popupRotateMenu); i++) {
	if (popupRotateMenu[i].name[0] == '\0')
	    continue;

	ADDCALLBACK(popupRotateMenu, i, paint, rotateTo);
        XtAddCallback(paint, XtNregionCallback, (XtCallbackProc) prCallback,
		      (XtPointer) popupRotateMenu[i].widget);
	prCallback(paint, popupRotateMenu[i].widget, False);
    }

    XtAddCallback(paint, XtNregionCallback, (XtCallbackProc) prCallback,
		  (XtPointer) popupRegionMenu[P_REGION_ROTATE].widget);
    prCallback(paint, popupRegionMenu[P_REGION_ROTATE].widget, False);
    ADDCALLBACK(popupRegionMenu, P_REGION_EXPAND, paint, expandRegion);
    ADDCALLBACK(popupRegionMenu, P_REGION_DOWNSCALE, paint, downscaleRegion);
    ADDCALLBACK(popupRegionMenu, P_REGION_LINEAR, paint, linearRegion);
    ADDCALLBACK(popupRegionMenu, P_REGION_RESET, paint, resetMat);
    XtAddCallback(paint, XtNregionCallback, (XtCallbackProc) prCallback,
		  (XtPointer) popupRegionMenu[P_REGION_RESET].widget);
    prCallback(paint, popupRegionMenu[P_REGION_RESET].widget, False);

    ADDCALLBACK(popupRegionMenu, P_REGION_CROP, paint, cropToRegion);
    XtAddCallback(paint, XtNregionCallback, (XtCallbackProc) prCallback,
		  (XtPointer) popupRegionMenu[P_REGION_CROP].widget);
    prCallback(paint, popupRegionMenu[P_REGION_CROP].widget, False);
    ADDCALLBACK(popupRegionMenu, P_REGION_DELIMIT, paint, StdRegionDelimit);
    ADDCALLBACK(popupRegionMenu, P_REGION_COMPLEMENT, paint, StdRegionComplement);   
    ccpAddCloneRegion(popupRegionMenu[P_REGION_CLONE].widget, paint);
    ccpAddOCRRegion(popupRegionMenu[P_REGION_OCR].widget, paint);
    ADDCALLBACK(popupRegionMenu, P_REGION_AUTOCROP, paint, autocropCallback);

    ADDCALLBACK(popupFilterMenu, P_FILTER_INVERT, paint, StdRegionInvert);
    ADDCALLBACK(popupFilterMenu, P_FILTER_SHARPEN, paint, StdRegionSharpen);
    ADDCALLBACK(popupFilterMenu, P_FILTER_SMOOTH, paint, doSmooth);
    ADDCALLBACK(popupFilterMenu, P_FILTER_DIRFILT, paint, StdRegionDirFilt);
    ADDCALLBACK(popupFilterMenu, P_FILTER_DESPECKLE, paint, doDespeckle);
    ADDCALLBACK(popupFilterMenu, P_FILTER_EDGE, paint, StdRegionEdge);
    ADDCALLBACK(popupFilterMenu, P_FILTER_EMBOSS, paint, StdRegionEmboss);
    ADDCALLBACK(popupFilterMenu, P_FILTER_OIL, paint, oilPaint);
    ADDCALLBACK(popupFilterMenu, P_FILTER_NOISE, paint, addNoise);
    ADDCALLBACK(popupFilterMenu, P_FILTER_SPREAD, paint, doSpread);
    ADDCALLBACK(popupFilterMenu, P_FILTER_PIXELIZE, paint, doPixelize);
    ADDCALLBACK(popupFilterMenu, P_FILTER_TILT, paint, StdRegionTilt);
    ADDCALLBACK(popupFilterMenu, P_FILTER_BLEND, paint, StdRegionBlend);
    ADDCALLBACK(popupFilterMenu, P_FILTER_SOLARIZE, paint, doSolarize);
    ADDCALLBACK(popupFilterMenu, P_FILTER_TOGREY, paint, StdRegionGrey);
    ADDCALLBACK(popupFilterMenu, P_FILTER_TOMASK, paint, StdRegionBWMask);
    ADDCALLBACK(popupFilterMenu, P_FILTER_CONTRAST, paint, doContrast);
    ADDCALLBACK(popupFilterMenu, P_FILTER_MODIFY_RGB, paint, doModifyRGB);
    ADDCALLBACK(popupFilterMenu, P_FILTER_QUANTIZE, paint, doQuantize);
    ADDCALLBACK(popupFilterMenu, P_FILTER_USERDEF, paint, StdRegionUserDefined);
    ADDCALLBACK(popupFilterMenu, P_FILTER_LAST, paint, doLast);
    prCallback(paint, popupFilterMenu[P_FILTER_LAST].widget, False);
    ADDCALLBACK(popupFilterMenu, P_FILTER_UNDO, paint, StdRegionUndo);
    prCallback(paint, popupFilterMenu[P_FILTER_UNDO].widget, False);

    if (info) {
       ADDCALLBACK(popupSelectorMenu, P_SELECTOR_PATTERNS, info, PatternSelectCallback);
    } else {
       prCallback(paint, popupSelectorMenu[P_SELECTOR_PATTERNS].widget, False);
    }

    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_CHROMA, paint, selectColorRange);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_BACKGROUND, paint, changeBackground);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_FATBITS, paint, fatCallback);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_TOOLS, GetShell(paint), ToolSelectCallback);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_BRUSH, NULL, BrushSelectCallback);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_FONT, NULL, FontSelectCallback);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_LUPE, NULL, LupeCallback);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_SCRIPT, NULL, ScriptEditor);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_CHANGE_SIZE, info, WHZSizeCallback);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_SIZE_ZOOM_DEFS, paint, defaultWHZSizeCallback);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_CHANGE_ZOOM, info, zoomCallback);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_SNAP, paint, snapCallback);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_SNAP_SPACING, paint, snapSpacingCallback);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_TRANSPARENT, paint, transparencyCallback);
    MenuCheckItem(popupSelectorMenu[P_SELECTOR_TRANSPARENT].widget, True);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_INTERPOLATION, paint, interpolationCallback);
    MenuCheckItem(popupSelectorMenu[P_SELECTOR_INTERPOLATION].widget, True);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_GRID, paint, gridCallback);
    ADDCALLBACK(popupSelectorMenu, P_SELECTOR_GRID_PARAM, paint, gridParamCallback);
    if (info) {
        ADDCALLBACK(popupSelectorMenu, P_SELECTOR_HIDE_MENUBAR, info, hideMenuBar);
        ADDCALLBACK(popupSelectorMenu, P_SELECTOR_SHOW_MENUBAR, info, showMenuBar);
    } else {
       prCallback(paint, popupSelectorMenu[P_SELECTOR_HIDE_MENUBAR].widget, False);
       prCallback(paint, popupSelectorMenu[P_SELECTOR_SHOW_MENUBAR].widget, False);
    }

    for (i=0; i<4; i++)
    ADDCALLBACK(popupAlphaMenu, P_ALPHA_MODES+i, info, alphaModeCallback);
    ADDCALLBACK(popupAlphaMenu, P_ALPHA_PARAMS, paint, alphaParametersCallback);
    ADDCALLBACK(popupAlphaMenu, P_ALPHA_CREATE, paint, createAlphaCallback);
    ADDCALLBACK(popupAlphaMenu, P_ALPHA_EDIT, paint, editAlphaCallback);
    ADDCALLBACK(popupAlphaMenu, P_ALPHA_SAVE, paint, saveAlphaCallback);
    ADDCALLBACK(popupAlphaMenu, P_ALPHA_DELETE, paint, deleteAlphaCallback);
    MenuCheckItem(popupAlphaMenu[P_ALPHA_MODES].widget, True);

    if (info) {
       ADDCALLBACK(popupMemoryMenu, P_MEMORY_STACK, info, StdMemorySetCallback);
       ADDCALLBACK(popupMemoryMenu, P_MEMORY_RECALL, info, StdMemoryRecallCallback);
       ADDCALLBACK(popupMemoryMenu, P_MEMORY_DISCARD, info, StdMemoryRemoveCallback);
       ADDCALLBACK(popupMemoryMenu, P_MEMORY_ERASE, info, memoryEraseCallback);
    } else {
       prCallback(paint, popupMemoryMenu[P_MEMORY_STACK].widget, False);
    }

    prCallback(paint, popupFileMenu[P_FILE_PRINT].widget, False);
    prCallback(paint, popupFileMenu[P_FILE_EXTERN].widget, False);
    prCallback(paint, popupEditMenu[P_EDIT_PASTE].widget, True);
    prCallback(paint, popupSelectorMenu[P_SELECTOR_FATBITS].widget, False);
    prCallback(paint, popupMemoryMenu[P_MEMORY_RECALL].widget, False);
    prCallback(paint, popupMemoryMenu[P_MEMORY_DISCARD].widget, False);
}
