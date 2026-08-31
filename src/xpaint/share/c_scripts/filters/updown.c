/* XPaint-filter */

#include <Xpaint.h>

/* 
 * The key-word "FilterProcess" is reserved for user-defined filter routines;
 * Such a filter processes an "input" image and renders an "output" image.
 * 
 * Pixels are unsigned char arrays p[0]=red, p[1]=green, p[2]=blue
 * (thus each value should be in the range 0..255)
 *
 * In the example below, ip = input pixel, op = output pixel
 * the procedure just reflects the image up <--> down
 */

void FilterProcess(Image * input, Image * output)
{
    unsigned char *ip, *op;
    int x, y;

    for (y = 0; y < input->height; y++) {
        for (x = 0; x < input->width; x++) {
            ip = ImagePixel(input, x, input->height-1-y);
            op = ImagePixel(output, x, y);
            memcpy(op, ip, 3);
        }
    }
}


