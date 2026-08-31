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

/* $Id: readJPEG.c,v 1.21 2005/03/20 20:15:34 demailly Exp $ */

/* Updated for IJG release 6 by Torsten Martinsen, Fri Dec 8 1995 */

#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <jpeglib.h>
#include "image.h"
#include "rwTable.h"

Image *ReadJPEG(char *file);
int TestJPEG(char *file);


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

Image *
ReadJPEG(char *file)
{
    struct jpeg_decompress_struct cinfo;
    FILE *input_file;
    int w, h, row_stride, ncolors;
    JSAMPROW scanline[1];
    Image *image;


    if ((input_file = fopen(file, "r")) == NULL)
	return NULL;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
	/* If we get here, the JPEG code has signaled an error.
	 * We need to clean up the JPEG object, close the input file,
	 * and return.
	 */
	jpeg_destroy_decompress(&cinfo);
	fclose(input_file);
	return NULL;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, input_file);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    h = cinfo.output_height;
    w = cinfo.output_width;

    if (cinfo.jpeg_color_space == JCS_GRAYSCALE) {
	ncolors = cinfo.desired_number_of_colors;
	image = ImageNewGrey(w, h);
	row_stride = w;
    } else {
	image = ImageNew(w, h);
	row_stride = w * 3;
    }
    cinfo.quantize_colors = FALSE;
    while (cinfo.output_scanline < h) {
	scanline[0] = (JSAMPROW) image->data + cinfo.output_scanline * row_stride;
	(void) jpeg_read_scanlines(&cinfo, scanline, (JDIMENSION) 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    fclose(input_file);

    if (jerr.pub.num_warnings > 0) {	/* XXX */
	RWSetMsg("JPEG warning, image may be corrupted");
	longjmp(jerr.setjmp_buffer, 1);
    }
    return image;
}

int
TestJPEG(char *file)
{
    unsigned char buf[2];
    FILE *fd = fopen(file, "r");
    int ret = 0;

    if (fd == NULL)
	return 0;

    if (2 == fread(buf, sizeof(char), 2, fd)) {
	if (buf[0] == 0xff && buf[1] == 0xd8)
	    ret = 1;
    }
    fclose(fd);

    return ret;
}
