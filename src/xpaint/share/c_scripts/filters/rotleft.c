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
 * the procedure just rotates the image to the left
 */

void FilterProcess(Image * input, Image * output)
{
    unsigned char *ip, *op;
    int x, y;

    if (input->height < input->width)
    for (y = 0; y < input->height; y++) {
        for (x = 0; x < input->height; x++) {
            ip = ImagePixel(input, input->height-1-x, y);
            op = ImagePixel(output, y, x);
            memcpy(op, ip, 3);
        }
    } else
    for (y = 0; y < input->width; y++) {
        for (x = 0; x < input->width; x++) {
            ip = ImagePixel(input, input->width-1-x, y);
            op = ImagePixel(output, y, x);
            memcpy(op, ip, 3);
        }
    }
}


