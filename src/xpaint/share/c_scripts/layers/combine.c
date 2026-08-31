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
    unsigned char *ip, *op;
    int x, y, dx, width, height;

    /* Open files */
    image[0] = ImageFromFile("/usr/share/xpaint/XPaintIcon.xpm");
    image[1] = ImageFromFile("/usr/share/xpaint/c_scripts/3d_surfaces/ellipsoid.c");

    if (!image[0] || !image[1]) {
       fprintf(stderr, "Cannot find XPaintIcon.xpm or penguin.png !!\n");
       return NULL;
    }

    /* Create output image */
    width = image[0]->width + image[1]->width + 10;
    height = image[0]->height;
    if (height < image[1]->height) height = image[1]->height;
    output = ImageNew(width, height);
  
    /* Initialize output with pale pink */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            op = ImagePixel(output, x, y);
	    op[0] = 255;
	    op[1] = 240;
	    op[2] = 240;
	}
    }
    
    /* Copy image[0] into output */
    input = image[0];
    for (y = 0; y < input->height; y++) {
        for (x = 0; x < input->width; x++) {
            ip = ImagePixel(input, x, y);
            op = ImagePixel(output, x, y);
            memcpy(op, ip, 3);
        }
    }

    /* Copy image[1] into output */
    input = image[1];
    dx = image[0]->width + 10;
    for (y = 0; y < input->height; y++) {
        for (x = 0; x < input->width; x++) {
            ip = ImagePixel(input, x, y);
            op = ImagePixel(output, x+dx, y);
            memcpy(op, ip, 3);
        }
    }

    /* Copy image[0] somewhere over image[1] */
    input = image[0];
    for (y = 0; y < input->height; y++) {
        for (x = 0; x < input->width; x++) {
            ip = ImagePixel(input, x, y);
            op = ImagePixel(output, x+300, y+100);
            memcpy(op, ip, 3);
        }
    }

    /* Another translucent copy of  image[0] somewhere */
    input = image[0];
    for (y = 0; y < input->height; y++) {
        for (x = 0; x < input->width; x++) {
            ip = ImagePixel(input, x, y);
            op = ImagePixel(output, x+450, y+180);
            op[0] = (4*op[0] + ip[0])/5;
            op[1] = (4*op[1] + ip[1])/5;
            op[2] = (4*op[2] + ip[2])/5;
        }
    }

    /* Free images from memory since we are done with them */
    ImageDelete(image[0]);
    ImageDelete(image[1]);

    return output;
}


