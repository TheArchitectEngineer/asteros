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

/* $Id: readWriteSGI.c,v 1.21 2005/03/20 20:15:34 demailly Exp $ */

#include <stdio.h>
#include <gl/image.h>
#include <sys/fcntl.h>
#include "image.h"

extern void *xmalloc(size_t n);

int WriteSGI(char *file, Image * image)
{
    IMAGE *o;
    short *rbuf = NULL, *gbuf = NULL, *bbuf = NULL;
    int x, y;
    unsigned char *ip;
    int d = 3;

    if (image->isGrey)
	d = 1;
    if ((o = iopen(file, "w", RLE(1), 3, image->width, image->height, d)) == NULL)
	return 1;

    if (d == 1) {
	rbuf = (short *) xmalloc(image->width * sizeof(short));
    } else {
	rbuf = (short *) xmalloc(image->width * sizeof(short));
	gbuf = (short *) xmalloc(image->width * sizeof(short));
	bbuf = (short *) xmalloc(image->width * sizeof(short));
    }

    for (y = 0; y < image->height; y++) {
	for (x = 0; x < image->width; x++) {
	    ip = ImagePixel(image, x, y);
	    rbuf[x] = ip[0];
	    if (d != 1) {
		gbuf[x] = ip[1];
		bbuf[x] = ip[2];
	    }
	}
	if (rbuf != NULL)
	    putrow(o, rbuf, image->height - y - 1, 0);
	if (gbuf != NULL)
	    putrow(o, gbuf, image->height - y - 1, 1);
	if (bbuf != NULL)
	    putrow(o, bbuf, image->height - y - 1, 2);
    }

    iclose(o);

    if (rbuf != NULL)
	free(rbuf);
    if (gbuf != NULL)
	free(gbuf);
    if (bbuf != NULL)
	free(bbuf);

    return 0;
}

int TestSGI(char *file)
{
    int f = open(file, O_RDONLY);
    unsigned short c;
    int ret = 0;

    if (f < 0)
	return 0;

    ret = ((read(f, &c, 2) == 2) && c == IMAGIC);

    close(f);

    return ret;
}

Image *ReadSGI(char *file)
{
    Image *image;
    IMAGE *in;
    short *rbuf, *gbuf, *bbuf;
    int x, y;
    unsigned char *ip;

    if ((in = iopen(file, "r")) == 0)
	return NULL;

    if (in->zsize == 1)
	image = ImageNewGrey(in->xsize, in->ysize);
    else
	image = ImageNew(in->xsize, in->ysize);

    rbuf = (short *) xmalloc(in->xsize * sizeof(short));
    gbuf = bbuf = rbuf;
    if (in->zsize != 1) {
	gbuf = (short *) xmalloc(in->xsize * sizeof(short));
	bbuf = (short *) xmalloc(in->xsize * sizeof(short));
    }
    ip = image->data;

    for (y = in->ysize - 1; y >= 0; y--) {
	getrow(in, rbuf, y, 0);
	if (gbuf != rbuf)
	    getrow(in, gbuf, y, 1);
	if (bbuf != rbuf)
	    getrow(in, bbuf, y, 2);

	for (x = 0; x < in->xsize; x++) {
	    *ip++ = rbuf[x] & 0xff;
	    if (in->zsize != 1) {
		*ip++ = gbuf[x] & 0xff;
		*ip++ = bbuf[x] & 0xff;
	    }
	}
    }

    free(rbuf);
    if (gbuf != rbuf)
	free(gbuf);
    if (bbuf != rbuf)
	free(bbuf);

    iclose(in);

    return image;
}
