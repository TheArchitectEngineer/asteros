/* XPaint-image */

#include <Xpaint.h>
#include <Xpaint3d.h>

/* Sinusoidal circular waves */

void waves(point p, double x, double y)
{
   p[0] = x;
   p[1] = y;
   p[2] = -0.1*cos(sqrt(x*x+y*y)*15)/(1+x*x+y*y);
}

Image * ImageCreate()
{
   Image * image;
   unsigned char * p;
   int x, y, i, j;

   width = 640;
   height = 480;

   x_1 = -2.4; x_2 = 2.4;
   y_1= -1.8; y_2 = 1.8;
   z_1 = -1.8; z_2 = 1.8;

   light_x = -0.3; light_y = -0.5; light_z = -1.1;

   create_3d_buffer();

   rotation_angles(0, 20, -60);
   surface(waves, -2.8, 2.8, 250, -2.8, 2.8, 250);
   
   image = ImageNew(width, height);

   for (y = 0; y < height; y++) {
       for (x = 0; x < width; x++) {
           p = ImagePixel(image, x, y);
           i = x+y*width;
           if (luminosity[i]) {
              p[2] = luminosity[i]>>8;
              p[0] = (3*p[2]+125)/4;
              p[1] = (2*p[2]+205)/3;
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
