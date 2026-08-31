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

/* 
** Copyright (C) 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
 */

/* $Id: readWriteXWD.c,v 1.21 2005/03/20 20:15:34 demailly Exp $ */

#include "image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __VMS
# include "XWDFile.h"
#else
# include <X11/XWDFile.h>
#endif

/*
**  Some versions of XWDFile.h don't have the 'typedef' in front of
**    the XWDColor definition.
 */
typedef struct {
    CARD32 pixel B32;
    CARD16 red B16;
    CARD16 green B16;
    CARD16 blue B16;
    CARD8 flags;
    CARD8 pad;
} myXWDColor;

#define XWDColor	myXWDColor


#ifndef __VMS
void *calloc(size_t, size_t);
#endif

#define True	1
#define False	0

#define LSBFirst	0
#define MSBFirst	1

#define XYBitmap	0
#define XYPixmap	1
#define ZPixmap		2

#define StaticGray	0
#define GrayScale	1
#define StaticColor	2
#define PseudoColor	3
#define TrueColor	4
#define DirectColor	5

#if XWD_FILE_VERSION != 7
#error Do not know how to write anything but version 7 X window dumps
#endif

/*
**  Some read and write routines for long values
 */

static unsigned long readlong(FILE * fp)
{
    unsigned char a, b, c, d;

    a = getc(fp);
    b = getc(fp);
    c = getc(fp);
    d = getc(fp);

    return ((a & 0xff) << 24) |
	((b & 0xff) << 16) |
	((c & 0xff) << 8) |
	(d & 0xff);
}

static unsigned short readshort(FILE * fp)
{
    unsigned char a, b;
    a = getc(fp);
    b = getc(fp);

    return ((a & 0xff) << 8) | (b & 0xff);
}

static void writelong(FILE * fp, long l)
{
    putc((l >> 24) & 0xff, fp);
    putc((l >> 16) & 0xff, fp);
    putc((l >> 8) & 0xff, fp);
    putc(l & 0xff, fp);
}

static void writeshort(FILE * fp, unsigned short s)
{
    putc((s >> 8) & 0xff, fp);
    putc(s & 0xff, fp);
}

static long swaplong(long l)
{
    unsigned char b1, b2, b3, b4;

    b1 = (l >> 24) & 0xff;
    b2 = (l >> 16) & 0xff;
    b3 = (l >> 8) & 0xff;
    b4 = l & 0xff;

    return ((long) b4 << 24) | ((long) b3 << 16) | (b2 << 8) | b1;
}

static short swapshort(short l)
{
    unsigned char b1, b2;

    b1 = (l >> 8) & 0xff;
    b2 = l & 0xff;

    return (b2 << 8) | b1;
}

/*
**
 */
static int bits_per_item, bits_used, bit_shift, bits_per_pixel;
static unsigned long pixel_mask;
static int byte_order, bit_order;


static unsigned long getpixnum(FILE * fp)
{
    static unsigned char b8;
    static unsigned short b16;
    static unsigned long b32;
    unsigned long val = 0;

    if (bits_used == bits_per_item) {
	switch (bits_per_item) {
	case 8:
	    b8 = getc(fp);
	    break;
	case 16:
	    b16 = readshort(fp);
	    if (byte_order != MSBFirst)
		b16 = swapshort(b16);
	    break;
	case 32:
	    b32 = readlong(fp);
	    if (byte_order != MSBFirst)
		b32 = swaplong(b32);
	    break;
	}
	bits_used = 0;

	if (bit_order == MSBFirst)
	    bit_shift = bits_per_item - bits_per_pixel;
	else
	    bit_shift = 0;
    }
    switch (bits_per_item) {
    case 8:
	val = (b8 >> bit_shift) & pixel_mask;
	break;
    case 16:
	val = (b16 >> bit_shift) & pixel_mask;
	break;
    case 32:
	val = (b32 >> bit_shift) & pixel_mask;
	break;
    }

    if (bit_order == MSBFirst)
	bit_shift -= bits_per_pixel;
    else
	bit_shift += bits_per_pixel;
    bits_used += bits_per_pixel;

    return val;
}



/*
**  Straight forward writer.
**   Take advantage of the fact that a Grey image is 8 bits deep
**   and some simple properties of colormap images for simplicity.
 */
int WriteXWD(char *file, Image * image)
{
    static char *name = "XPaint Image";
    XWDColor color;
    XWDFileHeader hdr;
    FILE *fp;
    int i, x, y;
    int isBW = 0, isGrey = 0;

    if ((fp = fopen(file, "w")) == NULL)
	return True;

    /* 
    **	Set up the header.
     */
    hdr.header_size = sizeof(hdr) + strlen(name) + 1;
    hdr.file_version = XWD_FILE_VERSION;
    hdr.pixmap_format = ZPixmap;
    hdr.pixmap_width = image->width;
    hdr.pixmap_height = image->height;
    hdr.xoffset = 0;
    hdr.byte_order = MSBFirst;
    hdr.bitmap_bit_order = MSBFirst;
    hdr.window_width = image->width;
    hdr.window_height = image->height;
    hdr.window_x = 0;
    hdr.window_y = 0;
    hdr.window_bdrwidth = 0;

    if (image->isBW) {
	hdr.pixmap_depth = 1;
	hdr.bits_per_pixel = 1;
	hdr.colormap_entries = image->cmapSize;
	hdr.ncolors = image->cmapSize;
	hdr.bytes_per_line = (image->width + 7) / 8;

	isBW = 1;

	/*
	**  Common between grey and BW
	 */
	hdr.bitmap_unit = 8;
	hdr.bitmap_pad = 8;
	hdr.visual_class = StaticGray;
	hdr.red_mask = 0;
	hdr.green_mask = 0;
	hdr.blue_mask = 0;
    } else if (image->isGrey) {
	hdr.pixmap_depth = 8;
	hdr.bits_per_pixel = 8;
	hdr.colormap_entries = image->cmapSize;
	hdr.ncolors = image->cmapSize;
	hdr.bytes_per_line = image->width;

	isGrey = 1;

	/*
	**  Common between grey and BW
	 */
	hdr.bitmap_unit = 8;
	hdr.bitmap_pad = 8;
	hdr.visual_class = StaticGray;
	hdr.red_mask = 0;
	hdr.green_mask = 0;
	hdr.blue_mask = 0;
    } else if (image->cmapSize > 0) {
	/*
	**  Color mapped image
	 */
	hdr.pixmap_depth = (image->cmapSize > 256) ? 16 : 8;
	hdr.bits_per_pixel = (image->cmapSize > 256) ? 16 : 8;
	hdr.visual_class = PseudoColor;
	hdr.colormap_entries = image->cmapSize;
	hdr.ncolors = image->cmapSize;
	hdr.red_mask = 0;
	hdr.green_mask = 0;
	hdr.blue_mask = 0;
	hdr.bytes_per_line = image->width;
	hdr.bitmap_unit = 8;
	hdr.bitmap_pad = 8;
    } else {
	/*
	**  RGB Image
	 */
	hdr.pixmap_depth = 24;
	hdr.bitmap_unit = 32;
	hdr.bitmap_pad = 32;
	hdr.bits_per_pixel = 32;
	hdr.visual_class = DirectColor;
	hdr.colormap_entries = 256;
	hdr.ncolors = 0;
	hdr.red_mask = 0xff0000;
	hdr.green_mask = 0xff00;
	hdr.blue_mask = 0xff;
	hdr.bytes_per_line = image->width * 4;
    }
    hdr.bits_per_rgb = hdr.pixmap_depth;

    /* Write out the header in big-endian order. */
    writelong(fp, hdr.header_size);
    writelong(fp, hdr.file_version);
    writelong(fp, hdr.pixmap_format);
    writelong(fp, hdr.pixmap_depth);
    writelong(fp, hdr.pixmap_width);
    writelong(fp, hdr.pixmap_height);
    writelong(fp, hdr.xoffset);
    writelong(fp, hdr.byte_order);
    writelong(fp, hdr.bitmap_unit);
    writelong(fp, hdr.bitmap_bit_order);
    writelong(fp, hdr.bitmap_pad);
    writelong(fp, hdr.bits_per_pixel);
    writelong(fp, hdr.bytes_per_line);
    writelong(fp, hdr.visual_class);
    writelong(fp, hdr.red_mask);
    writelong(fp, hdr.green_mask);
    writelong(fp, hdr.blue_mask);
    writelong(fp, hdr.bits_per_rgb);
    writelong(fp, hdr.colormap_entries);
    writelong(fp, hdr.ncolors);
    writelong(fp, hdr.window_width);
    writelong(fp, hdr.window_height);
    writelong(fp, hdr.window_x);
    writelong(fp, hdr.window_y);
    writelong(fp, hdr.window_bdrwidth);

    /* 
    **	Write out the dump name.
     */
    fwrite(name, 1, strlen(name) + 1, fp);

    /* 
    **	 Write out the colormap, big-endian order.
     */
    color.flags = 7;
    color.pad = 0;
    for (i = 0; i < image->cmapSize; ++i) {
	color.pixel = i;

	if (isBW) {
	    color.red = i * 65535L;
	    color.green = color.red;
	    color.blue = color.red;
	} else if (isGrey) {
	    color.red = i << 8;
	    color.green = color.red;
	    color.blue = color.red;
	} else {
	    color.red = image->cmapData[i * 3 + 0] << 8;
	    color.green = image->cmapData[i * 3 + 1] << 8;
	    color.blue = image->cmapData[i * 3 + 2] << 8;
	}
	writelong(fp, color.pixel);
	writeshort(fp, color.red);
	writeshort(fp, color.green);
	writeshort(fp, color.blue);
	(void) putc(color.flags, fp);
	(void) putc(color.pad, fp);
    }

    /* 
    ** Finally, write out the data.
     */
    for (y = 0; y < image->height; y++) {
	if (image->cmapSize == 0) {
	    /*
	    **	Write RGB Image
	     */
	    unsigned char *dp = &image->data[y * image->width * 3];
	    for (x = 0; x < image->width; x++) {
		putc(0, fp);
		putc(*dp, fp);
		dp++;
		putc(*dp, fp);
		dp++;
		putc(*dp, fp);
		dp++;
	    }
	} else if (isBW) {
	    /*
	    **	Write a B&W Image, which is 1 bit per pixel.
	     */
	    unsigned char *dp = &image->data[y * image->width];
	    int v = 0, idx = 7;
	    for (x = 0; x < image->width; x++, dp++) {
		v |= *dp << idx;
		if (--idx < 0) {
		    idx = 7;
		    putc(v & 0xff, fp);
		    v = 0;
		}
	    }
	    if (idx != 7)
		putc(v & 0xff, fp);
	} else if (image->cmapSize > 256) {
	    /*
	    **	Write wide colormapped image
	     */
	    unsigned short *dp = &((unsigned short *) image->data)[y * image->width];
	    for (x = 0; x < image->width; x++, dp++)
		writeshort(fp, *dp);
	} else {
	    /*
	    **	Write normal colormapped image, which is also a Grey image
	     */
	    unsigned char *dp = &image->data[y * image->width];
	    for (x = 0; x < image->width; x++, dp++)
		putc(*dp, fp);
	}
    }

    fclose(fp);

    return False;
}

int TestXWD(char *file)
{
    FILE *fd = fopen(file, "r");
    long v;

    if (fd == NULL)
	return False;

    readlong(fd);
    v = readlong(fd);

    fclose(fd);

    return v == XWD_FILE_VERSION;
}

Image *
 ReadXWD(char *file)
{
    int isGrey = True, isBW = False;
    FILE *fp;
    XWDFileHeader hdr;
    Image *image = NULL;
    unsigned char *ibp;
    unsigned short *isp;
    int i, x, y;
    unsigned long red_mask, green_mask, blue_mask;
    int byte_swap = False;
    int scale, padright;

    if ((fp = fopen(file, "r")) == NULL)
	return NULL;

    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
	fclose(fp);
	return NULL;
    }
    if (swaplong(hdr.file_version) == XWD_FILE_VERSION) {
	byte_swap = True;
	hdr.header_size = swaplong(hdr.header_size);
	hdr.file_version = swaplong(hdr.file_version);
	hdr.pixmap_format = swaplong(hdr.pixmap_format);
	hdr.pixmap_depth = swaplong(hdr.pixmap_depth);
	hdr.pixmap_width = swaplong(hdr.pixmap_width);
	hdr.pixmap_height = swaplong(hdr.pixmap_height);
	hdr.xoffset = swaplong(hdr.xoffset);
	hdr.byte_order = swaplong(hdr.byte_order);
	hdr.bitmap_unit = swaplong(hdr.bitmap_unit);
	hdr.bitmap_bit_order = swaplong(hdr.bitmap_bit_order);
	hdr.bitmap_pad = swaplong(hdr.bitmap_pad);
	hdr.bits_per_pixel = swaplong(hdr.bits_per_pixel);
	hdr.bytes_per_line = swaplong(hdr.bytes_per_line);
	hdr.visual_class = swaplong(hdr.visual_class);
	hdr.red_mask = swaplong(hdr.red_mask);
	hdr.green_mask = swaplong(hdr.green_mask);
	hdr.blue_mask = swaplong(hdr.blue_mask);
	hdr.bits_per_rgb = swaplong(hdr.bits_per_rgb);
	hdr.colormap_entries = swaplong(hdr.colormap_entries);
	hdr.ncolors = swaplong(hdr.ncolors);
	hdr.window_width = swaplong(hdr.window_width);
	hdr.window_height = swaplong(hdr.window_height);
	hdr.window_x = swaplong(hdr.window_x);
	hdr.window_y = swaplong(hdr.window_y);
	hdr.window_bdrwidth = swaplong(hdr.window_bdrwidth);
    } else if (hdr.file_version != XWD_FILE_VERSION) {
	goto out;
    }
    /*
    **	Skip stored name
     */
    for (i = 0; i < hdr.header_size - sizeof(hdr); ++i)
	if (getc(fp) == EOF)
	    goto out;

    /* Check whether we can handle this dump. */
    if (hdr.pixmap_depth > 24)
	goto out;
    if (hdr.bits_per_rgb > 24)
	goto out;
    if (hdr.pixmap_format != ZPixmap && hdr.pixmap_depth != 1)
	goto out;
    if (hdr.bitmap_unit != 8 && hdr.bitmap_unit != 16 && hdr.bitmap_unit != 32)
	goto out;

    if (hdr.ncolors > 0) {
	XWDColor *colors;

	if ((colors = (XWDColor *) calloc(sizeof(XWDColor), hdr.ncolors)) == NULL)
	    goto out;
	if (fread(colors, sizeof(XWDColor), hdr.ncolors, fp) != hdr.ncolors)
	    goto out;

	image = ImageNewCmap(hdr.pixmap_width, hdr.pixmap_height, hdr.ncolors);

	for (i = 0; i < hdr.ncolors; i++) {
	    unsigned char r, g, b;
	    if (byte_swap) {
		colors[i].red = swapshort(colors[i].red);
		colors[i].green = swapshort(colors[i].green);
		colors[i].blue = swapshort(colors[i].blue);
	    }
	    r = colors[i].red >> 8;
	    g = colors[i].green >> 8;
	    b = colors[i].blue >> 8;
	    ImageSetCmap(image, i, r, g, b);

	    if (r != g || g != b)
		isGrey = False;
	}

	if (isGrey && image->cmapData[0] == 0 && image->cmapData[3] == 255)
	    isBW = True;
    } else {
	/*
	**  RGB Image
	 */
	isGrey = False;
    }

    if (hdr.visual_class == TrueColor || hdr.visual_class == DirectColor) {
    } else if (hdr.visual_class == StaticGray && hdr.bits_per_pixel == 1) {
	/*
	**  B&W bitmap
	 */
    } else if (hdr.visual_class == StaticGray) {
    }
    switch (hdr.visual_class) {
    case TrueColor:
    case DirectColor:
	image = ImageNew(hdr.pixmap_width, hdr.pixmap_height);
	if (hdr.bits_per_pixel == 16)
	    scale = 4;
	else
	    scale = 1;
	break;
    case StaticGray:
	if (image != NULL)
	    break;
	if (hdr.bits_per_pixel == 1) {
	    image = ImageNewBW(hdr.pixmap_width, hdr.pixmap_height);
	} else {
	    image = ImageNewGrey(hdr.pixmap_width, hdr.pixmap_height);
	    scale = 256 / (1 << hdr.bits_per_pixel) - 1;
	}
	break;
    default:			/* StaticColor, PseudoColor, GrayScale */
	if (image != NULL)
	    break;
	image = ImageNew(hdr.pixmap_width, hdr.pixmap_height);
	break;
    }

    padright = hdr.bytes_per_line * 8 / hdr.bits_per_pixel - hdr.pixmap_width;

    bits_per_item = hdr.bitmap_unit;
    bits_used = bits_per_item;
    bits_per_pixel = hdr.bits_per_pixel;
    byte_order = hdr.byte_order;
    bit_order = hdr.bitmap_bit_order;
    if (bits_per_pixel == sizeof(pixel_mask) * 8)
	pixel_mask = -1;
    else
	pixel_mask = (1 << bits_per_pixel) - 1;
    red_mask = hdr.red_mask;
    green_mask = hdr.green_mask;
    blue_mask = hdr.blue_mask;

    ibp = image->data;
    isp = (unsigned short *) image->data;
    for (y = 0; y < image->height; y++) {
	switch (hdr.visual_class) {
	case StaticGray:
	case GrayScale:
	case StaticColor:
	case PseudoColor:
	    for (x = 0; x < image->width; x++) {
		if (image->cmapSize > 256)
		    *isp++ = getpixnum(fp);
		else if (image->isBW == 2)
		    *ibp++ = (getpixnum(fp) == 1) ? 0 : 1;
		else
		    *ibp++ = getpixnum(fp);
	    }
	    break;
	case TrueColor:
	case DirectColor:
	    for (x = 0; x < image->width; x++) {
		unsigned long ul = getpixnum(fp);
		switch (hdr.bits_per_pixel) {
		case 16:
		    *ibp++ = (ul & red_mask) >> 0;
		    *ibp++ = (ul & green_mask) >> 5;
		    *ibp++ = (ul & blue_mask) >> 10;
		    break;
		case 24:
		case 32:
		    *ibp++ = (ul & 0xff0000) >> 16;
		    *ibp++ = (ul & 0x00ff00) >> 8;
		    *ibp++ = (ul & 0x0000ff);
		    break;
		}
	    }
	}

	for (x = 0; x < padright; x++)
	    getpixnum(fp);
    }

  out:
    fclose(fp);
    return image;
}
