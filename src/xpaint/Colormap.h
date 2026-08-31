#ifndef _Colormap_h
#define _Colormap_h

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

/* $Id: Colormap.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

/****************************************************************
 *
 * Colormap widget
 *
 ****************************************************************/

/* Resources:

   Name              Class              RepType         Default Value
   ----              -----              -------         -------------
   background        Background         Pixel           XtDefaultBackground
   border            BorderColor        Pixel           XtDefaultForeground
   borderWidth       BorderWidth        Dimension       1
   destroyCallback   Callback           Pointer         NULL
   height            Height             Dimension       0
   mappedWhenManaged MappedWhenManaged  Boolean         True
   sensitive         Sensitive          Boolean         True
   width             Width              Dimension       0
   x                 Position           Position        0
   y                 Position           Position        0

 */

/* define any special resource names here that are not in <X11/StringDefs.h> */

#define XtNcellWidth  "cellWidth"
#define XtNcellHeight "cellHeight"
#define XtNcolor      "color"
#define XtNreadOnly   "readOnly"

#define XtCCellWidth  "CellWidth"
#define XtCCellHeight "CellHeight"
#ifndef XtCColor
#define XtCColor      "Color"
#endif

/* declare specific ColormapWidget class and instance datatypes */

typedef struct _ColormapClassRec *ColormapWidgetClass;
typedef struct _ColormapRec *ColormapWidget;

/* declare the class constant */

extern WidgetClass colormapWidgetClass;

extern void CwGetColor(Widget, XColor *);
extern void CwSetColor(Widget, XColor *);

#endif				/* _Colormap_h */
