/* XPaint-image */

#include <Xpaint.h>
#include <Xpaint3d.h>

/* Draw trefoil knot */

double A, B, C;

/*
   Tube of radius C around the parametric "trefoil curve"
   t=0..2*pi --> [ cos(2t)/(A+1-Acos(3t)), sin(2t)/(A+1-Acos(3t)), Bsin(3t) ]
   The tube is computed by taking the normal plane to the curve at each point, 
   and by drawing a circle of radius C in this normal plane...
*/

void trefoil(point p, double x, double y)
{
double c1,s1, c2,s2, c3,s3, cy, sy, r, rho;
double alpha, beta, gamma;
point v,w, vv, ww;

   /* cosines and sines of x, 2*x, 3*x */
   c1 = cos(x); s1 = sin(x);
   c2 = c1*c1 -s1*s1 ; s2 = 2*s1*c1;
   c3 = c1*c2-s1*s2 ; s3= s1*c2+c1*s2;
   rho = 1.0/(A+1.0 - A*c3);

   /* tangent vector in cylindric coordinates */
   alpha = -3*A*s3*rho*rho;
   beta  = 2*rho;
   gamma = 3*B*c3;

   /* compute orthonormal basis of normal plane */
   /* first vector v */
   v[0] = -beta;   v[1] = alpha;
   w[2] = v[0]*v[0]+v[1]*v[1];
   r = 1.0/sqrt(w[2]);
   v[0] = r*v[0];   v[1] = r*v[1];
  
   /* second vector w */
   w[0] = -alpha*gamma;   w[1] = -beta*gamma;
   r = 1.0/sqrt(w[0]*w[0]+w[1]*w[1]+w[2]*w[2]);
   w[0] = r*w[0];   w[1] = r*w[1];  ww[2] = r*w[2];

   /* return from cylindric to cartesian coordinates */ 
   /* denote those new column vectors by vv, ww */
   vv[0] = v[0]*c2-v[1]*s2;   vv[1] = v[0]*s2+v[1]*c2;
   ww[0] = w[0]*c2-w[1]*s2;   ww[1] = w[0]*s2+w[1]*c2;

   /* plot circle in normal plane */
   cy = C*cos(y);   sy = C*sin(y);
   p[0] = rho*c2 + cy*vv[0] + sy*ww[0];
   p[1] = rho*s2 + cy*vv[1] + sy*ww[1];
   p[2] = B*s3 + sy*ww[2];
}

Image * ImageCreate()
{
   Image * image;
   unsigned char * p;
   int x, y, i, j;

   width = 640;
   height = 480;

   x_1 = -1.6; x_2 = 1.6;
   y_1= -1.2; y_2 = 1.2;
   z_1 = -1.2; z_2 = 1.2;

   light_x = -0.3; light_y = -0.5; light_z = -1.1;

   create_3d_buffer();

   rotation_angles(0, 0, 0);

   /* Draw trefoil knot with suitable parameters */
   orient = -1;
   A = 1; B = 0.6; C = 0.15;
   surface(trefoil, 0, 2*M_PI, 256, 0, 2*M_PI, 64);

   image = ImageNew(width, height);

   for (y = 0; y < height; y++) {
       for (x = 0; x < width; x++) {
           p = ImagePixel(image, x, y);
           i = x+y*width;
           if (luminosity[i]) {
              p[0] = luminosity[i]>>8;
              p[1] = p[0]/3;
              p[2] = p[0];
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
