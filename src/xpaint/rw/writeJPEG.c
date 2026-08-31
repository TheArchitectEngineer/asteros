/*
 * Copyright 1996, Torsten Martinsen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: writeJPEG.c,v 1.21 2005/03/20 20:15:34 demailly Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <jpeglib.h>
#include "image.h"
#include "rwTable.h"

extern void *xmalloc(size_t n);

static void
trace_message(j_common_ptr cinfo)
{
    char buf[JMSG_LENGTH_MAX];

    cinfo->err->format_message(cinfo, buf);
    RWSetMsg(buf);
}

struct error_mgr {
    struct jpeg_error_mgr pub;	/* "public" fields */
    jmp_buf setjmp_buffer;	/* for return to caller */
};

static struct error_mgr jerr;

typedef struct error_mgr *error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */
static void
error_exit(j_common_ptr cinfo)
{
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    error_ptr err = (error_ptr) cinfo->err;

    trace_message(cinfo);

    /* Return control to the setjmp point */
    longjmp(err->setjmp_buffer, 1);
}

int
WriteJPEG(char *file, Image * outImage)
{
    struct jpeg_compress_struct cinfo;
    FILE *output_file;
    int w, i, row_stride;
    JSAMPROW scanline[1];
    unsigned char *p, *d, *theline = NULL;

    if ((output_file = fopen(file, "w")) == NULL)
	return 1;

    w = outImage->width;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
	/* If we get here, the JPEG code has signaled an error.
	 * We need to clean up the JPEG object, close the output file,
	 * and return.
	 */
	jpeg_destroy_compress(&cinfo);
	fclose(output_file);
	return 1;
    }
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, output_file);

    cinfo.image_height = outImage->height;
    cinfo.image_width = w;
    if (outImage->isGrey) {
	cinfo.in_color_space = JCS_GRAYSCALE;
	cinfo.input_components = 1;
    } else {
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;
	theline = xmalloc(w * 3);
    }

    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);
    row_stride = w * cinfo.input_components;

    while (cinfo.next_scanline < cinfo.image_height) {
	/*
	 * If we have a greyscale or TrueColor image, just feed
	 * the raw data to the JPEG routine. Otherwise, we must
	 * build an array of RGB triples in 'theline'.
	 */
	if (outImage->isGrey || (outImage->cmapSize == 0))
	    scanline[0] = (JSAMPROW) outImage->data +
		cinfo.next_scanline * row_stride;
	else {
	    d = theline;
	    for (i = 0; i < w; ++i) {
		p = ImagePixel(outImage, i, cinfo.next_scanline);
		*d++ = *p++;
		*d++ = *p++;
		*d++ = *p;
	    }
	    scanline[0] = theline;
	}
	jpeg_write_scanlines(&cinfo, scanline, (JDIMENSION) 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(output_file);
    if (!outImage->isGrey && theline)
	free(theline);

    if (jerr.pub.num_warnings > 0) {	/* XXX */
	RWSetMsg("JPEG warning, image may be corrupted");
	longjmp(jerr.setjmp_buffer, 1);
    }
    return 0;
}
