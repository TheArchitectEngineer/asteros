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

/* $Id: iprocess.c,v 1.18 2005/03/20 20:15:32 demailly Exp $ */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <dlfcn.h>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>

#include "xaw_incdir/AsciiText.h"
#include "xaw_incdir/Command.h"
#include "xaw_incdir/Form.h"
#include "xaw_incdir/Paned.h"

#include "Paint.h"
#include "PaintP.h"
#include "xpaint.h"
#include "image.h"
#include "menu.h"
#include "messages.h"
#include "misc.h"
#include "protocol.h"
#include "graphic.h"

#ifdef XAW3D
#define BORDERWIDTH 1
#else
#define BORDERWIDTH 0
#endif

typedef struct {
    Widget shell, pane, name, program, cclog;
    int mode;
    char *scriptfile;
} LocalInfo;

#define BUFSIZE 65536

void *dl_filter = NULL;
void *dl_proc = NULL;
void *dl_image = NULL;

/*
**  Some real image processing
 */

extern int file_isSpecialImage;
extern int file_transparent;

extern struct imageprocessinfo ImgProcessInfo;
extern FILE * openTempFile(char **np);
extern void removeTempFile(void);

#define CLAMP(low, value, high) \
		if (value < low) value = low; else if (value > high) value = high
#define DEG2RAD	(M_PI/180.0)

#define ConvMatrixSize	3
typedef float ConvMatrix[ConvMatrixSize * ConvMatrixSize];

static int *histogram(Image * input);
static LocalInfo *head;

static PaintMenuItem fileMenu[] =
{
#define FILE_LOAD 0
    MI_SIMPLE("load"),
#define FILE_SAVE 1
    MI_SIMPLE("save"),
#define FILE_SAVEAS 2
    MI_SIMPLE("saveas"),
#define FILE_EDIT 3
    MI_SIMPLE("editor"),
#define FILE_CLOSE 4
    MI_SIMPLE("close"),
};
static PaintMenuItem predefMenu[] =
{
#define PREDEF_FILTERS 0
    MI_SIMPLE("filters"),
#define PREDEF_IMAGES 1
    MI_SIMPLE("images"),
#define PREDEF_3D_CURVES 2
    MI_SIMPLE("3d_curves"),
#define PREDEF_3D_SURFACES 3
    MI_SIMPLE("3d_surfaces"),
#define PREDEF_LAYERS 4
    MI_SIMPLE("layers"),
#define PREDEF_PROCEDURES 5
    MI_SIMPLE("procedures"),
#define PREDEF_BATCH 6
    MI_SIMPLE("batch"),
#define PREDEF_SEPARATOR 7
    MI_SEPARATOR(),
#define PREDEF_HELP 8
    MI_SIMPLECB("help", HelpDialog, "script_editor"),
};

static PaintMenuBar printMenuBar[] =
{
    {None, "script_files", XtNumber(fileMenu), fileMenu},
    {None, "script_predef", XtNumber(predefMenu), predefMenu},
};

static Image *
convolve(Image * input, float *mat, int n,
	 unsigned char *basePixel, Boolean absFlag)
{
    int x, y, xx, yy, xv, yv;
    float sum;
    unsigned char *p;
    unsigned char *op;
    float r, g, b;
    int ir, ig, ib;
    Image *output;

    const int xy = -n/2;
    const int ye = input->height + xy;
    const int xe = input->width + xy;
    const int hh = input->height + input->height;
    const int ww = input->width + input->width;

    output = ImageNew(input->width, input->height);
    op = output->data;

    sum = 0;
    for (yy = 0; yy < n; yy++) {
	for (xx = 0; xx < n; xx++) {
	    sum += mat[xx + n * yy];
	}
    }
    if (sum <= 0)
	sum = 1;

    for (y = xy; y < ye; y++) {
	for (x = xy; x < xe; x++) {
	    r = g = b = 0;
	    for (yy = 0; yy < n; yy++) {
		for (xx = 0; xx < n; xx++) {
		    xv = x + xx;
		    yv = y + yy;
		    if (xv < 0)
			xv = -xv;
		    if (yv < 0) 
			yv = -yv;
		    if (xv >= input->width) {
			xv = xv % ww;
			if (xv >= input->width)
			    xv = ww - xv - 1;
		    }
		    if (yv >= input->height) {
			yv = yv % hh;
			if (yv >= input->height)
			    yv = hh - yv - 1;
		    }
		    p = ImagePixel(input, xv, yv);
		    r += (float) *p++ * mat[xx + n * yy];
		    g += (float) *p++ * mat[xx + n * yy];
		    b += (float) *p * mat[xx + n * yy];
		}
	    }
	    if (absFlag) {
		if (r < 0)
		    r = -r;
		if (g < 0)
		    g = -g;
		if (b < 0)
		    b = -b;
	    }
	    ir = r / sum;
	    ig = g / sum;
	    ib = b / sum;
	    if (basePixel) {
		ir += basePixel[0];
		ig += basePixel[1];
		ib += basePixel[2];
	    }
	    CLAMP(0, ir, 255);
	    CLAMP(0, ig, 255);
	    CLAMP(0, ib, 255);

	    *op++ = ir;
	    *op++ = ig;
	    *op++ = ib;
	}

	if (y % 16 == 0)
	    StateTimeStep();
    }

    return output;
}

/*
**  rescale values from 0..255
 */
static void 
normalize(Image * image)
{
    int i, count;
    unsigned char *sp;
    char *ip;
    int maxval = 0;

    if (image->cmapSize != 0) {
	sp = image->cmapData;
	count = image->cmapSize * 3;
    } else {
	sp = image->data;
	count = image->width * image->height * 3;
    }

    for (ip = (char*) sp, i = 0; i < count; i++, ip++)
	if (*ip > maxval)
	    maxval = *ip;
    if (maxval == 0)
	return;
    for (ip = (char*) sp, i = 0; i < count; i++, ip++)
	*ip = ((int) *ip * 255) / maxval;
}

/*
**  Convert image into a monochrome image
 */
static void 
monochrome(Image * image)
{
    int i, count;
    unsigned char *sp;
    unsigned char *ip;
    int v;

    if (image->cmapSize != 0) {
	sp = image->cmapData;
	count = image->cmapSize;
    } else {
	sp = image->data;
	count = image->width * image->height;
    }

    for (ip = sp, i = 0; i < count; i++) {
	v = (ip[0] * 11 + ip[1] * 16 + ip[2] * 5) >> 5;		/* pp = .33R+.5G+.17B */
	*ip++ = v;
	*ip++ = v;
	*ip++ = v;
    }
}


Image *
ImageSmooth(Image * input)
{
    float *mat;
    int n, i;
    Image *output;

    /*
     * Build n x n convolution matrix (all 1's)
     */
    n = ImgProcessInfo.smoothMaskSize;
    mat = xmalloc(n * n * sizeof(float));
    for (i = 0; i < n * n; ++i)
	mat[i] = 1.0;

    output = convolve(input, mat, n, NULL, False);
    free(mat);
    return output;
}

Image *
ImageSharpen(Image * input)
{
    static ConvMatrix mat =
    {
	-1, -2, -1,
	-2, 20, -2,
	-1, -2, -1
    };
    return convolve(input, mat, ConvMatrixSize, NULL, False);
}

Image *
ImageEdge(Image * input)
{
    static ConvMatrix mat =
    {
	-1, -2, 0,
	-2, 0, 2,
	0, 2, 1
    };
    Image *image = convolve(input, mat, ConvMatrixSize, NULL, True);

    normalize(image);

    return image;
}

Image *
ImageEmboss(Image * input)
{
    static ConvMatrix mat =
    {
	-1, -2, 0,
	-2, 0, 2,
	0, 2, 1
    };
    static unsigned char base[3] =
    {128, 128, 128};
    Image *image = convolve(input, mat, ConvMatrixSize, base, False);

    monochrome(image);
    normalize(image);

    return image;
}

Image *
ImageInvert(Image * input)
{
    Image *output;
    unsigned char *op;
    int x,y;
    unsigned char *p;

    output = ImageNew(input->width, input->height);
    op = output->data;

    for (y = 0; y < input->height; y++)
    {
	for (x = 0; x < input->width; x++)
	{
	    p = ImagePixel(input, x, y);
	    *op++ = 255 - *p++;
	    *op++ = 255 - *p++;
	    *op++ = 255 - *p;
	}
    }

#if 0
    /*
    **	 If the input has a colormap, just invert that.
     */
    if (input->scale == 1 && input->cmapSize != 0) {
	output = ImageNewCmap(input->width, input->height, input->cmapSize);
	ip = input->cmapData;
	op = output->cmapData;
	count = input->cmapSize;

	memcpy(output->data, input->data,
	     sizeof(char) * input->scale * input->width * input->height);
    } else {
	output = ImageNew(input->width, input->height);
	ip = input->data;
	op = output->data;
	count = input->width * input->height;
    }

    for (i = 0; i < count; i++) {
	*op++ = 255 - *ip++;
	*op++ = 255 - *ip++;
	*op++ = 255 - *ip++;
    }
#endif

    return output;
}

Image *
ImageOilPaint(Image * input)
{
    Image *output = ImageNew(input->width, input->height);
    unsigned char *op = output->data;
    int x, y, xx, yy, i;
    int rVal, gVal, bVal;
    int rCnt, gCnt, bCnt;
    int rHist[256], gHist[256], bHist[256];
    int oilArea_2;


    oilArea_2 = ImgProcessInfo.oilArea / 2;

    for (y = 0; y < input->height; y++) {
	for (x = 0; x < input->width; x++) {
	    /*
	    **	compute histogram of (on-screen hunk of) n*n 
	    **	  region centered plane
	     */

	    rCnt = gCnt = bCnt = 0;
	    rVal = gVal = bVal = 0;
	    for (i = 0; i < XtNumber(rHist); i++)
		rHist[i] = gHist[i] = bHist[i] = 0;

	    for (yy = y - oilArea_2; yy < y + oilArea_2; yy++) {
		if (yy < 0 || yy >= input->height)
		    continue;
		for (xx = x - oilArea_2; xx < x + oilArea_2; xx++) {
		    int c, p;
		    unsigned char *rgb;

		    if (xx < 0 || xx >= input->width)
			continue;

		    rgb = ImagePixel(input, xx, yy);

		    if ((c = ++rHist[(p = rgb[0]) / 4]) > rCnt) {
			rVal = p;
			rCnt = c;
		    }
		    if ((c = ++gHist[(p = rgb[1]) / 4]) > gCnt) {
			gVal = p;
			gCnt = c;
		    }
		    if ((c = ++bHist[(p = rgb[2]) / 4]) > bCnt) {
			bVal = p;
			bCnt = c;
		    }
		}
	    }

	    *op++ = rVal;
	    *op++ = gVal;
	    *op++ = bVal;
	}

	if (y % 16 == 0)
	    StateTimeStep();
    }

    return output;
}


/*
 * Add random noise to an image.
 */
Image *
ImageAddNoise(Image * input)
{
    unsigned char *p;
    int x, y, r, g, b, ddelta;
    Image *output = ImageNew(input->width, input->height);
    unsigned char *op = output->data;


    ddelta = ImgProcessInfo.noiseDelta * 2;

    for (y = 0; y < input->height; y++) {
	for (x = 0; x < input->width; x++) {
	    p = ImagePixel(input, x, y);
	    r = p[0] + ddelta * gauss();
	    CLAMP(0, r, 255);
	    g = p[1] + ddelta * gauss();
	    CLAMP(0, g, 255);
	    b = p[2] + ddelta * gauss();
	    CLAMP(0, b, 255);
	    *op++ = r;
	    *op++ = g;
	    *op++ = b;
	}
    }
    return output;
}

/*
 * This function works in-place.
 * Because it swaps pixels in the input image, which may be colour mapped
 * or not, is has knowledge about the Image format that should probably
 * have stayed in image.h. Too bad.
 */
Image *
ImageSpread(Image * input)
{
    int w = input->width, h = input->height;
    unsigned char *p, *np;
    int x, y, minx, miny, maxx, maxy, xn, yn, dist;


    dist = ImgProcessInfo.spreadDistance;
    for (y = 0; y < h; y++) {
	for (x = 0; x < w; x++) {
	    p = ImagePixel(input, x, y);
	    /* find random neighbour pixel within +- dist */
	    minx = x - dist;
	    if (minx < 0)
		minx = 0;
	    maxx = x + dist;
	    if (maxx >= w)
		maxx = w - 1;
	    xn = RANDOMI2(minx, maxx);

	    miny = y - dist;
	    if (miny < 0)
		miny = 0;
	    maxy = y + dist;
	    if (maxy >= h)
		maxy = h - 1;
	    yn = RANDOMI2(miny, maxy);
	    /* swap pixel with neighbour */
	    np = ImagePixel(input, xn, yn);
	    if (input->cmapSize == 0) {
		unsigned char rn, gn, bn;

		rn = np[0];
		gn = np[1];
		bn = np[2];
		np[0] = p[0];
		np[1] = p[1];
		np[2] = p[2];
		p[0] = rn;
		p[1] = gn;
		p[2] = bn;
	    } else if (input->cmapSize > 256) {
		unsigned short i, *ps, *nps;

		ps = &((unsigned short *) input->data)[y * w + x];
		nps = &((unsigned short *) input->data)[yn * w + xn];
		i = *nps;
		*nps = *ps;
		*ps = i;
	    } else {		/* colour map of 256 or less */
		unsigned char i;

		p = &input->data[y * w + x];
		np = &input->data[yn * w + xn];
		i = *np;
		*np = *p;
		*p = i;
	    }
	}
    }

    return input;
}

Image *
ImageBlend(Image * input)
{
    int w = input->width, h = input->height;
    Image *output = ImageNew(w, h);
    unsigned char *op = output->data;
    unsigned char *rgb;
    int x, y, n, ox, oy, px, py, dx, dy;
    int rSum, gSum, bSum, r, g, b;
    float diagonal, gradient, ap;


    ox = w / 2;
    oy = h / 2;
    diagonal = ((double) h) / w;

    /*
     * Compute average of pixels on edge of region. While we are
     * at it, we copy the edge to the output as well.
     */
    rSum = gSum = bSum = 0;
    for (x = 0; x < w; ++x) {
	rgb = ImagePixel(input, x, 0);
	r = rgb[0];
	g = rgb[1];
	b = rgb[2];
	rSum += r;
	gSum += g;
	bSum += b;
	*op++ = r;
	*op++ = g;
	*op++ = b;
    }
    op = ImagePixel(output, 0, h - 1);
    for (x = 0; x < w; ++x) {
	rgb = ImagePixel(input, x, h - 1);
	r = rgb[0];
	g = rgb[1];
	b = rgb[2];
	rSum += r;
	gSum += g;
	bSum += b;
	*op++ = r;
	*op++ = g;
	*op++ = b;
    }
    for (y = 1; y < h - 1; ++y) {
	rgb = ImagePixel(input, 0, y);
	r = rgb[0];
	g = rgb[1];
	b = rgb[2];
	rSum += r;
	gSum += g;
	bSum += b;
	op = ImagePixel(output, 0, y);
	*op++ = r;
	*op++ = g;
	*op++ = b;

	rgb = ImagePixel(input, w - 1, y);
	r = rgb[0];
	g = rgb[1];
	b = rgb[2];
	rSum += r;
	gSum += g;
	bSum += b;
	op = ImagePixel(output, w - 1, y);
	*op++ = r;
	*op++ = g;
	*op++ = b;
    }

    n = 2 * (w + h - 2);	/* total # of pixels on edge */
    rSum /= n;
    gSum /= n;
    bSum /= n;
    op = ImagePixel(output, 1, 1);

    /* compute colour of each point in the interior */
    for (y = 1; y < h - 1; y++) {
	for (x = 1; x < w - 1; x++) {
	    dx = x - ox;
	    dy = y - oy;
	    if (dx == 0 && dy == 0) {	/* this is the centre point */
		r = rSum;
		g = gSum;
		b = bSum;
	    } else {
		if (dx == 0) {	/* special case 1 */
		    px = ox;
		    py = (dy > 0) ? h - 1 : 0;
		} else if (dy == 0) {	/* special case 2 */
		    py = oy;
		    px = (dx > 0) ? w - 1 : 0;
		} else {	/* general case */
		    gradient = ((double) dy) / dx;
		    if (fabs(gradient) < fabs(diagonal)) {
			px = (dx > 0) ? w - 1 : 0;
			py = oy + ((px - ox) * dy) / dx;
		    } else {
			py = (dy > 0) ? h - 1 : 0;
			px = ox + ((py - oy) * dx) / dy;
		    }
		}

		/*
		 * given O(ox,oy), P(px,py), and A(x,y), compute
		 *   |AO|/|PO|
		 */
		ap = sqrt((double) (dx * dx) + (double) (dy * dy)) /
		    sqrt((double) ((px - ox) * (px - ox)) +
			 (double) ((py - oy) * (py - oy)));

		rgb = ImagePixel(input, px, py);
		r = (1 - ap) * rSum + ap * rgb[0];
		g = (1 - ap) * gSum + ap * rgb[1];
		b = (1 - ap) * bSum + ap * rgb[2];
	    }
	    *op++ = r;
	    *op++ = g;
	    *op++ = b;
	}
	op += 6;		/* skip last on this line and first on next */
    }

    return output;
}

Image *
ImagePixelize(Image * input)
{
    int width = input->width, height = input->height;
    Image *output = ImageNew(width, height);
    unsigned char *op, *rgb;
    int x, y, xx, yy, n;
    int rSum, gSum, bSum;
    int xsize, ysize;


    xsize = ImgProcessInfo.pixelizeXSize;
    ysize = ImgProcessInfo.pixelizeYSize;

    if (xsize > width)
	xsize = width;
    if (ysize > height)
	ysize = height;
    n = xsize * ysize;

    for (y = 0; y < height; y += ysize) {
	for (x = 0; x < width; x += xsize) {
	    /*
	     *	compute average of pixels inside megapixel
	     */
	    rSum = gSum = bSum = 0;
	    for (yy = y; yy < y + ysize; yy++) {
		if (yy >= height)
		    continue;
		for (xx = x; xx < x + xsize; xx++) {
		    if (xx >= width)
			continue;
		    rgb = ImagePixel(input, xx, yy);
		    rSum += rgb[0];
		    gSum += rgb[1];
		    bSum += rgb[2];
		}
	    }
	    rSum /= n;
	    gSum /= n;
	    bSum /= n;
	    /*
	     * replace each pixel in megapixel with average
	     */
	    for (yy = y; yy < y + ysize; yy++) {
		if (yy >= height)
		    continue;
		for (xx = x; xx < x + xsize; xx++) {
		    if (xx >= width)
			continue;
		    op = ImagePixel(output, xx, yy);
		    *op++ = rSum;
		    *op++ = gSum;
		    *op = bSum;
		}
	    }
	}

	if (y % 16 == 0)
	    StateTimeStep();
    }

    /* if any incomplete megapixels are left, copy them to the output */
    for (x = (width / xsize) * xsize; x < width; ++x)
	for (y = 0; y < height; ++y) {
	    rgb = ImagePixel(input, x, y);
	    op = ImagePixel(output, x, y);
	    *op++ = *rgb++;
	    *op++ = *rgb++;
	    *op = *rgb;
	}
    for (y = (height / ysize) * ysize; y < height; ++y)
	for (x = 0; x < width; ++x) {
	    rgb = ImagePixel(input, x, y);
	    op = ImagePixel(output, x, y);
	    *op++ = *rgb++;
	    *op++ = *rgb++;
	    *op = *rgb;
	}

    return output;
}


#define SWAP(a, b)	{ int t = (a); (a) = (b); (b) = t; }

Image *
ImageDespeckle(Image * input)
{
    int width = input->width, height = input->height;
    Image *output = ImageNew(width, height);
    unsigned char *op, *rgb;
    int x, y, xx, yy, mask, mask2, i, j, k, l;
    int *ra, *ga, *ba;

    mask = ImgProcessInfo.despeckleMask;
    if (mask > width)
	mask = width;
    if (mask > height)
	mask = height;
    mask2 = mask / 2;

    /* arrays for storing pixels inside mask */
    ra = xmalloc(mask * mask * sizeof(int));
    ga = xmalloc(mask * mask * sizeof(int));
    ba = xmalloc(mask * mask * sizeof(int));

    op = output->data;
    for (y = 0; y < height; ++y) {
	for (x = 0; x < width; ++x) {
	    i = 0;
	    for (yy = MAX(0, y - mask2); yy < MIN(height, y + mask2); ++yy)
		for (xx = MAX(0, x - mask2); xx < MIN(width, x + mask2); ++xx) {
		    rgb = ImagePixel(input, xx, yy);
		    ra[i] = *rgb++;
		    ga[i] = *rgb++;
		    ba[i] = *rgb;
		    ++i;
		}
	    /*
	     * now find median by (shell-)sorting the arrays and
	     * picking the center value
	     */
	    for (j = i / 2; j > 0; j = j / 2)
		for (k = j; k < i; k++) {
		    for (l = k - j; l >= 0 && ra[l] > ra[l + j]; l -= j)
			SWAP(ra[l], ra[l + j]);
		    for (l = k - j; l >= 0 && ga[l] > ga[l + j]; l -= j)
			SWAP(ga[l], ga[l + j]);
		    for (l = k - j; l >= 0 && ba[l] > ba[l + j]; l -= j)
			SWAP(ba[l], ba[l + j]);
		}
	    if (i & 1) {	/* uneven number of data points */
		*op++ = ra[i / 2];
		*op++ = ga[i / 2];
		*op++ = ba[i / 2];
	    } else {		/* even, take average */
		*op++ = (ra[i / 2 - 1] + ra[i / 2]) / 2;
		*op++ = (ga[i / 2 - 1] + ga[i / 2]) / 2;
		*op++ = (ba[i / 2 - 1] + ba[i / 2]) / 2;
	    }
	}
	if (y % 16 == 0)
	    StateTimeStep();
    }

    free(ra);
    free(ga);
    free(ba);

    return output;
}


/*
 * Normalize the contrast of an image.
 */
Image *
ImageNormContrast(Image * input)
{
    unsigned char *p;
    int *hist;
    int x, y, w, h, i, max = 0, cumsum, limit;
    Image *output = ImageNew(input->width, input->height);
    unsigned char *op = output->data;
    int blackpcnt, blackval, whitepcnt, whiteval;


    blackpcnt = ImgProcessInfo.contrastB;
    whitepcnt = 100 - ImgProcessInfo.contrastW;

    hist = histogram(input);

    w = input->width;
    h = input->height;

    /* Find lower knee point in histogram to determine 'black' threshold. */
    cumsum = 0;
    limit = w * h * blackpcnt / 100;
    for (i = 0; i < 256; ++i)
	if ((cumsum += hist[i]) > limit)
	    break;
    blackval = i;

    /* Likewise for 'white' threshold. */
    cumsum = 0;
    limit = w * h * whitepcnt / 100;
    for (i = 256; i > 0; --i) {
	if ((max == 0) && hist[i])
	    max = i + 1;	/* max is index of highest non-zero entry */
	if ((cumsum += hist[i]) > limit)
	    break;
    }
    whiteval = i;

    if (whiteval == blackval) {
        for (y = 0; y < h; y++) {
	    for (x = 0; x < w; x++) {
	        p = ImagePixel(input, x, y);
	        *op++ = *p++;
	        *op++ = *p++;
	        *op++ = *p;
	    }
	}
        free(hist);
        return output;
    }

    /*
     * Now create a histogram with a linear ramp
     * from (blackval, 0) to (whiteval, max)
     */
    for (i = 0; i < blackval; ++i)
	hist[i] = 0;
    for (; i <= whiteval; ++i)
	hist[i] = max * (i - blackval) / (whiteval - blackval);
    for (; i < 256; ++i)
	hist[i] = max;

    for (y = 0; y < h; y++) {
	for (x = 0; x < w; x++) {
	    p = ImagePixel(input, x, y);
	    *op++ = hist[*p++];
	    *op++ = hist[*p++];
	    *op++ = hist[*p];
	}
    }
    free(hist);

    return output;
}

/*
 * Build histogram data for the image.
 */
static int *
histogram(Image * input)
{
    unsigned char *p;
    int x, y, w, h, i, r, g, b, *hist;

    w = input->width;
    h = input->height;

    /* Make a table of 256 zeros plus one sentinel */
    hist = xmalloc(257 * sizeof(int));
    for (i = 0; i <= 256; ++i)
	hist[i] = 0;

    /*
     * Build histogram of intensity values.
     */
    for (y = 0; y < h; y++) {
	for (x = 0; x < w; x++) {
	    p = ImagePixel(input, x, y);
	    r = *p++;
	    g = *p++;
	    b = *p;
	    i = (r * 169 + g * 256 + b * 87) / 512;	/* i = .33R+.5G+.17B */
	    ++hist[i];
	}
    }
    return hist;
}

#if 0

#define HIST_H	  120		/* Height of histogram */
#define HIST_W	  256		/* Width of histogram */
#define HIST_B	    2		/* Horizontal border width for histogram */
#define HIST_R	   12		/* Height of ruler below histogram */
#define HIST_T	    9		/* Height of tick marks on ruler */
#define LIGHTGREY 200		/* Greyscale level used for bars */
#define MIDGREY	  255		/* Greyscale level used for ruler */
#define DARKGREY  120		/* Greyscale level used for background */

/*
 * Create an image (width 256, height HIST_H) depicting the histogram data.
 */
Image *
HistogramImage(char *hist)
{
    unsigned char *p;
    int x, y, i;
    int max, e:
    unsigned char *ihist;
    Image *output = ImageNewGrey(HIST_W + 2 * HIST_B, HIST_H + HIST_R);;

    max = 0;
    ihist = xmalloc(128 * sizeof(int));
    for (i = 0; i < 128; ++i) {
	e = ihist[i] = hist[i * 2] + hist[i * 2 + 1];
	if (e > max)
	    max = e;
    }
    /*
     * Now create histogram image.
     * Only display 128 bins to make the histogram easier to read.
     * Leave a border of a few pixels in each side.
     */

    memset(output->data, DARKGREY, (HIST_W + 2 * HIST_B) * (HIST_H + HIST_R));
    for (x = 0; x < 128; ++x) {
	i = ihist[x] * (HIST_H - 1) / max;
	for (y = HIST_H - 1 - i; y < HIST_H - 1; y++) {
	    p = output->data + y * (HIST_W + 2 * HIST_B) + x * 2 + HIST_B;
	    *p++ = LIGHTGREY;
	    *p = LIGHTGREY;
	}
    }
    /* Ruler */
    memset(output->data + HIST_H * (HIST_W + 2 * HIST_B) + HIST_B,
	   MIDGREY, HIST_W);
    for (x = 0; x < 5; ++x) {
	int tick[] =
	{0, HIST_W / 4, HIST_W / 2, HIST_W * 3 / 4, HIST_W - 1};

	for (y = 0; y < HIST_T; y++) {
	    p = output->data + (y + HIST_H + 1) * (HIST_W + 2 * HIST_B)
		+ tick[x] + HIST_B;
	    *p = MIDGREY;
	}
    }
    free(ihist);

    return output;
}
#endif

/*
 * Tilt an image.
 */

/*
 * Compute the interpolated RGB values of a 1 x 1 pixel centered
 * at (x,y) and store these values in op[0] trough op[2].
 * The interpolation is based on weighting the RGB values proportionally
 * to the overlapping areas.
 */
static void 
interpol(float x, float y, Image * input, unsigned char *op)
{
    int xl, xh, yl, yh;
    float a, b, c, d, A, B, C, D;
    unsigned char *plh, *phh, *pll, *phl;

    xl = floor(x);
    xh = ceil(x);
    yl = floor(y);
    yh = ceil(y);

    pll = ImagePixel(input, xl, yl);

    if (xh == xl) {
	if (yh == yl) {
	    /* pixel coincides with one pixel */
	    *op++ = *pll++;
	    *op++ = *pll++;
	    *op++ = *pll++;
	} else {
	    /* pixel overlaps with two pixels on top of each other */
	    plh = ImagePixel(input, xl, yh);
	    A = y - yl;
	    B = yh - y;
	    *op++ = A * *plh++ + B * *pll++;
	    *op++ = A * *plh++ + B * *pll++;
	    *op++ = A * *plh++ + B * *pll++;
	}
    } else if (yh == yl) {
	/* pixel overlaps with two side-by-side pixels */
	phl = ImagePixel(input, xh, yl);
	A = x - xl;
	B = xh - x;
	*op++ = A * *phl++ + B * *pll++;
	*op++ = A * *phl++ + B * *pll++;
	*op++ = A * *phl++ + B * *pll++;
    } else {
	/* general case: pixel overlaps all four neighbour pixels */
	a = xh - x;
	b = y - yl;
	c = x - xl;
	d = yh - y;
	A = a * b;
	B = b * c;
	C = a * d;
	D = c * d;
	plh = ImagePixel(input, xl, yh);
	phl = ImagePixel(input, xh, yl);
	phh = ImagePixel(input, xh, yh);
	*op++ = A * *plh++ + B * *phh++ + C * *pll++ + D * *phl++;
	*op++ = A * *plh++ + B * *phh++ + C * *pll++ + D * *phl++;
	*op++ = A * *plh++ + B * *phh++ + C * *pll++ + D * *phl++;
    }
}

Image *
ImageTilt(Image * input)
{
    int x, y, br, bg, bb;
    int width = input->width, height = input->height;
    Image *output;
    unsigned char *op;
    float xt, yt;
    float X1, Y1, X2, Y2;


    X1 = width * ImgProcessInfo.tiltX1 / 100;
    X2 = width * ImgProcessInfo.tiltX2 / 100;

    Y1 = height * ImgProcessInfo.tiltY1 / 100;
    Y2 = height * ImgProcessInfo.tiltY2 / 100;

    if (((Y1 >= 0) && (Y1 < height)) || ((X2 >= 0) && (X2 < width)))
	return input;

    output = ImageNew(width, height);

    /* Get RGB values of background colour */
    br = ImgProcessInfo.background->red / 256;
    bg = ImgProcessInfo.background->green / 256;
    bb = ImgProcessInfo.background->blue / 256;

    op = output->data;
    for (x = 0; x < width * height; ++x) {
	*op++ = br;
	*op++ = bg;
	*op++ = bb;
    }

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    op = ImagePixel(output, x, y);
	    /* find the input coords corresponding to output coords (x,y) */
	    xt = (X1 * y - Y1 * x) / (y - Y1);
	    yt = (-X2 * y + x * Y2) / (x - X2);
	    if ((xt >= 0) && (xt < width) && (yt >= 0) && (yt < height)) {
#if 0
		unsigned char *p;
		/* rounding */
		p = ImagePixel(input, (int) (xt + 0.5), (int) (yt + 0.5));
		*op++ = *p++;
		*op++ = *p++;
		*op = *p;
#else
		interpol(xt, yt, input, op);
#endif
	    }
	}
    }

    return output;
}

/*
 * Produce the 'solarization' effect seen when exposing a 
 * photographic film to light during the development process.
 * Done here by inverting all pixels above threshold level.
 */
Image *
ImageSolarize(Image * input)
{
    Image *output = ImageNewCmap(input->width, input->height, input->cmapSize);
    int i;
    unsigned char *ip, *op;
    int count, limit;

    limit = ImgProcessInfo.solarizeThreshold * 255 / 100;

    /*
    **	 If the input has a colormap, just tweak that.
     */
    if (input->cmapSize != 0) {
	ip = input->cmapData;
	op = output->cmapData;
	count = input->cmapSize;

	memcpy(output->data, input->data,
	     sizeof(char) * input->scale * input->width * input->height);
    } else {
	ip = input->data;
	op = output->data;
	count = input->width * input->height;
    }

    for (i = 0; i < count; i++) {
	*op++ = (*ip > limit) ? 255 - *ip++ : *ip++;
	*op++ = (*ip > limit) ? 255 - *ip++ : *ip++;
	*op++ = (*ip > limit) ? 255 - *ip++ : *ip++;
    }

    return output;
}

/*
 * Colourmap quantization. Hacked from ppmqvga.c, which is part of Netpbm and
 * was originally written by Lyle Rains (lrains@netcom.com) and Bill Davidsen
 * (davidsen@crd.ge.com).
 * You are probably not supposed to understand this.
 */

#define RED_BITS   5
#define GREEN_BITS 6
#define BLUE_BITS  5

#define MAX_RED	   (1 << RED_BITS)
#define MAX_GREEN  (1 << GREEN_BITS)
#define MAX_BLUE   (1 << BLUE_BITS)

#define MAXWEIGHT  128
#define STDWEIGHT_DIV  (2 << 8)
#define STDWEIGHT_MUL  (2 << 10)
#define GAIN	   4

static int r, g, b, clutx, rep_threshold, rep_weight, dr, dg, db;
static int *color_cube, ncolors;
static unsigned char *clut;

#define CUBEINDEX(r,g,b)  (r)*(MAX_GREEN*MAX_BLUE) + (g)*MAX_BLUE + (b)

static void 
diffuse(void)
{
    int _7_32nds, _3_32nds, _1_16th;

    if (clutx < ncolors) {
	if (color_cube[CUBEINDEX(r, g, b)] > rep_threshold) {
	    clut[clutx * 4 + 0] = ((2 * r + 1) * 256) / (2 * MAX_RED);
	    clut[clutx * 4 + 1] = ((2 * g + 1) * 256) / (2 * MAX_GREEN);
	    clut[clutx * 4 + 2] = ((2 * b + 1) * 256) / (2 * MAX_BLUE);
	    ++clutx;
	    color_cube[CUBEINDEX(r, g, b)] -= rep_weight;
	}
	_7_32nds = (7 * color_cube[CUBEINDEX(r, g, b)]) / 32;
	_3_32nds = (3 * color_cube[CUBEINDEX(r, g, b)]) / 32;
	_1_16th = color_cube[CUBEINDEX(r, g, b)] - 3 * (_7_32nds + _3_32nds);
	color_cube[CUBEINDEX(r, g, b)] = 0;
	/* spread error evenly in color space. */
	color_cube[CUBEINDEX(r, g, b + db)] += _7_32nds;
	color_cube[CUBEINDEX(r, g + dg, b)] += _7_32nds;
	color_cube[CUBEINDEX(r + dr, g, b)] += _7_32nds;
	color_cube[CUBEINDEX(r, g + dg, b + db)] += _3_32nds;
	color_cube[CUBEINDEX(r + dr, g, b + db)] += _3_32nds;
	color_cube[CUBEINDEX(r + dr, g + dg, b)] += _3_32nds;
	color_cube[CUBEINDEX(r + dr, g + dg, b + db)] += _1_16th;

	/*
	 * Conserve the error at edges if possible
	 * (which it is, except the last pixel)
	 */
	if (color_cube[CUBEINDEX(r, g, b)] != 0) {
	    if (dg != 0)
		color_cube[CUBEINDEX(r, g + dg, b)] +=
		    color_cube[CUBEINDEX(r, g, b)];
	    else if (dr != 0)
		color_cube[CUBEINDEX(r + dr, g, b)] +=
		    color_cube[CUBEINDEX(r, g, b)];
	    else if (db != 0)
		color_cube[CUBEINDEX(r, g, b) + db] +=
		    color_cube[CUBEINDEX(r, g, b)];
	    else
		fprintf(stderr, "%s\n", msgText[LOST_ERROR_TERM]);
	}
    }
    color_cube[CUBEINDEX(r, g, b)] = -1;
}

/*
** Find representative color nearest to requested color.  Check color cube
** for a cached color index.  If not cached, compute nearest and cache result.
 */
static int 
nearest_color(unsigned char *p)
{
    register unsigned char *test;
    register unsigned i;
    unsigned long min_dist_sqd, dist_sqd;
    int nearest = 0;
    int *cache;
    int r, g, b;

    r = *p++;
    g = *p++;
    b = *p;
    cache = &(color_cube[CUBEINDEX((r << RED_BITS) / 256,
				   (g << GREEN_BITS) / 256,
				   (b << BLUE_BITS) / 256)]);
    if (*cache >= 0)
	return *cache;
    min_dist_sqd = ~0;
    for (i = 0; i < ncolors; ++i) {
	test = &clut[i * 4];
	dist_sqd =
	    3 * (r - test[0]) * (r - test[0]) +
	    4 * (g - test[1]) * (g - test[1]) +
	    2 * (b - test[2]) * (b - test[2]);
	if (dist_sqd < min_dist_sqd) {
	    nearest = i;
	    min_dist_sqd = dist_sqd;
	}
    }
    return (*cache = nearest);
}

Image *
ImageQuantize(Image * input)
{
    int w = input->width, h = input->height;
    Image *output = ImageNew(w, h);
    unsigned char *op = output->data;
    int *weight_convert;
    int k, x, y, i, j, nearest;
    int total_weight, cum_weight[MAX_GREEN];
    int *erropt;
    int *errP;
    unsigned char *p, *clutP;


    ncolors = ImgProcessInfo.quantizeColors;

    /* Make a color cube, initialized to zeros */
    color_cube = calloc(MAX_RED * MAX_GREEN * MAX_BLUE, sizeof(int));
    clut = calloc(ncolors * 4, sizeof(int));
    erropt = calloc(ncolors * 4, sizeof(int));

    clutx = 0;

    /* Count all occurrences of each color */
    for (y = 0; y < h; ++y)
	for (x = 0; x < w; ++x) {
	    p = ImagePixel(input, x, y);
	    r = p[0] / (256 / MAX_RED);
	    g = p[1] / (256 / MAX_GREEN);
	    b = p[2] / (256 / MAX_BLUE);
	    ++color_cube[CUBEINDEX(r, g, b)];
	}

    /* Initialize logarithmic weighting table */
    weight_convert = xmalloc(MAXWEIGHT * sizeof(int));
    weight_convert[0] = 0;
    for (i = 1; i < MAXWEIGHT; ++i) {
	weight_convert[i] = (int) (100.0 * log((double) i));
    }

    k = w * h;
    if ((k /= STDWEIGHT_DIV) == 0)
	k = 1;
    total_weight = i = 0;
    for (g = 0; g < MAX_GREEN; ++g) {
	for (r = 0; r < MAX_RED; ++r) {
	    for (b = 0; b < MAX_BLUE; ++b) {
		register int weight;
		/* Normalize the weights, independent of picture size. */
		weight = color_cube[CUBEINDEX(r, g, b)] * STDWEIGHT_MUL;
		weight /= k;
		if (weight)
		    ++i;
		if (weight >= MAXWEIGHT)
		    weight = MAXWEIGHT - 1;
		total_weight += (color_cube[CUBEINDEX(r, g, b)]
				 = weight_convert[weight]);
	    }
	}
	cum_weight[g] = total_weight;
    }
    rep_weight = total_weight / ncolors;

    /* Magic foo-foo dust here.	 What IS the correct way to select threshold? */
    rep_threshold = total_weight * (28 + 110000 / i) / 95000;

    /*
     * Do a 3-D error diffusion dither on the data in the color cube
     * to select the representative colors.  Do the dither back and forth in
     * such a manner that all the error is conserved (none lost at the edges).
     */

    dg = 1;
    for (g = 0; g < MAX_GREEN; ++g) {
	dr = 1;
	for (r = 0; r < MAX_RED; ++r) {
	    db = 1;
	    for (b = 0; b < MAX_BLUE - 1; ++b)
		diffuse();
	    db = 0;
	    diffuse();
	    ++b;
	    if (++r == MAX_RED - 1)
		dr = 0;
	    db = -1;
	    while (--b > 0)
		diffuse();
	    db = 0;
	    diffuse();
	}
	/* Modify threshold to keep rep points proportionally distributed */
	if ((j = clutx - (ncolors * cum_weight[g]) / total_weight) != 0)
	    rep_threshold += j * GAIN;

	if (++g == MAX_GREEN - 1)
	    dg = 0;
	dr = -1;
	while (r-- > 0) {
	    db = 1;
	    for (b = 0; b < MAX_BLUE - 1; ++b)
		diffuse();
	    db = 0;
	    diffuse();
	    ++b;
	    if (--r == 0)
		dr = 0;
	    db = -1;
	    while (--b > 0)
		diffuse();
	    db = 0;
	    diffuse();
	}
	/* Modify threshold to keep rep points proportionally distributed */
	if ((j = clutx - (ncolors * cum_weight[g]) / total_weight) != 0)
	    rep_threshold += j * GAIN;
    }

    /*
     * Check the error associated with the use of each color, and
     * change the value of the color to minimize the error.
     */
    for (y = 0; y < h; ++y) {
	for (x = 0; x < w; ++x) {
	    p = ImagePixel(input, x, y);
	    nearest = nearest_color(p);
	    errP = &erropt[nearest * 4];
	    clutP = &clut[nearest * 4];
	    errP[0] += *p++ - clutP[0];
	    errP[1] += *p++ - clutP[1];
	    errP[2] += *p - clutP[2];
	    ++errP[3];
	}
    }

    for (i = 0; i < ncolors; ++i) {
	clutP = &clut[i * 4];
	errP = &erropt[i * 4];
	j = errP[3];
	if (j > 0)
	    j *= 4;
	else if (j == 0)
	    j = 1;
	clutP[0] += (errP[0] / j) * 4;
	clutP[1] += (errP[1] / j) * 4;
	clutP[2] += (errP[2] / j) * 4;
    }

    /* Reset the color cache. */
    for (i = 0; i < MAX_RED * MAX_GREEN * MAX_BLUE; ++i)
	color_cube[i] = -1;

    /*
     * Map the colors in the image to their closest match in the new colormap.
     */
    for (y = 0; y < h; ++y) {
	for (x = 0; x < w; ++x) {
	    p = ImagePixel(input, x, y);
	    nearest = nearest_color(p);
	    clutP = &clut[nearest * 4];
	    *op++ = *clutP++;
	    *op++ = *clutP++;
	    *op++ = *clutP;
	}
    }

    free(clut);
    free(erropt);
    free(weight_convert);

    return output;
}

/*
 * Convert an image to grey scale.
 */
Image *
ImageGrey(Image * input)
{
    Image *output = ImageNew(input->width, input->height);
    unsigned char *p, *op = output->data;
    int x, y, v;


    for (y = 0; y < input->height; y++) {
	for (x = 0; x < input->width; x++) {
	    p = ImagePixel(input, x, y);
	    v = (32*p[0]+50*p[1]+18*p[2])/100;	/* 0.32R + 0.5G + 0.18B */
	    *op++ = v;
	    *op++ = v;
	    *op++ = v;
	}
    }
    return output;
}

Image *
ImageBWMask(Image * input)
{
    Image *output = ImageNew(input->width, input->height);
    unsigned char *p, *op = output->data;
    int x, y, v;

    for (y = 0; y < input->height; y++) {
	for (x = 0; x < input->width; x++) {
	    p = ImagePixel(input, x, y);
            if (memcmp(p, Global.bg, 3)) v = 0xff; else v = 0;
	    *op++ = v;
	    *op++ = v;
	    *op++ = v;
	}
    }
    return output;
}

/*
 * Modify RGB values of an image.
 */
Image *
ImageModifyRGB(Image * input)
{
    Image *output;
    unsigned char *p, *op;
    int x, y;
    double fr, fg, fb, frp, fgp, fbp;

    if (!ImgProcessInfo.redshift &&
        !ImgProcessInfo.greenshift &&
        !ImgProcessInfo.blueshift) return input;
  
    output = ImageNew(input->width, input->height);
    op = output->data;

    fr = 2.0*ImgProcessInfo.redshift / (100.01-ImgProcessInfo.redshift);
    fg = 2.0*ImgProcessInfo.greenshift / (100.01-ImgProcessInfo.greenshift);
    fb = 2.0*ImgProcessInfo.blueshift / (100.01-ImgProcessInfo.blueshift);
    frp = fr / 255.0;
    fgp = fg / 255.0;
    fbp = fb / 255.0;
    fr = fr + 1.0;
    fg = fg + 1.0;
    fb = fb + 1.0;

    for (y = 0; y < input->height; y++) {
	for (x = 0; x < input->width; x++) {
	    p = ImagePixel(input, x, y);
	    *op++ = (char) ( 0.25 + p[0] * fr / (1.0 + p[0] * frp) );
	    *op++ = (char) ( 0.25 + p[1] * fg / (1.0 + p[1] * fgp) );
	    *op++ = (char) ( 0.25 + p[2] * fb / (1.0 + p[2] * fbp) );
	}
    }

    return output;
}

/*
 * Pipe command routines
 */

void pipe_command(command, input, output, error)
    char *command;
    int input(char*, int);
    void output(const char*, int);
    void error(const char*, int);
{
    int in_pipe[2];
    int out_pipe[2];
    int err_pipe[2];
    int pid, bytes;
    char buffer[BUFSIZE];
    
    if (pipe(in_pipe)==-1)
    {
        return;
    }
    if (pipe(out_pipe)==-1)
    {
        return;
    }
    if (pipe(err_pipe)==-1)
    {
        return;
    }
    
    pid = fork();
    if (pid==-1)
    {
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        close(err_pipe[0]);
        close(err_pipe[1]);
        return;
    }
    
    if (!pid)
    {    /* child's thread */
        close(in_pipe[1]); /* there's no writing on input pipe! */
        close(out_pipe[0]); /* there's no reading of output pipe! */
        close(err_pipe[0]); /* there's no reading of error pipe! */        
        /* redirect stdin */
        if (input)
        {            
            if (dup2(in_pipe[0], fileno(stdin))==-1)
            {
                exit(1);
            }
        }
        /* redirect stdout */
        if (output)
        {            
            if (dup2(out_pipe[1], fileno(stdout))==-1)
            {
                exit(1);
            }
        }
        /* redirect stderr */
        if (error)
        {
            if (dup2(err_pipe[1], fileno(stderr))==-1)
            {
                exit(1);
            }
        }
                  system(command);
        exit(1);        
    }
    else
    {    /* parent's thread */
        close(in_pipe[0]); /* there's no reading of input pipe! */
        close(out_pipe[1]); /* there's no writing on output pipe! */
        close(err_pipe[1]); /* there's no writing on error pipe! */
        /* start writing to pipe */
        if (input)
        {            
            while((bytes=input(buffer, BUFSIZE-1))>0)
            {  
                bytes = write(in_pipe[1], buffer, bytes);
                if (bytes==-1)
                {                  
                    break;
                }            
            }
            close(in_pipe[1]);
        }
        /* start reading pipe output */
        if (output)
        {
            while((bytes=read(out_pipe[0], buffer, BUFSIZE-1))>0)
                output(buffer, bytes);
            close(out_pipe[0]);
        }
        /* start reading pipe error*/
        if (error)
        {
            while((bytes=read(err_pipe[0], buffer, BUFSIZE-1))>0)
                error(buffer, bytes);
            close(err_pipe[0]);
        }
    }        
}

static void append_status(char *string)
{
char *ptr, *catptr;

    XtVaGetValues(head->cclog, XtNstring, &ptr, NULL);
    catptr = xmalloc(strlen(ptr)+strlen(string)+10);
    strcpy(catptr, ptr);
    strcat(catptr, "\n");
    strcat(catptr, string);
    XtVaSetValues(head->cclog, XtNstring, catptr, NULL);
}

static void CollectOutput(const char* data, int i)
{
    static int FlagNewLine = 1;
    int pos=0;
    char superstring[2000];

    while(i--)
    {
    if(data[pos] == '\n')
    {
        char tmp[2000];
        strncpy(tmp, data, pos);
        tmp[pos] = '\0';
        tmp[pos+1] = '\0';
        if (!FlagNewLine)
        {
        strcat(superstring, tmp);
        }
        else
        {
            superstring[0] = '\0';
        strcpy(superstring, tmp);
        }
        data = data + pos + 1;
        pos = 0;
        FlagNewLine = 1;
            append_status(superstring);
    }
    else
        pos++;
    }
    if (pos)
    {
    if (!FlagNewLine)
        strcat(superstring, data);
    else
        superstring[0] = '\0';
    FlagNewLine = 0;
    }
}


/*
 * User defined script procedures
 */

static void
closeCallback(Widget w, XtPointer wlArg, XtPointer junk)
{
   LocalInfo *info = (LocalInfo *) wlArg;
   PopdownMenusGlobal();
   XtDestroyWidget(info->shell);
}

static void
compileCallback(Widget w, XtPointer wlArg, XtPointer junk)
{
   LocalInfo *info = (LocalInfo *) wlArg;
   FILE *fd;
   void (* proc)(Widget wid);
   Image * (* iproc)();
   Image * image;
   Screen *screen;
   Visual *visual;
   Pixmap pix;
   Colormap cmap;
   Widget paint;
   char *tmp, *ptr, *text;
   char cmd[512];
   char header[30];
   char * error;
   void *dl_handle = NULL;
   unsigned char *alpha = NULL;
   int i;

   /* create temporary name */
   fd= openTempFile(&tmp);
   fclose(fd);

   /* copy buffer to temporary C file */
   XtVaGetValues(info->program, XtNstring, &text, NULL);
   if (!text || !*text) {
      return;
   }
   sprintf(cmd, "%s.c", tmp);
   fd = fopen(cmd, "w");
   if (!fd) return;

   ptr = text;
   i = 0;
   while (*ptr) {
      fputc(*ptr, fd);
      if (i <= 25) {
         header[i] = *ptr;
         if (*ptr == '\n') header[i] = '\0';
	 i++;
      }
      ++ptr;
   }
   fclose(fd);
   header[i] = '\0';

   info->mode = -1;
   if (strncasecmp(header, "/* Xpaint-image */", 18) == 0)
      info->mode = PREDEF_IMAGES;
   if (strncasecmp(header, "/* Xpaint-filter */", 19) == 0)
      info->mode = PREDEF_FILTERS;
   if (strncasecmp(header, "/* Xpaint-procedure */", 22) == 0)
      info->mode = PREDEF_PROCEDURES;

   /* compile C script */
   sprintf(cmd, "cd /tmp\ngcc -fPIC -I%s/include -I/usr/include/X11 "
                "-c %s.c -o %s.o\n"
                "gcc -fPIC -shared -Wl,-soname,%s.so %s.o -o %s.so\n",
	        GetShareDir(),
                tmp, tmp, strstr(tmp, "XPaint"), tmp, tmp);
   XtVaSetValues(info->cclog, XtNstring, cmd, NULL);
   pipe_command(cmd, NULL, CollectOutput, CollectOutput);

   /* clean *.c and *.o file */
   sprintf(cmd, "%s.c", tmp);
   unlink(cmd);
   sprintf(cmd, "%s.o", tmp);
   unlink(cmd);

   /* load dll library */
   sprintf(cmd, "%s.so", tmp);

   if (info->mode >= 0)
      dl_handle = dlopen(cmd, RTLD_LAZY);

   /* clean dll .so file */
   unlink(cmd);
   removeTempFile();

   if (dl_handle)
      sprintf(cmd, "echo \"%s\"",
	      msgText[C_SCRIPT_SUCCESSFULLY_COMPILED_AND_LINKED]);
   else
      sprintf(cmd, "echo \"\"; echo \"%s\"; echo \"%s\"",
              msgText[C_SCRIPT_COULD_NOT_BE_COMPILED_OR_LINKED], dlerror());

   pipe_command(cmd, NULL, CollectOutput, CollectOutput);
   if (!dl_handle) return;

   if (info->mode == PREDEF_FILTERS) {
      if (dl_filter) dlclose(dl_filter);
      dl_filter = dl_handle;
   }

   if (info->mode == PREDEF_IMAGES) {
      if (dl_image) dlclose(dl_image);
      dl_image = dl_handle;   
      iproc = dlsym(dl_image, "ImageCreate");
      if ((error = dlerror()) != NULL || !iproc) {
	 if (!error) error = msgText[UNABLE_TO_CREATE_IMAGE];
         Notice(Global.toplevel, error);
         return;
      }
      pix = None;
      image = iproc();
      if (image) {
	 screen = XtScreen(Global.toplevel);
	 visual = DefaultVisualOfScreen(screen);
	 cmap = XCreateColormap(XtDisplay(Global.toplevel), 
		    RootWindowOfScreen(screen), visual, AllocNone);
         alpha = image->alpha;
	 ImageToPixmap(image, Global.toplevel, &pix, &cmap);
      }
      if (!pix) {
         Notice(Global.toplevel, msgText[UNABLE_TO_CREATE_IMAGE]);
	 return;
      }
      if (alpha) {
         file_isSpecialImage = 1;
         file_transparent = 1;
      }
      paint = graphicCreate(makeGraphicShell(Global.toplevel), 0, 0, -1, 
                            pix, cmap, alpha);
      SetAlphaMode((Widget)paint, -1);
      free(alpha);
      return;
   }

   if (info->mode == PREDEF_PROCEDURES) {
      if (dl_proc) dlclose(dl_proc);
      dl_proc = dl_handle;
      proc = dlsym(dl_proc, "PaintProcedure");
      if ((error = dlerror()) != NULL) {
         Notice(Global.toplevel, error);
         return;
      }
      if (proc) proc(Global.toplevel);
      return;
   }
}

static void
externCallback(Widget w, XtPointer wlArg, XtPointer junk)
{
   FILE *fd;
   LocalInfo *info = (LocalInfo *) wlArg;
   int l;
   char *tmp, *ptr, *text;
   char name[256];
   char buf[2048];

   /* write buffer to file */
   PopdownMenusGlobal();
   fd = openTempFile(&tmp);
   fclose(fd);
   sprintf(name, "%s.c", tmp);
   removeTempFile();
   fd = fopen(name, "w");
   if (!fd) {
      Notice(w, msgText[UNABLE_TO_OPEN_FILE], name);
      return;
   }
   XtVaGetValues(info->program, XtNstring, &text, NULL);
   if (text && *text) {
      ptr = text;
      while (*ptr) {
         fputc(*ptr, fd);
         ++ptr;
      }
   }
   fclose(fd);

   /* call external editor */
   sprintf(buf, "%s %s", EDITOR, name);
   system(buf);

   /* Now copy edited file into buffer */
   fd = fopen(name, "r");
   if (!fd) {
      Notice(w, msgText[UNABLE_TO_OPEN_FILE], name);
      return;
   }

   text = xmalloc(2);
   *text = '\0';
   l = 0;
   while (fgets(buf, 2040, fd)) {
      l += strlen(buf);
      text = realloc(text, l+2);
      strcat(text, buf);
   }
   fclose(fd);
   unlink(name);
   XtVaSetValues(info->program, XtNstring, text, NULL);
}

static void 
loadFileCallbackOK(Widget w, XtPointer wlArg, char *file)
{
   FILE *fd;
   LocalInfo *info = (LocalInfo *) wlArg;
   static char *text;
   char buf[2048];
   int l;

   if (!file || !*file) return;
   fd = fopen(file, "r");
   if (!fd) {
      Notice(w, msgText[UNABLE_TO_OPEN_FILE], file);
      return;
   }
   info->scriptfile = file;
   XtVaSetValues(info->name, XtNlabel, file, NULL);
   text = xmalloc(2);
   *text = '\0';
   l = 0;
   while (fgets(buf, 2040, fd)) {
      l += strlen(buf);
      text = realloc(text, l+2);
      strcat(text, buf);
   }
   fclose(fd);
   XtVaSetValues(info->program, XtNstring, text, NULL);
}

static void
loadFileCallback(Widget w, XtPointer wlArg, XtPointer junk)
{
   char buf[256];
   *buf = '\0';
   if (getcwd(buf, 256)) strcat(buf, "/");
   Global.explore = True;
   GetFileName(GetShell(w), BROWSER_SIMPLEREAD, 
               buf, (XtCallbackProc) loadFileCallbackOK, wlArg);
   Global.explore = False;
}

static void 
saveFileCallbackOK(Widget w, XtPointer wlArg, char *file)
{
   FILE *fd;
   LocalInfo *info = (LocalInfo *) wlArg;
   char *ptr, *text;
 
   if (!file || !*file) return;
   fd = fopen(file, "w");
   if (!fd) {
      Notice(w, msgText[UNABLE_TO_OPEN_FILE], file);
      return;
   }
   info->scriptfile = file;
   XtVaSetValues(info->name, XtNlabel, file, NULL);
   XtVaGetValues(info->program, XtNstring, &text, NULL);
   if (!text || !*text) {
      fclose(fd);
      return;
   }
   ptr = text;
   while (*ptr) {
      fputc(*ptr, fd);
      ++ptr;
   }
   fclose(fd);
}

static void
saveFileCallback(Widget w, XtPointer wlArg, XtPointer junk)
{
   LocalInfo *info = (LocalInfo *) wlArg;
   char buf[256];
   *buf = '\0';
   if (getcwd(buf, 256)) strcat(buf, "/");
   Global.explore = True;
   if (!info->scriptfile || !info->scriptfile[0])
      GetFileName(GetShell(w), BROWSER_SIMPLESAVE, 
               buf, (XtCallbackProc) saveFileCallbackOK, wlArg);
   else
      saveFileCallbackOK(w, wlArg, info->scriptfile);
   Global.explore = False;
}

static void
saveasFileCallback(Widget w, XtPointer wlArg, XtPointer junk)
{
   char buf[256];
   *buf = '\0';
   if (getcwd(buf, 256)) strcat(buf, "/");
   Global.explore = True;
   GetFileName(GetShell(w), BROWSER_SIMPLESAVE, 
               buf, (XtCallbackProc) saveFileCallbackOK, wlArg);
   Global.explore = False;
}

static void
loadScriptCallback(Widget w, XtPointer wlArg, XtPointer junk)
{
   LocalInfo *info = (LocalInfo *) wlArg;
   static char directory[256];
   int i;

   info->mode = PREDEF_IMAGES;
   for (i=0; i<PREDEF_SEPARATOR; i++)
      if (w == predefMenu[i].widget) {
	 info->mode = i;
	 break;
      }

   Global.explore = True;
   sprintf(directory, "%s/c_scripts/%s/", GetShareDir(),
	              predefMenu[info->mode].name);

   GetFileName(GetShell(w), BROWSER_SIMPLEREAD, directory, 
      (XtCallbackProc) loadFileCallbackOK, wlArg);
   Global.explore = False;
}

void
editorResized(Widget w, LocalInfo * l, XConfigureEvent * event, Boolean * flg)
{
    Dimension width, height;
    Dimension width1, height1, height2;

    XtVaGetValues(l->shell, XtNwidth, &width, XtNheight, &height, NULL);
    XtResizeWidget(XtParent(l->name), width, 38, BORDERWIDTH);
    XtResizeWidget(l->pane, width, height, BORDERWIDTH);
    XtVaSetValues(l->pane, XtNwidth, width, XtNheight, height, NULL);
    XtVaGetValues(XtParent(l->program), XtNheight, &height1, NULL);

#ifdef XAW3DG   
    width1 = (width>12)? width-10 : 2;
    height1 = (height1>10)? height1-8 : 2;
    height2 = (height>height1+63)? height - height1 - 53 : 10;
#else
    width1 = (width>10)? width-8 : 2;
    height1 = (height1>10)? height1-8 : 2;
    height2 = (height>height1+60)? height - height1 - 50 : 10;
#endif   
    XtResizeWidget(XtParent(l->program), width1+8, height1+8, BORDERWIDTH);
    XtResizeWidget(l->program, width1, height1, BORDERWIDTH);
    XtVaSetValues(XtParent(l->program), 
       XtNwidth, width1+8, XtNheight, height1+8, NULL);
    XtVaSetValues(l->program, 
       XtNwidth, width1, XtNheight, height1, NULL);
    XtMoveWidget(XtParent(l->program), 0, 39);
    XtMoveWidget(l->program, 4, 0);

    XtResizeWidget(XtParent(l->cclog), width1+8, height2+8, BORDERWIDTH);
    XtResizeWidget(l->cclog, width1, height2, BORDERWIDTH);
    XtVaSetValues(XtParent(l->cclog), 
       XtNwidth, width1+8, XtNheight, height2+8, NULL);
    XtVaSetValues(l->cclog, 
       XtNwidth, width1, XtNheight, height2, NULL);
    XtMoveWidget(XtParent(l->cclog), 0, 46+height1);
    XtMoveWidget(l->cclog, 4, 0);
}

static void
popupHandler(Widget w, LocalInfo * info, XEvent * event, Boolean * flag)
{
    if (Global.popped_up) {
        if (event->type == ButtonRelease)
            PopdownMenusGlobal();
        event->type = None;       
    }
}

void *
ScriptEditor(Widget w, Widget paint)
{
    Display *dpy;
    Window root;
    Widget topform, midform, botform;
    Widget bar, cancel, compile;
    LocalInfo *info = XtNew(LocalInfo);
    Arg args[4];
    int nargs = 0;
    int i;

    PopdownMenusGlobal();

    head = info;
    info->mode = 0;
    info->scriptfile = NULL;

    dpy = XtDisplay(GetShell(w));
    root = RootWindowOfScreen(XtScreen(GetShell(w)));

    info->shell = XtVisCreatePopupShell("XPaint / C scripts", 
                                  topLevelShellWidgetClass,
				  Global.toplevel, args, nargs);
    info->pane = XtVaCreateManagedWidget("pane", panedWidgetClass, info->shell,
                                   NULL);
    topform = XtVaCreateManagedWidget("topform", formWidgetClass, info->pane, 
                                      NULL);

    bar = MenuBarCreate(topform, XtNumber(printMenuBar), printMenuBar);
    XtVaSetValues(bar, XtNvertDistance, 2, NULL);
    
    compile = XtVaCreateManagedWidget("compile",
		 		    commandWidgetClass, topform,
				    XtNfromHoriz, bar,
				    XtNhorizDistance, 32,
				    XtNvertDistance, 5,
				    NULL);
    cancel = XtVaCreateManagedWidget("close",
		 		    commandWidgetClass, topform,
				    XtNfromHoriz, compile,
				    XtNhorizDistance, 4,
				    XtNvertDistance, 5,
				    NULL);

    info->name = XtVaCreateManagedWidget("name",
				    labelWidgetClass, topform,
				    XtNlabel, "",
				    XtNwidth, 350,
				    XtNfromHoriz, cancel,
                                    XtNhorizDistance, 24,
                                    XtNvertDistance, 6,
				    XtNborderWidth, 0,
                                    NULL);

    midform = XtVaCreateManagedWidget("midform", 
                                    formWidgetClass, info->pane, NULL);
    botform = XtVaCreateManagedWidget("botform", 
                                    formWidgetClass, info->pane, NULL);

    info->program = 
          XtVaCreateManagedWidget("program", asciiTextWidgetClass, midform,
                                  XtNeditType, XawtextEdit,
                                  XtNresizable, True,
                                  XtNwidth, 620,
                                  XtNheight, 300,
				  XtNstring, NULL,
				  XtNscrollHorizontal, XawtextScrollWhenNeeded,
				  XtNscrollVertical, XawtextScrollWhenNeeded,
				  NULL);
    XtInsertRawEventHandler(info->program, 
			    ButtonPressMask | ButtonReleaseMask,
                            False, (XtEventHandler) popupHandler, info, 
                            XtListHead);


    info->cclog = 
          XtVaCreateManagedWidget("cclog", asciiTextWidgetClass, botform,
                                  XtNeditType, XawtextRead,
                                  XtNresizable, True,
                                  XtNwidth, 620,
                                  XtNheight, 100,
				  XtNstring, NULL,
				  XtNscrollHorizontal, XawtextScrollWhenNeeded,
				  XtNscrollVertical, XawtextScrollWhenNeeded,
				  NULL);
    XtInsertRawEventHandler(info->cclog, 
			    ButtonPressMask | ButtonReleaseMask,
                            False, (XtEventHandler) popupHandler, info, 
                            XtListHead);


    AddDestroyCallback(info->shell,
                       (DestroyCallbackFunc) closeCallback, (XtPointer) info);

    XtAddCallback(fileMenu[FILE_LOAD].widget, XtNcallback, 
		     (XtCallbackProc) loadFileCallback, (XtPointer) info);
    XtAddCallback(fileMenu[FILE_SAVE].widget, XtNcallback, 
		     (XtCallbackProc) saveFileCallback, (XtPointer) info);
    XtAddCallback(fileMenu[FILE_SAVEAS].widget, XtNcallback, 
		     (XtCallbackProc) saveasFileCallback, (XtPointer) info);
    XtAddCallback(fileMenu[FILE_EDIT].widget, XtNcallback, 
		     (XtCallbackProc) externCallback, (XtPointer) info);
    XtAddCallback(fileMenu[FILE_CLOSE].widget, XtNcallback, 
		     (XtCallbackProc) closeCallback, (XtPointer) info);
    XtAddCallback(compile, XtNcallback,
                  (XtCallbackProc)compileCallback, (XtPointer) info);
    XtAddCallback(cancel, XtNcallback,
                  (XtCallbackProc)closeCallback, (XtPointer) info);

    for (i=0; i<PREDEF_SEPARATOR; i++)
       XtAddCallback(predefMenu[i].widget, XtNcallback, 
		     (XtCallbackProc) loadScriptCallback, (XtPointer) info);

    /* XtRealizeWidget(info->shell); */
    XtPopup(info->shell, XtGrabNone);
    XtVaSetValues(topform, XtNshowGrip, False, NULL);
    XtVaSetValues(midform, XtNshowGrip, True, NULL);
    XtVaSetValues(botform, XtNshowGrip, True, NULL);

    XtUnmanageChild(info->name);
    XMapWindow(dpy, XtWindow(info->name));
    XtUnmanageChild(cancel);
    XMapWindow(dpy, XtWindow(cancel));
    XtUnmanageChild(compile);
    XMapWindow(dpy, XtWindow(compile));
    XtUnmanageChild(info->program);
    XMapWindow(dpy, XtWindow(info->program));
    XtUnmanageChild(info->cclog);
    XMapWindow(dpy, XtWindow(info->cclog));      
   
    XtAddEventHandler(midform, StructureNotifyMask, False,
		      (XtEventHandler) editorResized, (XtPointer) info);
    XtAddEventHandler(info->shell, StructureNotifyMask, False,
		      (XtEventHandler) editorResized, (XtPointer) info);
    XtAddEventHandler(info->program, ButtonPressMask, False,
		      (XtEventHandler) mousewheelScroll, (XtPointer) NULL);
    XtAddEventHandler(info->cclog, ButtonPressMask, False,
		      (XtEventHandler) mousewheelScroll, (XtPointer) NULL);
    XtSetMinSizeHints(info->shell, 300, 160);
    return info;
}

/* convenience routines */
int
RegionX()
{
    PaintWidget pw = (PaintWidget)Global.curpaint;
    return pw->paint.region.rect.x;
}
int
RegionY()
{
    PaintWidget pw = (PaintWidget)Global.curpaint;
    return pw->paint.region.rect.y;
}
int
RegionWidth()
{
    PaintWidget pw = (PaintWidget)Global.curpaint;
    return pw->paint.region.rect.width;
}
int
RegionHeight()
{
    PaintWidget pw = (PaintWidget)Global.curpaint;
    return pw->paint.region.rect.height;
}

void ImageToRegion(Image * input, XRectangle rect, Pixmap mask)
{
    Pixmap pix;
    Colormap cmap;
    ImageToPixmap(input, Global.curpaint, &pix, &cmap);
    PwRegionSet(Global.curpaint, &rect, pix, mask);
}

void ImageToCanvas(Image * input, Pixmap mask)
{
    Pixmap pix;
    Colormap cmap;
    XRectangle rect;
    if (!input || !Global.curpaint) return;
    rect.width = input->width;
    rect.height = input->height;
    rect.x = 0;
    rect.y = 0;
    ImageToPixmap(input, Global.curpaint, &pix, &cmap);
    PwRegionSet(Global.curpaint, &rect, pix, mask);
    PwRegionSet(Global.curpaint, None, None, None);
}

Image *
CanvasToImage()
{
    PaintWidget pw = (PaintWidget)Global.curpaint;
    Image * image;

    image = PixmapToImage((Widget)pw, GET_PIXMAP(pw), Global.clipboard.cmap);
    return image;
}

Boolean
isFilterDefined()
{
    return (dl_filter)? True : False;
}

Image *
ImageUserDefined(Image * input)
{
    Image *output;
    char * error;
    void (* proc)(Image *, Image *);
    
    if (!dl_filter) return NULL;

    proc = dlsym(dl_filter, "FilterProcess");
    if ((error = dlerror()) != NULL) {
       Notice(Global.toplevel, error);
       return input;
    }

    output = ImageNew(input->width, input->height);

    proc(input, output);

    return output;
}


#define GRAY(p)		(((p)[0]*169 + (p)[1]*256 + (p)[2]*87)/512)
#define SQ(x)		((x)*(x))

/*
 * Directional filter, according to the algorithm described on p. 60 of:
 * _Algorithms_for_Graphics_and_Image_Processing_. Theo Pavlidis.
 * Computer Science Press, 1982.
 * For each pixel, detect the most prominent edge and apply a filter that
 * does not degrade that edge.
 */
Image *
ImageDirectionalFilter(Image * input)
{
    unsigned char *p00, *p10, *p_0, *p11, *p__, *p01, *p0_, *p_1, *p1_;
    int g00, g10, g_0, g11, g__, g01, g0_, g_1, g1_;
    int v0, v45, v90, v135, vmin, theta;
    int x, y;
    Image *output = ImageNew(input->width, input->height);
    unsigned char *op = output->data;

    /*
     * We don't process the border of the image tto avoid the hassle
     * of having do deal with boundary conditions. Hopefully no one
     * will notice.
     */
    /* copy first row unchanged */
    for (x = 0; x < input->width; x++) {
	p00 = ImagePixel(input, x, 0);
	*op++ = *p00++;
	*op++ = *p00++;
	*op++ = *p00++;
    }

    for (y = 1; y < input->height - 1; y++) {
	/* copy first column unchanged */
	p00 = ImagePixel(input, 0, y);
	*op++ = *p00++;
	*op++ = *p00++;
	*op++ = *p00++;
	for (x = 1; x < input->width - 1; x++) {
	    /* find values of pixel and all neighbours */
	    p00 = ImagePixel(input, x, y);
	    p10 = ImagePixel(input, x + 1, y);
	    p_0 = ImagePixel(input, x - 1, y);
	    p11 = ImagePixel(input, x + 1, y + 1);
	    p__ = ImagePixel(input, x - 1, y - 1);
	    p01 = ImagePixel(input, x, y + 1);
	    p0_ = ImagePixel(input, x, y - 1);
	    p_1 = ImagePixel(input, x - 1, y + 1);
	    p1_ = ImagePixel(input, x + 1, y - 1);

	    /* get grayscale values */
	    g00 = GRAY(p00);
	    g01 = GRAY(p01);
	    g10 = GRAY(p10);
	    g11 = GRAY(p11);
	    g0_ = GRAY(p0_);
	    g_0 = GRAY(p_0);
	    g__ = GRAY(p__);
	    g_1 = GRAY(p_1);
	    g1_ = GRAY(p1_);

	    /* estimate direction of edge, if any */
	    v0 = SQ(g00 - g10) + SQ(g00 - g_0);
	    v45 = SQ(g00 - g11) + SQ(g00 - g__);
	    v90 = SQ(g00 - g01) + SQ(g00 - g0_);
	    v135 = SQ(g00 - g_1) + SQ(g00 - g1_);

	    vmin = MIN(MIN(v0, v45), MIN(v90, v135));
	    theta = 0;
	    if (vmin == v45)
		theta = 1;
	    else if (vmin == v90)
		theta = 2;
	    else if (vmin == v135)
		theta = 3;

	    /* apply filtering according to direction of edge */
	    switch (theta) {
	    case 0:		/* 0 degrees */
		*op++ = (*p_0++ + *p00++ + *p10++) / 3;
		*op++ = (*p_0++ + *p00++ + *p10++) / 3;
		*op++ = (*p_0++ + *p00++ + *p10++) / 3;
		break;
	    case 1:		/* 45 degrees */
		*op++ = (*p__++ + *p00++ + *p11++) / 3;
		*op++ = (*p__++ + *p00++ + *p11++) / 3;
		*op++ = (*p__++ + *p00++ + *p11++) / 3;
		break;
	    case 2:		/* 90 degrees */
		*op++ = (*p0_++ + *p00++ + *p01++) / 3;
		*op++ = (*p0_++ + *p00++ + *p01++) / 3;
		*op++ = (*p0_++ + *p00++ + *p01++) / 3;
		break;
	    case 3:		/* 135 degrees */
		*op++ = (*p1_++ + *p00++ + *p_1++) / 3;
		*op++ = (*p1_++ + *p00++ + *p_1++) / 3;
		*op++ = (*p1_++ + *p00++ + *p_1++) / 3;
		break;
	    }
	}
	/* copy last column unchanged */
	p00 = ImagePixel(input, x, y);
	*op++ = *p00++;
	*op++ = *p00++;
	*op++ = *p00++;
    }

    /* copy last row unchanged */
    for (x = 0; x < input->width; x++) {
	p00 = ImagePixel(input, x, y);
	*op++ = *p00++;
	*op++ = *p00++;
	*op++ = *p00++;
    }
    return output;
}

#if 0
#define ISFOREGND(p) ((p) ? \
		      (deltaR-deltaRV <= p[0]) && (p[0] <= deltaR+deltaRV) && \
		      (deltaG-deltaGV <= p[1]) && (p[1] <= deltaG+deltaGV) && \
		      (deltaB-deltaBV <= p[2]) && (p[2] <= deltaB+deltaBV) : 1)

/*
 * Thicken an image.
 */
Image *
ImageThicken(Image * input)
{
    unsigned char *p00, *p10, *p_0, *p01, *p0_;
    int x, y, width, height, br, bg, bb;
    int deltaR, deltaRV, deltaG, deltaGV, deltaB, deltaBV;
    Image *output;
    unsigned char *op;

    width = input->width;
    height = input->height;
    output = ImageNew(width, height);
    op = output->data;

    /* Get RGB values of background colour */
    br = ImgProcessInfo.background->red / 256;
    bg = ImgProcessInfo.background->green / 256;
    bb = ImgProcessInfo.background->blue / 256;

    for (y = 0; y < height; y++)
	for (x = 0; x < width; x++) {
	    /* find values of pixel and all d-neighbours */
	    p00 = ImagePixel(input, x, y);
	    p10 = x < width - 1 ? ImagePixel(input, x + 1, y) : NULL;
	    p_0 = x > 0 ? ImagePixel(input, x - 1, y) : NULL;
	    p01 = y < height - 1 ? ImagePixel(input, x, y + 1) : NULL;
	    p0_ = y > 0 ? ImagePixel(input, x, y - 1) : NULL;

	    if (ISFOREGND(p00)) {
		*op++ = *p00++;
		*op++ = *p00++;
		*op++ = *p00++;
	    } else {
		*op++ = br;
		*op++ = bg;
		*op++ = bb;
	    }
	}

    return output;
}
#endif
