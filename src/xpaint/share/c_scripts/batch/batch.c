/* Xpaint-image */

#include <Xpaint.h>

/* This is an example of a batch script - its normal use is to
 * be invoked by xpaint from command line, e.g. via
 *   
 *  xpaint -iconic batch.c
 * 
 * This batch file opens by default the 'xpaint_input' file
 * and writes the output as PNG file    'xpaint_output'
 * 
 * (see code below)
 * 
 * Copyright 2007, J.-P. Demailly <demailly@fourier.ujf-grenoble.fr>
 * Licence : GPLv3
 */

/* 
 * The key-word "ImageCreate" is reserved for user-defined image routines;
 * Such a routine should create and process an image.
 * 
 * Pixels are unsigned char arrays p[0]=red, p[1]=green, p[2]=blue
 * (thus each value should be in the range 0..255)
 *
 * In the example below, op = output pixel.
 * The procedure below just creates an image by mapping given functions
 * as red, green, blue components...
 */

#define THRESHOLD 700
#define BORDER_WIDTH 20
#define SHIFT_HORIZ 30

Image * ImageCreate()
{
    Image * input, * output;
    unsigned char *ip, *op;
    int x, y, j;
    
    input = ImageFromFile("xpaint_input");
   
/* Don't forget this otherwise xpaint may crash if input does not exist */
    if (!input) {
       RWSetMsg("Cause: cannot read default input file 'xpaint_input' !!");
       return NULL;
    }
   
    output = ImageNew(input->width, input->height);

/* Example of batch procedure:
 * 1) shift the input image horizontally by SHIFT_HORIZ pixels
 * 2) convert to grey level image
 * 3) blank all pixels such that r+g+b>THRESHOLD
 * 4) put a white frame of BORDER_WIDTH pixel width on the bordure of the image
 * 
 * Just modify to your fancy !!
 */
   
   
    for (y = 0; y < input->height; y++) {
        for (x = 0; x < input->width; x++) {
	    if (x < input->width-SHIFT_HORIZ) {
               ip = ImagePixel(input, x+SHIFT_HORIZ, y);
               j = ip[0]+ip[1]+ip[2];
	    } else
	       j = 767;
            op = ImagePixel(output, x, y);
            if ((j>THRESHOLD) || 
                (x<=BORDER_WIDTH)||(y<=BORDER_WIDTH)||
                (x>=input->width-BORDER_WIDTH) || 
                (y>=input->height-BORDER_WIDTH))
	       op[0]=op[1]=op[2]=255;
            else {
	       j = j/3;
	    }
        }
    }
    WritePNGn("xpaint_output", output);
    exit(0);
    return output;
}


