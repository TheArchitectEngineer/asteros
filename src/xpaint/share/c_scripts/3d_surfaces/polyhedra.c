/* Xpaint-image */

#include <Xpaint.h>
#include <Xpaint3d.h>

/* Draw regular polyhedra in 3-space */

void tetrahedron()
{
   point p[4] = { {1,1, 1}, {-1,-1, 1}, {-1,1,-1}, {1,-1,-1} };

   mesh(p,0,2,1); 
   mesh(p,0,1,3);
   mesh(p,0,3,2);
   mesh(p,1,2,3);
}

void cube()
{
   point p[8] = { {1,1, 1}, {-1,1, 1}, {1,-1, 1}, {-1,-1, 1},
                  {1,1,-1}, {-1,1,-1}, {1,-1,-1}, {-1,-1,-1} };

   mesh(p,0,1,2); mesh(p,1,3,2);
   mesh(p,4,6,5); mesh(p,5,6,7);
   mesh(p,0,2,4); mesh(p,2,6,4);
   mesh(p,1,5,3); mesh(p,3,5,7);
   mesh(p,0,4,1); mesh(p,1,4,5);
   mesh(p,2,3,6); mesh(p,3,7,6);
}

void octahedron()
{
   point p[6] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };

   mesh(p,0,2,4); 
   mesh(p,0,5,2);
   mesh(p,0,4,3);
   mesh(p,0,3,5);
   mesh(p,1,4,2);
   mesh(p,1,2,5);
   mesh(p,1,3,4);
   mesh(p,1,5,3);
}

#define G 1.61803398875
#define H 0.61803398875

void dodecahedron()
{
point p[20] = { {1,1, 1}, {-1,1, 1}, {1,-1, 1}, {-1,-1, 1},
                {1,1,-1}, {-1,1,-1}, {1,-1,-1}, {-1,-1,-1},
                {0,G,H},  {0,-G,H},  {0,G,-H},  {0,-G,-H},
                {H,0,G},  {H,0,-G},  {-H,0,G},  {-H,0,-G},
                {G,H,0},  {-G,H,0},  {G,-H,0},  {-G,-H,0} };

void face(int a, int b, int c, int d, int e)
{
   mesh(p,a,b,c); mesh(p,a,c,d); mesh(p,a,d,e);
}

   face(0,8,1,14,12);
   face(2,12,14,3,9);
   face(0,12,2,18,16);
   face(3,14,1,17,19);
   face(4,16,18,6,13);
   face(0,16,4,10,8);
   face(2,9,11,6,18);
   face(3,19,7,11,9);
   face(1,8,10,5,17);
   face(5,15,7,19,17);
   face(6,11,7,15,13);
   face(4,13,15,5,10);
}

void icosahedron()
{
point p[12] = { {0,1,G}, {0,-1,G}, {0,1,-G}, {0,-1,-G},
                {G,0,1}, {G,0,-1}, {-G,0,1}, {-G,0,-1},
                {1,G,0}, {-1,G,0}, {1,-G,0}, {-1,-G,0} };

   mesh(p,0,1,4); mesh(p,1,0,6); mesh(p,3,2,5); mesh(p,2,3,7);
   mesh(p,4,5,8); mesh(p,5,4,10); mesh(p,7,6,9); mesh(p,6,7,11);
   mesh(p,8,9,0); mesh(p,9,8,2); mesh(p,11,10,1); mesh(p,10,11,3);
   mesh(p,0,4,8); mesh(p,6,0,9); mesh(p,4,1,10); mesh(p,1,6,11); 
   mesh(p,5,2,8); mesh(p,2,7,9); mesh(p,3,5,10); mesh(p,7,3,11);
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

   rotation_angles(32,27,12); 

   translation(-2,1,0);
   build(dodecahedron);

   translation(2, 1, 0);
   build(icosahedron);

   translation(-2, -1, 0);
   build(tetrahedron);

   translation(0, -1, 0);
   build(cube);

   translation(1.8, -1.4, 0);
   build(octahedron);

   image = ImageNew(width, height);

   for (y = 0; y < height; y++) {
       for (x = 0; x < width; x++) {
           p = ImagePixel(image, x, y);
           i = x+y*width;
           if (luminosity[i]) {
              p[0] = luminosity[i]>>8;
              p[1] = p[0]/2;
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
