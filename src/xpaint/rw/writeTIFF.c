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

/* $Id: writeTIFF.c,v 1.21 2005/03/20 20:15:34 demailly Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <tiffio.h>

#include <X11/Intrinsic.h>
#include "xpaint.h"
#include "image.h"

#undef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#define	streq(a,b)	(strcmp(a,b) == 0)

extern void *xmalloc(size_t n);

int WriteTIFF(char *file, Image * image)
{
    TIFF *out;
    int samplesPP, bitsPP;
    int x, y;
    unsigned char *tbuf = NULL;
    unsigned char *buf = NULL, *bp;
    unsigned char *ucp = (unsigned char *) image->data;
    unsigned short photometric;

    if (image->isGrey) {
	photometric = PHOTOMETRIC_MINISBLACK;
	samplesPP = 1;
	bitsPP = image->isBW ? 1 : 8;
	if (bitsPP == 1)
	    buf = (unsigned char *) xmalloc(sizeof(char) * image->width / 8 + 4);
    } else {
	Image *cmapImage = NULL;   /* GRR 960525 */
        int depth = 
              DefaultDepth(Global.display, DefaultScreen(Global.display));

        if (depth <= 8) {
  	      /* try compressing image to a colormap, but don't force it */
	  if (!image->alpha)   
              /* can TIFF store alpha channel with palette? */
	      cmapImage = ImageCompress(image, 256, 1);
	  if (cmapImage)
	      image = cmapImage;  /* original was deleted in ImageCompress() */
	}

	if (depth > 8 || image->cmapSize == 0 || image->cmapSize > 256) {
	    photometric = PHOTOMETRIC_RGB;
	    samplesPP = 3;
	    bitsPP = 8;
	} else {
	    photometric = PHOTOMETRIC_PALETTE;
	    samplesPP = 1;
	    bitsPP = 8;
	}

    }


    if ((out = TIFFOpen(file, "w")) == NULL)
	return 1;

    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, (unsigned long) image->width);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, (unsigned long) image->height);
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, samplesPP);
    TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, bitsPP);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, photometric);
    TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, 1);
    TIFFSetField(out, TIFFTAG_SOFTWARE, "XPaint 2.1");

    if (photometric == PHOTOMETRIC_PALETTE) {
	unsigned short *r, *g, *b;
	int i;

	r = (unsigned short *) xmalloc(sizeof(unsigned short) * 3 * image->cmapSize);
	g = r + image->cmapSize;
	b = g + image->cmapSize;
	for (i = 0; i < image->cmapSize; i++) {
	    r[i] = image->cmapData[i * 3 + 0] * 256;
	    g[i] = image->cmapData[i * 3 + 1] * 256;
	    b[i] = image->cmapData[i * 3 + 2] * 256;
	}
	TIFFSetField(out, TIFFTAG_COLORMAP, r, g, b);

	free(r);
    }
    /*
    **	Here is where we are lucky that TIFF and 
    **	  the Image format are the same.
    **	[ except for large colormap images ]
     */
    if (image->cmapSize > 256)
	tbuf = (unsigned char *) xmalloc(image->width * sizeof(char) * 3);
    for (y = 0; y < image->height; y++) {
	if (bitsPP == 8) {
	    if (image->cmapSize <= 256) {
		TIFFWriteScanline(out, ucp, y, 0);
		ucp += image->width * samplesPP;
	    } else {
		unsigned char *p, *tp = tbuf;

		for (x = 0; x < image->width; x++) {
		    p = ImagePixel(image, x, y);
		    *tp++ = *p++;
		    *tp++ = *p++;
		    *tp++ = *p++;
		}
		TIFFWriteScanline(out, tbuf, y, 0);
	    }
	} else {
	    int shift = 7;

	    bp = buf;
	    *bp = 0;
	    for (x = 0; x < image->width; x++, ucp++) {
		*bp |= (*ucp > 128 ? 1 : 0) << shift--;
		if (shift < 0) {
		    shift = 7;
		    bp++;
		    *bp = 0;
		}
	    }
	    TIFFWriteScanline(out, buf, y, 0);
	}
    }

    if (image->alpha != NULL) {
	TIFFWriteDirectory(out);

	TIFFSetDirectory(out, 1);

	samplesPP = 1;
	bitsPP = 1;
	TIFFSetField(out, TIFFTAG_IMAGEWIDTH, (unsigned long) image->width);
	TIFFSetField(out, TIFFTAG_IMAGELENGTH, (unsigned long) image->height);
	TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, samplesPP);
	TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, bitsPP);
	TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MASK);
	TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_PACKBITS);
	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, 1);
	TIFFSetField(out, TIFFTAG_SOFTWARE, "XPaint");

	ucp = image->alpha;

	if (buf == NULL)
	    buf = (unsigned char *) xmalloc(sizeof(char) * image->width / 8 + 4);
	for (y = 0; y < image->height; y++) {
	    int shift = 7;

	    bp = buf;
	    *bp = 0;
	    for (x = 0; x < image->width; x++, ucp++) {
		*bp |= (*ucp ? 1 : 0) << shift--;
		if (shift < 0) {
		    shift = 7;
		    bp++;
		    *bp = 0;
		}
	    }
	    TIFFWriteScanline(out, buf, y, 0);
	}
    }
    (void) TIFFClose(out);

    if (buf != NULL)
	free(buf);
    if (tbuf != NULL)
	free(tbuf);

    return 0;
}
