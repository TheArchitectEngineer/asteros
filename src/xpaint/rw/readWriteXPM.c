/* +-------------------------------------------------------------------+ */
/* | Copyright 1993, David Koblas (koblas@netcom.com)		       | */
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

/* $Id: readWriteXPM.c,v 1.21 2005/03/20 20:15:34 demailly Exp $ */

#include <stdio.h>

#include <X11/Intrinsic.h>
#include <X11/xpm.h>

#include "xpaint.h"
#include "image.h"
#include "rwTable.h"

extern int file_transparent;

int WriteXPM(char *file, Image * image)
{
    XpmAttributes attr;
    Display *dpy = Global.display;

    if (!image) return 1;
    if (!image->sourcePixmap) return 1;
    attr.valuemask = XpmColormap;
    if (image->sourceColormap)
        attr.colormap = (Colormap) image->sourceColormap;
    else
        attr.colormap = DefaultColormap(dpy, DefaultScreen(dpy));

    return (XpmWriteFileFromPixmap(Global.display, file,
	      image->sourcePixmap, image->sourceMask, &attr) != XpmSuccess);
}

int TestXPM(char *file)
{
    FILE *fd = fopen(file, "r");
    char buf[40];
    int i, n, ret = 0;
    int cstart = False;

    if (fd == NULL)
	return 0;

    n = fread(buf, sizeof(char), sizeof(buf), fd);

    for (i = 0; i < n && ret == 0; i++) {
	if (!cstart) {
	    if (buf[i] == '/' && buf[i + 1] == '*')
		cstart = True;
	} else {
	    if (buf[i] == 'X' && buf[i + 1] == 'P' && buf[i + 2] == 'M')
		ret = 1;
	}
    }

    fclose(fd);
    return ret;
}

Image * ReadXPM(char *file)
{
    Display *dpy = Global.display;
    XImage *xim, *mim = NULL;
    int x, y, i;
    Image *image;
    XpmAttributes attr;
    XpmColorSymbol bg;
    XColor bgcolor;
    XColor *col;
    unsigned char *ucp, *ump = NULL;
    unsigned short *usp;
    Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
    int status;

    bgcolor.flags = DoRed | DoGreen | DoBlue;
    bgcolor.red = Global.bg[0]*257;
    bgcolor.green = Global.bg[1]*257;
    bgcolor.blue = Global.bg[2]*257;
    XAllocColor(dpy, cmap, &bgcolor);
    attr.valuemask = XpmColormap | XpmReturnPixels | XpmColorSymbols;
    attr.colormap =  cmap;
    attr.numsymbols = 1;
    attr.colorsymbols = &bg;
    bg.pixel = bgcolor.pixel;
    bg.name = NULL;
    bg.value = "None";
    
    if ((status = XpmReadFileToImage(dpy, file, &xim, &mim, &attr)) != XpmSuccess) {
	switch (status) {
	case XpmColorError:
	    RWSetMsg("XPM Color Error");
	    break;
	case XpmSuccess:
	    RWSetMsg("Success, shouldn't have error & success");
	    break;
	case XpmOpenFailed:
	    RWSetMsg("XPM Open Failed");
	    break;
	case XpmFileInvalid:
	    RWSetMsg("File Invalid");
	    break;
	case XpmNoMemory:
	    RWSetMsg("Not enough memory");
	    break;
	case XpmColorFailed:
	    RWSetMsg("Unable to allocate color");
	    break;
	}
	XpmFreeAttributes(&attr);
	return NULL;
    }

    image = ImageNewCmap(attr.width, attr.height, attr.npixels);
    ucp = (unsigned char *) image->data;
    usp = (unsigned short *) image->data;

    col = (XColor *) XtMalloc(sizeof(XColor) * attr.npixels);
    for (i = 0; i < attr.npixels; i++) {
	col[i].pixel = attr.pixels[i];
	col[i].flags = DoRed | DoGreen | DoBlue;
    }
    XQueryColors(dpy, cmap, col, attr.npixels);

    if (mim != NULL) {
        file_transparent = 1;
	ImageMakeMask(image);
	ump = image->alpha;
    }

    for (i = 0; i < attr.npixels; i++)
	ImageSetCmap(image, i, col[i].red>>8, col[i].green>>8, col[i].blue>>8);

    for (y = 0; y < xim->height; y++) {
	for (x = 0; x < xim->width; x++) {
	    Pixel p;

	    if (mim != NULL) {
		Pixel f = XGetPixel(mim, x, y);

		if (!(*ump++ = (f?255:0))) {
		    if (attr.npixels > 256)
			*usp++ = 0;
		    else
			*ucp++ = 0;
		    continue;
		}
	    }
	    p = XGetPixel(xim, x, y);

	    for (i = 0; i < attr.npixels && col[i].pixel != p; i++);

	    if (attr.npixels > 256)
		*usp++ = i;
	    else
		*ucp++ = i;
	}
    }

    XtFree((XtPointer) col);
    XDestroyImage(xim);
    XpmFreeAttributes(&attr);

    return image;
}
