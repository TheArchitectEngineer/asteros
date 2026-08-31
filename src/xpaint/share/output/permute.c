#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "/usr/share/xpaint/include/image.h"

/* 
 * The key-word "filter_process" is reserved for user-defined filter routines;
 * Such a filter processes an "input" image and renders an "output" image.
 * 
 * Pixels are unsigned char arrays p[0]=red, p[1]=green, p[2]=blue
 * (thus each value should be in the range 0..255)
 *
 * In the example below, ip = input pixel, op = output pixel
 * the procedure just rotates red, green, blue values for each pixel
 */

void filter_process(Image * input, Image * output)
{
    unsigned char *ip, *op;
    int x, y;

    for (y = 0; y < input->height; y++) {
	for (x = 0; x < input->width; x++) {
	    ip = ImagePixel(input, x, y);
            op = ImagePixel(output, x, y);
	    op[0] = ip[1];
	    op[1] = ip[2];
	    op[2] = ip[0];
	}
    }
}


