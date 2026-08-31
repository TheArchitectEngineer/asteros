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

/* $Id: image.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtScreen XTSCREEN
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include "Paint.h"
#include "PaintP.h"
#include "xpaint.h"
#include "image.h"
#include "hash.h"
#include "palette.h"
#include "misc.h"
#include "protocol.h"

#define HASH_SIZE	128

/*
**  Faster macros for "fast" Image <-> XImage loops
 */

#define ZINDEX(x, y, img) (((y) * img->bytes_per_line) + \
			   (((x) * img->bits_per_pixel) >> 3))

#define ZINDEX32(x, y, img) ((y) * img->bytes_per_line) + ((x) << 2)
#define ZINDEX8(x, y, img)  ((y) * img->bytes_per_line) + (x)
#define ZINDEX1(x, y, img)  ((y) * img->bytes_per_line) + ((x) >> 3)

static void 
imageToPixmapLoop(Display *, Image *, Palette *, XImage *, Pixmap *);

/*
 * The functions below are written from X11R5 MIT's code (XImUtil.c)
 *
 * The idea is to have faster functions than the standard XGetPixel function
 * to scan the image data. Indeed we can speed up things by suppressing tests
 * performed for each pixel. We do exactly the same tests but at the image
 * level. Assuming that we use only ZPixmap images.
 */

static unsigned long low_bits_table[] =
{
    0x00000000, 0x00000001, 0x00000003, 0x00000007,
    0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
    0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
    0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
    0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
    0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
    0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
    0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
    0xffffffff
};

/*
**  Actual Image routines
**
**
 */


Image *
ImageNew(int width, int height)
{
    Image *image = XtNew(Image);

    image->refCount = 1;
    image->isBW = False;
    image->isGrey = False;
    image->cmapPacked = False;
    image->cmapSize = 0;
    image->cmapData = NULL;
    image->width = width;
    image->height = height;
    image->sourceColormap = None;
    image->sourcePixmap = None;
    image->sourceMask = None;
    image->scale = 3;
    image->alpha = NULL;
    if (width == 0 || height == 0)
	image->data = NULL;
    else
	image->data = (unsigned char *) XtMalloc(width * height * sizeof(char) * 3);
    return image;
}

Image *
ImageNewCmap(int width, int height, int size)
{
    Image *image = ImageNew(0, 0);

    if (size == 0)
	image->scale = 3;
    else if (size <= 256)
	image->scale = 1;
    else
	image->scale = sizeof(short) / sizeof(char);

    image->width = width;
    image->height = height;
    image->data = (unsigned char *)
	XtMalloc(width * height * sizeof(char) * image->scale);
    if (size != 0)
	image->cmapData = (unsigned char *) XtMalloc(size * sizeof(char) * 3);
    image->cmapSize = size;

    return image;
}

Image *
ImageNewBW(int width, int height)
{
    Image *image = ImageNewCmap(width, height, 2);

    image->isBW = True;
    image->cmapData[0] = 0;
    image->cmapData[1] = 0;
    image->cmapData[2] = 0;
    image->cmapData[3] = 255;
    image->cmapData[4] = 255;
    image->cmapData[5] = 255;

    return image;
}

Image *
ImageNewGrey(int width, int height)
{
    int i;
    Image *image = ImageNewCmap(width, height, 256);

    image->isGrey = True;
    for (i = 0; i < image->cmapSize; i++) {
	image->cmapData[i * 3 + 0] = i;
	image->cmapData[i * 3 + 1] = i;
	image->cmapData[i * 3 + 2] = i;
    }

    return image;
}

void 
ImageMakeMask(Image * image)
{
    image->alpha = (unsigned char *)
	XtMalloc(image->width * image->height * sizeof(char));
}

void 
ImageDelete(Image * image)
{
    image->refCount--;
    if (image->refCount > 0)
	return;

    if (image->cmapSize > 0 && image->cmapData != NULL)
	XtFree((XtPointer) image->cmapData);
    if (image->data != NULL)
	XtFree((XtPointer) image->data);
    if (image->alpha != NULL)
	XtFree((XtPointer) image->alpha);
    XtFree((XtPointer) image);
}

/* 
 * Straightforward image copy
 */
void
ImageCopy(Image * image, Image * output)
{
    unsigned char *ip, *op;
    int x, y;

    for (y = 0; y < image->height; y++) {
        for (x = 0; x < image->width; x++) {
            ip = ImagePixel(image, x, y);
            op = ImagePixel(output, x, y);
            memcpy(op, ip, 3);
	}
    }
}

/*
**  Convert a colormap image into a RGB image
**    useful for writers which only deal with RGB and not
**    colormaps
 */
Image *
ImageToRGB(Image * image)
{
    unsigned char *ip, *op;
    Image *out;
    int x, y;

    if (image->cmapSize == 0) {
	image->refCount++;
	return image;
    }
    out = ImageNew(image->width, image->height);
    op = image->data;

    for (y = 0; y < image->height; y++) {
	for (x = 0; x < image->width; x++) {
	    ip = ImagePixel(image, x, y);
	    *op++ = *ip++;
	    *op++ = *ip++;
	    *op++ = *ip++;
	}
    }

    out->isBW = image->isBW;
    out->isGrey = False;
    out->width = image->width;
    out->height = image->height;

    image->cmapPacked = False;

    return out;
}

/*
**  Create a nice image for writing routines.
 */
Image *
PixmapToImage(Widget w, Pixmap pix, Colormap cmap)
{
    XImage *xim;
    Image *image;
    Display *dpy = XtDisplay(w);
    int x, y;
    int width, height;
    unsigned char *ptr, *data;
    unsigned short *sptr;
    int format = 0;
    void *htable = NULL;
    Palette *map = PaletteFind(w, cmap);
    unsigned long lbt;

    GetPixmapWHD(dpy, pix, &width, &height, NULL);
    xim = XGetImage(dpy, pix, 0, 0, width, height, AllPlanes, ZPixmap);

    if (map == NULL)
	map = PaletteGetDefault(w);

    if (map->isMapped) {
	unsigned char *cptr;

	image = ImageNewCmap(width, height, map->ncolors);
	cptr = image->cmapData;

	for (y = 0; y < map->ncolors; y++, cptr += 3) {
	    XColor *col = PaletteLookup(map, y);
	    unsigned char r = col->red >> 8;
	    unsigned char g = col->green >> 8;
	    unsigned char b = col->blue >> 8;

	    cptr[0] = r;
	    cptr[1] = g;
	    cptr[2] = b;
	}
    } else {
	image = ImageNew(width, height);
    }

    ptr = image->data;
    sptr = (unsigned short *) image->data;
    data = (unsigned char *) xim->data;
    lbt = low_bits_table[xim->depth];

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    XColor *col;
	    Pixel pixel;
	    unsigned char r, g, b;

	    if (xim->bits_per_pixel == 8)
		pixel = data[ZINDEX8(x, y, xim)] & lbt;
	    else
		pixel = XGetPixel(xim, x, y);

	    if (map->isMapped) {
		if (map->ncolors <= 256)
		    *ptr++ = pixel;
		else
		    *sptr++ = pixel;

		r = image->cmapData[pixel * 3 + 0];
		g = image->cmapData[pixel * 3 + 1];
		b = image->cmapData[pixel * 3 + 2];
	    } else {
		col = PaletteLookup(map, pixel);

		*ptr++ = r = col->red >> 8;
		*ptr++ = g = col->green >> 8;
		*ptr++ = b = col->blue >> 8;
	    }

	    if (r != g || g != b)
		format = 2;
	    else if (format == 0 && r != 0 && r != 255)
		format = 1;
	}

	if (y % 64 == 0)
	    StateTimeStep();
    }

    /*
    **	Check to see if we just created a B&W or Grey scale image?
     */
    if (format == 0 || format == 1) {
	int newSize;
	unsigned char *newMap;
	unsigned char *ip;

	if (format == 0) {
	    newSize = 2;
	    newMap = (unsigned char *) XtCalloc(sizeof(char) * 3, 2);
	    newMap[0 + 0] = 0;
	    newMap[0 + 1] = 0;
	    newMap[0 + 2] = 0;
	    newMap[3 + 0] = 255;
	    newMap[3 + 1] = 255;
	    newMap[3 + 2] = 255;
	} else {
	    newSize = 256;
	    newMap = (unsigned char *) XtCalloc(sizeof(char) * 3, 256);
	    for (y = 0; y < 256; y++) {
		newMap[y * 3 + 0] = y;
		newMap[y * 3 + 1] = y;
		newMap[y * 3 + 2] = y;
	    }
	}

	/* overwrite top of image->data with compressed version (scale == 1) */
	ip = image->data;
	for (y = 0; y < height; y++) {
	    for (x = 0; x < width; x++, ip++) {
		unsigned char *rgb;

		/* [image.h] return pointer to RGB triplet for pixel (x,y) */
		rgb = ImagePixel(image, x, y);

		/* we know we're B&W or grayscale, so check only red values */
		if (format == 0 && *rgb == 255) {
		    *ip = 1;
		} else {
		    *ip = *rgb;
		}
	    }
	}

	if (image->cmapData != NULL)
	    XtFree((XtPointer) image->cmapData);

	image->cmapSize = newSize;
	image->cmapData = newMap;

	if (format == 0)
	    image->isBW = True;
	else /* if (format == 1) */	/* [GRR 20010720:  necessarily true] */
	    image->isGrey = True;
	image->scale = 1;
    }
    image->sourceColormap = (unsigned long) cmap;
    image->sourcePixmap = (unsigned long) pix;


    if (htable != NULL)
	HashDestroy(htable);
    XDestroyImage(xim);

    return image;
}

void 
PixmapToImageMask(Widget w, Image * image, Pixmap mask)
{
    XImage *xim;
    int width, height;
    int x, y, endX, endY;
    unsigned char *ip;
    Display *dpy = XtDisplay(w);

    if (!image->alpha) return;
    image->sourceMask = mask;
    GetPixmapWHD(dpy, mask, &width, &height, NULL);
    xim = XGetImage(dpy, mask, 0, 0, width, height, AllPlanes, ZPixmap);

    ImageMakeMask(image);
    ip = image->alpha;

    if ((endX = image->width) > width)
	endX = width;
    if ((endY = image->height) > height)
	endY = height;

    for (y = 0; y < endY; y++) {
	for (x = 0; x < endX; x++)
	    *ip++ = (XGetPixel(xim, x, y))? 255:0;
	for (; x < image->width; x++)
	    *ip++ = 255;
    }
    XDestroyImage(xim);
}

Image *
ClipboardGetImage(int num)
{
    Image *image;
    Colormap cmap;
    if (num<0 || num>=Global.numregions || !Global.toplevel) return NULL;

    XtVaGetValues(Global.toplevel, XtNcolormap, &cmap, NULL);
    image = PixmapToImage(Global.toplevel, Global.regiondata[num].pix, cmap);
    if (image && Global.regiondata[num].mask)
       PixmapToImageMask(Global.toplevel, image, Global.regiondata[num].mask);
    return image;
}


/*
**  Compress an image into a nice number of
**   colors for display purposes.
 */
static Image *
quantizeColormap(Image * input, Palette * map, Boolean flag)
{
    Image *output;
    unsigned char *op;
    int x, y;
    int ncol;

    /*
    **	If the output is either B&W or grey do something
    **	 fast and easy.
     */
    if (!map->isGrey)
	return ImageCompress(input, map->ncolors, 0);

    ncol = map->ncolors > 256 ? 256 : map->ncolors;
    output = ImageNewCmap(input->width, input->height, ncol);

    op = output->data;

    for (y = 0; y < ncol; y++) {
	unsigned char v = ((float) y / (float) (ncol - 1)) * 255.0;
	ImageSetCmap(output, y, v, v, v);
    }

    for (y = 0; y < input->height; y++) {
	for (x = 0; x < input->width; x++, op++) {
	    unsigned char *dp, v;

	    dp = ImagePixel(input, x, y);
	    v = (dp[0] * 11 + dp[1] * 16 + dp[2] * 5) >> 5;	/* pp=.33R+.5G+.17B */
	    *op = (int) (((float) v / 256.0) * (float) ncol);
	}
    }

    output->alpha = input->alpha;
    input->alpha = NULL;
    ImageDelete(input);
    return output;
}

/* static */ void 			/* non-static for WritePNG() */
compressColormap(Image * image)
{
    unsigned char used[32768];
    int size = image->width * image->height;
    int i, count, newSize;
    unsigned char *newMap;
    unsigned short *isp;
    unsigned char *icp;

    if (image->cmapSize <= 2 || image->cmapPacked)
	return;

    memset(used, False, sizeof(used));

    /*
    **	Find out usage information over the colormap 
     */
    count = 0;
    if (image->cmapSize > 256) {
	isp = (unsigned short *) image->data;
	for (i = 0; i < size && count != image->cmapSize; i++, isp++) {
	    if (!used[*isp]) {
		used[*isp] = True;
		count++;
	    }
	}
    } else {
	icp = image->data;
	for (i = 0; i < size && count != image->cmapSize; i++, icp++) {
	    if (!used[*icp]) {
		used[*icp] = True;
		count++;
	    }
	}
    }

    /*
    **	Colormap is fully used.
     */
    if (count == image->cmapSize) {
	image->cmapPacked = True;
	return;
    }
    /*
    **	Now build the remapping colormap, and
    **	  set the index.
     */
    newSize = count;
    newMap = (unsigned char *) XtCalloc(count, sizeof(unsigned char) * 3);
    for (count = i = 0; i < image->cmapSize; i++) {
	if (!used[i])
	    continue;
	newMap[count * 3 + 0] = image->cmapData[i * 3 + 0];
	newMap[count * 3 + 1] = image->cmapData[i * 3 + 1];
	newMap[count * 3 + 2] = image->cmapData[i * 3 + 2];
	used[i] = count++;
    }

    if (image->cmapSize > 256 && newSize > 256) {
	isp = (unsigned short *) image->data;
	for (i = 0; i < size; i++, isp++)
	    *isp = used[*isp];
    } else if (image->cmapSize > 256 && newSize <= 256) {
	/*
	**  Map a big colormap down to a small one.
	 */
	isp = (unsigned short *) image->data;
	icp = image->data;
	for (i = 0; i < size; i++, icp++, isp++)
	    *icp = used[*isp];

	image->data = (unsigned char *)
	    XtRealloc((XtPointer) image->data, size * sizeof(unsigned char));
    } else {
	icp = image->data;
	for (i = 0; i < size; i++, icp++)
	    *icp = used[*icp];
    }

    XtFree((XtPointer) image->cmapData);
    image->cmapData = newMap;
    image->cmapSize = newSize;
    image->cmapPacked = True;
    image->isGrey = False;
}

/*
**  Convert an input image into a nice pixmap 
**   so we can edit it.
**
**  Side effect	 -- always destroy the input image
 */
Boolean
ImageToPixmap(Image * image, Widget w, Pixmap * pix, Colormap * cmap)
{
    Display *dpy = XtDisplay(w);
    Palette *map;
    XImage *xim;
    int width = image->width, height = image->height;

    map = PaletteCreate(w);
    *cmap = map->cmap;

    if ((*pix = XCreatePixmap(dpy, DefaultRootWindow(dpy),
			      width, height, map->depth)) == None)
	return False;

    if ((xim = NewXImage(dpy, NULL, map->depth, width, height)) == NULL) {
	XFreePixmap(dpy, *pix);
	return False;
    }
    if ((image->cmapSize > map->ncolors) ||
	(image->cmapSize == 0 && map->isMapped))
	image = quantizeColormap(image, map, False);

    if (image->cmapSize > 0)
	compressColormap(image);

    imageToPixmapLoop(dpy, image, map, xim, pix);

    return True;
}

Pixmap
AlphaToPixmap(Widget w, int width, int height,
	      char *data, XRectangle * rect)
{
    Display *dpy = XtDisplay(w);
    GC gc;
    Pixmap mask;
    XImage *xim;
    int x, y;
    int xs, ys;
    unsigned char *ucp;
    unsigned char threshold;
    int pWidth, pHeight;

    if (data == NULL)
	return None;

    if (rect == NULL) {
	xs = 0;
	ys = 0;
	pWidth = width;
	pHeight = height;
    } else {
	xs = rect->x;
	ys = rect->y;
	pWidth = rect->width;
	pHeight = rect->height;
    }

    mask = XCreatePixmap(dpy, DefaultRootWindow(dpy), pWidth, pHeight, 1);
    gc = XCreateGC(dpy, mask, 0, 0);
    xim = NewXImage(dpy, NULL, 1, pWidth, pHeight);

    if (xim->byte_order != xim->bitmap_bit_order) {
	for (y = 0; y < pHeight; y++) {
	    ucp = (unsigned char *) (data + (width * (y + ys)) + xs);
	    for (x = 0; x < pWidth; x++, ucp++)
		XPutPixel(xim, x, y, *ucp);
	}
    } else {
	unsigned char *op = (unsigned char *) xim->data;
	unsigned char *cp;

	if (xim->bitmap_bit_order == MSBFirst) {
	    for (y = 0; y < pHeight; y++) {
		ucp = (unsigned char *) (data + (width * (y + ys)) + xs);
		for (x = 0; x < pWidth; x++, ucp++) {
		    unsigned char v = 0x80 >> (x & 7);
		    cp = &op[ZINDEX1(x, y, xim)];
		    if (*ucp)
			*cp |= v;
		    else
			*cp &= ~v;
		}
	    }
	} else {
	    threshold = (unsigned char) Global.alpha_threshold;
	    for (y = 0; y < pHeight; y++) {
		ucp = (unsigned char *) (data + (width * (y + ys)) + xs);
		for (x = 0; x < pWidth; x++, ucp++) {
		    unsigned char v = 0x01 << (x & 7);
		    cp = &op[ZINDEX1(x, y, xim)];
		    if (*ucp >= threshold)
			*cp |= v;
		    else
			*cp &= ~v;
		}
	    }
	}

    }

    XPutImage(dpy, mask, gc, xim, 0, 0, 0, 0, pWidth, pHeight);
    XFreeGC(dpy, gc);

    XDestroyImage(xim);

    return mask;
}

Pixmap
ImageMaskToPixmap(Widget w, Image * image)
{
    return AlphaToPixmap(w, image->width,
			    image->height, (char *) image->alpha, NULL);
}

void
get_color_components(Pixel p, unsigned char *c)
{
    unsigned char u, v;

    if (Global.depth >= 24) {
        c[0] = p>>16;
        c[1] = (p>>8) & 255;
        c[2] = p & 255;
    } else
    if (Global.depth == 16) {
	u = p&255;
        v = p>>8;
        c[0] = ((v>>3)*255)/31;
        c[1] = ((((v&7)<<3) | (u>>5))*255)/63;
        c[2] = ((u&31)*255)/31;
    } else
    if (Global.depth == 15) {
	u = p&255;
        v = p>>8;
        c[0] = ((v>>2)*255)/31;
        c[1] = ((((v&3)<<3) | (u>>5))*255)/31;
        c[2] = ((u&31)*255)/31;
    } else
        c[0] = p&255;
}

Pixel
get_pixel_from_colors(unsigned char *c)
{
    Pixel p;
    if (Global.depth >= 24) {
        p = ((unsigned int)c[0]<<16) | ((unsigned int)c[1]<<8) | 
             (unsigned int)c[2];
    } else
    if (Global.depth == 16) {
        p = (((c[2]&248)>>3) | ((c[1]&28)<<3)) |
            ((((c[1]&224)>>5) | (c[0]&248)) << 8);
    } else
    if (Global.depth == 15) {
        p = (((c[2]&248)>>3) | ((c[1]&56)<<2)) |
            ((((c[1]&192)>>6) | ((c[0]&248)>>1)) << 8);
    } else
        p = c[0]||c[1]||c[2];
    return p;
}

/*
**  Convert an input image into a nice pixmap 
**   so we can edit it.
**
**  Side effect	 -- always destroy the input image
 */
static void 
imageToPixmapLoop(Display * dpy, Image * image,
		  Palette * map, XImage * xim, Pixmap * pix)
{
    GC gc;
    int x, y;
    int width = image->width, height = image->height;

    if (image->cmapSize > 0) {
	unsigned short *sdp = (unsigned short *) image->data;
	unsigned char *cdp = (unsigned char *) image->data;
	Pixel *list = (Pixel *) XtCalloc(sizeof(Pixel), image->cmapSize);
	XColor *xcol = (XColor *) XtCalloc(sizeof(XColor), image->cmapSize);

	for (y = 0; y < image->cmapSize; y++) {
	    xcol[y].red = image->cmapData[y * 3 + 0] << 8;
	    xcol[y].green = image->cmapData[y * 3 + 1] << 8;
	    xcol[y].blue = image->cmapData[y * 3 + 2] << 8;
	}

	PaletteAllocN(map, xcol, image->cmapSize, list);

	if (xim->bits_per_pixel == 8) {
	    unsigned char *data = (unsigned char *) xim->data;

	    for (y = 0; y < height; y++)
		for (x = 0; x < width; x++, sdp++, cdp++)
		    data[ZINDEX8(x, y, xim)] = list[image->cmapSize > 256 ? *sdp : *cdp];
	} else {
	    /*
	    **	Slow loop
	     */
	    for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++, sdp++, cdp++)
		    XPutPixel(xim, x, y, list[image->cmapSize > 256 ? *sdp : *cdp]);
		if (y % 256 == 0)
		    StateTimeStep();
	    }
	}

	XtFree((XtPointer) list);
	XtFree((XtPointer) xcol);
    } else {
	int step = 64 * 256 / width;
	unsigned char *cp = image->data;

	for (y = 0; y < height; y++) {
	    for (x = 0; x < width; x++) {
		XColor c;
		Pixel p;

		c.red = *cp++ << 8;
		c.green = *cp++ << 8;
		c.blue = *cp++ << 8;

		p = PaletteAlloc(map, &c);

		if (xim->bits_per_pixel == 8)
		    xim->data[ZINDEX8(x, y, xim)] = p;
		else
		    XPutPixel(xim, x, y, p);
	    }

	    if (y % step == 0)
		StateTimeStep();
	}
    }

    gc = XCreateGC(dpy, *pix, 0, 0);
    XPutImage(dpy, *pix, gc, xim, 0, 0, 0, 0, width, height);
    XFreeGC(dpy, gc);
    XDestroyImage(xim);
    ImageDelete(image);
}

/*
 * Convert an Image to a Pixmap, using specified colormap.
 * Unless pix == None, a new pixmap is created.
 */
Boolean
ImageToPixmapCmap(Image * image, Widget w, Pixmap * pix, Colormap cmap)
{
    Display *dpy = XtDisplay(w);
    Palette *map;
    XImage *xim;
    int width = image->width, height = image->height;

    if ((map = PaletteFind(w, cmap)) == NULL)
	map = PaletteGetDefault(w);

    if (*pix == None) {
	if ((*pix = XCreatePixmap(dpy, RootWindowOfScreen(XtScreen(w)),
				  width, height, map->depth)) == None)
	    return False;
    }
    if ((xim = NewXImage(dpy, NULL, map->depth, width, height)) == NULL) {
	XFreePixmap(dpy, *pix);
	return False;
    }
    if ((image->cmapSize > map->ncolors) ||
	(image->cmapSize == 0 && map->isMapped))
	image = quantizeColormap(image, map, False);

    if (image->cmapSize > 0)
	compressColormap(image);

    imageToPixmapLoop(dpy, image, map, xim, pix);

    return True;
}


