
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

/* $Id: typeConvert.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/StringDefs.h>
#include "palette.h"
#include "misc.h"

static Boolean
newCvtStringToPixel(Display * dpy, XrmValuePtr args,
		    Cardinal * nargs, XrmValuePtr from, XrmValuePtr to,
		    XtPointer junk)
{
    Screen *screen;
    Colormap cmap;
    Palette *map;
    String name = (String) (from->addr);
    Boolean isReverse = False;
    XColor scol, ecol;
    static Pixel op;

    if (*nargs != 2)
	return False;

    screen = *((Screen **) args[0].addr);
    cmap = *((Colormap *) args[1].addr);

    if (strcmp(name, XtDefaultBackground) == 0)
	name = isReverse ? "black" : "white";
    else if (strcmp(name, XtDefaultForeground) == 0)
	name = isReverse ? "white" : "black";

    if ((map = PaletteFindDpy(dpy, cmap)) == NULL) {

	/*
	**  Not a "mapped" window, use standard X
	 */
	if (XAllocNamedColor(dpy, cmap, name, &scol, &ecol) == 0) {
	    if (XParseColor(dpy, cmap, name, &scol) == 0)
		return False;
	    if (XAllocColor(dpy, cmap, &scol) == 0)
		return False;
	}
#if 0
	printf("name = %s   match = %d %d %d  screen = %d %d %d\n",
	       name, ecol.red / 256, ecol.green / 256, ecol.blue / 256,
	       scol.red / 256, scol.green / 256, scol.blue / 256);
#endif

	op = scol.pixel;
    } else {
	if (XLookupColor(dpy, map->cmap, name, &scol, &ecol) == 0 &&
	    XParseColor(dpy, map->cmap, name, &scol) == 0)
	    return False;
#if 0
	printf("name = %s   match = %d %d %d  screen = %d %d %d\n",
	       name, ecol.red / 256, ecol.green / 256, ecol.blue / 256,
	       scol.red / 256, scol.green / 256, scol.blue / 256);
#endif
	op = PaletteAlloc(map, &scol);
    }

    if (to->addr == NULL) {
	to->addr = (XtPointer) & op;
    } else {
	if (to->size < sizeof(Pixel))
	    return False;
	memcpy(to->addr, &op, sizeof(Pixel));
    }

    to->size = sizeof(Pixel);

    return True;
}

void 
InitTypeConverters(void)
{
    static XtConvertArgRec colorArgs[] =
    {
	{XtWidgetBaseOffset,
	 (XtPointer) XtOffsetOf(WidgetRec, core.screen),
	 sizeof(Screen *)},
	{XtWidgetBaseOffset,
	 (XtPointer) XtOffsetOf(WidgetRec, core.colormap),
	 sizeof(Colormap)},
    };

    XtSetTypeConverter(XtRString, XtRPixel,
		       (XtTypeConverter) newCvtStringToPixel,
		       colorArgs, XtNumber(colorArgs),
		       XtCacheRefCount, NULL);
}
