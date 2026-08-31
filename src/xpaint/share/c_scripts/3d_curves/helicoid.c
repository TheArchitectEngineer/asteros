/* XPaint-image */

#include <Xpaint.h>
#include <Xpaint3d.h>

/* Define and Plot an helicoid */

void helicoid(point p, double x)
{
   double r;
   r = exp(-0.015*x);
   p[0] = r * cos(x);
   p[1] = r * sin(x);
   p[2] = 0.01*x ;
}

Image * ImageCreate()
{
   Image * image;
   unsigned char * p;
   int x, y, i, j;

   width = 640;
   height = 480;

   x_1 = -2; x_2 = 2;
   y_1 = -2; y_2 = 2;
   z_1 = -1; z_2 = 4;

   create_3d_buffer();

   rotation_angles(0, 10, 30);
   curve(helicoid, 0, 100*M_PI, 4000);

   image = ImageNew(width, height);

   for (y = 0; y < height; y++) {
       for (x = 0; x < width; x++) {
           p = ImagePixel(image, x, y);
           i = x+y*width;
           if (luminosity[i]) {
	      /* color varies according to z coordinate -- why not ? */
              p[0] = 200-z_depth[i]/150;
              p[1] = 85;
              p[2] = 30+z_depth[i]/150;
           } else {
              p[0] = 255;
              p[1] = 255;
              p[2] = 255;
           }
       }
   }
   
   free_3d_buffer();
   return image;
}

