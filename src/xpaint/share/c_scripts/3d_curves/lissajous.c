/* XPaint-image */

#include <Xpaint.h>
#include <Xpaint3d.h>

/* Define and Plot an helicoid */

void lissajous(point p, double x)
{
   p[0] = cos(3*x);
   p[1] = sin(4*x);
   p[2] = 0 ;
}

Image * ImageCreate()
{
   Image * image;
   unsigned char * p;
   int x, y, i, j;
   char bg[3] = { 255,255,255 };  /* background = white */
   char fg[3] = { 240, 85, 30 };  /* foreground = brick */


   width = 640;
   height = 480;

   x_1 = -1.5; x_2 = 1.5;
   y_1 = -1.2; y_2 = 1.2;

   /* z is always set to 0 ; any interval which contains 0 works */
   z_1 = -1; z_2 = 4;

   create_3d_buffer();

   rotation_angles(0, 0, 0);
   curve(lissajous, 0, 2*M_PI, 1000);

   image = ImageNew(width, height);

   for (y = 0; y < height; y++) {
       for (x = 0; x < width; x++) {
           p = ImagePixel(image, x, y);
           i = x+y*width;
           if (luminosity[i]) setcolor(p, fg); else setcolor(p, bg);
       }
   }
   
   free_3d_buffer();
   return image;
}

