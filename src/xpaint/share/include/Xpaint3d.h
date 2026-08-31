#define COLORDEPTH 255
#define DEPTHEXTEN 32760

typedef double point[3];

typedef void (*BFunction)();
typedef double (*ZFunction)(double x, double y);
typedef void  (*SFunction)(point p, double x, double y);
typedef void  (*CFunction)(point p, double x);

const double CONV_RAD=M_PI/180;
int do_init = 1, orient = 1;
int width, height;

double x_1,x_2,y_1,y_2,z_1,z_2;
double ratio_x, ratio_y, ratio_z;
double light_x, light_y, light_z;
double m_rot[12]={1,0,0,0,0,1,0,0,0,0,1,0};

short int *z_depth = NULL;
short int *luminosity = NULL;

double max(double x, double y)
{
   if(x>=y) return x; else return y;
}

double min(double x, double y)
{
   if(x>=y) return x; else return y;
}

void setcolor(char *pix, char *val) 
{ 
   memcpy(pix, val, 3);
}

void create_3d_buffer()
{
   z_depth = (short int *)malloc(width*height*sizeof(short int));   
   luminosity = (short int *)malloc(width*height*sizeof(short int));   
}

void free_3d_buffer()
{
   if (z_depth) free(z_depth);
   if (luminosity) free(luminosity);
   z_depth = NULL;
   luminosity = NULL;
}

void init_3d_buffer()
{
   int i, j, k;
   double light;

   if (do_init) {
      for (j=0; j<height; j++) 
         for (i=0; i<width; i++) {
	    k = i+j*width;
            z_depth[k] = -DEPTHEXTEN;
	    luminosity[k] = 0;
	 }

      light = sqrt(light_x*light_x + light_y*light_y + light_z*light_z);
      if (light>0) {
         light_x = light_x/light;
         light_y = light_y/light;
         light_z = light_z/light;
      }
      ratio_x = (x_2-x_1)/(double)width;
      ratio_y = (y_2-y_1)/(double)height;
      if(ratio_x==0 || ratio_y==0) exit(1);
      ratio_z = z_2-z_1;
      if (ratio_z<=0) exit(1); else ratio_z = ((double)DEPTHEXTEN)/ratio_z;
      do_init = 0;
   }
}

void translation(double a, double b, double c)
{
   m_rot[3] = a;   
   m_rot[7] = b;   
   m_rot[11] = c;
}

void scale(double a)
{
   int i;
   for (i=0; i<=11; i++) m_rot[i] *= a;   
}

void rotation_angles(double theta, double phi, double psi)
{
   double ca,cb,cc, sa,sb,sc;

   ca=theta*CONV_RAD;
   cb=phi*CONV_RAD;
   cc=psi*CONV_RAD;

   sa=sin(ca); ca=cos(ca);
   sb=sin(cb); cb=cos(cb);
   sc=sin(cc); cc=cos(cc);

   m_rot[0] = ca*cb;
   m_rot[1] = -sa*cc-ca*sb*sc;
   m_rot[2] = sa*sc-ca*sb*cc;

   m_rot[4] = sa*cb;
   m_rot[5] = ca*cc-sa*sb*sc;
   m_rot[6] = -ca*sc-sa*sb*cc;

   m_rot[8] = sb;
   m_rot[9] = cb*sc;
   m_rot[10]= cb*cc;
}

void rotate(point p, point q)
{
   int i,j;
   for (i=0; i<=2; i++) {
      j=4*i;
      q[i] = m_rot[j]*p[0]+m_rot[j+1]*p[1]+m_rot[j+2]*p[2]+m_rot[j+3];
   }
}

void setpoint(point p, point q)
{
   int i;
   for(i=0; i<=2; i++) p[i] = q[i];
}

void mesh(point *p, int h, int i, int j)
{
   point a[3], b, c;
   double ca,cb,cc,cd, s,t, adds,addt, x, y, z, z0,z1,z2;
   double delta_x, delta_y, delta_z, delta;
   int u1, u2, v1, v2, col;
   int k,l,m,n;

   init_3d_buffer();
   rotate(p[h],a[0]);
   rotate(p[i],a[1]);
   rotate(p[j],a[2]);

   u1=width; u2=-1;
   v1=height; v2=-1;

   for(k=0; k<=2; k++) {
     l=(int)((a[k][0]-x_1)/ratio_x);
     if(l<u1) u1=l;
     if(l>u2) u2=l;
     l=(int)((y_2-a[k][1])/ratio_y);
     if(l<v1) v1=l;
     if(l>v2) v2=l;
   }

   if(u1<0) u1=0;
   if(u2>=width) u2=width-1;
   if(v1<0) v1=0;
   if(v2>=height) v2=height-1;
   if(u1>u2 || v1>v2) return;
   z0 = a[0][2];
   z1 = a[1][2]-z0;
   z2 = a[2][2]-z0;
   z0 = (z0-z_1)*ratio_z;
   z1 = z1*ratio_z;
   z2 = z2*ratio_z;
   for (k=0; k<=2; k++) {
      b[k] = a[1][k] - a[0][k];
      c[k] = a[2][k] - a[0][k];
   }
   delta_x = b[1]*c[2] - b[2]*c[1];
   delta_y = -b[0]*c[2] + b[2]*c[0];
   delta_z = b[0]*c[1] - b[1]*c[0];

   if (fabs(delta_z)<1E-5) return;
    
   delta = 1/delta_z;

   ca = c[1] * delta;
   cb = -c[0] * delta;
   cc = -b[1] * delta;
   cd = b[0] * delta;

   adds = ca*ratio_x;
   addt = cc*ratio_x;

   delta = -1/(sqrt(delta_x*delta_x + delta_y*delta_y + delta_z*delta_z));
   delta_x = delta_x * delta;
   delta_y = delta_y * delta;
   delta_z = delta_z * delta;

   if (orient<0) {
      /*
      for (k=0; k<=2; k++)
         c[k] = (a[0][k] + a[1][k] + a[2][k])/3.0 - m_rot[3+4*k];
      delta = c[0] * delta_x + c[1] * delta_y + c[2] * delta_z;
      if (delta>0) printf("+"); else printf("-");
      if (delta<0) {
	 delta_x = -delta_x;
	 delta_y = -delta_y;
	 delta_z = -delta_z;
      }
      */
      delta_x = -delta_x;
      delta_y = -delta_y;
      delta_z = -delta_z; 
   }
   col = 32767 * 
         (1.000001 + light_x * delta_x + light_y*delta_y + light_z * delta_z);

   for(l=v1; l<=v2; l++) {
      x = x_1 + u1*ratio_x - a[0][0];
      y = y_2 - l*ratio_y - a[0][1];
      s = ca*x + cb*y;
      t = cc*x + cd*y;
      for(k=u1; k<=u2; k++) {
        if (s>=-0.0001 && t>=-0.0001 && s+t<=1.0001) {
          m = (int) (z0 + s*z1 + t*z2);
          if (m<-DEPTHEXTEN) m=-DEPTHEXTEN;
          if (m>DEPTHEXTEN) m=DEPTHEXTEN;
	  n = k+l*width;
          if (m>z_depth[n]) {
	     z_depth[n] = m;
	     luminosity[n] = col;
	  }
	}
        s = s + adds;
        t = t + addt;
      }
   }
}

void build(BFunction bfunct)
{
   init_3d_buffer();
   bfunct();
}

void light(ZFunction zfunct)
{
int i, j;
int k, l;
double x,y;

   init_3d_buffer();

   for(j=0; j<height; j++) {
      y = y_2-ratio_y*j;
      for (i=0; i<width; i++) {
         x = ratio_x*i+x_1;
         k = (int)((zfunct(x,y)-z_1)*ratio_z);
         if (k<0) k=0;
         if (k>DEPTHEXTEN) k=DEPTHEXTEN;
	 l = i+j*width;
         if (k>z_depth[l]) z_depth[l] = k;
      }
   }
}

void surface(SFunction sfunct, double xi, double xs, int nx, 
                               double yi, double ys, int ny)
{
int i, j, l;
double hx,hy;
point p[4];

   if (nx<=0 || ny<=0) return;

   init_3d_buffer();

   hx = (xs-xi)/nx;
   hy = (ys-yi)/ny;

   for (j=0; j<ny; j++) {
      for (l=0; l<=1; l++)
         sfunct(p[l],xi,yi+(j+l)*hy);

      for (i=1; i<=nx; i++) {
         setpoint(p[2],p[0]); setpoint(p[3],p[1]); 
         for (l=0; l<=1; l++)
            sfunct(p[l], xi+i*hx,yi+(j+l)*hy);
         mesh(p,0,1,2); mesh(p,1,3,2);
      }
   }
}

void segment(point *p)
{
   int i, u, x0, y0, x1, y1, z, delta;   
   int nx, ny, dx, dy, dxp, dyp, min, max;
   double z0, dz;
   point a[2];

   init_3d_buffer();

   for (i=0; i<=1; i++)
      rotate(p[i], a[i]);

   x0 = (int)((a[0][0] - x_1)/ratio_x);
   y0 = (int)((a[0][1] - y_1)/ratio_y);
   x1 = (int)((a[1][0] - x_1)/ratio_x);
   y1 = (int)((a[1][1] - y_1)/ratio_y);
   dz = ((double)DEPTHEXTEN)/(z_2-z_1);
   z0 = (a[0][2] - z_1)*dz;
   dz = (a[1][2] - a[0][2])*dz;

   nx = x1-x0;
   dxp = 1;
   if (nx<0)  {
     dxp = -1;
     nx = -nx;
   }
   ny = y1-y0;
   dyp = 1;
   if (ny<0)  {
     dyp = -1;
     ny = -ny;     
   }
   if (nx>ny) {
      min = ny;
      max = nx;
      dx = dxp;
      dy = 0;
   } else {
      min = nx;
      max = ny;
      dx = 0;
      dy = dyp;
   }

   dz = dz/((double)max);

   delta = max/2 - max;
   for (i=0; i<=max; i++) {
      u = x0+width*y0;
      if (x0>=0 && x0<width && y0>=0 && y0<height) {
         u = x0+width*y0;
         z = (int) (z0 + ((double)i) * dz);
	 if (z>z_depth[u]) {
            luminosity[u] = 1;
	    z_depth[u] = z;
	 }
      }
      delta += min;
      if (delta>=max/2) {
	 delta -= max;
	 x0 += dxp;
	 y0 += dyp;
      } else {
         x0 += dx;
	 y0 += dy;
      }      
   }
}

void curve(CFunction cfunct, double ti, double ts, int n)
{
int i, l;
double h;
point p[2];

   if (n<=0) return;

   init_3d_buffer();

   h = (ts-ti)/n;

   for (i=0; i<=n; i++) {
       if (i>=1) setpoint(p[0], p[1]);
       cfunct(p[1], ti+i*h);
       if (i>=1) segment(p);
   }
}


