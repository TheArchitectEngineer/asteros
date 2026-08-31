/* Xpaint-procedure */

#include <Xpaint.h>

/* 
 * The key-word "PaintProcedure" is reserved for user-defined procedures;
 * The procedure should probably involve Xpaint's internal API in some way.
 * 
 * Pixels are unsigned char arrays p[0]=red, p[1]=green, p[2]=blue
 * (thus each value should be in the range 0..255)
 *
 * In the example below, op = output pixel.
 * The procedure below just creates an image by mapping given functions
 * as red, green, blue components and stores the image to the memory.
 */

void PaintProcedure()
{
    Image * image;
    unsigned char * p;
    int x, y;
    
    image = ImageNew(600, 400);

    for (y = 0; y < image->height; y++) {
        for (x = 0; x < image->width; x++) {
            p = ImagePixel(image, x, y);
            p[0] = y%256;
            p[1] = (x+y+x*(1000-x)/500)%256;
            p[2] = x%256;
        }
    }

    ImageToMemory(image);
}


