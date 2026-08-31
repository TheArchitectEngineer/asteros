/* XPaint-filter */

#include <Xpaint.h>

/* 
 * The key-word "FilterProcess" is reserved for user-defined filter routines;
 * Such a filter processes an "input" image and renders an "output" image.
 * 
 * Pixels are unsigned char arrays p[0]=red, p[1]=green, p[2]=blue
 * (thus each value should be in the range 0..255)
 *
 * In the example below, op = output pixel (input image is not used!)
 * The procedure below just creates an image by mapping given functions
 * as red, green, blue components...
 */

void FilterProcess(Image * input, Image * output)
{
    unsigned char *ip, *op;
    int x, y;

    for (y = 0; y < input->height; y++) {
        for (x = 0; x < input->width; x++) {
            op = ImagePixel(output, x, y);
            op[0] = (x+y+x*(1000-x)/500)%256;
            op[1] = y%256;
            op[2] = x%256;
        }
    }
}


