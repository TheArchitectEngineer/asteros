/* +-------------------------------------------------------------------+ */
/* | Copyright (C) 1993, David Koblas (koblas@netcom.com)	       | */
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

/* $Id: imageComp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

/* reduce.c:

 * reduce an image's colormap usage to a set number of colors.	this also
 * translates a true color image to a TLA-style image of `n' colors.
 *
 * this uses an algorithm by Paul Heckbert discussed in `Color Image
 * Quantization for Frame Buffer Display,' _Computer Graphics_ 16(3),
 * pp 297-307.	this implementation is based on one discussed in
 * 'A Few Good Colors,' _Computer Language_, Aug. 1990, pp 32-41 by
 * Dave Pomerantz.
 *
 * this function cannot reduce to any number of colors larger than 32768.
 *
 * jim frost 04.18.91
 *
 */

/*
 * Copyright 1989, 1990, 1991 Jim Frost
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.	 The author makes no representations
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <X11/Intrinsic.h>
#include <stdio.h>

#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#endif

/*
**  swiped from X11/Xfuncproto.h
**   since qsort() may or may not be defined with a constant sub-function
 */
#ifndef _Xconst
#if __STDC__ || defined(__cplusplus) || defined(c_plusplus) || (FUNCPROTO&4)
#define _Xconst const
#else
#define _Xconst
#endif
#endif				/* _Xconst */

#include "image.h"
#include "hash.h"
#include "protocol.h"
#include "misc.h"

/*
** This converts an RGB pixel into a 15-bit true color value.
 */

#define TO_15BITS(r,g,b) ((r & 0xf8) << 7)|((g & 0xf8) << 2)|((b & 0xf8) >> 3)

/* 
** These macros extract color intensities, in the compressed range.
 */

#define RED_INTENSITY(P)   (((P) & 0x7c00) >> 10)
#define GREEN_INTENSITY(P) (((P) & 0x03e0) >> 5)
#define BLUE_INTENSITY(P)   ((P) & 0x001f)

/*
** These macros extract the 0..255 range value from the "true color" version
 */
#define RED_VALUE(P)   (((P) & 0x7c00) >> 7)
#define GREEN_VALUE(P) (((P) & 0x03e0) >> 2)
#define BLUE_VALUE(P)  (((P) & 0x001f) << 3)

/*
 * This structure defines a color area which is made up of an array of pixel
 * values and a count of the total number of image pixels represented by
 * the area.  Color areas are kept in a list sorted by the number of image
 * pixels they represent.
 */

typedef struct color_area {
    unsigned short *pixels;	/* array of pixel values in this area */
    unsigned short npixels;	/* size of above array */
    /* predicate func to sort with before * splitting */
    int (*func) (unsigned short *, unsigned short *);
    unsigned long count;	/* # of image pixels we represent */
    struct color_area *prev, *next;
} Area_t;

typedef struct {
    unsigned short idx;
    unsigned char r, g, b;
} color_t;

/* 
** Predicate functions for qsort
**   Generated code as you can see.
 */

#ifndef __STDC__
#define SORT(ca, cb, cc)							\
static int sort/**/ca/**/cb/**/cc(unsigned short *p1, unsigned short *p2)	\
{										\
	unsigned int	R1,R2,G1,G2,B1,B2;					\
	R1 = RED_INTENSITY(*p1);						\
	G1 = GREEN_INTENSITY(*p1);						\
	B1 = BLUE_INTENSITY(*p1);						\
	R2 = RED_INTENSITY(*p2);						\
	G2 = GREEN_INTENSITY(*p2);						\
	B2 = BLUE_INTENSITY(*p2);						\
	if (ca/**/1 == ca/**/2)							\
		if (cb/**/1 == cb/**/2)						\
			return (cc/**/1 < cc/**/2) ? -1 : 1;			\
		else								\
			return (cb/**/1 < cb/**/2) ? -1 : 1;			\
	else									\
		return (ca/**/1 < ca/**/2) ? -1 : 1;				\
	}
#else
#define SORT(ca, cb, cc)							\
    static int sort ## ca ## cb ## cc(unsigned short *p1, unsigned short *p2)	\
	{									\
		unsigned int	R1,R2,G1,G2,B1,B2;				\
		R1 = RED_INTENSITY(*p1);					\
		G1 = GREEN_INTENSITY(*p1);					\
		B1 = BLUE_INTENSITY(*p1);					\
		R2 = RED_INTENSITY(*p2);					\
		G2 = GREEN_INTENSITY(*p2);					\
		B2 = BLUE_INTENSITY(*p2);					\
		if (ca ## 1 == ca ## 2)						\
			if (cb ## 1 == cb ## 2)					\
				return (cc ## 1 < cc ## 2) ? -1 : 1;		\
			else							\
				return (cb ## 1 < cb ## 2) ? -1 : 1;		\
		else								\
			return (ca ## 1 < ca ## 2) ? -1 : 1;			\
	}
#endif

SORT(R, G, B)
SORT(R, B, G)
SORT(G, R, B)
SORT(G, B, R)
SORT(B, G, R)
SORT(B, R, G)
#undef SORT

Image *outputImage;

/*
 * this does calculations on a color area following a split and inserts
 * the color area in the list of color areas.
 */

static void 
insertColorArea(unsigned long *counts,
		Area_t ** rlargest, Area_t ** rsmallest, Area_t * area)
{
    int i;
    unsigned int red, green, blue;
    unsigned int min_red, min_green, min_blue;
    unsigned int max_red, max_green, max_blue = 0;
    Area_t *largest, *smallest;

    /*
    ** update pixel count for this area and find RGB intensity widths
     */
    min_red = max_red = RED_INTENSITY(area->pixels[0]);
    min_green = max_green = GREEN_INTENSITY(area->pixels[0]);
    min_blue = max_blue = BLUE_INTENSITY(area->pixels[0]);

    area->count = 0;
    for (i = 1; i < area->npixels; i++) {
	area->count += counts[area->pixels[i]];
	red = RED_INTENSITY(area->pixels[i]);
	green = GREEN_INTENSITY(area->pixels[i]);
	blue = BLUE_INTENSITY(area->pixels[i]);

	if (red < min_red)
	    min_red = red;
	if (red > max_red)
	    max_red = red;
	if (green < min_green)
	    min_green = green;
	if (green > max_green)
	    max_green = green;
	if (blue < min_blue)
	    min_blue = blue;
	if (blue > max_blue)
	    max_blue = blue;
    }

    /*
    ** calculate widths and determine which predicate function to use based
    ** on the result
     */

    red = max_red - min_red;
    green = max_green - min_green;
    blue = max_blue - min_blue;

    if (red > green)
	if (green > blue)
	    area->func = sortRGB;
	else if (red > blue)
	    area->func = sortRBG;
	else
	    area->func = sortBRG;
    else if (green > blue)
	if (red > blue)
	    area->func = sortGRB;
	else
	    area->func = sortGBR;
    else
	area->func = sortBGR;

    /*
     * insert color area in color area list sorted by number of pixels that
     * the area represents
     */

    largest = *rlargest;
    smallest = *rsmallest;

    if (largest == NULL) {
	largest = smallest = area;
	area->prev = area->next = (Area_t *) NULL;
    } else if (area->npixels < 2) {
	/*
	**  if we only have one element, our pixel count is immaterial so we get
	**  stuck on the end of the list.
	 */
	smallest->next = area;
	area->prev = smallest;
	area->next = (Area_t *) NULL;
	smallest = area;
    } else {
	/*
	**  insert node into list
	 */
	Area_t *cur;

	for (cur = largest; cur != NULL; cur = cur->next) {
	    if ((area->count > cur->count) || (cur->npixels < 2)) {
		area->prev = cur->prev;
		area->next = cur;
		cur->prev = area;
		if (area->prev != NULL)
		    area->prev->next = area;
		else
		    largest = area;
		break;
	    }
	}
	if (cur == NULL) {
	    area->prev = smallest;
	    area->next = (Area_t *) NULL;
	    smallest->next = area;
	    smallest = area;
	}
    }
    *rlargest = largest;
    *rsmallest = smallest;
}

/*
**  hash table support functions.
 */
static int 
cmpColor(void *a, void *b)
{
    color_t *ca = (color_t *) a;
    color_t *cb = (color_t *) b;

    if (ca->r != cb->r)
	return ca->r < cb->r ? -1 : 1;
    if (ca->g != cb->g)
	return ca->g < cb->g ? -1 : 1;
    if (ca->b != cb->b)
	return ca->b < cb->b ? -1 : 1;
    return 0;
}

static void 
freeColor(void *junk)
{
    /*
    **	Don't apply a free function to the hashtable items.
     */
}

Image *
ImageCompress(Image * input, int ncolors, int noforce)
{
    unsigned long counts[32768];
    unsigned short array[XtNumber(counts)];
    unsigned char *ocp;
    unsigned short *osp;
    int x, y, i, count, nuniq;
    int allocated;
    Area_t *areas, *largest, *smallest;
    Image *output;
    void *htable;
    color_t *ctable;

    /*
    **	make sure we have array space...
     */
    if (ncolors > XtNumber(counts))
	ncolors = XtNumber(counts);

    /*
    **	Why are we trying to compress a b&w image?
    **	  or an image that already fits
     */
    if (input->isBW)
	return input;
    if (input->cmapSize != 0 && input->cmapSize < ncolors)
	return input;

    /*
    **	Create a histogram of 15-bit color values.
    **	  also, save the "real" color values.
    **	  or at least enough of them so that we know
    **	  that compression is really necessary.
     */
    htable = HashCreate(cmpColor, freeColor, 256);
    ctable = (color_t *) XtCalloc(sizeof(color_t), ncolors + 1);
    nuniq = 0;

    /* GRR:  use memset() instead? */
    for (i = 0; i < XtNumber(counts); i++)
	counts[i] = 0;

    for (y = 0; y < input->height; y++) {
	for (x = 0; x < input->width; x++) {
	    unsigned char *dp;

	    dp = ImagePixel(input, x, y);

	    if (nuniq <= ncolors && htable != NULL) {
		color_t *cptr = &ctable[nuniq];
		cptr->r = dp[0];
		cptr->g = dp[1];
		cptr->b = dp[2];
		if (HashFind(htable, dp[0], cptr) == NULL) {
		    cptr->idx = nuniq;
		    HashAdd(htable, dp[0], cptr);
		    nuniq++;
		}
	    }
	    counts[TO_15BITS(dp[0], dp[1], dp[2])]++;
	}

	if (y % 256 == 0)
	    StateTimeStep();
    }

    if (nuniq <= ncolors) {
	/*
	**  Wow, this has few enough colors to fit in the requested colormap.
	**    This should be able to renumber (compress) an existing
	**    colormap, rather than creating a new image.
	 */
	unsigned short *osp;
	unsigned char *ocp;

	output = ImageNewCmap(input->width, input->height, nuniq);
	for (y = 0; y < nuniq; y++)
	    ImageSetCmap(output, y,
			 ctable[y].r, ctable[y].g, ctable[y].b);

	osp = (unsigned short *) output->data;
	ocp = output->data;

	for (y = 0; y < input->height; y++) {
	    for (x = 0; x < input->width; x++, osp++, ocp++) {
		unsigned char *dp;
		color_t *cptr, col;

		dp = ImagePixel(input, x, y);

		col.r = dp[0];
		col.g = dp[1];
		col.b = dp[2];
		cptr = HashFind(htable, dp[0], &col);

		if (nuniq > 256)
		    *osp = cptr->idx;
		else
		    *ocp = cptr->idx;
	    }

	    if (y % 256 == 0)
		StateTimeStep();
	}

	output->cmapPacked = True;

	HashDestroy(htable);
	XtFree((XtPointer) ctable);
	output->alpha = input->alpha;
	input->alpha = NULL;
	ImageDelete(input);
	outputImage = output;
	return output;
    }
    HashDestroy(htable);
    XtFree((XtPointer) ctable);

    /* GRR 960525:  if not forcing compression to ncolors, return now */
    if (noforce)
        return NULL;

    /*
    **	Create an array of 15-bit pixel values that actually occur 
    **	 in the image.
     */
    count = 0;
    for (i = 0; i < XtNumber(counts); i++) {
	if (counts[i] != 0)
	    array[count++] = i;
    }

    /*
    **	Create the color areas and initialize the first element
     */
    areas = (Area_t *) XtCalloc(sizeof(Area_t), ncolors);
    areas[0].pixels = array;
    areas[0].npixels = count;

    largest = smallest = NULL;
    insertColorArea(counts, &largest, &smallest, areas);

    allocated = 1;

    /*
    **	While there are areas still to be broken down 
    **	  or the largest cannot be subdivided.
     */
    while (allocated < ncolors && largest->npixels >= 2) {
	Area_t *newArea, *oldArea;
	int i, midpoint;

	qsort(largest->pixels, largest->npixels, sizeof(short),
	       (int (*)(_Xconst void *, _Xconst void *)) largest->func);

	/*
	**  Find the midpoint of the largest area an split
	 */
	midpoint = largest->count / 2;
	for (i = 0, count = 0; i < largest->npixels && count < midpoint; i++)
	    count += counts[largest->pixels[i]];

	if (i == largest->npixels)
	    i--;
	if (i == 0)
	    i = 1;
	newArea = areas + allocated;
	newArea->pixels = largest->pixels + i;
	newArea->npixels = largest->npixels - i;
	largest->npixels = i;
	oldArea = largest;
	largest = largest->next;
	if (largest != NULL)
	    largest->prev = NULL;
	else
	    smallest = NULL;

	/* 
	**  recalculate for each area of split and insert in the area list
	 */
	insertColorArea(counts, &largest, &smallest, newArea);
	insertColorArea(counts, &largest, &smallest, oldArea);

	allocated++;

	if (allocated % 64 == 0)
	    StateTimeStep();
    }

    output = ImageNewCmap(input->width, input->height, allocated);
    output->alpha = input->alpha;
    input->alpha = NULL;

    for (i = 0; i < allocated; i++) {
	long red, green, blue;
	int j;

	/* 
	** Calculate RGB table from each color area.  This should really calculate
	** a new color by weighting the intensities by the number of pixels, but
	** it's a pain to scale so this just averages all the intensities.  It
	** works pretty well regardless.
	 */
	red = green = blue = 0;
	for (j = 0; j < areas[i].npixels; j++) {
	    unsigned short pixel = areas[i].pixels[j];

	    red += RED_VALUE(pixel) & 0xff;
	    green += GREEN_VALUE(pixel) & 0xff;
	    blue += BLUE_VALUE(pixel) & 0xff;

	    counts[pixel] = i;
	}
	red /= areas[i].npixels;
	green /= areas[i].npixels;
	blue /= areas[i].npixels;

	ImageSetCmap(output, i, red, green, blue);
    }

    /*
    **	Done with the areas information.
     */
    XtFree((XtPointer) areas);

    /*
    **	Copy input to output
     */

    ocp = output->data;
    osp = (unsigned short *) output->data;

    for (y = 0; y < input->height; y++) {
	for (x = 0; x < input->width; x++) {
	    unsigned char *dp;

	    dp = ImagePixel(input, x, y);

	    if (output->cmapSize > 256) {
		*osp++ = counts[TO_15BITS(dp[0], dp[1], dp[2])];
	    } else {
		*ocp++ = counts[TO_15BITS(dp[0], dp[1], dp[2])];
	    }
	}
    }

    output->cmapPacked = True;
    ImageDelete(input);
    outputImage = output;
    return output;
}
