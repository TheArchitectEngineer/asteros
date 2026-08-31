/* XPaint-image */

#include <Xpaint.h>
#include <Xpaint3d.h>

/* Define and plot an Ellipsoid of 1/2-axes A, B, C */

double A, B, C;

void ellipsoid(point p, double u, double v)
{
   double cv = cos(v);
   p[0] = A * cos(u) * cv;
   p[1] = B * sin(u) * cv;
   p[2] = C * sin(v);
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

   rotation_angles(0,45,23);
   translation(0, 0.1, 0);
   A = 2.2;  B= 0.5;  C = 1.1;

   surface(ellipsoid, -M_PI, M_PI, 160, -M_PI/2, M_PI/2, 80);

   image = ImageNew(width, height);
   for (y = 0; y < height; y++) {
       for (x = 0; x < width; x++) {
           p = ImagePixel(image, x, y);
           i = x+y*width;
           if (luminosity[i]) {
              p[1] = luminosity[i]>>8;
              p[0] = p[1]/3;
              p[2] = p[1]/4;
           } else {
              p[0] = 255;
              p[1] = 255;
              p[2] = 255;
           }
       }
   }


   translation(0, -0.1, 0);
   A = B= C = 1.0; 

   for (i = 0; i < width*height; i++)  luminosity[i] = -1;
   surface(ellipsoid, -M_PI, M_PI, 160, -M_PI/2, M_PI/2, 80);
   
   for (y = 0; y < height; y++) {
       for (x = 0; x < width; x++) {
           p = ImagePixel(image, x, y);
           i = x+y*width;
           if (luminosity[i]) {
              if (luminosity[i]!=-1) {
                 p[0] = luminosity[i]>>8;
                 p[1] = p[0]/3;
                 p[2] = p[0]/4;
              }     
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
