/* Xpaint-image */

#include <Xpaint.h>

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

Image * ImageCreate()
{
    Image * image;
    unsigned char * p;
    int x, y;
    
    image = ImageNew(600, 400);

    for (y = 0; y < image->height; y++) {
        for (x = 0; x < image->width; x++) {
            p = ImagePixel(image, x, y);
            p[0] = (x+y+x*(1000-x)/500)%256;
            p[1] = y%256;
            p[2] = x%256;
        }
    }

    return image;
}


