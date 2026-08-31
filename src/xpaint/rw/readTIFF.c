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

/* $Id: readTIFF.c,v 1.21 2005/03/20 20:15:34 demailly Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include "tiffio.h"
#include "image.h"
#include "rwTable.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

extern void *xmalloc(size_t n);

static void nop(void)
{
}

int TestTIFF(char *file)
{
    TIFF *tif;

    TIFFSetErrorHandler((TIFFErrorHandler) nop);
    if ((tif = TIFFOpen(file, "r")) == NULL)
	return 0;
    TIFFClose(tif);
    return 1;
}

static int forceRGB;
static unsigned short planarconfig;
static unsigned short *redmap, *greenmap, *bluemap;

static int getImage(TIFF * tif, Image * image, void *dp,
		    unsigned short bitsPS,
		    unsigned short samplesPP,
		    unsigned short photometric)
{
    unsigned short orientation;
    int row, rowStart, colStart, rowInc, colInc;
    int isWide;
    unsigned char *ucp;
    unsigned short *usp;
    unsigned char *sline = NULL, *sptr0 = NULL, *sptr1 = NULL, *sptr2 = NULL;
    int x, y;
    int maxval = (1 << bitsPS) - 1;
    int scansize = 0;

    if (planarconfig == PLANARCONFIG_SEPARATE) {
	scansize = TIFFStripSize(tif);

	sline = (unsigned char *) xmalloc(3 * scansize);
	sptr0 = sline;
	sptr1 = sptr0 + scansize;
	sptr2 = sptr1 + scansize;
    } else {
	if ((sline = (unsigned char *) xmalloc(TIFFScanlineSize(tif))) == NULL) {
	    RWSetMsg("Unable to allocate memory for scanline");
	    return TRUE;
	}
    }

    isWide = image->cmapSize > 256;

    usp = (unsigned short *) dp;
    ucp = (unsigned char *) dp;

    if (!TIFFGetField(tif, TIFFTAG_ORIENTATION, &orientation))
	orientation = ORIENTATION_TOPLEFT;

    switch (orientation) {
    default:
    case ORIENTATION_LEFTTOP:
    case ORIENTATION_TOPLEFT:
	colStart = 0;
	colInc = 1;
	rowStart = 0;
	rowInc = 1;
	break;
    case ORIENTATION_RIGHTTOP:
    case ORIENTATION_TOPRIGHT:
	colStart = image->width;
	colInc = -1;
	rowStart = 0;
	rowInc = 1;
	break;
    case ORIENTATION_RIGHTBOT:
    case ORIENTATION_BOTRIGHT:
	colStart = image->width;
	colInc = -1;
	rowStart = image->height;
	rowInc = -1;
	break;
    case ORIENTATION_LEFTBOT:
    case ORIENTATION_BOTLEFT:
	colStart = 0;
	colInc = 1;
	rowStart = image->height;
	rowInc = -1;
	break;
    }

    for (row = rowStart, y = 0; y < image->height; y++, row += rowInc) {
	unsigned char *pp = sline;
	int bitsleft, sample0, sample1, sample2;

#define NEXT3(p0, p1, p2) {			\
	if (bitsleft == 0) {			\
	  p0++;					\
	  p1++;					\
	  p2++;					\
	  bitsleft = 8;				\
	}					\
	bitsleft -= bitsPS;			\
	sample0 = (*p0 >> bitsleft) & maxval;	\
	sample1 = (*p1 >> bitsleft) & maxval;	\
	sample2 = (*p2 >> bitsleft) & maxval;	\
}

#define NEXT(pp) do {				\
	if (bitsleft == 0) {			\
	  pp++;					\
	  bitsleft = 8;				\
	}					\
	bitsleft -= bitsPS;			\
	sample0 = (*pp >> bitsleft) & maxval;	\
} while (0)

	if (planarconfig == PLANARCONFIG_SEPARATE) {
	    if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 0),
				     sptr0, scansize) < 0)
		break;
	    if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 1),
				     sptr1, scansize) < 0)
		break;
	    if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 2),
				     sptr2, scansize) < 0)
		break;
	} else if (TIFFReadScanline(tif, sline, row, 0) < 0)
	    break;

	/*
	**  If colInc == -1, then just reorder the samples.  
	 */

	bitsleft = 8;

	switch (photometric) {
	case PHOTOMETRIC_PALETTE:
	    if (forceRGB) {
		for (x = 0; x < image->width; x++) {
		    NEXT(pp);
		    *ucp++ = redmap[sample0];
		    *ucp++ = greenmap[sample0];
		    *ucp++ = bluemap[sample0];
		}
		break;
	    }
	    /*	fall through	    */
	case PHOTOMETRIC_MINISWHITE:
	case PHOTOMETRIC_MINISBLACK:
	    for (x = 0; x < image->width; x++) {
		NEXT(pp);
		if (isWide)
		    *usp++ = sample0;
		else
		    *ucp++ = sample0;
	    }
	    break;
	case PHOTOMETRIC_RGB:
	    if (planarconfig == PLANARCONFIG_SEPARATE) {
		unsigned char *p0 = sptr0, *p1 = sptr1, *p2 = sptr2;
		for (x = 0; x < image->width; x++) {
		    NEXT3(p0, p1, p2);
		    *ucp++ = sample0;
		    *ucp++ = sample1;
		    *ucp++ = sample2;
		}
	    } else {
		for (x = 0; x < image->width; x++) {
		    NEXT(pp);
		    *ucp++ = sample0;
		    NEXT(pp);
		    *ucp++ = sample0;
		    NEXT(pp);
		    *ucp++ = sample0;
		    if (samplesPP == 4)
			NEXT(pp);
		}
	    }
	    break;
	case PHOTOMETRIC_MASK:
	    for (x = 0; x < image->width; x++) {
		NEXT(pp);
		*ucp++ = sample0 ? 1 : 0;
	    }
	    break;
	}
    }

    if (sline != NULL)
	free(sline);

    return FALSE;
}

Image *ReadTIFF(char *file)
{
    TIFF *tif;
    int maxval, ncolors;
    Image *image = NULL;
    unsigned short bitsPS, samplesPP, photometric;
    int width, height;
    int i;

    forceRGB = FALSE;

    TIFFSetErrorHandler((TIFFErrorHandler) nop);
    if ((tif = TIFFOpen(file, "r")) == NULL) {
	RWSetMsg("Error opening input file");
	return NULL;
    }
    if (!TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPS))
	bitsPS = 1;
    if (!TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPP))
	samplesPP = 1;
    if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric)) {
	RWSetMsg("error getting photometric");
	goto out;
    }
    if (TIFFIsTiled(tif)) {
	RWSetMsg("Unable to handle TIFF tiled images");
	goto out;
    }
    if (samplesPP != 1 && samplesPP != 3 && samplesPP != 4) {
	RWSetMsg("Can only handle 1-channel gray scale or 1- or 3-channel color");
	goto out;
    }
    if (photometric == PHOTOMETRIC_YCBCR || photometric == PHOTOMETRIC_CIELAB ||
	photometric == PHOTOMETRIC_SEPARATED || photometric == PHOTOMETRIC_MASK) {
	RWSetMsg("Can only handle RGB, grey, or colormaped images");
	goto out;
    }
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planarconfig);

    if (planarconfig == PLANARCONFIG_SEPARATE && photometric != PHOTOMETRIC_RGB) {
	RWSetMsg("Can only handle planarconfig=SEPARATE on photometric=RGB images");
	goto out;
    }
    ncolors = 1 << bitsPS;
    maxval = ncolors - 1;
    if (maxval == 1 && samplesPP == 1) {
	image = ImageNewCmap(width, height, ncolors);
	if (photometric == PHOTOMETRIC_MINISBLACK) {
	    ImageSetCmap(image, 0, 0, 0, 0);
	    ImageSetCmap(image, 1, 255, 255, 255);
	} else {
	    ImageSetCmap(image, 1, 0, 0, 0);
	    ImageSetCmap(image, 0, 255, 255, 255);
	}
    } else
	switch (photometric) {
	case PHOTOMETRIC_MINISBLACK:
	    image = ImageNewCmap(width, height, ncolors);
	    for (i = 0; i < ncolors; i++) {
		int v = (i * 255) / maxval;
		ImageSetCmap(image, i, v, v, v);
	    }
	    break;
	case PHOTOMETRIC_MINISWHITE:
	    image = ImageNewCmap(width, height, ncolors);
	    for (i = 0; i < ncolors; i++) {
		int v = ((maxval - i) * 255) / maxval;
		ImageSetCmap(image, i, v, v, v);
	    }
	    break;

	case PHOTOMETRIC_PALETTE:
	    if (!TIFFGetField(tif, TIFFTAG_COLORMAP, &redmap, &greenmap, &bluemap)) {
		RWSetMsg("error getting colormaps");
		goto out;
	    }
	    if (ncolors > 0xffff) {
		forceRGB = TRUE;
		image = ImageNew(width, height);
		break;
	    } else {
		image = ImageNewCmap(width, height, ncolors);
	    }
	    for (i = 0; i < ncolors; i++) {
		ImageSetCmap(image, i, redmap[i] / 256, greenmap[i] / 256, bluemap[i] / 256);
	    }
	    break;

	case PHOTOMETRIC_RGB:
	    image = ImageNew(width, height);
	    break;
	}

    if (getImage(tif, image, (void *) image->data, bitsPS, samplesPP, photometric)) {
	ImageDelete(image);
	goto out;
    }
    /*
    **	Now see if there is a mask image in this TIFF file.
     */

    if (TIFFSetDirectory(tif, 1)) {
	if (!TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPS))
	    bitsPS = 1;
	if (!TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPP))
	    samplesPP = 1;
	if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric))
	    goto out;

	/*
	**  This has to be the MASK for the preceding image.
	 */
	if (photometric != PHOTOMETRIC_MASK)
	    goto out;

	ImageMakeMask(image);
	if (getImage(tif, image, (void *) image->alpha,
		     bitsPS, samplesPP, photometric)) {
	    free(image->alpha);
	    image->alpha = NULL;
	}
    }
  out:
    TIFFClose(tif);

    return image;
}
