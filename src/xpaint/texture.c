/*
 * This file is part of XPaint.
 *
 * Copyright 1996 by Torsten Martinsen
 * Copyright 1994 by Jer Johnson
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <X11/X.h>
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#ifdef __EMX__
#include <float.h>
#endif

#include "xpaint.h"
#include "misc.h"
#include "image.h"

#if FEATURE_FRACTAL

static void draw_grid(int x, int y, unsigned char col, unsigned char *grid);
static void subdiv(int x1, int y1, int x2, int y2, unsigned char *grid);
static void adjust(int x1, int y1, int x, int y, int x2, int y2,
		   unsigned char *grid);
static void fourn(float data[], int nn[], int ndim, int isign);
static void spectralsynth(float **x, unsigned int n, double h);

static int number, numcolors, width, height;
extern char fractalDensityStr[10];

/*
 * draw_plasma() and friends are stolen from Xtacy, written by Jer Johnson
 * (mpython@gnu.ai.mit.edu) who says that his work in turn is based on OOZE.C,
 * written by Jeff Clough.
 */
Image *
draw_plasma(int w, int h)
{
    int i, n;
    unsigned char *grid;
    Image *output;
    unsigned char *op;


    /* customizable parameters */
    numcolors = 100;
    number = 10;

    width = w;

    n = numcolors / 5;
    numcolors = n * 5;
    output = ImageNewCmap(w, h, numcolors);
    grid = output->data;
    memset(grid, 0, w * h);

    /*
     * Make points at (0,0) (0, h-1) (w-1, 0) (w-1, h-1) of random colors
     */
    draw_grid(0, 0, RANDOMI2(0, numcolors-1), grid);
    draw_grid(w - 1, 0, RANDOMI2(0, numcolors-1), grid);
    draw_grid(0, h - 1, RANDOMI2(0, numcolors-1), grid);
    draw_grid(w - 1, h - 1, RANDOMI2(0, numcolors-1), grid);

    subdiv(0, 0, w - 1, h - 1, grid);

    /* Build the colour map */
    op = output->cmapData;
    for (i = 0; i < n; ++i) {
	*op++ = 255;
	*op++ = 255 * i / n;
	*op++ = 0;
    }
    for (i = 0; i < n; ++i) {
	*op++ = 255 - 255 * i / n;
	*op++ = 255;
	*op++ = 0;
    }
    for (i = 0; i < n; ++i) {
	*op++ = 0;
	*op++ = 255;
	*op++ = 255 * i / n;
    }
    for (i = 0; i < n; ++i) {
	*op++ = 255 * i / n;
	*op++ = 0;
	*op++ = 255;
    }
    for (i = 0; i < n; ++i) {
	*op++ = 255;
	*op++ = 0;
	*op++ = 255 - 255 * i / n;
    }
#if 0
    op = output->cmapData;
    for (i = 0; i < 5 * n; ++i) {
	printf("%d: %d %d %d\n", i, op[0], op[1], op[2]);
	op += 3;
    }
#endif

    return output;
}

static void 
draw_grid(int x, int y, unsigned char col, unsigned char *grid)
{
    grid[x + y * width] = col;
}

static void 
subdiv(int x1, int y1, int x2, int y2, unsigned char *grid)
{
    int x, y;

    if ((x2 - x1 < 2) && (y2 - y1 < 2))
	return;

    x = (x1 + x2) / 2;
    y = (y1 + y2) / 2;
    adjust(x1, y1, x, y1, x2, y1, grid);
    adjust(x2, y1, x2, y, x2, y2, grid);
    adjust(x1, y2, x, y2, x2, y2, grid);
    adjust(x1, y1, x1, y, x1, y2, grid);

    if (grid[x + y * width] == 0) {
	unsigned char spooge;

	spooge = ((int) grid[x1 + y1 * width] +
		  (int) grid[x1 + y2 * width] +
		  (int) grid[x2 + y1 * width] +
		  (int) grid[x2 + y2 * width]) / 4;
	draw_grid(x, y, spooge, grid);
    }
    subdiv(x1, y1, x, y, grid);
    subdiv(x, y1, x2, y, grid);
    subdiv(x1, y, x, y2, grid);
    subdiv(x, y, x2, y2, grid);
}

static void 
adjust(int x1, int y1, int x, int y, int x2, int y2, unsigned char *grid)
{
    int c, d, spoo;
    int clr1, clr2, clr3, clr4, horz, vert;


    if (grid[x + y * width] != 0)
	return;

    clr1 = grid[x1 + y1 * width];
    clr2 = grid[x2 + y2 * width];
    clr3 = grid[x1 + y2 * width];
    clr4 = grid[x2 + y1 * width];
    horz = x2 - x1;
    vert = y2 - y1;
    d = hypot(horz, vert);

#if 1				/* 0 for band effect */
    if ((spoo = RANDOMI2(0, 100)) > number) {
	if (spoo % 2 == 1)
	    c = (((clr1 + clr2) / 2) + ((int) RANDOMI2(0, d))) % numcolors;
	else
	    c = (((clr1 + clr2) / 2) - ((int) RANDOMI2(0, d))) % numcolors;
    } else
#endif
	c = (clr1 + clr2 + clr3 + clr4) / 4;
    if (c < 0)
	c = -c;
    else if (c == 0)
	c = 1;
    draw_grid(x, y, c, grid);
}

/*
 * Fractal landscape generator from ppmforge.c,
 * written by John Walker (kelvin@Autodesk.com).
 */

#define Real(v, x, y)  v[1 + (((x) * meshsize) + (y)) * 2]
#define Imag(v, x, y)  v[2 + (((x) * meshsize) + (y)) * 2]

#define ELEMENTS(array) (sizeof(array)/sizeof((array)[0]))

/* customize: */
static double fracdim = 1.8;	/* Fractal dimension - ]0,3] */
static double powscale = 1.0;	/* Power law scaling exponent - ]0,3] */
static int meshsize = 256;	/* FFT mesh size - generally >= max(w,h),
				   but small numbers can give special effects */
/* colours */

static unsigned char pgnd[][3] =
{
    {206, 205, 0},
    {208, 207, 0},
    {211, 208, 0},
    {214, 208, 0},
    {217, 208, 0},
    {220, 208, 0},
    {222, 207, 0},
    {225, 205, 0},
    {227, 204, 0},
    {229, 202, 0},
    {231, 199, 0},
    {232, 197, 0},
    {233, 194, 0},
    {234, 191, 0},
    {234, 188, 0},
    {233, 185, 0},
    {232, 183, 0},
    {231, 180, 0},
    {229, 178, 0},
    {227, 176, 0},
    {225, 174, 0},
    {223, 172, 0},
    {221, 170, 0},
    {219, 168, 0},
    {216, 166, 0},
    {214, 164, 0},
    {212, 162, 0},
    {210, 161, 0},
    {207, 159, 0},
    {205, 157, 0},
    {203, 156, 0},
    {200, 154, 0},
    {198, 152, 0},
    {195, 151, 0},
    {193, 149, 0},
    {190, 148, 0},
    {188, 147, 0},
    {185, 145, 0},
    {183, 144, 0},
    {180, 143, 0},
    {177, 141, 0},
    {175, 140, 0},
    {172, 139, 0},
    {169, 138, 0},
    {167, 137, 0},
    {164, 136, 0},
    {161, 135, 0},
    {158, 134, 0},
    {156, 133, 0},
    {153, 132, 0},
    {150, 132, 0},
    {147, 131, 0},
    {145, 130, 0},
    {142, 130, 0},
    {139, 129, 0},
    {136, 128, 0},
    {133, 128, 0},
    {130, 127, 0},
    {127, 127, 0},
    {125, 127, 0},
    {122, 127, 0},
    {119, 127, 0},
    {116, 127, 0},
    {113, 127, 0},
    {110, 128, 0},
    {107, 128, 0},
    {104, 128, 0},
    {102, 127, 0},
    {99, 126, 0},
    {97, 124, 0},
    {95, 122, 0},
    {93, 120, 0},
    {92, 117, 0},
    {92, 114, 0},
    {92, 111, 0},
    {93, 108, 0},
    {94, 106, 0},
    {96, 104, 0},
    {98, 102, 0},
    {100, 100, 0},
    {103, 99, 0},
    {106, 99, 0},
    {109, 99, 0},
    {111, 100, 0},
    {114, 101, 0},
    {117, 102, 0},
    {120, 103, 0},
    {123, 102, 0},
    {125, 102, 0},
    {128, 100, 0},
    {130, 98, 0},
    {132, 96, 0},
    {133, 94, 0},
    {134, 91, 0},
    {134, 88, 0},
    {134, 85, 0},
    {133, 82, 0},
    {131, 80, 0},
    {129, 78, 0}
};


/*      FOURN  --  Multi-dimensional fast Fourier transform

   Called with arguments:

   data       A  one-dimensional  array  of  floats  (NOTE!!!   NOT
   DOUBLES!!), indexed from one (NOTE!!!   NOT  ZERO!!),
   containing  pairs of numbers representing the complex
   valued samples.  The Fourier transformed results     are
   returned in the same array.

   nn         An  array specifying the edge size in each dimension.
   THIS ARRAY IS INDEXED FROM  ONE,     AND  ALL  THE  EDGE
   SIZES MUST BE POWERS OF TWO!!!

   ndim       Number of dimensions of FFT to perform.  Set to 2 for
   two dimensional FFT.

   isign      If 1, a Fourier transform is done; if -1 the  inverse
   transformation is performed.

   This  function  is essentially as given in Press et al., "Numerical
   Recipes In C", Section 12.11, pp.  467-470.
 */

static void 
fourn(float data[], int nn[], int ndim, int isign)
{
    register int i1, i2, i3;
    int i2rev, i3rev, ip1, ip2, ip3, ifp1, ifp2;
    int ibit, idim, k1, k2, n, nprev, nrem, ntot;
    float tempi, tempr;
    double theta, wi, wpi, wpr, wr, wtemp;

#define SWAP(a,b) tempr=(a); (a) = (b); (b) = tempr

    ntot = 1;
    for (idim = 1; idim <= ndim; idim++)
	ntot *= nn[idim];
    nprev = 1;
    for (idim = ndim; idim >= 1; idim--) {
	n = nn[idim];
	nrem = ntot / (n * nprev);
	ip1 = nprev << 1;
	ip2 = ip1 * n;
	ip3 = ip2 * nrem;
	i2rev = 1;
	for (i2 = 1; i2 <= ip2; i2 += ip1) {
	    if (i2 < i2rev) {
		for (i1 = i2; i1 <= i2 + ip1 - 2; i1 += 2) {
		    for (i3 = i1; i3 <= ip3; i3 += ip2) {
			i3rev = i2rev + i3 - i2;
			SWAP(data[i3], data[i3rev]);
			SWAP(data[i3 + 1], data[i3rev + 1]);
		    }
		}
	    }
	    ibit = ip2 >> 1;
	    while (ibit >= ip1 && i2rev > ibit) {
		i2rev -= ibit;
		ibit >>= 1;
	    }
	    i2rev += ibit;
	}
	ifp1 = ip1;
	while (ifp1 < ip2) {
	    ifp2 = ifp1 << 1;
	    theta = isign * (M_PI * 2) / (ifp2 / ip1);
	    wtemp = sin(0.5 * theta);
	    wpr = -2.0 * wtemp * wtemp;
	    wpi = sin(theta);
	    wr = 1.0;
	    wi = 0.0;
	    for (i3 = 1; i3 <= ifp1; i3 += ip1) {
		for (i1 = i3; i1 <= i3 + ip1 - 2; i1 += 2) {
		    for (i2 = i1; i2 <= ip3; i2 += ifp2) {
			k1 = i2;
			k2 = k1 + ifp1;
			tempr = wr * data[k2] - wi * data[k2 + 1];
			tempi = wr * data[k2 + 1] + wi * data[k2];
			data[k2] = data[k1] - tempr;
			data[k2 + 1] = data[k1 + 1] - tempi;
			data[k1] += tempr;
			data[k1 + 1] += tempi;
		    }
		}
		wr = (wtemp = wr) * wpr - wi * wpi + wr;
		wi = wi * wpr + wtemp * wpi + wi;
	    }
	    ifp1 = ifp2;
	}
	nprev *= n;
    }
}
#undef SWAP

/*
 * SPECTRALSYNTH -- Spectrally synthesised fractal motion in two dimensions.
 */
static void 
spectralsynth(float **x, unsigned int n, double h)
{
    unsigned bl;
    int i, j, i0, j0, nsize[3];
    double rad, phase, rcos, rsin;
    float *a;

    bl = ((((unsigned long) n) * n) + 1) * 2 * sizeof(float);
    a = (float *) XtCalloc(bl, 1);
    *x = a;

    for (i = 0; i <= n / 2; i++) {
	for (j = 0; j <= n / 2; j++) {
	    phase = 2 * M_PI * (RANDOMI() & 0x7FFF) / 32767.0;
	    if (i != 0 || j != 0) {
		rad = pow((double) (i * i + j * j), -(h + 1) / 2) * gauss();
	    } else {
		rad = 0;
	    }
	    rcos = rad * cos(phase);
	    rsin = rad * sin(phase);
	    Real(a, i, j) = rcos;
	    Imag(a, i, j) = rsin;
	    i0 = (i == 0) ? 0 : n - i;
	    j0 = (j == 0) ? 0 : n - j;
	    Real(a, i0, j0) = rcos;
	    Imag(a, i0, j0) = -rsin;
	}
    }
    Imag(a, n / 2, 0) = 0;
    Imag(a, 0, n / 2) = 0;
    Imag(a, n / 2, n / 2) = 0;
    for (i = 1; i <= n / 2 - 1; i++) {
	for (j = 1; j <= n / 2 - 1; j++) {
	    phase = 2 * M_PI * (RANDOMI() & 0x7FFF) / 32767.0;
	    rad = pow((double) (i * i + j * j), -(h + 1) / 2) * gauss();
	    rcos = rad * cos(phase);
	    rsin = rad * sin(phase);
	    Real(a, i, n - j) = rcos;
	    Imag(a, i, n - j) = rsin;
	    Real(a, n - i, j) = rcos;
	    Imag(a, n - i, j) = -rsin;
	}
    }

    nsize[0] = 0;
    nsize[1] = nsize[2] = n;	/* Dimension of frequency domain array */
    fourn(a, nsize, 2, -1);	/* Take inverse 2D Fourier transform */
}

Image *
draw_landscape(int w, int h, int clouds)
{
    float *a;
    int density = atoi(fractalDensityStr);
    double rmin = HUGE_VAL, rmax = -HUGE_VAL, rmean, rrange;
    Image *output = ImageNew(w, h);
    unsigned char *op = output->data; 
    int x, y, i, j;
    unsigned char *cp, *ap;
    double *u, *u1;
    unsigned int *bxf, *bxc;

    width = w;
    height = h;
    spectralsynth(&a, meshsize, 3.0 - fracdim);

    /* Apply power law scaling if non-unity scale is requested. */

    if (powscale != 1.0)
	for (i = 0; i < meshsize; i++)
	    for (j = 0; j < meshsize; j++) {
		double r = Real(a, i, j);

		if (r > 0)
		    Real(a, i, j) = pow(r, powscale);
	    }
    /* Compute extrema for autoscaling. */
    for (i = 0; i < meshsize; i++)
	for (j = 0; j < meshsize; j++) {
	    double r = Real(a, i, j);

	    rmin = MIN(rmin, r);
	    rmax = MAX(rmax, r);
	}

    rmean = (rmin + rmax) / 2;
    rrange = (rmax - rmin) / 2;
    for (i = 0; i < meshsize; i++)
	for (j = 0; j < meshsize; j++)
	    Real(a, i, j) = (Real(a, i, j) - rmean) / rrange;

    u = (double *) XtMalloc((unsigned int) (width * sizeof(double)));
    u1 = (double *) XtMalloc((unsigned int) (width * sizeof(double)));
    bxf = (unsigned int *) XtMalloc((unsigned int) width *
				    sizeof(unsigned int));
    bxc = (unsigned int *) XtMalloc((unsigned int) width *
				    sizeof(unsigned int));

    /* Prescale the grid points into intensities. */

    cp = (unsigned char *) XtMalloc(meshsize * meshsize + 1);
    ap = cp;
    for (i = 0; i < meshsize; i++)
	for (j = 0; j < meshsize; j++)
	    *ap++ = (255.0 * (Real(a, i, j) + 1.0)) / 2.0;
    /* Hack: make an extra array entry */
    *ap = ap[-1];

    /* Fill the screen from the computed intensity grid by mapping screen points
       onto the grid, then calculating each pixel value by bilinear
       interpolation from the surrounding grid points.  (N.b. the pictures would
       undoubtedly look better when generated with small grids if a more
       well-behaved interpolation were used.)

       Before  we get started, precompute the line-level interpolation
       parameters and store them in an array so we don't  have  to  do
       this every time around the inner loop. */

#define UPRJ(a,size) (((a))? (a)/((size)-1.0) : 0)

    for (x = 0; x < width; x++) {
	double bx = (meshsize - 1) * UPRJ(x, width);

	bxf[x] = floor(bx);
	bxc[x] = bxf[x] + 1;
	u[x] = bx - bxf[x];
	u1[x] = 1 - u[x];
    }

    for (y = 0; y < height; y++) {
	double t, t1, by;
	int byf, byc;

	by = (meshsize - 1) * UPRJ(y, height);
	byf = floor(by) * meshsize;
	byc = byf + meshsize;
	t = by - floor(by);
	t1 = 1 - t;

	if (clouds) {
	    /* Render the FFT output as clouds. */

	    for (x = 0; x < width; x++) {
		double r = t1 * u1[x] * cp[byf + bxf[x]] +
		t * u1[x] * cp[byc + bxf[x]] +
		t * u[x] * cp[byc + bxc[x]] +
		t1 * u[x] * cp[byf + bxc[x]];
		unsigned char w = (r > 127.0) ? (255 * ((r - 127.0) / 128.0)) : 0;

                w = (255*(100-density) + density * (unsigned int)w) / 100;
                /* blue of some density through white */
		*op++ = w; 
		*op++ = w; 
		*op++ = 255;
	    }
	} else
	    for (x = 0; x < width; x++) {
		double r = t1 * u1[x] * cp[byf + bxf[x]] +
		t * u1[x] * cp[byc + bxf[x]] +
		t * u[x] * cp[byc + bxc[x]] +
		t1 * u[x] * cp[byf + bxc[x]];
		int ir, ig, ib;

		if (r >= 128) {
		    int ix = ((r - 128) * (ELEMENTS(pgnd) - 1)) / 127;

		    /* Land area.  Look up colour based on elevation from
		       precomputed colour map table. */

		    ir = pgnd[ix][0];
		    ig = pgnd[ix][1];
		    ib = pgnd[ix][2];
		} else {
		    /* Water.  Generate clouds above water based on elevation.  */
		    ir = ig = density * ((r > 64) ? (r - 64)*4 : 0) / 100;
 		    ib = 255;
		}

		*op++ = ir;
		*op++ = ig;
		*op++ = ib;
	    }
    }

    XtFree((char *) cp);
    XtFree((char *) u);
    XtFree((char *) u1);
    XtFree((char *) bxf);
    XtFree((char *) bxc);
    XtFree((char *) a);

    return output;
}

#endif				/* FEATURE_FRACTAL */
