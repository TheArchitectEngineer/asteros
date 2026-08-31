/* XPaint-image */

#include <Xpaint.h>
#include <Xpaint3d.h>

/* Draw heart */

void heart(point p, double x, double y)
{
double a, b, s, t;

   t = fabs(x);
   if (t<M_PI/2) s = 1; else s = -1;

   a = sqrt(1-y*y);
   b = 1 - 0.9 * s * pow( pow(sin(t) , 1-0.18*a*a*t) - 1 , 2);
   p[0] = a * ( b * cos(x) + 1);
   p[1] = 1.2 * a * b * sin(x);
   p[2] = 0.8 * y - 0.3 ;

}

Image * ImageCreate()
{
   Image * image;
   unsigned char * p;
   int x, y, i, j;

   width = 640;
   height = 480;

   x_1 = -2; x_2 = 2;
   y_1= -1.5; y_2 = 1.5;
   z_1 = -1.5; z_2 = 1.5;

   light_x = -0.3; light_y = -0.5; light_z = -1.1;

   create_3d_buffer();

   rotation_angles(90, 0, 0);
   translation(0, -0.5, 0);

   surface(heart, -M_PI, M_PI, 300, -1, 1, 120);

   image = ImageNew(width, height);

   for (y = 0; y < height; y++) {
       for (x = 0; x < width; x++) {
           p = ImagePixel(image, x, y);
           i = x+y*width;
           if (luminosity[i]) {
              p[0] = luminosity[i]>>8;
              p[1] = p[0];
              p[2] = p[0]/3;
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
