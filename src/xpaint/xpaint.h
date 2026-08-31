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

/* $Id: xpaint.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#if defined(HAVE_PARAM_H)
#include <sys/param.h>
#endif

#ifndef MIN
#define MIN(a,b)	(((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b)	(((a) > (b)) ? (a) : (b))
#endif
#ifndef ABS
#define ABS(a)		((a > 0) ? (a) : 0 - (a))
#endif
#ifndef SIGN
#define SIGN(a)		((a > 0) ? 1 : -1)
#endif

#ifdef __STDC__
#define CONCAT(a,b)	a##b
#else
#define CONCAT(a,b)	a/**/b
#endif

#ifdef AIXV3
#ifdef NULL
#undef NULL
#endif				/* NULL */
#define NULL 0
#endif				/* AIXV3 */

enum {BROWSER_READ=0, BROWSER_MULTIREAD, BROWSER_LOADED, 
      BROWSER_SAVE, BROWSER_SIMPLEREAD, BROWSER_SIMPLESAVE};

extern char *routine;

typedef struct {
    unsigned int s1;
    unsigned int s2;
} CRCdata;

typedef struct {
    int x, y;
    Pixmap pix, mask;
    unsigned char *alpha;
    CRCdata crcdata;
} RegionData;

extern struct Global_s {
    struct {
	void *image;
	Colormap cmap;
	int width, height;
	Pixmap pix, mask;
        unsigned char *alpha;
    } clipboard;
    Display *display;
    Visual *visual;
    XtAppContext appContext;
    int depth;
    Boolean timeToDie;
    Boolean explore;
    Boolean transparent;
    Boolean astext;
    Time currentTime;
    XVisualInfo vis;
    char * xft_name;   /* name of Xft font */
    char * xft_text;
    void * xft_font;
    double xft_size;   /* size of font */
    double xft_height; /* height of font */
    double xft_ascent, xft_descent, xft_maxadv; 
                       /* ascent, descent, max advance */
    double xft_rotation;     /* angle of rotation of font */
    double xft_inclination;  /* inclination */
    double xft_dilation;     /* dilation */
    double xft_linespacing;  /* linespacing factor */
    unsigned int alpha_threshold, alpha_bg, alpha_fg;
    unsigned int default_width, default_height;
    int operation;
    int default_zoom;
    int numpage;
    int cap, join, escape;  /* line caps and joins */
    int dashnumber, dashoffset; /* dash parameters */
    char dashlist[72];
    unsigned char bg[8];
    double dpi;
    Widget toplevel, bar, back, canvas, curpaint, 
           patternshell, brushpopup, popped_up, popped_parent;
    Pixmap brushpix;
    int nbrushes;
    void ** brushes;
    void * patterninfo;
    int numfiles;
    char ** loadedfiles;
    int numregions;
    RegionData * regiondata;
} Global;

typedef void *(*OperationFunc) (Widget,...);

typedef void *(*OperationAddProc) (Widget);
typedef void (*OperationRemoveProc) (Widget, void *);
typedef OperationFunc Operation_t;
#ifdef remove	/* remove is a macro in sco 3.2v4 */
#undef remove
#endif
typedef struct {
    OperationAddProc add;
    OperationRemoveProc remove;
} OperationPair;

#ifdef DEFINE_GLOBAL
#define EXTERN(var, val)	var = val ;
#else
#define EXTERN(var, val)	extern var ;
#endif

EXTERN(OperationPair * CurrentOp, NULL)
#ifdef DEFINE_GLOBAL
struct Global_s Global;
#endif

#define AllButtonsMask \
	(Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask)

/* gradient fill modes */
#define GFILL_RADIAL 0
#define GFILL_LINEAR 1
#define GFILL_CONE   2
#define GFILL_SQUARE 3

/*
**  Value at which zoom starts putting in "white" lines
**    between pixels
 */
#define ZOOM_THRESH 5

#define XYtoRECT(x1,y1,x2,y2,rect)					\
			(rect)->x = MIN(x1,x2); 			\
			(rect)->y = MIN(y1,y2);				\
			(rect)->width = MAX(x1,x2) - (rect)->x + 1;	\
			(rect)->height = MAX(y1,y2) - (rect)->y + 1;
