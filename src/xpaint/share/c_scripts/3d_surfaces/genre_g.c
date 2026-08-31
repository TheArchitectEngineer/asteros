/* XPaint-image */

#include <Xpaint.h>
#include <Xpaint3d.h>

/* Define and plot a Torus */

#define R 1.2
#define r 0.5
#define shift 2
#define ratio 1.8
double t;
int n, g;

void genre_g(point p, double u, double v)
{
  double a, b, u0, u0p, u1, u2;
   a = R + r * cos(v);
   b = r * sin(v);

   u0 = (2*a-shift*R)/(2*a+shift*R);
   if (u0>0) u0 = 2.0*atan(sqrt(u0)); else u0 = 0.0;
   u0p = ratio*u0;
   u1 = u2 = u;
   if (n>0 && fabs(u)>M_PI-u0p) {
     u1 = 0.4*(M_PI-u0p)+0.6*fabs(u);
     if (u1>M_PI-u0) u1 = M_PI-u0;
     u2 = M_PI-u0p-u1;
     u2 = u1 - u2*u2/u0p;
   }
   if (n<g-1 && fabs(u)<u0p) {
     u1 = 0.4*u0p+0.6*fabs(u);
     if (u1<u0) u1 = u0;
     u2 = u0p-u1;
     u2 = u1+u2*u2/u0p;
   }
   if (u<0 && u2>=0) u2 = -u2;
   p[0] = a * cos(u1) + n*shift*R;
   p[1] = a * sin(u2);
   p[2] = b;
}

Image * ImageCreate()
{
   Image * image;
   unsigned char * p;
   int x, y, i, j;
   
   g = 2;
   width = 1024;
   height = 768/g;

   x_1 = -(R+1.5*r); x_2 = shift*(g-1)*R + (R+1.5*r);
   y_1= -1.8; y_2 = 1.8;
   z_1 = -1.8; z_2 = 1.8;

   light_x = -0.3; light_y = -0.5; light_z = -1.1;

   create_3d_buffer();

   rotation_angles(0, 0, -52);
   for (n=0; n<=g-1; n++)
      surface(genre_g, -M_PI, M_PI, 128, -M_PI, M_PI, 128);

   image = ImageNew(width, height);

   for (y = 0; y < height; y++) {
       for (x = 0; x < width; x++) {
           p = ImagePixel(image, x, y);
           i = x+y*width;
           if (luminosity[i]) {
              p[1] = luminosity[i]>>8;
              p[0] = p[1]/3;
              p[2] = p[1]/2;
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
