/* XPaint-image */
#include <Xpaint.h>

/*
 * The key-word "ImageCreate" is reserved for user-defined image routines;
 *
 * Pixels are unsigned char arrays p[0]=red, p[1]=green, p[2]=blue
 * (thus each value should be in the range 0..255)
 *
 * In the example below,  ip = input pixel,  op = output pixel 
 * The procedure below opens two images from files on the disk
 * and creates a new image containing both.
 */

Image * ImageCreate()
{
    Image *image[2], *input, *output;
    unsigned char *ip, *ipp, *op;
    int x, y, w, h, wp, hp, a;

    /* Open files */
    image[0] = ImageFromFile(ArchiveFile("file.png"));
    image[1] = ImageFromFile(ArchiveFile("XPaintIcon.xpm"));

    a = 80;
    w = image[0]->width;
    h = image[0]->height;
    wp = image[1]->width;
    hp = image[1]->height;
    output = ImageNew(w, h);

    /* Merge image[0] and image[1], using image[1] as a  translucent stamp */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            ip = ImagePixel(image[0], x, y);
            op = ImagePixel(output, x, y);
            if (x < wp && y < hp) {
                ipp = ImagePixel(image[1], x, y);
	        op[0] = (a*ip[0]+(100-a)*ipp[0])/100;
	        op[1] = (a*ip[1]+(100-a)*ipp[1])/100;
	        op[2] = (a*ip[2]+(100-a)*ipp[2])/100;
	    } else
	        memcpy(op, ip, 3);
	}
    }
    /* Free images from memory since we are done with them */
    ImageDelete(image[0]);
    ImageDelete(image[1]);

    return output;
}

