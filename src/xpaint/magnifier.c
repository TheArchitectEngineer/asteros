   /*-
    * Copyright (c) 1999/2000/2001 Thomas Runge (runge@rostock.zgdv.de)
    * All rights reserved.
    *
    * Redistribution and use in source and binary forms, with or without
    * modification, are permitted provided that the following conditions
    * are met:
    * 1. Redistributions of source code must retain the above copyright
    *    notice, this list of conditions and the following disclaimer.
    * 2. Redistributions in binary form must reproduce the above copyright
    *    notice, this list of conditions and the following disclaimer in the
    *    documentation and/or other materials provided with the distribution.
    *
    * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
    * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
    * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    * SUCH DAMAGE.
    *
    */

#define MAXZOOM 50
#define DEBUG 0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef linux
#include <getopt.h>
#endif /* linux */
#include <math.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/Shell.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/WinUtil.h>
#include "xaw_incdir/Form.h"
#include "xaw_incdir/Simple.h"
#include "xaw_incdir/Command.h"
#include "xaw_incdir/Toggle.h"

#ifdef NeedWidePrototypes
#undef NeedWidePrototypes
#define XAW_HACK
#endif
#include "xaw_incdir/Scrollbar.h"
#ifdef XAW_HACK
#define NeedWidePrototypes
#endif

#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include "xpaint.h"
#include "graphic.h"
#include "image.h"
#include "misc.h"
#include "protocol.h"

/* static char const id[] = "$Id: xlupe Version 1.2 by Thomas Runge (runge@rostock.zgdv.de) $"; */

#define APPNAME "xpaint"
#define APPCLASS "XPaint"

#define NEW_WIN True
#define OLD_WIN False

int magnifier_closing_down = 0;

static Widget shell=None, drawW, formW, sliderW;
static Widget rotateB, symmetryB, zoomB, freezeB, jumpB, infoB;
static Widget canvasB, memoryB, quitB;
static XImage* image;
static XtAppContext app_con;

static char *app_title;

static unsigned int position = 0, position1 = 0, position2 = 0, position3 = 0;
static unsigned int pcounter = 0;
static unsigned int psequence[] = { 0, 6, 3, 5 };   

static unsigned int width, height, zoomx, zoomy;
static unsigned int zoomflags = 3;
static int winheight, winwidth;
static int dpyheight, dpywidth;
static unsigned int LUPEWIDTH = 480;
static unsigned int LUPEHEIGHT = 360;
static int lupe_x = 0, lupe_y = 0, init = 0;

static int N0, N1;
static Display *dpy;
static GC gc, ovlgc;
static XtWorkProcId wpid = None;
#if DEBUG
static XtIntervalId counterId = None;
#endif
static XtIntervalId limitId = None;
static Window rwin, window;
static Boolean ready, frozen, old_frozen, smooth, have_shm, changed;
static Pixel bg_pixel;
static XShmSegmentInfo shminfo;
static unsigned long limit;
static int counter;
static int ErrorFlag;

typedef struct
{
 char *winname;
 char *vistype;
 int depth;
 Visual *new_visual;
 Visual *this_visual;
 Colormap cmap;
 Window win;
 int x;
 int y;   
 int width;
 int height;
} visinfo;
visinfo vinf, definf;

typedef struct
{
 Dimension width, height;
 Position x, y;
 Boolean set;
} win_attr;
win_attr old_win;

int CreateInterface();
void MakeAthenaInterface();
void MakeMotifInterface();
void PrepareForJump();
void GetWinAttribs();
int HandleXError(Display*, XErrorEvent*);
void CalcTables();
int check_for_xshm();
XImage *alloc_xshm_image(int, int, int);
void destroy_xshm_image(XImage*);
void zoomCB(Widget, XtPointer, XtPointer);
void rotateCB(Widget, XtPointer, XtPointer);
void symmetryCB(Widget, XtPointer, XtPointer);
void freezeCB(Widget, XtPointer, XtPointer);
void infoCB(Widget, XtPointer, XtPointer);
void memoryCB(Widget, XtPointer, XtPointer);
void canvasCB(Widget, XtPointer, XtPointer);
void quitCB(Widget, XtPointer, XtPointer);
void sliderCB(Widget, XtPointer, XtPointer);
void sliderScrollCB(Widget, XtPointer, XtPointer);
Boolean drawCB(XtPointer);
void frozenDraw();
void counterCB(XtPointer, XtIntervalId*);
void limitCB(XtPointer, XtIntervalId*);
void setXYtitle();
void resizeCB(Widget, XtPointer, XEvent*);
void exposeCB(Widget, XtPointer, XEvent*);
void zoom_8(XImage *newimage, XImage *image, int width, int height);
void zoom_normal(XImage *newimage, XImage *image, int width, int height);
void zoom_smooth(XImage *newimage, XImage *image, int width, int height);
void usage(char *progname);
Window Select_Window();
int FillVisinfo(Window window);

void pix2rgb(unsigned char *pix, unsigned char *rgb, int depth)
{
   
 if (depth>=24)
 {
  memcpy(rgb, pix, 3);
  return;  
 }
 if (depth==16)
 {
  rgb[0] = ((pix[N1]>>3)*255)/31;
  rgb[1] = ((((pix[N1]&7)<<3)|(pix[N0]>>5))*255)/63;
  rgb[2] = ((pix[N0]&31)*255)/31;
  return;  
 }   
 if (depth==15)
 {
  rgb[0] = ((pix[N1]>>2)*255)/31;
  rgb[1] = ((((pix[N1]&3)<<3)|(pix[N0]>>5))*255)/31;
  rgb[2] = ((pix[N0]&31)*255)/31;    
  return;  
 }   
}

void rgb2pix(unsigned char *rgb, unsigned char *pix, int depth)
{
 if (depth>=24)
 {
  memcpy(pix, rgb, 3);
  return;  
 }
 if (depth==16)
 {
  pix[N0] = (rgb[2]&248)>>3 | (rgb[1]&28)<<3;
  pix[N1] = (rgb[1]&224)>>5 | (rgb[0]&248);        
  return;  
 }
 if (depth==15)
 {
  pix[N0] = (rgb[2]&248)>>3 | (rgb[1]&56)<<2;
  pix[N1] = (rgb[1]&192)>>6 | (rgb[0]&248)>>1;    
  return;  
 }
}

void quitCB(Widget widget, XtPointer clientData, XtPointer callData)
{
  magnifier_closing_down = 1;
  PrepareForJump();
}

void memoryCB(Widget widget, XtPointer clientData, XtPointer callData)
{
 XImage *xim;
 Image *image;
 Pixmap pix;
 
 xim = XGetImage(dpy, XtWindow(drawW), 
		 0, 0, width, height, AllPlanes, ZPixmap);
 if (!xim) return;
 pix = XCreatePixmap(dpy, XtWindow(Global.toplevel), 
                     xim->width, xim->height, xim->depth);
 if (!pix) {
  XDestroyImage(xim);
  return;
 }
 XPutImage(dpy, pix, gc, xim, 0, 0, 0, 0, xim->width, xim->height);
 image = PixmapToImage(Global.toplevel, pix, vinf.cmap);
 if (!image) {
  XFreePixmap(dpy, pix);
  XDestroyImage(xim);
  return;
 }
 ImageToMemory(image);
}

void canvasCB(Widget widget, XtPointer clientData, XtPointer callData)
{
 XImage *xim;
 Pixmap pix;
 
 xim = XGetImage(dpy, XtWindow(drawW), 
		 0, 0, width, height, AllPlanes, ZPixmap);
 if (!xim) return;
 pix = XCreatePixmap(dpy, XtWindow(Global.toplevel), 
                     xim->width, xim->height, xim->depth);
 if (!pix) {
  XDestroyImage(xim);
  return;
 }
 XPutImage(dpy, pix, gc, xim, 0, 0, 0, 0, xim->width, xim->height);
 graphicCreate(makeGraphicShell(Global.toplevel), xim->width, xim->height, 1,
               pix, None, NULL);
 quitCB(shell, NULL, NULL);
}

int HandleXError(Display *dpy, XErrorEvent *event)
{
 char msg[80];

 XGetErrorText(dpy, event->error_code, msg, 80);
 fprintf(stderr, "Error code %s\n", msg);

 ErrorFlag = 1;
 return 0;
}

/*
 * Check if the X Shared Memory extension is available.
 * Return:  0 = not available
 *          1 = shared XImage support available
 *          2 = shared Pixmap support available also
 */
int check_for_xshm()
{
 int major, minor, ignore;
 Bool pixmaps;

 if(XQueryExtension(dpy, "MIT-SHM", &ignore, &ignore, &ignore))
 {
  if(XShmQueryVersion(dpy, &major, &minor, &pixmaps) == True)
   return(pixmaps==True) ? 2 : 1;
  else
   return 0;
 }
 else
  return 0;
}

/*
 * Allocate a shared memory XImage.
 */
XImage *alloc_xshm_image(int width, int height, int depth)
{
 XImage *img;

 img = XShmCreateImage(dpy, vinf.this_visual, depth, ZPixmap, NULL, &shminfo,
                       width, height);
 if(img == NULL)
 {
#if DEBUG
   printf("XShmCreateImage failed!\n");
#endif
  return NULL;
 }

 shminfo.shmid = shmget(IPC_PRIVATE, img->bytes_per_line * img->height,
                        IPC_CREAT|0777 );
 if(shminfo.shmid < 0)
 {
#if DEBUG
  perror("shmget");
#endif
  XDestroyImage(img);
  img = NULL;
#if DEBUG
  printf("Shared memory error (shmget), disabling.\n");
#endif
  return NULL;
 }

 shminfo.shmaddr = img->data = (char*)shmat(shminfo.shmid, 0, 0);
 if(shminfo.shmaddr == (char *) -1)
 {
#if DEBUG
  perror("alloc_xshm_image");
#endif
  XDestroyImage(img);
  img = NULL;
#if DEBUG
  printf("shmat failed\n");
#endif
  return NULL;
 }

 shminfo.readOnly = False;
 ErrorFlag = 0;
 XShmAttach(dpy, &shminfo);
 XSync(dpy, False);

 if(ErrorFlag)
 {
  /* we are on a remote display, this error is normal, don't print it */
  XFlush(dpy);
  ErrorFlag = 0;
  XDestroyImage(img);
  shmdt(shminfo.shmaddr);
  shmctl(shminfo.shmid, IPC_RMID, 0);
  return NULL;
 }

 shmctl(shminfo.shmid, IPC_RMID, 0); /* nobody else needs it */

 return img;
}

void destroy_xshm_image(XImage *img)
{
 XShmDetach(dpy, &shminfo);
 XDestroyImage(img);
 shmdt(shminfo.shmaddr);
}

static XPoint old_p[5];
void btnDownCB(Widget w_, XtPointer clientData, XEvent *event)
{
 int x, y;
 unsigned int w, h;

 if(event->xbutton.button != Button1)
  return;

 x = event->xbutton.x_root;
 y = event->xbutton.y_root;
 if (position3)
 {
  h = (unsigned int) ((width+zoomx-1) / zoomx);
  w = (unsigned int) ((height+zoomy-1) / zoomy);
 }
 else
 {
  w = (unsigned int) ((width+zoomx-1) / zoomx);
  h = (unsigned int) ((height+zoomy-1) / zoomy);
 }
 w = (w+3)>>1;
 h = (h+3)>>1;
   
 old_p[0].x = x - w;
 old_p[0].y = y - h;
 old_p[1].x = x + w;
 old_p[1].y = old_p[0].y;
 old_p[2].x = old_p[1].x;
 old_p[2].y = y + h;
 old_p[3].x = old_p[0].x;
 old_p[3].y = old_p[2].y;
 old_p[4]   = old_p[0];

 XDrawLines(dpy, vinf.win, ovlgc, old_p, 5, CoordModeOrigin);
}

void hexaConvert(unsigned int num, char *val)
{
  unsigned int i;
  i = num/16;
  val[0] = (i<10)? '0'+i : 'A'+(i-10);
  i = num%16;
  val[1] = (i<10)? '0'+i : 'A'+(i-10);   
}

#ifdef XAW3DXFT
extern XftFont * XftDefaultFont;
#endif

void DrawString(int x, int y, char *str)
{
 XRenderColor xre_color;
 XftColor color;
 XftDraw *draw = NULL;
#ifdef XAW3DXFT
 XftFont * font = XftDefaultFont;
#else
 static XftFont * font = NULL;
 if (!font) font = XftFontOpenName(dpy, DefaultScreen(dpy), "Liberation-8");
#endif

 xre_color.red = 255<<8;
 xre_color.green = 0;
 xre_color.blue = 0;
 xre_color.alpha = 255<<8;
 draw = XftDrawCreate(dpy, window,
                      DefaultVisual(dpy, DefaultScreen(dpy)),
		      definf.cmap);
 XftColorAllocValue(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
		    definf.cmap, &xre_color, &color);
 XftDrawStringUtf8(draw, &color, font, x, y, str, strlen(str));
 XftDrawDestroy(draw);
 if (DefaultDepth(dpy, DefaultScreen(dpy))>8)
     XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
	          definf.cmap, &color);
}

void btnUpCB(Widget w_, XtPointer clientData, XEvent *event)
{
 char buffer[80];
 char value[8];   
 XColor xc;
 XImage *xim;
   
 if(event->xbutton.button == Button2) {
  if (!frozen) return;
  frozenDraw();
 }
   
 if(event->xbutton.button == Button3) {
  if (!frozen) return;
  frozenDraw();
  XFlush(dpy);
  xim = XGetImage (dpy, XtWindow(drawW), event->xbutton.x, event->xbutton.y,
		   1, 1, AllPlanes, ZPixmap);
  xc.pixel = XGetPixel(xim, 0, 0);
  XQueryColor(dpy, vinf.cmap, &xc);
  hexaConvert(xc.red>>8, value);
  hexaConvert(xc.green>>8, value+2);
  hexaConvert(xc.blue>>8, value+4);
  value[6] = '\0';
  sprintf(buffer, " Pixel (%d,%d) color %s",
    (event->xbutton.x<0)? 0:event->xbutton.x/zoomx, 
    (event->xbutton.y<0)? 0:event->xbutton.y/zoomy, value);
  XDestroyImage(xim);
  XSetBackground(dpy, gc, WhitePixelOfScreen(XtScreen(shell)));
  if (event->xbutton.y>LUPEHEIGHT/2)
    DrawString(0, 11, buffer);
  else
    DrawString(0, LUPEHEIGHT-2, buffer);
  return;
 }

 if(event->xbutton.button != Button1)
  return;

 XDrawLines(dpy, vinf.win, ovlgc, old_p, 5, CoordModeOrigin);
 drawCB(clientData);
   
 frozen = False;
 freezeCB(freezeB, clientData, NULL);      
}

void btnMotionCB(Widget w_, XtPointer clientData, XEvent *event)
{
 int x, y;
 unsigned int w, h;
 XPoint p[5];

 x = event->xbutton.x_root;
 y = event->xbutton.y_root;
 if (position3)
 {
  h = (unsigned int) ((width+zoomx-1) / zoomx);
  w = (unsigned int) ((height+zoomy-1) / zoomy);
 }
 else
 {
  w = (unsigned int) ((width+zoomx-1) / zoomx);
  h = (unsigned int) ((height+zoomy-1) / zoomy);
 } 
 w = (w+3)>>1;
 h = (h+3)>>1;
   
 p[0].x = x - w;
 p[0].y = y - h;
 p[1].x = x + w;
 p[1].y = p[0].y;
 p[2].x = p[1].x;
 p[2].y = y + h;
 p[3].x = p[0].x;
 p[3].y = p[2].y;
 p[4]   = p[0];

 XDrawLines(dpy, vinf.win, ovlgc, old_p, 5, CoordModeOrigin);
 XDrawLines(dpy, vinf.win, ovlgc, p, 5, CoordModeOrigin);

 memcpy(&old_p, &p, 5*sizeof(XPoint));
}

void keyReleaseCB(Widget w_, XtPointer clientData, XEvent *event)
{
 KeySym keysym;
 char buffer[2];
 int shift;
   
 if (event->type != KeyRelease) return;
 XLookupString((XKeyEvent *) event, buffer, 1, &keysym, NULL);
 shift = (((XKeyEvent *) event)->state & ControlMask)? 10 : 1;
   
 switch(keysym) {
  case XK_q:
  case XK_Q:
  case XK_Escape:    
    quitCB(shell, NULL, NULL);
    break;
  case XK_equal:
  case XK_KP_Equal:
    zoomflags = 3;
    zoomx = (zoomx + zoomy)/2;
    zoomy = zoomx;
    XawScrollbarSetThumb(sliderW, 1. - (double)zoomx/((double)MAXZOOM), -1);
    XtVaSetValues(zoomB, XtNlabel, "=", NULL);
    setXYtitle();
    frozenDraw();
    break;
  case XK_Left:
    lupe_x -= shift;
    if (lupe_x<0) lupe_x = 0;
    frozenDraw();
    break;    
  case XK_Right:
    lupe_x += shift;
    if (lupe_x>dpywidth) lupe_x = dpywidth;
    frozenDraw();
    break;
  case XK_Up:
    lupe_y -= shift;
    if (lupe_y<0) lupe_y = 0;
    frozenDraw();
    break;    
  case XK_Down:
    lupe_y += shift;
    if (lupe_y>dpyheight) lupe_y = dpyheight;
    frozenDraw();
    break;
  case XK_plus:
  case XK_KP_Add:
    if (zoomflags&1)
     ++zoomx;
    if (zoomflags&2)
     ++zoomy;    
    if (zoomx>MAXZOOM) zoomx = MAXZOOM;
    if (zoomy>MAXZOOM) zoomy = MAXZOOM;    
    if (zoomflags&1)
     XawScrollbarSetThumb(sliderW, 1. - (double)zoomx/((double)MAXZOOM), -1);
    else
     XawScrollbarSetThumb(sliderW, 1. - (double)zoomy/((double)MAXZOOM), -1);
    setXYtitle();    
    frozenDraw();
    break;
  case XK_minus:
  case XK_KP_Subtract:
    if (zoomflags&1)    
     --zoomx;
    if (zoomflags&2)    
     --zoomy;    
    if (zoomx<1) zoomx = 1;
    if (zoomy<1) zoomy = 1;    
    if (zoomflags&1)
     XawScrollbarSetThumb(sliderW, 1. - (double)zoomx/((double)MAXZOOM), -1);
    else
     XawScrollbarSetThumb(sliderW, 1. - (double)zoomy/((double)MAXZOOM), -1);
    setXYtitle();    
    frozenDraw();    
    break;
  case XK_c:
  case XK_C:
    canvasCB(canvasB, NULL, NULL);
    break;
  case XK_m:
  case XK_M:
    memoryCB(memoryB, NULL, NULL);
    break;
  case XK_t:
  case XK_T:    
    position1 = 1-position1;
    position = (position&6) | position1;
    XtVaSetValues(symmetryB, XtNstate, position1, NULL);
    frozenDraw();
    break;
  case XK_r:
  case XK_R:
    pcounter = (pcounter+1)%4;
    position = psequence[pcounter];
    position1 = (position&1);
    position2 = (position&2)>>1;
    position3 = (position&4)>>2;
    frozenDraw();    
    break;    
  case XK_s:
  case XK_S:
    if (definf.depth==8) break;
    smooth = !smooth;
    XtVaSetValues(jumpB, XtNstate, smooth, NULL);    
    frozenDraw();
    break;
  case XK_x:
  case XK_X:
    zoomflags = 1;
    XtVaSetValues(zoomB, XtNlabel, "X", NULL);
    XawScrollbarSetThumb(sliderW, 1. - (double)zoomx/((double)MAXZOOM), -1);
    setXYtitle();    
    break;
  case XK_y:
  case XK_Y:
    zoomflags = 2;
    XtVaSetValues(zoomB, XtNlabel, "Y", NULL);
    XawScrollbarSetThumb(sliderW, 1. - (double)zoomy/((double)MAXZOOM), -1);
    setXYtitle();    
    break;
  case XK_space: 
  case XK_f:     
  case XK_F:         
    freezeCB(freezeB, clientData, NULL); 
    break;
  case XK_i:     
  case XK_I:
    infoCB(infoB, clientData, NULL);
    break;
 }
}

void zoomCB(Widget w, XtPointer clientData, XtPointer callData)
{
 zoomflags = 1 + (zoomflags%3);
 if (zoomflags==1)
 {
  XawScrollbarSetThumb(sliderW, 1. - (double)zoomx/((double)MAXZOOM), -1);          
  XtVaSetValues(zoomB, XtNlabel, "X", NULL);
 }
 if (zoomflags==2)
 {
  XawScrollbarSetThumb(sliderW, 1. - (double)zoomy/((double)MAXZOOM), -1);          
  XtVaSetValues(zoomB, XtNlabel, "Y", NULL);
 }   
 if (zoomflags==3)
 {
  zoomx = (zoomx + zoomy)/2;
  zoomy = zoomx;
  XawScrollbarSetThumb(sliderW, 1. - (double)zoomx/((double)MAXZOOM), -1);          
  XtVaSetValues(zoomB, XtNlabel, "=", NULL);
 }
 setXYtitle();       
}
   
void sliderCB(Widget w, XtPointer clientData, XtPointer callData)
{
 if (zoomflags&1) {
  zoomx = MAXZOOM - (int)((MAXZOOM+0.01)*(*(float*)callData));
  if(zoomx<=0)
   zoomx = 1;
 }

 if (zoomflags&2) {
  zoomy = MAXZOOM - (int)((MAXZOOM+0.01)*(*(float*)callData));
  if(zoomy<=0)
   zoomy = 1;
 }

 setXYtitle();       
 frozenDraw();   
 return;
}

void sliderScrollCB(Widget w, XtPointer clientData, XtPointer callData)
{
 if (zoomflags&1) {
   if ((long)callData<0) 
   {
    ++zoomx;
    if (zoomx>MAXZOOM) zoomx = MAXZOOM;
   }
   else
   {	 
    --zoomx;
    if (zoomx<=0) zoomx = 1;
   }
 }

 if (zoomflags&2) {
   if ((long)callData<0)
   {
    ++zoomy;
    if (zoomy>MAXZOOM) zoomy = MAXZOOM;
   }
   else
   {	 
    --zoomy;
    if (zoomy<=0) zoomy = 1;
   }
 }

 if (zoomflags&1)
  XawScrollbarSetThumb(sliderW, 1. - (double)zoomx/((double)MAXZOOM), -1);
 else
  XawScrollbarSetThumb(sliderW, 1. - (double)zoomy/((double)MAXZOOM), -1);

 setXYtitle();   
 frozenDraw();   
 return;
}
   
#if DEBUG
void counterCB(XtPointer clientData, XtIntervalId* id)
{
 if (magnifier_closing_down) return;
 printf("frame count: %.2f fps\n", counter/10.);
 counter = 0;
 counterId = XtAppAddTimeOut(app_con, 10000, counterCB, (XtPointer)NULL);
}
#endif

void limitCB(XtPointer clientData, XtIntervalId* id)
{
 if (magnifier_closing_down) return;
 ready = True;
 limitId = XtAppAddTimeOut(app_con, limit, limitCB, (XtPointer)NULL);
}

void rotateCB(Widget w, XtPointer clientData, XtPointer callData)
{
 pcounter = (pcounter+1)%4;
 position = psequence[pcounter];
 position1 = (position&1);
 position2 = (position&2)>>1;
 position3 = (position&4)>>2;
 frozenDraw();
}

void symmetryCB(Widget w, XtPointer clientData, XtPointer callData)
{
 position1 = 1-position1;
 position = (position&6) | position1;
 frozenDraw();
}

Boolean drawCB(XtPointer clientData)
{
 XImage *newimage;
 Window dummy_window1, dummy_window2;
 int x, y;
 static int p_x, p_y, r_x, r_y;
 unsigned int dummy_int;
 unsigned int w, h;
 static size_t data_size=0;

 if (magnifier_closing_down) return(False);
 if(limit)
 {
  if(!ready)
   return(False);
  else
   ready = False;
 }

 if (init && frozen) {
  if (position3)
  {	 
   h = (width+zoomx-1)/zoomx;
   w = (height+zoomy-1)/zoomy;
   x = lupe_x - (h>>1);
   y = lupe_y - (w>>1);   
  } else
  {
   w = (width+zoomx-1)/zoomx;
   h = (height+zoomy-1)/zoomy;     
   x = lupe_x - (w>>1);
   y = lupe_y - (h>>1);
  }
  if (x<0) x = 0;  
  if (y<0) y = 0;      
  goto capture;
 }
	
 XQueryPointer(dpy, vinf.win, &dummy_window1, &dummy_window2,
               &r_x, &r_y, &p_x, &p_y, &dummy_int);

 if (position3)
 {	
  h = (unsigned int) ((width+zoomx-1) / zoomx);
  w = (unsigned int) ((height+zoomy-1) / zoomy);
 }
 else
 {	
  w = (unsigned int) ((width+zoomx-1) / zoomx);
  h = (unsigned int) ((height+zoomy-1) / zoomy);
 }   
 x = p_x - (w>>1);
 y = p_y - (h>>1);

 if(x<0)
  x = 0;
 if(y<0)
  y = 0;

 if(x+w>winwidth)
  x = winwidth-w;
 if(y+h>winheight)
  y = winheight-h;

 if(vinf.win != rwin)
 {
  if(r_x+w>dpywidth)
   x = dpywidth-w-r_x+p_x;
  if(r_y+h>dpyheight)
   y = dpyheight-h-r_y+p_y;
 }

 lupe_x = x + (w>>1);
 lupe_y = y + (h>>1);
   
capture:

 init = 0;   
   
 if(w>winwidth)
  w = winwidth;
 if(h>winheight)
  h = winheight;

 newimage = XGetImage(dpy, vinf.win, x, y, w, h, AllPlanes, ZPixmap);

 if(ErrorFlag)
 {
  ErrorFlag = 0;
  return(False);
 }

 if(newimage == NULL)
 {
#if DEBUG
  fprintf(stderr, "Couldn't get the new image...\n");
#endif
  return(False);
 }

 if(changed)
 {
  if(have_shm)
  {
   if(image)
    destroy_xshm_image(image);
   image = alloc_xshm_image(width, height, newimage->depth);
   if(image == NULL)
   {
#if DEBUG
    printf("remote display, disabling shared memory.\n");
#endif
    have_shm = False;
    goto goon;
   }
   data_size = image->bytes_per_line * image->height;
  }
  else
  {
   if(image)
    XDestroyImage(image);
   image = XCreateImage(dpy, vinf.this_visual, newimage->depth,
                        ZPixmap, 0, 0, width, height,
                        newimage->bitmap_pad, 0);
   data_size = image->bytes_per_line * image->height;
   image->data = XtMalloc(data_size);
  }
  changed = False;
 }

 if(vinf.depth == 8)
  memset(image->data, bg_pixel, data_size);
 else
  memset(image->data, WhitePixelOfScreen(XtScreen(shell)), data_size);

#if DEBUG
 counter ++;
#endif

 if(zoomx==1 && zoomy==1 && !position)
 {
  memcpy(image->data, newimage->data, data_size);
 }
 else
 {
  /* zoom_8 is appr. 1/3 faster then zoom_normal */
  if(vinf.depth==8 && !position)
   zoom_8(newimage, image, w, h);
  else 
  {
   if (smooth)
   {
    if (position3)
     zoom_smooth(newimage, image, h, w);
    else
     zoom_smooth(newimage, image, w, h);
   }
   else
   {
    if (position3)      
     zoom_normal(newimage, image, h, w);
    else
     zoom_normal(newimage, image, w, h);
   }
  }
 }

 if(have_shm)
  XShmPutImage(dpy, window, gc, image, 0, 0, 0, 0, width, height, False);
 else
  XPutImage(dpy, window, gc, image, 0, 0, 0, 0, width, height);

goon:

 XDestroyImage(newimage);
   
 if(frozen)
  return(True);

 return(False);
}

void zoom_8(XImage *newimage, XImage *image, int w, int h)
{
 char c;
 char *data;
 int x, y, dy, xz, yz, fx, fy;

 data = image->data;
 xz = -zoomx;
 for(x = 0; x < w; x++)
 {
  xz += zoomx;
  yz = -zoomy;
  if (xz+zoomx-1<image->width)
   fx = zoomx;
  else
   fx = image->width - xz;
  for(y = 0; y < h; y++)
  {
   yz += zoomy;
   if (yz+zoomy-1<image->height)
    fy = zoomy;
   else
    fy = image->height - yz;
   c = newimage->data[x+y*newimage->bytes_per_line];
   for(dy=0; dy<fy; dy++)
    memset(&data[image->bytes_per_line*(yz+dy)+xz], (int)c, fx);
  }
 }
}

void frozenDraw()
{  
 if (frozen)
 {
  init = 1;
  drawCB(NULL);
 }
 XFlush(dpy);
}

void zoom_normal(XImage *newimage, XImage *image, int w, int h)
{
 char *c;
 char *data;
 int u, v, x1, y1, x, y, xz, yz, dx, dy, bpp, fx, fy;

 bpp = newimage->bits_per_pixel>>3;
 data = image->data;

 xz = -zoomx;
 for(x = 0; x < w; x++)
 {
  xz += zoomx;
  yz = -zoomy;
  if (xz+zoomx-1<image->width)
   fx = zoomx;
  else
   fx = image->width - xz;
  if (position1) x1 = w-1-x; else x1 = x;
  for(y = 0; y < h; y++)
  {
   yz += zoomy;
   if (yz+zoomy-1<image->height)
    fy = zoomy;
   else
    fy = image->height - yz;
   if (position2) y1 = h-1-y; else y1 = y;
   if (position3)      
     c = &(newimage->data[bpp*y1 + x1*newimage->bytes_per_line]);
   else
     c = &(newimage->data[bpp*x1 + y1*newimage->bytes_per_line]);
   u = bpp*xz + yz*image->bytes_per_line;       
   for(dy=0; dy<fy; dy++)
   {
    v = u + dy*image->bytes_per_line;
    if(dy==0)
     for(dx=0; dx<fx; dx++)
      memcpy(&data[v+dx*bpp], c, bpp);
    else
      memcpy(&data[v], &data[u], bpp*fx);	
   }
  }
 }
}

void zoom_smooth(XImage *newimage, XImage *image, int w, int h)
{
 unsigned char *c11, *c12, *c21, *c22, *d;
 unsigned char rgb11[3], rgb12[3], rgb21[3], rgb22[3];
 unsigned char res[3], res1[3], res2[3];
 unsigned char *data;
 int u, v, x1, y1, x, y, xz, yz, dx, dy, bpp, fx, fy, depth;
 int shiftx[8], shifty[8];

 bpp = newimage->bits_per_pixel>>3;
 data = (unsigned char *)image->data;
 depth = definf.depth;
 if (depth==16 && definf.this_visual->green_mask==992) depth = 15;

 shiftx[0] = bpp;
 shiftx[1] = -bpp;     
 shiftx[2] = bpp;     
 shiftx[3] = -bpp;
 shiftx[4] = newimage->bytes_per_line;
 shiftx[5] = -newimage->bytes_per_line;
 shiftx[6] = newimage->bytes_per_line;     
 shiftx[7] = -newimage->bytes_per_line;

 shifty[0] = newimage->bytes_per_line;
 shifty[1] = newimage->bytes_per_line;
 shifty[2] = -newimage->bytes_per_line;     
 shifty[3] = -newimage->bytes_per_line;
 shifty[4] = bpp;
 shifty[5] = bpp;     
 shifty[6] = -bpp;     
 shifty[7] = -bpp;
   
 xz = -zoomx;
 for(x = 0; x < w; x++)
 {
  xz += zoomx;
  yz = -zoomy;
  if (xz+zoomx-1<image->width)
   fx = zoomx;
  else
   fx = image->width - xz;
  if (position1) x1 = w-1-x; else x1 = x;    
  for(y = 0; y < h; y++)
  {
   yz += zoomy;
   if (yz+zoomy-1<image->height)
    fy = zoomy;
   else
    fy = image->height - yz;
   if (position2) y1 = h-1-y; else y1 = y;
   if (position3) 
    c11 = (unsigned char *)
          &(newimage->data[bpp*y1 + x1*newimage->bytes_per_line]);
   else
    c11 = (unsigned char *)
          &(newimage->data[bpp*x1 + y1*newimage->bytes_per_line]);     
   if (y==h-1)
     c21 = c11;
   else 
     c21 = c11 + shifty[position];
   if (x==w-1)
   {
    c12 = c11;
    c22 = c21;
   }
   else
   {
    c12 = c11 + shiftx[position];
    c22 = c21 + shiftx[position];
   }
   u = bpp*xz + yz*image->bytes_per_line;
   if (y==0)
   {
    pix2rgb(c11,rgb11,depth);
    pix2rgb(c12,rgb12,depth);
   }
   else
   {
    memcpy(rgb11,rgb21,3);
    memcpy(rgb12,rgb22,3);      
   }
   pix2rgb(c21,rgb21,depth);
   pix2rgb(c22,rgb22,depth);              
   for(dy=0; dy<fy; dy++)
   {
    v = u + dy*image->bytes_per_line;
    res[0] = (rgb11[0]*(fy-dy) + rgb21[0]*dy)/fy;
    res[1] = (rgb11[1]*(fy-dy) + rgb21[1]*dy)/fy;
    res[2] = (rgb11[2]*(fy-dy) + rgb21[2]*dy)/fy;
    rgb2pix(res,&data[v],depth);      
    for(dx=1; dx<fx; dx++) {
     d = &data[v+dx*bpp];
     res1[0] = (rgb11[0]*(fx-dx) + rgb12[0]*dx)/fx;
     res1[1] = (rgb11[1]*(fx-dx) + rgb12[1]*dx)/fx;
     res1[2] = (rgb11[2]*(fx-dx) + rgb12[2]*dx)/fx;
     if (dy==0)
      rgb2pix(res1,d,depth);
     else
     {
      res2[0] = (rgb21[0]*(fx-dx) + rgb22[0]*dx)/fx;
      res2[1] = (rgb21[1]*(fx-dx) + rgb22[1]*dx)/fx;
      res2[2] = (rgb21[2]*(fx-dx) + rgb22[2]*dx)/fx;
      res[0] = (res1[0]*(fy-dy) + res2[0]*dy)/fy;
      res[1] = (res1[1]*(fy-dy) + res2[1]*dy)/fy;
      res[2] = (res1[2]*(fy-dy) + res2[2]*dy)/fy;
      rgb2pix(res,d,depth);
     }
    }
   }
  }
 }
}

void freezeCB(Widget w, XtPointer clientData, XtPointer callData)
{
 frozen=!frozen;
 if (frozen)
 {
  XtVaSetValues(w, XtNstate, True, NULL);
 }
 else
 {
  XtVaSetValues(w, XtNstate, False, NULL);
  wpid = XtAppAddWorkProc(app_con, drawCB, (XtPointer)NULL);
 }
 XFlush(dpy);
 return;
}

void infoCB(Widget w, XtPointer clientData, XtPointer callData)
{
 char buffer[80];
 int l, y;
 Window win;

 win = Select_Window();
 FillVisinfo(win);
 l = 18;
 y = 6;
 XClearWindow(dpy, window);
 sprintf(buffer, "depth:      %d", vinf.depth);
 DrawString(6, (y+=l), buffer);
 sprintf(buffer, "visual:     %s", vinf.vistype);
 DrawString(6, (y+=l), buffer);      
 sprintf(buffer, "dpy width:  %d", dpywidth);
 DrawString(6, (y+=l), buffer);      
 sprintf(buffer, "dpy height: %d", dpyheight);
 DrawString(6, (y+=l), buffer);
 sprintf(buffer, "window:     %s", vinf.winname);
 DrawString(6, (y+=l), buffer);
 sprintf(buffer, "win  x:     %d", vinf.x);
 DrawString(6, (y+=l), buffer);      
 sprintf(buffer, "win  y:     %d", vinf.y);
 DrawString(6, (y+=l), buffer);      
 sprintf(buffer, "win width:  %d", vinf.width);
 DrawString(6, (y+=l), buffer);      
 sprintf(buffer, "win height: %d", vinf.height);
 DrawString(6, (y+=l), buffer);      
 XFlush(dpy) ;
 if (!frozen)
  usleep(3000000);
}

void jumpCB(Widget w, XtPointer clientData, XtPointer callData)
{
 Window win;
 
 char buffer[80];

 if (definf.depth > 8) {
  XtVaGetValues(w, XtNstate, &smooth, NULL);
  if (frozen)
   frozenDraw();
  else
   drawCB(clientData);
  return;
 }
	
 win = Select_Window();
 if(!win)
 {
  XClearWindow(dpy, window);    
  sprintf(buffer, "Can't get window.\n");
  DrawString(6, 24, buffer);
  XFlush(dpy);    
  usleep(1000000);
  return;
 }

 if(FillVisinfo(win) == NEW_WIN)
 {
  GetWinAttribs();
  PrepareForJump();
  CreateInterface();
 }
}

int FillVisinfo(Window win)
{
 XWindowAttributes winattr;
 Window root, junk;
 char *name;

 if(!win)
  return(OLD_WIN);

 if(!XGetWindowAttributes(dpy, win, &winattr))
 {
  fprintf(stderr, "Can't get window attributes.\n");
  return(OLD_WIN);
 }

 if(vinf.winname)
  XtFree(vinf.winname);

 if(XFetchName(dpy, win, &name))
  vinf.winname = XtNewString(name);
 else
 {
  if(win == rwin)
   vinf.winname = XtNewString(" (root window)");
  else
   vinf.winname = XtNewString(" (has no name)");
 }
 XFree(name);

 root = DefaultRootWindow(dpy);
 XTranslateCoordinates(dpy, win, root, 0, 0, &vinf.x, &vinf.y, &junk);
 vinf.width      = winattr.width;
 vinf.height     = winattr.height;
   
 if(winattr.depth    == vinf.depth &&
    winattr.visual   == vinf.this_visual &&
    winattr.colormap == vinf.cmap)
 {
  return(OLD_WIN);
 }

 vinf.depth      = winattr.depth;
 vinf.new_visual = winattr.visual;
 vinf.cmap       = winattr.colormap;

 if(winattr.depth    == definf.depth &&
    winattr.visual   == definf.this_visual &&
    winattr.colormap == definf.cmap)
 {
  vinf.win = rwin;
 }
 else
 {
  vinf.win = win;
  winheight = winattr.height;
  winwidth  = winattr.width;
 }
   
 if(vinf.vistype)
  XtFree(vinf.vistype);

 switch(winattr.visual->class)
 {
  case StaticGray:  vinf.vistype = XtNewString("StaticGray");
                     break;
  case GrayScale:   vinf.vistype = XtNewString("GrayScale");
                     break;
  case StaticColor: vinf.vistype = XtNewString("StaticColor");
                     break;
  case PseudoColor: vinf.vistype = XtNewString("PseudoColor");
                     break;
  case TrueColor:   vinf.vistype = XtNewString("TrueColor");
                     break;
  case DirectColor: vinf.vistype = XtNewString("DirectColor");
                     break;
  default:          vinf.vistype = XtNewString("*unknown* visualtype ?!?");
                     break;
 }

 return(NEW_WIN);
}

void exposeCB(Widget widget, XtPointer clientData, XEvent *event)
{
#if DEBUG
 printf("expose.\n");
#endif

 if(window && image && !changed)
   XPutImage(XtDisplay(widget), window, gc, image, 0, 0, 0, 0, width, height);

 return;
}

void resizeCB(Widget widget, XtPointer clientData, XEvent *event)
{
 Dimension w, h, qw;

#if DEBUG
 printf("resize.\n");
#endif

 XtVaGetValues(widget, XtNwidth,  &w,
                       XtNheight, &h,
                       NULL);
 width  = (unsigned int) w;
 height = (unsigned int) h;
 changed = True;

 FillVisinfo(window);
   
 XtUnmanageChild(zoomB);
 XtUnmanageChild(quitB);   
 XtVaSetValues(zoomB,
#ifdef XAW3D
               XtNwidth, 17, XtNheight, 17,
#else	       
	       XtNwidth, 14, XtNheight, 14,
#endif	       
	       XtNfromHoriz, drawW, NULL);
 XtVaGetValues(quitB, XtNwidth, &qw, NULL);
 XtVaSetValues(quitB,
#ifdef XAW3D	       
	       XtNhorizDistance, -qw+21,
#else	       
	       XtNhorizDistance, -qw+18, 
#endif	       
	       NULL);
 XtManageChild(zoomB);
 XtManageChild(quitB);
 frozenDraw();
 return;
}

void iconifyCB(Widget w, XtPointer clientData, XEvent *event)
{
 if (magnifier_closing_down) return;
 if(event->type == MapNotify)
 {
  frozen = old_frozen;
  if(!frozen)
   wpid = XtAppAddWorkProc(app_con, drawCB, (XtPointer)NULL);
#if DEBUG
  printf("normal\n");
#endif

 }
 if(event->type == UnmapNotify)
 {
  old_frozen = frozen;
  frozen = True;
#if DEBUG
  printf("iconified\n");
#endif
 }
 return;
}

int CreateInterface()
{
 Dimension minW, minH;
 Colormap cmap;
 Arg args[6];
 Cardinal nargs = 0;
 Position x, y;

 changed = True;
 XtVaGetValues(Global.toplevel, XtNcolormap, &cmap, XtNx, &x, XtNy, &y, NULL);
 XtSetArg(args[nargs], XtNcolormap, cmap); nargs++; 
 XtSetArg(args[nargs], XtNx,  x+24);  nargs++;
 XtSetArg(args[nargs], XtNy,  y+24);  nargs++;

 shell =  XtCreatePopupShell("magnifier", topLevelShellWidgetClass,
			     Global.toplevel, args, nargs);

 XtVaSetValues(shell,
               XtNallowShellResize, False,
               NULL);

 MakeAthenaInterface();

 XtVaGetValues(drawW, XtNwidth,  &minW,
                      XtNheight, &minH,
                      XtNdepth,  &(vinf.depth),
                      NULL);
#if DEBUG
 printf("using depth %d\n", vinf.depth);
#endif

 width  = (int)minW;
 height = (int)minH;

 XtAddEventHandler(shell, StructureNotifyMask, False,
                   (XtEventHandler)iconifyCB, (XtPointer)NULL);

 if (!frozen)
  wpid = XtAppAddWorkProc(app_con, drawCB, (XtPointer)NULL);

#if DEBUG
 counterId = XtAppAddTimeOut(app_con, 10000, counterCB, (XtPointer)NULL);
#endif

 if(limit)
  limitId = XtAppAddTimeOut(app_con, limit, limitCB, (XtPointer)NULL);

 vinf.this_visual = vinf.new_visual;

 XtRealizeWidget(shell);

 window = XtWindow(drawW);
 XtVaGetValues(shell, XtNbackground, &bg_pixel, NULL);

 XFreeGC(dpy, gc);
 gc = XCreateGC(dpy, window, 0, NULL);

 XtAddEventHandler(drawW, ButtonPressMask, False,
                   (XtEventHandler)btnDownCB, (XtPointer)NULL);
 XtAddEventHandler(drawW, ButtonReleaseMask, False,
                   (XtEventHandler)btnUpCB, (XtPointer)NULL);
 XtAddEventHandler(drawW, Button1MotionMask, False,
                   (XtEventHandler)btnMotionCB, (XtPointer)NULL);
 XtAddEventHandler(drawW, KeyPressMask|KeyReleaseMask, True,
                   (XtEventHandler)keyReleaseCB, (XtPointer)NULL);

 XtSetKeyboardFocus(shell, drawW);
		
 return(True);
}

void MakeAthenaInterface()
{
 Dimension qw;
 formW =
   XtVaCreateManagedWidget("form",
                           formWidgetClass,
                           shell,
                           NULL);

 drawW =
   XtVaCreateManagedWidget("draw",
                           simpleWidgetClass,
                           formW,
                           XtNtop,    XawChainTop,
                           XtNright,  XawChainRight,
                           XtNleft,   XawChainLeft,
                           XtNbottom, XawChainBottom,
                           XtNwidth,  LUPEWIDTH,
                           XtNheight, LUPEHEIGHT,
                           NULL);

 XtAddEventHandler(drawW, ExposureMask, False,
                   (XtEventHandler)exposeCB, (XtPointer)NULL);
 XtAddEventHandler(drawW, StructureNotifyMask, False,
                   (XtEventHandler)resizeCB, (XtPointer)NULL);

 zoomB =
   XtVaCreateManagedWidget("zoom",
                           commandWidgetClass,
                           formW,
			   XtNlabel,  "=",
			   XtNstate,  True,
                           XtNfromHoriz, drawW,			   
                           XtNtop,    XawChainTop,
                           XtNright,  XawChainRight,
                           XtNleft,   XawChainLeft,
                           XtNbottom, XawChainBottom,
#ifdef XAW3D			   
                           XtNwidth,  17, XtNheight, 17,
#else
                           XtNwidth,  14, XtNheight, 14,
#endif			   
                           NULL);
 XtAddCallback(zoomB, XtNcallback, zoomCB, (XtPointer)NULL);
     
 sliderW =
   XtVaCreateManagedWidget("slider",
                           scrollbarWidgetClass,
                           formW,
                           XtNfromHoriz, drawW,
                           XtNfromVert,  zoomB,
                           XtNtop,       XawChainTop,
                           XtNbottom,    XawChainBottom,
                           XtNright,     XawChainRight,
                           XtNleft,      XawChainRight,
#ifdef XAW3D			   
                           XtNheight,    LUPEHEIGHT - 19,
			   XtNwidth, 17,
#else			   
                           XtNheight,    LUPEHEIGHT - 20,
#endif			   
                           NULL);
   
 XtAddCallback(sliderW, XtNjumpProc, sliderCB, (XtPointer)NULL);
 XtAddCallback(sliderW, XtNscrollProc, sliderScrollCB, (XtPointer)NULL);
   
 XawScrollbarSetThumb(sliderW, 1. - (double)zoomx/((double)MAXZOOM), -1);

 rotateB =
   XtVaCreateManagedWidget("rotate",
                           commandWidgetClass,
                           formW,	
                           XtNfromVert, drawW,
                           XtNheight,   20,
                           XtNright,    XawChainLeft,
                           XtNleft,     XawChainLeft,
                           XtNbottom,   XawChainBottom,
                           XtNtop,      XawChainBottom,
                           NULL);
 XtAddCallback(rotateB, XtNcallback, rotateCB, (XtPointer)NULL);

 symmetryB =
   XtVaCreateManagedWidget("symmetry",
                           toggleWidgetClass,
                           formW,
                           XtNheight,   20,
                           XtNfromHoriz, rotateB,			   
                           XtNfromVert, drawW,
                           XtNright,    XawChainLeft,
                           XtNleft,     XawChainLeft,
                           XtNbottom,   XawChainBottom,
                           XtNtop,      XawChainBottom,
                           NULL);
 XtAddCallback(symmetryB, XtNcallback, symmetryCB, (XtPointer)NULL);

 freezeB =
   XtVaCreateManagedWidget("freeze",
                           toggleWidgetClass,
                           formW,
                           XtNheight,   20,
                           XtNfromHoriz, symmetryB,
                           XtNfromVert, drawW,
                           XtNright,    XawChainLeft,
                           XtNleft,     XawChainLeft,
                           XtNbottom,   XawChainBottom,
                           XtNtop,      XawChainBottom,
                           NULL);
 XtAddCallback(freezeB, XtNcallback, freezeCB, (XtPointer)NULL);

 jumpB =
   XtVaCreateManagedWidget((definf.depth > 8)?
			   "smooth" : "cmap",
			   (definf.depth > 8)?
                           toggleWidgetClass:commandWidgetClass,
                           formW,
                           XtNheight,    20,
                           XtNfromHoriz,  freezeB,
                           XtNfromVert,  drawW,
                           XtNright,     XawChainLeft,
                           XtNleft,      XawChainLeft,
                           XtNbottom,    XawChainBottom,
                           XtNtop,       XawChainBottom,
                           NULL);
 XtAddCallback(jumpB, XtNcallback, jumpCB, (XtPointer)NULL);


 infoB =
   XtVaCreateManagedWidget("info",
                           commandWidgetClass,
                           formW,
                           XtNheight,    20,
                           XtNfromHoriz, jumpB,
                           XtNfromVert,  drawW,
                           XtNright,     XawChainLeft,
                           XtNleft,      XawChainLeft,
                           XtNbottom,    XawChainBottom,
                           XtNtop,       XawChainBottom,
                           NULL);
 XtAddCallback(infoB, XtNcallback, infoCB, (XtPointer)NULL);

 memoryB =
   XtVaCreateManagedWidget("memory",
                           commandWidgetClass,
                           formW,
                           XtNheight,    20,
                           XtNfromHoriz, infoB,
                           XtNfromVert,  drawW,
                           XtNleft,      XawChainLeft,
                           XtNright,     XawChainLeft,
                           XtNbottom,    XawChainBottom,
                           XtNtop,       XawChainBottom,
                           NULL);

 XtAddCallback(memoryB, XtNcallback, memoryCB, (XtPointer)NULL);

 canvasB =
   XtVaCreateManagedWidget("canvas",
                           commandWidgetClass,
                           formW,
                           XtNheight,    20,
                           XtNfromHoriz, memoryB,
                           XtNfromVert,  drawW,
                           XtNleft,      XawChainLeft,
                           XtNright,     XawChainLeft,
                           XtNbottom,    XawChainBottom,
                           XtNtop,       XawChainBottom,
                           NULL);

 XtAddCallback(canvasB, XtNcallback, canvasCB, (XtPointer)NULL);

 quitB =
   XtVaCreateManagedWidget("exit",
                           commandWidgetClass,
                           formW,
                           XtNheight,    20,
                           XtNfromVert,  drawW,
                           XtNfromHoriz, drawW,
			   XtNleft,      XawChainRight,
			   XtNright,     XawChainRight,			   
                           XtNbottom,    XawChainBottom,
                           XtNtop,       XawChainBottom,
                           NULL);

 XtVaGetValues(quitB, XtNwidth, &qw, NULL);
 XtVaSetValues(quitB,
#ifdef XAW3D	       
	       XtNhorizDistance, -qw+21,
#else	       
	       XtNhorizDistance, -qw+18, 
#endif	       
	       NULL);
 XtAddCallback(quitB, XtNcallback, quitCB, (XtPointer)NULL);
}

void GetWinAttribs()
{
 XtVaGetValues(shell, XtNx, &old_win.x,
                         XtNy, &old_win.y,
                         XtNwidth,  &old_win.width,
                         XtNheight, &old_win.height,
                         NULL);
 old_win.set = True;
}

void PrepareForJump()
{
 XtUnmapWidget(shell);

 if (wpid) XtRemoveWorkProc(wpid);
 wpid = None;

#if DEBUG
 if (counterId) XtRemoveTimeOut(counterId);
 counterId = None;
#endif

 if(limit) {
  if (limitId) XtRemoveTimeOut(limitId);
  limitId = None;
 }

 XtDestroyWidget(shell);
}

void setXYtitle()
{
 char newtitle[40];
 sprintf(newtitle, "%s  %d x %d", app_title, zoomx, zoomy);
 XStoreName(dpy, XtWindow(shell), newtitle);
}

/*
 * Routine to let user select a window using the mouse
 * from xwininfo(1)
 */

Window Select_Window()
{
 int status;
 Cursor cursor;
 XEvent event;
 Window target_win = None;
 int buttons = 0;

 /* Make the target cursor */
 cursor = XCreateFontCursor(dpy, XC_crosshair);

 /* Grab the pointer using target cursor, letting it room all over */
 status = XGrabPointer(dpy, rwin, False,
            ButtonPressMask|ButtonReleaseMask, GrabModeSync,
            GrabModeAsync, rwin, cursor, CurrentTime);
 if(status != GrabSuccess)
 {
  fprintf(stderr, "can't grab the mouse.\n");
  return((Window)0);
 }

 /* Let the user select a window... */
 while((target_win == None) || (buttons != 0))
 {
  /* allow one more event */
  XAllowEvents(dpy, SyncPointer, CurrentTime);
  XWindowEvent(dpy, rwin, ButtonPressMask|ButtonReleaseMask, &event);
  switch(event.type)
  {
   case ButtonPress:
      if(target_win == None)
      {
       target_win = event.xbutton.subwindow; /* window selected */
       if(target_win == None)
	    target_win = rwin;
      }
      buttons++;
      break;
    case ButtonRelease:
      if(buttons > 0) /* there may have been some down before we started */
       buttons--;
      break;
   }
 }

 XUngrabPointer(dpy, CurrentTime);      /* Done with pointer */

 if(target_win)
 {
   if(target_win != rwin)
    target_win = XmuClientWindow(dpy, target_win);
 }

 return(target_win);
}

void
StartMagnifier(Widget w)
{
 Boolean wants_shm;
 unsigned long XORvalue;

 image = NULL;
 zoomx = 4;
 zoomy = 4;
 changed = True;
 frozen = False;
 smooth = False;
 counter = 0;
 wants_shm = True;
 limit = 0;
 vinf.winname = NULL;
 vinf.depth = 0;
 vinf.vistype = NULL;
 old_win.set = False;
 magnifier_closing_down = 0;

 PopdownMenusGlobal();
 dpy = Global.display;
 app_con = XtWidgetToApplicationContext(Global.toplevel);

 if (ImageByteOrder(dpy) == MSBFirst)
 { 
  N0 = 1;
  N1 = 0;    
 }
 else
 { 
  N0 = 0;
  N1 = 1;    
 }     

 rwin = DefaultRootWindow(dpy);
 vinf.new_visual = DefaultVisual(dpy, DefaultScreen(dpy));
 
 definf.depth       = DefaultDepth(dpy, DefaultScreen(dpy));
 definf.this_visual = DefaultVisual(dpy, DefaultScreen(dpy));
 definf.cmap        = DefaultColormap(dpy, DefaultScreen(dpy));
      
 dpyheight = winheight = DisplayHeight(dpy, DefaultScreen(dpy));
 dpywidth  = winwidth  = DisplayWidth(dpy, DefaultScreen(dpy));
 XORvalue = (((unsigned long)1) << definf.depth) - 1;

 //XSetErrorHandler(HandleXError);

 /* test if XShm available */
 if(!check_for_xshm())
 {
#if DEBUG
  printf("shared memory extension not available\n");
#endif
  have_shm = False;
 }
 else
 {
#if DEBUG
  printf("found shared memory extension\n");
#endif
  if(!wants_shm)
  {
#if DEBUG
   printf("   but not using it...\n");
#endif
   have_shm = False;
  }
  else
   have_shm = True;
 }

 gc = XCreateGC(dpy, rwin, 0, NULL);
 ovlgc = XCreateGC(dpy, rwin, 0, NULL);
 XSetForeground(dpy, ovlgc, XORvalue);
 XSetFunction(dpy, ovlgc, GXxor);
 XSetSubwindowMode(dpy, ovlgc, IncludeInferiors);

 old_frozen = frozen;
 CreateInterface();
 GetWinAttribs();
 FillVisinfo(window);

 XtRealizeWidget(shell);
 XtPopup(shell, XtGrabNonexclusive);
 XtVaGetValues(shell, XtNtitle, &app_title, NULL);
 AddDestroyCallback(shell, (DestroyCallbackFunc) quitCB, NULL);
 setXYtitle();
}

