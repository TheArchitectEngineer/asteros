#ifndef _ColormapP_h
#define _ColormapP_h

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

/* $Id: ColormapP.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include "Colormap.h"
/* include superclass private header file */
#include <X11/CoreP.h>

/* define unique representation types not found in <X11/StringDefs.h> */

#define XtRColormapResource "ColormapResource"

typedef struct {
    int empty;
} ColormapClassPart;

typedef struct _ColormapClassRec {
    CoreClassPart core_class;
    ColormapClassPart colormap_class;
} ColormapClassRec;

extern ColormapClassRec colormapClassRec;

typedef struct {
    Boolean editable;
    int ncel, curcel;
    Pixel pixel;
    Colormap cmap;
    XtCallbackProc select;
    int cheight, cwidth;	/* cell width, height */
    int ncheight, ncwidth;	/* cell width, height */
    XtCallbackList callbacks;
    int thickness;
    Pixel foreground;
} ColormapPart;

typedef struct _ColormapRec {
    CorePart core;
    ColormapPart color;
} ColormapRec;

#endif				/* _ColormapP_h */
