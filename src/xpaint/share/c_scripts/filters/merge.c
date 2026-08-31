/* XPaint-filter */

#include <Xpaint.h>

/* 
 * The key-word "FilterProcess" is reserved for user-defined filter routines;
 * Such a filter processes an "input" image and renders an "output" image.
 * 
 * Pixels are unsigned char arrays p[0]=red, p[1]=green, p[2]=blue
 * (thus each value should be in the range 0..255)
 *
 * In the example below, cp = canvas pixel, ip = input pixel, op = output pixel
 * the procedure merges the input (region) with the underlying canvas image
 */

void FilterProcess(Image * input, Image * output)
{
    Image * canvas;
    unsigned char *cp, *ip, *op;
    int x, y, rx, ry, xc, yc, percent, complement;

    /* merge with 63% of input region intensity */
    percent = 63;
    complement = 100 - percent;

    /* extract image from canvas */
    canvas = CanvasToImage();
    
    /* get region position on canvas */
    rx = RegionX();
    ry = RegionY();

    for (y = 0; y < input->height; y++) {
        for (x = 0; x < input->width; x++) {
            ip = ImagePixel(input, x, y);
            op = ImagePixel(output, x, y);
            xc = x+rx;
            yc = y+ry;
            if (xc >= 0 && yc >= 0 &&
                xc < canvas->width && yc < canvas->height)
               cp = ImagePixel(canvas, xc, yc);
            else
               cp = ip;
            op[0] = (complement*cp[0]+percent*ip[0])/100;
            op[1] = (complement*cp[1]+percent*ip[1])/100;
            op[2] = (complement*cp[2]+percent*ip[2])/100;
        }
    }

    /* release canvas image buffer which is no longer used */
    ImageDelete(canvas);
}


