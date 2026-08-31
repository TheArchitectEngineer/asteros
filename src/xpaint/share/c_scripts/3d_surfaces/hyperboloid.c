/* XPaint-image */

#include <Xpaint.h>
#include <Xpaint3d.h>

/* Define and Plot an hyperboloid */

void hyperboloid(point p, double x, double y)
{
double expy, coshy, sinhy;

   expy = exp(y) ;
   coshy = (expy+1/expy)/2; sinhy = (expy-1/expy)/2;

   p[0] = cos(x) * coshy;
   p[1] = sin(x) * coshy;
   p[2] = sinhy ;
}

Image * ImageCreate()
{
   Image * image;
   unsigned char * p;
   int x, y, i, j;

   width = 640;
   height = 480;

   x_1 = -4; x_2 = 4;
   y_1= -3; y_2 = 3;
   z_1 = -3; z_2 = 3;

   light_x = -0.3; light_y = -0.5; light_z = -1.1;

   create_3d_buffer();

   rotation_angles(0,45,23);
   surface(hyperboloid, 0, 2*M_PI, 64, -1.1, 1.1, 24);

   image = ImageNew(width, height);

   for (y = 0; y < height; y++) {
       for (x = 0; x < width; x++) {
           p = ImagePixel(image, x, y);
           i = x+y*width;
           if (luminosity[i]) {
              p[0] = luminosity[i]>>8;
              p[1] = p[0]/3;
              p[2] = p[0]/2;
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

