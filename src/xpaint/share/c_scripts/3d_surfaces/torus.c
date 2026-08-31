/* XPaint-image */

#include <Xpaint.h>
#include <Xpaint3d.h>

/* Define and plot a Torus */

#define R 1.2
#define r 0.5

void torus(point p, double u, double v)
{
   double a, b;
   a = R + r * cos(v);
   b = r * sin(v);
   p[0] = a * cos(u);
   p[1] = a * sin(u);
   p[2] = b;
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

   rotation_angles(0, 15, -58);
   surface(torus, -M_PI, M_PI, 128, -M_PI, M_PI, 128);

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
