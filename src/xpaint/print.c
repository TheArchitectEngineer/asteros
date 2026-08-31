/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)            | */
/* | Copyright 1995, 1996 Torsten Martinsen (bullestock@dk-online.dk)  | */
/* |                                                                   | */
/* | Permission to use, copy, modify, and to distribute this software  | */
/* | and its documentation for any purpose is hereby granted without   | */
/* | fee, provided that the above copyright notice appear in all       | */
/* | copies and that both that copyright notice and this permission    | */
/* | notice appear in supporting documentation.  There is no           | */
/* | representations about the suitability of this software for        | */
/* | any purpose.  this software is provided "as is" without express   | */
/* | or implied warranty.                                              | */
/* |                                                                   | */
/* +-------------------------------------------------------------------+ */

/* $Id: print.c,v 1.12 2005/03/20 20:15:32 demailly Exp $ */

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>

#include <X11/IntrinsicP.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/CoreP.h>
#include <X11/cursorfont.h>

#include "xaw_incdir/Box.h"
#include "xaw_incdir/Form.h"
#include "xaw_incdir/Scrollbar.h"
#include "xaw_incdir/Viewport.h"
#include "xaw_incdir/Command.h"
#include "xaw_incdir/Paned.h"
#include "xaw_incdir/AsciiText.h"
#include "xaw_incdir/Text.h"
#include "xaw_incdir/MenuButton.h"
#include "xaw_incdir/SimpleMenu.h"
#include "xaw_incdir/SmeBSB.h"
#include "xaw_incdir/ToggleP.h"

#include "Colormap.h"
#include "Paint.h"
#include "PaintP.h"
#include "palette.h"
#include "print.h"
#include "xpaint.h"
#include "menu.h"
#include "image.h"
#include "messages.h"
#include "misc.h"
#include "region.h"
#include "text.h"
#include "graphic.h"
#include "operation.h"
#include "color.h"
#include "protocol.h"

#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#endif

extern void openTempDir(char *buf);
extern void pipe_command(char *command, 
  int input(char*, int), 
  void output(const char*, int),
  void error(const char*, int));
extern int WriteGIF(char *file, Image * outImage);
extern int WritePNGn(char *file, Image * outImage);
extern int WritePNM(char *file, Image * outImage);
extern int WriteXPM(char *file, Image * outImage);
extern int WriteResizedPS(char *file, Image * image, void *data);

extern Image *outputImage;

static int PAGEWIDTH = 200;
static int PAGEHEIGHT = 283;
#if defined(XAW95) || defined(XAW3DG)
#define EXTRAWIDTH 15
#define EXTRAHEIGHT 13
#else
#define EXTRAWIDTH 10
#define EXTRAHEIGHT 10
#endif

static String textTranslations =
    "#override\n\
<Key>Return: print-text-ok()\n\
<Key>Linefeed: print-text-ok()\n\
Ctrl<Key>M: print-text-ok()\n\
Ctrl<Key>J: print-text-ok()\n";

static XtTranslations translations = None;

static PaintMenuItem fileMenu[] =
{
    MI_SIMPLE("print"),
    MI_SIMPLE("preview"),
    MI_SIMPLE("close"),
};
static PaintMenuItem formatMenu[] =
{
    MI_FLAG("Letter", MF_CHECK | MF_GROUP1),
    MI_FLAG("Legal", MF_CHECK | MF_GROUP1),
    MI_FLAG("Statement", MF_CHECK | MF_GROUP1),
    MI_FLAG("Tabloid", MF_CHECK | MF_GROUP1),
    MI_FLAG("Ledger", MF_CHECK | MF_GROUP1),
    MI_FLAG("Folio", MF_CHECK | MF_GROUP1),
    MI_FLAG("Quarto", MF_CHECK | MF_GROUP1),
    MI_FLAG("Executive", MF_CHECK | MF_GROUP1),
    MI_FLAG("10x14", MF_CHECK | MF_GROUP1),
    MI_FLAG("A2", MF_CHECK | MF_GROUP1),
    MI_FLAG("A3", MF_CHECK | MF_GROUP1),
    MI_FLAG("A4", MF_CHECK | MF_GROUP1),
    MI_FLAG("A5", MF_CHECK | MF_GROUP1),
    MI_FLAG("B4", MF_CHECK | MF_GROUP1),
    MI_FLAG("B5", MF_CHECK | MF_GROUP1),
};

int paper_sizes[XtNumber(formatMenu)][2] = {
      {/* Letter */           612,  792},
      {/* Legal */            612, 1008},
      {/* Statement */        396,  612},
      {/* Tabloid */          792, 1224},
      {/* Ledger */          1224,  792},
      {/* Folio */            612,  936},
      {/* Quarto */           610,  780},
      {/* Executive */        540,  720},
      {/* 10x14 */            720, 1008},
      {/* A2 */              1191, 1684},
      {/* A3 */               842, 1191},
      {/* A4 */               595,  842},
      {/* A5 */               420,  595},
      {/* B4 */               729, 1032},
      {/* B5 */               516,  729}
    };

static PaintMenuItem helpMenu[] =
{
    MI_SIMPLECB("help", HelpDialog, "printMenu"),
};

static PaintMenuBar printMenuBar[] =
{
    {None, "file", XtNumber(fileMenu), fileMenu},
    {None, "help", XtNumber(helpMenu), helpMenu},
    {None, "format", XtNumber(formatMenu), formatMenu},
};

static PaintMenuItem externMenu[] =
{
    MI_FLAG("GIF", MF_CHECK | MF_GROUP1),
    MI_FLAG("PNG", MF_CHECK | MF_GROUP1),
    MI_FLAG("PPM", MF_CHECK | MF_GROUP1),
    MI_FLAG("XPM", MF_CHECK | MF_GROUP1),
};

static PaintMenuBar externMenuBar[] =
{
    {None, "format", XtNumber(externMenu), externMenu},
};

typedef struct {
    float wpercent, hpercent;
    int pwidth, pheight, width, height, rx, ry, wsubdiv, hsubdiv;
    int papertype, dwidth, dheight, x, y;
    Boolean tied, orient, output;
    Colormap cmap;
    GC gc1, gc2;
    char *tmpfile;
    PaintWidget paintwidget;
    Widget shell, form, bar;
    Widget format, print, preview, cancel;
    Widget page, box, vp, equal;
    Widget wplus, wpplus, wminus, wmminus, wcenter;
    Widget hplus, hpplus, hminus, hmminus, hcenter;
    Widget portraitlabel, portraittoggle;
    Widget landscapelabel, landscapetoggle;
    Widget graylabel, graytoggle;
    Widget compresslabel, compresstoggle;
    Widget pdflabel, pdftoggle;
    Widget sizelabel, size;
    Widget positionlabel, position;
    Widget samplinglabel, sampling;
    Widget printlabel, printcmd, printtoggle;
    Widget filelabel, filename, fileselect, filetoggle;
    Widget psviewlabel, psviewcmd;
    Widget printerlist, printentry;
    Widget printermenu;
    Widget resultlabel, printresult;
    Widget formatChecks[XtNumber(formatMenu)];
} PrintInfo;

typedef struct {
    int mode;
    char *cmd;
    Colormap cmap;
    PaintWidget paintwidget;
    Widget shell, form, bar;
    Widget formatlabel, extviewlabel;
    Widget extview, viewbut, cancel;
    Widget formatChecks[XtNumber(externMenu)];
} ExternInfo;

static PrintInfo *printinfo = NULL;
static ExternInfo *externinfo = NULL;
static char *printcmd_str = NULL;
static char *printcmd_ini = NULL;
static char *psviewcmd_str = NULL;
static char **printer_names = NULL;
static int num_printers = -1;

#ifdef LPCCMD
static void
ListPrinters(const char * data, int i)
{
   char *list0, *list, *ptr;

   if (!data || !*data) return;
   list0 = strdup(data);
   list = list0;
   num_printers = 0;
   while ((ptr=index(list,':'))) {
      *ptr = '\0';
      list = ptr-1;
      while (list>list0 && !isspace(*list)) --list;
      if (list<list0 || isspace(*list)) ++list;
      if (*list) {
         ++num_printers;
         printer_names = (char **)realloc(printer_names, 
	 		                  (num_printers+1)*sizeof(char *));
         printer_names[num_printers] = strdup(list);
      }
      list = ptr+1;
   }
   free(list0);
   if (num_printers>0)
       printer_names[0] = strdup("(-)");
   else
       num_printers = -1;
}
  
#endif

#ifdef PRINTCAP
static void
parsePrintcap()
{
    FILE *fd;
    int jumpnext;
    char *printcap, *ptr0, *ptr1;
    char *pline;
    char line[1024];
   
    if (strlen(PRINTCAP)<=1) return;

    printcap = strdup(PRINTCAP);
    ptr0 = printcap;
    ptr1 = index(ptr0, ':');
    if (ptr1) *ptr1 = '\0';
   
    while((fd=fopen(ptr0,"r"))==NULL) {
	    if (ptr1) {
	       ptr0 = ptr1+1;
               ptr1 = index(ptr0, ':');
	       if (ptr1) *ptr1 = '\0';
	    }
	    else {
	       fprintf(stderr, msgText[CANT_FIND_PRINTCAP_FILE], printcap);
	       return;
	    }
    }

    num_printers = 0;   
    jumpnext = 0;
    while ((pline=fgets(line,1020,fd))!=NULL) {
            while(isspace(*pline) && *pline!='\0') ++pline;
            if (*pline=='\0' || *pline=='#') { 
	       jumpnext=0; 
	       continue;
	    }
	    pline=pline+strlen(pline)-1;
            if (*pline=='\n') {
	       *pline = '\0';
	       --pline;
	    }
            if(*pline=='\\') 
	      ++jumpnext;
            if (jumpnext>1) continue;
            pline=line;
	    while(*pline!=':' && pline<line+strlen(line)) ++pline;
	    *pline='\0';	
	    pline=line;
	    while(*pline!='|' && *pline!='\0') ++pline;
	    *pline='\0';
	    pline=line;
	    while(isspace(*pline) && *pline!='\0') ++pline;
            ++num_printers;
            printer_names = realloc(printer_names, 
				    (num_printers+1)*sizeof(char *));
            printer_names[num_printers] = strdup(pline);
    }
   
    fclose(fd);
    free(printcap);
    if (num_printers>0)
       printer_names[0] = strdup("(-)");
    else
       num_printers = -1;
}
#endif

static void
printerMenuSelect(Widget w, XtPointer junk, XtPointer garbage)
{
    char *name;
    if (!printcmd_ini) {
       XtVaGetValues(printinfo->printcmd, XtNstring, &name, NULL);
       printcmd_ini = strdup(name);
    } 
    XtVaGetValues(w, XtNlabel, &name, NULL);
    if (strcmp(name, "(-)")) {
       printcmd_str = realloc(printcmd_str,
          strlen(printcmd_ini)+strlen(name)+4);
       sprintf(printcmd_str, "%s -P%s", printcmd_ini, name);
    }
    else {
       if (printcmd_str) free(printcmd_str);
       printcmd_str = strdup(printcmd_ini);
    }
    XtVaSetValues(printinfo->printcmd, XtNstring, printcmd_str, NULL);
    XtVaSetValues(printinfo->printtoggle, XtNstate, True, NULL);   
    XtVaSetValues(printinfo->filetoggle, XtNstate, False, NULL);      
}

    
static void
showDrawing(PrintInfo * l)
{
    Display *dpy = XtDisplay(l->page);
    Window win = XtWindow(l->page);
    int i, x, y;

    i = l->papertype;
    x = l->x*l->pwidth/paper_sizes[i][0] - l->dwidth/2;
    y = l->y*l->pheight/paper_sizes[i][1] - l->dheight/2;

    if (x>=0)
        XClearArea(dpy, win, 0, 0, x, l->pheight+1, False);
    if (x+l->dwidth<l->pwidth)
        XClearArea(dpy, win, x+l->dwidth+1, 0, 
              l->pwidth-x-l->dwidth-1, l->pheight+1, False);
    if (y>=0) 
        XClearArea(dpy, win, 0, 0, l->pwidth+1, y, False);
    if (y+l->dheight<l->pheight)
        XClearArea(dpy, win, 0, y+l->dheight+1, l->pwidth+1, 
              l->pheight-y-l->dheight, False);
    XFillRectangle(dpy, win, l->gc1, x, y, l->dwidth, l->dheight);
    XDrawRectangle(dpy, win, l->gc2, x, y, l->dwidth, l->dheight);
    XDrawLine(dpy, win, l->gc2, x, y, x+l->dwidth, y+l->dheight);
    XDrawLine(dpy, win, l->gc2, x+l->dwidth, y, x, y+l->dheight);

    if (l->orient && l->dheight>4) {
        y = y+l->dheight/2;
        for (i=1; i<=3; i++) 
            XDrawLine(dpy, win, l->gc2, x+i, y-3+i, x+i, y+3-i);
    }
    if (!l->orient && l->dwidth>4) {
        x = x+l->dwidth/2;
        for (i=1; i<=3; i++) 
            XDrawLine(dpy, win, l->gc2, x-3+i, y+i, x+3-i, y+i);
    }
}

static void
drawArrows(Widget w, PrintInfo * l, XEvent * event, Boolean * flg)
{
    Display *dpy = XtDisplay(l->form);
    Window win = XtWindow(l->form);
    int i, j, top;

#ifdef XAW3D
    i = 126;
    j = PAGEHEIGHT+EXTRAHEIGHT+60;
#else
    i = 120;   
    j = PAGEHEIGHT+EXTRAHEIGHT+51;
#endif   
    XDrawLine(dpy, win, l->gc2, i, j-2, PAGEWIDTH+EXTRAWIDTH, j-2);
    XDrawLine(dpy, win, l->gc2, i, j+2, PAGEWIDTH+EXTRAWIDTH, j+2);
    XDrawLine(dpy, win, l->gc2, i-6, j, i+4, j+5);
    XDrawLine(dpy, win, l->gc2, i-6, j, i+4, j-5);

#ifdef XAW3D 
    i = PAGEWIDTH+EXTRAWIDTH+16;
    j = PAGEHEIGHT+EXTRAHEIGHT+42;
    top = 183;
#else
    i = PAGEWIDTH+EXTRAWIDTH+17;   
    j = PAGEHEIGHT+EXTRAHEIGHT+34;
    top = 172;
#endif   
    XDrawLine(dpy, win, l->gc2, i-2, top, i-2, j);
    XDrawLine(dpy, win, l->gc2, i+2, top, i+2, j);
    XDrawLine(dpy, win, l->gc2, i, top-6, i+5, top+4);
    XDrawLine(dpy, win, l->gc2, i, top-6, i-5, top+4);
}

static void 
closeMenus(Widget w, PrintInfo * l, XEvent * event, Boolean * flg)
{
    PopdownMenusGlobal();
}

static void
setPosition(PrintInfo *l)
{
    char val[120];
    l->rx = l->x - (int)((l->orient?l->height:l->width)*l->wpercent*0.005);
    l->ry = paper_sizes[l->papertype][1] - l->y -
            (int)((l->orient?l->width:l->height)*l->hpercent*0.005);
    sprintf(val, "%d pt ; %d pt", l->rx, l->ry);
    XtVaSetValues(l->position, XtNstring, val, NULL);
}

static void 
setDrawingPosition(Widget w, PrintInfo * l, XEvent * event, Boolean * flg)
{
    while (XCheckTypedWindowEvent(XtDisplay(w), XtWindow(w),
				  MotionNotify, (XEvent *) event));

    if (event->type == ButtonPress || event->type == MotionNotify) {
	l->x = event->xbutton.x * paper_sizes[l->papertype][0] / l->pwidth;
	l->y = event->xbutton.y * paper_sizes[l->papertype][1] / l->pheight;
	setPosition(l);
    }
    showDrawing(l);
}

static void 
doWritePS(PrintInfo *info, char *filename)
{
    Image * image;
    PageInfo pageinfo;
    char *title;

    image = PixmapToImage((Widget)info->paintwidget, 
			  info->paintwidget->paint.current.pixmap, info->cmap);

    memcpy(&pageinfo, info, sizeof(PageInfo));
    pageinfo.wbbox = paper_sizes[info->papertype][0];
    pageinfo.hbbox = paper_sizes[info->papertype][1];
    pageinfo.eps = False;

    XtVaGetValues(info->graytoggle, XtNstate, &pageinfo.gray, NULL);
    XtVaGetValues(info->compresstoggle, XtNstate, &pageinfo.compress, NULL);
    XtVaGetValues(info->pdftoggle, XtNstate, &pageinfo.pdf, NULL);
    XtVaGetValues(info->filename, XtNstring, &title, NULL);

    if (title && *title) 
        pageinfo.title = title;
    else
        pageinfo.title = filename;

    if ((pageinfo.orient = info->orient)) {
        int exch = pageinfo.hsubdiv;
	pageinfo.hsubdiv = pageinfo.wsubdiv;
	pageinfo.wsubdiv = exch;
    }
    WriteResizedPS(filename, image, &pageinfo);
    ImageDelete(image);
}

static void FlashString(char *str)
{
    int i;
    for (i=1; i<=4; i++) {
	XtVaSetValues(printinfo->printresult, XtNstring, str, NULL);
        XFlush(XtDisplay(printinfo->printresult));
        if (i==4) return;
        usleep(300000);
	XtVaSetValues(printinfo->printresult, XtNstring, "", NULL);
        XFlush(XtDisplay(printinfo->printresult));
        usleep(100000);
    }
}

static void 
printCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    PrintInfo *info = (PrintInfo *) infoArg;
    char cmd[512];
    char *name;
    Boolean dopdf;

    PopdownMenusGlobal();

    if (info->output) {
        XtVaGetValues(printinfo->filename, XtNstring, &name, NULL);
        XtVaGetValues(info->pdftoggle, XtNstate, &dopdf, NULL);

        if (dopdf) {
            doWritePS(info, info->tmpfile);
            sprintf(cmd, "ps2pdf %s %s", info->tmpfile, name);
            system(cmd);
            unlink(info->tmpfile);
	} else
            doWritePS(info, name);
        sprintf(cmd, msgText[FILE_WRITTEN], name);
    } else {
        char *name;
	XtVaGetValues(info->printcmd, XtNstring, &name, NULL);
        doWritePS(info, info->tmpfile);
        sprintf(cmd, "( %s %s ; rm -f %s ) &", 
            name, info->tmpfile, info->tmpfile);
        system(cmd);
        sprintf(cmd, msgText[FILE_SENT_TO_PRINTER], name);
    }
    FlashString(cmd);
}

static void 
selectionOkCallback(Widget w, XtPointer infoArg, char* file)
{
    PrintInfo *info = (PrintInfo *) infoArg;
    XtVaSetValues(info->filename, XtNstring, file, NULL);
    PopdownMenusGlobal();
}
 
static void 
fileselectCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    PrintInfo *info = (PrintInfo *) infoArg;
    char *name;
    PopdownMenusGlobal();
    XtVaGetValues(info->filename, XtNstring, &name, NULL);
    XtVaSetValues(info->filetoggle, XtNstate, True, NULL);
    GetFileName(GetToplevel(w), BROWSER_SIMPLESAVE, name, 
                (XtCallbackProc) selectionOkCallback, (XtPointer)infoArg);
}

static void 
previewCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    PrintInfo *info = (PrintInfo *) infoArg;
    char cmd[2048];
    char *ps_viewer = NULL;
    char *name = NULL;
    Boolean dopdf, dofile, bool;

    PopdownMenusGlobal();
    XtVaGetValues(info->psviewcmd, XtNstring, &ps_viewer, NULL);

    if (!ps_viewer) return;

    XtVaGetValues(info->filetoggle, XtNstate, &dofile, NULL);
    XtVaGetValues(info->pdftoggle, XtNstate, &dopdf, NULL);
    XtVaGetValues(info->filename, XtNstring, &name, NULL);

    doWritePS(info, info->tmpfile);

    if ((bool=dofile && name && *name)) {
       if (dopdf) 
          sprintf(cmd, "( ps2pdf %s %s ; %s %s ; rm -f %s ) &", 
	          info->tmpfile, name, ps_viewer, name, info->tmpfile);
       else
          sprintf(cmd, "mv -f %s %s ; %s %s &", 
	          info->tmpfile, name, ps_viewer, name);
       system(cmd);
    } else {
       sprintf(cmd, "( %s %s ; rm -f %s ) &", 
	       ps_viewer, info->tmpfile, info->tmpfile);
       system(cmd);
    }
    sprintf(cmd, msgText[FILE_WRITTEN], (bool)? name:info->tmpfile);
    FlashString(cmd);
}

static void 
closePrintCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    PrintInfo *info = (PrintInfo *) infoArg;
    Display *dpy;
    char *ptr;

    PopdownMenusGlobal();
    XtVaGetValues(info->printcmd, XtNstring, &ptr, NULL);
    if (ptr) {
       if (printcmd_str) free(printcmd_str);
       printcmd_str = strdup(ptr);
    }	
    XtVaGetValues(info->psviewcmd, XtNstring, &ptr, NULL);
    if (ptr) {
       if (psviewcmd_str) free(psviewcmd_str);
       psviewcmd_str = strdup(ptr);
    }	   
    dpy = XtDisplay(info->shell);
    XFreeGC(dpy, info->gc1);
    XFreeGC(dpy, info->gc2);
    XtDestroyWidget(info->shell);
    info->shell = 0;
}

void
setPageSize(int *w, int *h)
{
    int i, j;
    if (!printinfo) {
       *w = 595;
       *h = 842;
       return;
    }
    i = printinfo->papertype;
    if (printinfo->orient) j = 1; else j = 0;
    if (i<0 || i>=XtNumber(formatMenu)) i = 11;
    *w = paper_sizes[i][j];
    *h = paper_sizes[i][1-j];
}

static void
findDrawSize(PrintInfo *l)
{
    int i;

    i = l->papertype;
    l->pwidth = PAGEWIDTH;
    l->pheight = paper_sizes[i][1]*PAGEWIDTH/paper_sizes[i][0];
    if (l->pheight>PAGEHEIGHT) {
        l->pheight = PAGEHEIGHT;
	l->pwidth = paper_sizes[i][0]*PAGEHEIGHT/paper_sizes[i][1];
    }
    if (l->orient) {
        l->dwidth = (int)((float)(l->height*l->pwidth)*l->wpercent*0.01/
                      (float)(paper_sizes[i][0])+0.5);
        l->dheight = (int)((float)(l->width*l->pheight)*l->hpercent*0.01/
                      (float)(paper_sizes[i][1])+0.5);
    } else {
        l->dwidth = (int)((float)(l->width*l->pwidth)*l->wpercent*0.01/
                      (float)(paper_sizes[i][0])+0.5);
        l->dheight = (int)((float)(l->height*l->pheight)*l->hpercent*0.01/
                      (float)(paper_sizes[i][1])+0.5);
    }
    XtVaSetValues(l->page, XtNwidth, l->pwidth, XtNheight, l->pheight, NULL);
}

static void 
setPapertype(char * str)
{
    char *ptr;
    int i;

    printinfo->papertype = i = 0;
    if (!str || !*str) {
        XtVaGetValues(printinfo->format, XtNstring, &str, NULL);
    }
    if (str) {
        while (isspace(*str)) ++str;
	if (*str)
        for (i=0; i<XtNumber(formatMenu); i++) {
	    XtVaGetValues(printinfo->formatChecks[i], XtNlabel, &ptr, NULL);
	    if (!strcasecmp(str, ptr)) {
	        printinfo->papertype = i;
                break;
	    }
	}
    }
    findDrawSize(printinfo);
    i = printinfo->papertype;
    printinfo->x = paper_sizes[i][0]/2;
    printinfo->y = paper_sizes[i][1]/2;
    XtVaGetValues(printinfo->formatChecks[i], XtNlabel, &ptr, NULL);
    XtVaSetValues(printinfo->format, XtNstring, ptr, NULL);
    MenuCheckItem(printinfo->formatChecks[i], True);
}

static void 
formatCallback(Widget w, XtPointer larg, XtPointer junk)
{
    PrintInfo *l = (PrintInfo *) larg;
    int i;
    char *str;

    PopdownMenusGlobal();
    if (l->formatChecks[l->papertype] == w) return;
    MenuCheckItem(w, True);

    for (i=0; i<XtNumber(formatMenu); i++)
        if (l->formatChecks[i] == w) {
            XtVaGetValues(w, XtNlabel, &str, NULL);
            setPapertype(str);
	    setPosition(l);
	    showDrawing(l);
	    return;
	}
}

static void 
setPercent(PrintInfo * l)
{
    char str[80];
    char *ptr;
    int u, v;

    if (l->wpercent<0.5) l->wpercent =0.5;
    if (l->hpercent<0.5) l->hpercent =0.5;
    if (l->tied)
        sprintf(str, "%g%%", l->wpercent);
    else
        sprintf(str, "%g%% x %g%%", l->wpercent, l->hpercent);
    XtVaSetValues(l->size, XtNstring, str, NULL);

    XtVaGetValues(l->sampling, XtNstring, &ptr, NULL);
    if (sscanf(ptr, "%d x %d", &u, &v)==2) {
        if (u>=1 && v>=1) {
	    if (u>255) u = 255;
	    if (v>255) v = 255;
	    printinfo->wsubdiv = u;
            printinfo->hsubdiv = v;
	}
    }
    sprintf(str, "%d x %d (%d x %d dpi)", 
	    l->wsubdiv, l->hsubdiv,
            (int)(0.5 + 7200.0*l->wsubdiv / l->wpercent),
            (int)(0.5 + 7200.0*l->hsubdiv / l->hpercent));
    XtVaSetValues(l->sampling, XtNstring, str, NULL);

    findDrawSize(l);
    setPosition(l);
    showDrawing(l);
}

static void 
findPercent(char *str)
{
    if (!str || !*str) return;
    if (printinfo->tied) {
        sscanf(str, "%g%%", &printinfo->wpercent);
        printinfo->hpercent = printinfo->wpercent;
    } else
        sscanf(str, "%g%% x %g%%", &printinfo->wpercent, &printinfo->hpercent);
    setPercent(printinfo);
}

static void 
viewExternCallback(Widget w, XtPointer infoArg, XtPointer junk2);

static void 
textAction(Widget w, XEvent * event, String * prms,  Cardinal * nprms)
{
    char *str;
    int u, v;

    if (externinfo && w == externinfo->extview)
       viewExternCallback(w, (XtPointer) externinfo, NULL);

    if (!printinfo) return;

    if (w == printinfo->size) {
       XtVaGetValues(w, XtNstring, &str, NULL);
       findPercent(str);
       setPosition(printinfo);
       showDrawing(printinfo);
    }
    else
    if (w == printinfo->sampling) {
       setPercent(printinfo);
    }
    else
    if (w == printinfo->position) {
       char *ptr, val[120];
       XtVaGetValues(printinfo->position, XtNstring, &ptr, NULL);
       if (sscanf(ptr, "%d pt ; %d", &u, &v)==2) {
	   printinfo->rx = u;
           printinfo->ry = v;
	   printinfo->x = u + (int)((printinfo->orient?printinfo->height:printinfo->width) *
              printinfo->wpercent*0.005);
	   printinfo->y = paper_sizes[printinfo->papertype][1] - v -
              (int)((printinfo->orient?printinfo->width:printinfo->height) * 
              printinfo->hpercent*0.005);
	   showDrawing(printinfo);
       }
       sprintf(val, "%d pt ; %d pt", printinfo->rx, printinfo->ry);
       XtVaSetValues(printinfo->position, XtNstring, val, NULL);
    }
    else
    if (w == printinfo->format) {
       XtVaGetValues(w, XtNstring, &str, NULL);
       setPapertype(str);
       setPosition(printinfo);
       showDrawing(printinfo);
    }
}

static void 
switchCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    PrintInfo *info = (PrintInfo *) infoArg;
    char *name;
    struct stat buf;
    int i;
    Boolean state, statepdf;

    XtVaGetValues(w, XtNstate, &state, NULL);

    if (w == info->portraittoggle) {
       XtVaSetValues(info->landscapetoggle, XtNstate, !state, NULL);
       info->orient = !state;
       findDrawSize(info);       
       setPosition(info);
       showDrawing(info);
    }
    if (w == info->landscapetoggle) {
       XtVaSetValues(info->portraittoggle, XtNstate, !state, NULL);
       info->orient = state;
       findDrawSize(info);
       setPosition(info);
       showDrawing(info);
    }
    if (w == info->printtoggle) {
       XtVaSetValues(info->filetoggle, XtNstate, !state, NULL);
       info->output = !state;
    }
    if (w == info->pdftoggle) {
       w = info->filetoggle;
       state = True;
       XtVaSetValues(info->filetoggle, XtNstate, state, NULL);
    }
    if (w == info->filetoggle) {
       XtVaSetValues(info->printtoggle, XtNstate, !state, NULL);
       info->output = state;
       if (state) {
          XtVaGetValues(info->filename, XtNstring, &name, NULL);
	  if (!*name || !strncmp(name, "painting", 8)) {
	     i = 0;
	     name = xmalloc(20*sizeof(char));
	     incr:
             XtVaGetValues(info->pdftoggle, XtNstate, &statepdf, NULL);
             sprintf(name, "painting%d.%s", i, (statepdf)?"pdf":"ps");
	     if (stat(name, &buf)!=-1) {
	        ++i; 
		if (i<1000) goto incr;
	     }
	     XtVaSetValues(info->filename, XtNstring, name, NULL);
	     free(name);
	  }
       }
    }
    if (w == info->equal) {
       info->tied = state;
       if (state) {
	   if (info->hpercent > info->wpercent) 
               info->wpercent = info->hpercent;
	   if (info->wpercent > info->hpercent) 
               info->hpercent = info->wpercent;
       }
       setPercent(info);
    }
}

static void 
wcenterCallback(Widget w, XtPointer larg, XtPointer junk)
{
    PrintInfo *l = (PrintInfo *) larg;
    int i = l->papertype;
    l->x = paper_sizes[i][0]/2;
    setPosition(l);
    showDrawing(l);
}

static void 
wplusCallback(Widget w, XtPointer larg, XtPointer junk)
{
    PrintInfo *l = (PrintInfo *) larg;
    l->wpercent += 0.5;
    if (l->tied) l->hpercent = l->wpercent;
    setPercent(l);
}

static void 
wpplusCallback(Widget w, XtPointer larg, XtPointer junk)
{
    PrintInfo *l = (PrintInfo *) larg;
    l->wpercent += 5.0;
    if (l->tied) l->hpercent = l->wpercent;
    setPercent(l);
}

static void 
wminusCallback(Widget w, XtPointer larg, XtPointer junk)
{
    PrintInfo *l = (PrintInfo *) larg;
    l->wpercent -= 0.5;
    if (l->tied) l->hpercent = l->wpercent;
    setPercent(l);
}

static void 
wmminusCallback(Widget w, XtPointer larg, XtPointer junk)
{
    PrintInfo *l = (PrintInfo *) larg;
    l->wpercent -= 5.0;
    if (l->tied) l->hpercent = l->wpercent;
    setPercent(l);
}

static void 
hcenterCallback(Widget w, XtPointer larg, XtPointer junk)
{
    PrintInfo *l = (PrintInfo *) larg;
    int i = l->papertype;
    l->y = paper_sizes[i][1]/2;
    setPosition(l);
    showDrawing(l);
}

static void 
hplusCallback(Widget w, XtPointer larg, XtPointer junk)
{
    PrintInfo *l = (PrintInfo *) larg;
    l->hpercent += 0.5;
    if (l->tied) l->wpercent = l->hpercent;
    setPercent(l);
}

static void 
hpplusCallback(Widget w, XtPointer larg, XtPointer junk)
{
    PrintInfo *l = (PrintInfo *) larg;
    l->hpercent += 5.0;
    if (l->tied) l->wpercent = l->hpercent;
    setPercent(l);
}

static void 
hminusCallback(Widget w, XtPointer larg, XtPointer junk)
{
    PrintInfo *l = (PrintInfo *) larg;
    l->hpercent -= 0.5;
    if (l->tied) l->wpercent = l->hpercent;
    setPercent(l);
}

static void 
hmminusCallback(Widget w, XtPointer larg, XtPointer junk)
{
    PrintInfo *l = (PrintInfo *) larg;
    l->hpercent -= 5.0;
    if (l->tied) l->wpercent = l->hpercent;
    setPercent(l);
}

void createPrintData() 
{
    int fd;
    char buf[256];

    printinfo = (PrintInfo *) XtMalloc(sizeof(PrintInfo));
    printinfo->tied = True;
    printinfo->orient = False;
    printinfo->output = False;
    printinfo->page = 0;

    openTempDir(buf);
    buf[200] = '\0';
    strcat(buf, "/XPaint-XXXXXX");
    fd = mkstemp(buf);
    if (fd == -1) return;
    close(fd);
    unlink(buf);
    strcat(buf, ".ps");
    printinfo->tmpfile = XtNewString(buf);
}
void
printpopupResized(Widget w, PrintInfo * l, XConfigureEvent * event, 
                  Boolean * flg)
{
    Dimension width, height;
    Widget *wg;
    static int oldw;
    int x, y, i, n;
    XtVaGetValues(l->shell, XtNwidth, &width, XtNheight, &height, NULL);
    XtVaGetValues(l->sampling, XtNhorizDistance, &x, NULL);
    XResizeWindow(XtDisplay(l->form), XtWindow(l->form), width, height);

    XClearArea(XtDisplay(l->form), XtWindow(l->form),
               0, 30, PAGEWIDTH+100, PAGEHEIGHT+100, 0);
    oldw = PAGEWIDTH;
    PAGEWIDTH = width - x - 218;
    if (PAGEWIDTH<156) PAGEWIDTH = 156;
    PAGEHEIGHT = height - 87;
    if (PAGEHEIGHT<220) PAGEHEIGHT = 220;
    XtResizeWidget(l->vp, PAGEWIDTH+EXTRAWIDTH, PAGEHEIGHT+EXTRAHEIGHT, 0);
    XtResizeWidget(l->box, PAGEWIDTH+EXTRAWIDTH, PAGEHEIGHT+EXTRAHEIGHT, 0);
    XtResizeWidget(l->page, PAGEWIDTH, PAGEHEIGHT, 1);

#ifdef XAW3D   
    XtMoveWidget(l->equal, PAGEWIDTH+EXTRAWIDTH+8, PAGEHEIGHT+EXTRAHEIGHT+49);
#else   
    XtMoveWidget(l->equal, PAGEWIDTH+EXTRAWIDTH+8, PAGEHEIGHT+EXTRAHEIGHT+42);
#endif
   
    wg = &l->wplus;

    for (i=0; i<5; i++) {
       XtVaGetValues(wg[i], XtNx, &x, NULL);
#ifdef XAW3D       
       XtMoveWidget(wg[i], x, PAGEHEIGHT+EXTRAHEIGHT+49);
#else       
       XtMoveWidget(wg[i], x, PAGEHEIGHT+EXTRAHEIGHT+42);
#endif       
    }

    wg = &l->hplus;
    n = &l->printresult - wg;
    for (i=0; i<=n; i++) if (wg[i]) {
       XtVaGetValues(wg[i], XtNx, &x, XtNy, &y, NULL);
       XtMoveWidget(wg[i], x+PAGEWIDTH-oldw, y);
    }

    drawArrows(l->shell, l, NULL, False);
    findDrawSize(l);
    showDrawing(l);
}

void 
PrintPopup(Widget w, XtPointer paintArg)
{
    static Cursor pageCursor = None;
    static XGCValues gcv;
    static XtTranslations paint_trans = None;

    Display *dpy;
    Window root;
    char *defname = "yellow", *canvasfile;
    char wname[8];
    XColor scol, ecol;
    Pixel white, black, defpix;
    int i;
    Position x, y;
    Arg args[6];
    int nargs = 0, new = 0;

    dpy = XtDisplay(GetShell(w));
    root = RootWindowOfScreen(XtScreen(GetShell(w)));

    if (!printinfo) {
        new = 1;
	createPrintData();
	printinfo->shell = 0;
        printinfo->printerlist = 0;
    }

    if (printinfo->shell) {
        if (printinfo->paintwidget == (PaintWidget)paintArg) {
            RaiseWindow(dpy, XtWindow(printinfo->shell));
            return;
	} else
	    XtDestroyWidget(printinfo->shell);
    }
    printinfo->paintwidget = (PaintWidget)paintArg;

    canvasfile = printinfo->paintwidget->paint.filename;

    if (canvasfile) {
        char *ptr;
        canvasfile = xmalloc(strlen(canvasfile)+4);
	strcpy(canvasfile, ((PaintWidget) paintArg)->paint.filename);
	ptr = strrchr(canvasfile, '.');
	if (ptr) 
           strcpy(ptr, ".ps");
	else
           strcat(canvasfile, ".ps");
    }

    XtVaGetValues(GetShell(w), XtNcolormap, &printinfo->cmap, 
                  XtNx, &x, XtNy, &y, NULL);
    XtSetArg(args[nargs], XtNcolormap, printinfo->cmap); nargs++; 
    XtSetArg(args[nargs], XtNx,  x+24);  nargs++;
    XtSetArg(args[nargs], XtNy,  y+24);  nargs++;
    
    XtVaGetValues((Widget)printinfo->paintwidget, XtNdrawWidth, 
                  &printinfo->width, XtNdrawHeight, &printinfo->height, NULL);

    printinfo->shell = XtVisCreatePopupShell("print", topLevelShellWidgetClass,
				  GetShell(w), args, nargs);
    white = WhitePixelOfScreen(XtScreen(printinfo->shell));
    black = BlackPixelOfScreen(XtScreen(printinfo->shell));

    if (XAllocNamedColor(dpy, printinfo->cmap, defname, &scol, &ecol) != 0)
        defpix = scol.pixel;
    else 
    if (XLookupColor(dpy, printinfo->cmap, defname, &scol, &ecol) != 0)
        defpix = scol.pixel;
    else 
        defpix = black;

    gcv.foreground = defpix;
    printinfo->gc1 = XCreateGC(dpy, root, GCForeground, &gcv);
    gcv.foreground = black;
    printinfo->gc2 = XCreateGC(dpy, root, GCForeground, &gcv);

    printinfo->form = XtVaCreateManagedWidget("form", formWidgetClass, printinfo->shell, NULL);

    XtAddEventHandler(printinfo->form, ButtonPressMask,
                  False, (XtEventHandler)closeMenus, (XtPointer) printinfo);

    printinfo->bar = MenuBarCreate(printinfo->form, XtNumber(printMenuBar), printMenuBar);

    printinfo->format = XtVaCreateManagedWidget("formattype",
				       asciiTextWidgetClass, printinfo->form,
				       XtNtranslations, translations,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNresize, XawtextResizeWidth,
				       XtNfromHoriz, printinfo->bar,
				       XtNvertDistance, 7,
				       XtNwidth, 100,
				       NULL);

    printinfo->print = XtVaCreateManagedWidget("print",
				       commandWidgetClass, printinfo->form,
				       XtNfromHoriz, printinfo->format,
				       XtNvertDistance, 8,
				       NULL);

    printinfo->preview = XtVaCreateManagedWidget("preview",
					 commandWidgetClass, printinfo->form,
				         XtNfromHoriz, printinfo->print,
				         XtNhorizDistance, 9,
				         XtNvertDistance, 8,
					 NULL);

    printinfo->cancel = XtVaCreateManagedWidget("cancel",
					   commandWidgetClass, printinfo->form,
				           XtNfromHoriz, printinfo->preview,
				           XtNhorizDistance, 9,
				           XtNvertDistance, 8,
					   NULL);

    printinfo->vp = XtVaCreateManagedWidget("viewport", viewportWidgetClass, 
                                 printinfo->form,
				 XtNfromVert, printinfo->bar,
				 XtNvertDistance, 8,
				 XtNallowVert, False,
				 XtNallowHoriz, False,
				 XtNleft, XtChainLeft,
				 XtNright, XtChainRight,
				 XtNwidth, PAGEWIDTH+EXTRAWIDTH,
				 XtNheight, PAGEHEIGHT+EXTRAHEIGHT,
			         XtNbackground, white,
				 NULL);

    printinfo->box = XtVaCreateManagedWidget("printBox", boxWidgetClass, 
                            printinfo->vp,
			    XtNbackgroundPixmap, GetBackgroundPixmap(printinfo->vp),
			    NULL);

    printinfo->page = XtVaCreateManagedWidget("page", paintWidgetClass, 
                                   printinfo->box,
                                   XtNfromHoriz, printinfo->box,
				   XtNfillRule, FillSolid,
				   XtNzoom, 1,
				   XtNbackground, white,
			           XtNwidth, PAGEWIDTH, XtNheight, PAGEHEIGHT,
				   NULL);
    if (paint_trans == None) {
	paint_trans = XtParseTranslationTable(
	   "#override\n\t<BtnDown>,<BtnUp>,<BtnMotion>: \n");
    }
    XtVaSetValues(printinfo->page, XtNtranslations, paint_trans, NULL);
    XtVaSetValues(printinfo->page, XtNmenuwidgets, NULL, NULL);

    if (pageCursor == None)
        pageCursor = XCreateFontCursor(dpy, XC_crosshair);


    printinfo->wpplus = XtVaCreateManagedWidget("wpplus",
				       commandWidgetClass, printinfo->form,
				       XtNfromVert, printinfo->vp,
				       XtNwidth, 18,
				       NULL);

    printinfo->wplus = XtVaCreateManagedWidget("wplus",
				       commandWidgetClass, printinfo->form,
				       XtNfromVert, printinfo->vp,
				       XtNfromHoriz, printinfo->wpplus,
				       XtNwidth, 18,
				       NULL);

    printinfo->wminus = XtVaCreateManagedWidget("wminus",
				       commandWidgetClass, printinfo->form,
				       XtNfromVert, printinfo->vp,
				       XtNfromHoriz, printinfo->wplus,
				       XtNwidth, 18,
				       NULL);

    printinfo->wmminus = XtVaCreateManagedWidget("wmminus",
				       commandWidgetClass, printinfo->form,
				       XtNfromVert, printinfo->vp,
				       XtNfromHoriz, printinfo->wminus,
				       XtNwidth, 18,
				       NULL);

    printinfo->wcenter = XtVaCreateManagedWidget("wcenter",
				       commandWidgetClass, printinfo->form,
				       XtNfromVert, printinfo->vp,
				       XtNfromHoriz, printinfo->wmminus,
				       XtNwidth, 18,
				       NULL);

    printinfo->hpplus = XtVaCreateManagedWidget("hpplus",
				       commandWidgetClass, printinfo->form,
				       XtNfromHoriz, printinfo->vp,
				       XtNfromVert, printinfo->bar,
				       XtNvertDistance, 6,
				       XtNwidth, 18,
				       NULL);

    printinfo->hplus = XtVaCreateManagedWidget("hplus",
				       commandWidgetClass, printinfo->form,
				       XtNfromHoriz, printinfo->vp,
				       XtNfromVert, printinfo->hpplus,
				       XtNwidth, 18,
				       NULL);

    printinfo->hminus = XtVaCreateManagedWidget("hminus",
				       commandWidgetClass, printinfo->form,
				       XtNfromHoriz, printinfo->vp,
				       XtNfromVert, printinfo->hplus,
				       XtNwidth, 18,
				       NULL);

    printinfo->hmminus = XtVaCreateManagedWidget("hmminus",
				       commandWidgetClass, printinfo->form,
				       XtNfromHoriz, printinfo->vp,
				       XtNfromVert, printinfo->hminus,
				       XtNwidth, 18,
				       NULL);

    printinfo->hcenter = XtVaCreateManagedWidget("hcenter",
				       commandWidgetClass, printinfo->form,
				       XtNfromHoriz, printinfo->vp,
				       XtNfromVert, printinfo->hmminus,
				       XtNwidth, 18,
				       NULL);

    printinfo->equal = XtVaCreateManagedWidget("equal",
				       toggleWidgetClass, printinfo->form,
				       XtNfromVert, printinfo->vp,
				       XtNfromHoriz, printinfo->vp,
				       XtNstate, printinfo->tied,
				       XtNwidth, 18,
				       NULL);

    if (translations == None) {
	static XtActionsRec act =
	{"print-text-ok", (XtActionProc) textAction};

	XtAppAddActions(XtWidgetToApplicationContext(w), &act, 1);

	translations = XtParseTranslationTable(textTranslations);
    }

    printinfo->portraitlabel = XtVaCreateManagedWidget("portraitlabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNfromVert, printinfo->bar,
				       XtNhorizDistance, 30,
				       XtNvertDistance, 6,
				       NULL);

    printinfo->portraittoggle = XtVaCreateManagedWidget("portraittoggle",
				       toggleWidgetClass, printinfo->form,
                                       XtNstate, !printinfo->orient,
				       XtNfromHoriz, printinfo->portraitlabel,
				       XtNfromVert, printinfo->bar,
				       XtNhorizDistance, 12,
				       XtNvertDistance, 6,
				       XtNwidth, 18,
				       NULL);

    printinfo->landscapelabel = XtVaCreateManagedWidget("landscapelabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromHoriz, printinfo->portraittoggle,
				       XtNfromVert, printinfo->bar,
				       XtNvertDistance, 6,
				       NULL);

    printinfo->landscapetoggle = XtVaCreateManagedWidget("landscapetoggle",
				       toggleWidgetClass, printinfo->form,
                                       XtNstate, printinfo->orient,
				       XtNfromHoriz, printinfo->hpplus,
                                       XtNradioGroup,printinfo->portraittoggle,
				       XtNfromVert, printinfo->bar,
				       XtNhorizDistance, 324,
				       XtNvertDistance, 6,
				       XtNwidth, 18,
				       NULL);

    printinfo->graylabel = XtVaCreateManagedWidget("graylabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNfromVert, printinfo->portraittoggle,
				       XtNhorizDistance, 30,
				       XtNvertDistance, 6,
				       NULL);

    printinfo->graytoggle = XtVaCreateManagedWidget("graytoggle",
				       toggleWidgetClass, printinfo->form,
                                       XtNstate, 0,
				       XtNfromHoriz, printinfo->graylabel,
				       XtNfromVert, printinfo->portraittoggle,
				       XtNhorizDistance, 12,
				       XtNvertDistance, 6,
				       XtNwidth, 18,
				       NULL);

    printinfo->compresslabel = XtVaCreateManagedWidget("compresslabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromHoriz, printinfo->graytoggle,
				       XtNfromVert, printinfo->portraittoggle,
				       XtNhorizDistance, 16,
				       XtNvertDistance, 6,
				       NULL);

    printinfo->compresstoggle = XtVaCreateManagedWidget("compresstoggle",
				       toggleWidgetClass, printinfo->form,
                                       XtNstate, 0,
				       XtNfromHoriz, printinfo->compresslabel,
				       XtNfromVert, printinfo->portraittoggle,
				       XtNhorizDistance, 12,
				       XtNvertDistance, 6,
				       XtNstate, True,
				       XtNwidth, 18,
				       NULL);

    printinfo->pdflabel = XtVaCreateManagedWidget("pdflabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNfromVert, printinfo->portraittoggle,
				       XtNvertDistance, 6,
				       XtNhorizDistance, 280,
				       NULL);

    printinfo->pdftoggle = XtVaCreateManagedWidget("pdftoggle",
				       toggleWidgetClass, printinfo->form,
                                       XtNstate, 0,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNfromVert, printinfo->portraittoggle,
				       XtNhorizDistance, 324,
				       XtNvertDistance, 6,
				       XtNwidth, 18,
				       NULL);

    printinfo->sizelabel = XtVaCreateManagedWidget("sizelabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNfromVert, printinfo->graytoggle,
				       XtNhorizDistance, 30,
				       XtNvertDistance, 12,
				       NULL);

    printinfo->size = XtVaCreateManagedWidget("sizevalue",
				       asciiTextWidgetClass, printinfo->form,
				       XtNtranslations, translations,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNresize, XawtextResizeWidth,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNfromVert, printinfo->graytoggle,
				       XtNwidth, 150,
				       XtNvertDistance, 12,
				       NULL);

    printinfo->positionlabel = XtVaCreateManagedWidget("positionlabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNfromVert, printinfo->size,
				       XtNhorizDistance, 30,
				       NULL);

    printinfo->position = XtVaCreateManagedWidget("positionvalue",
				       asciiTextWidgetClass, printinfo->form,
				       XtNtranslations, translations,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNresize, XawtextResizeWidth,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNfromVert, printinfo->size,
				       XtNwidth, 150,
				       NULL);

    printinfo->samplinglabel = XtVaCreateManagedWidget("samplinglabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNfromVert, printinfo->position,
				       XtNhorizDistance, 30,
				       NULL);

    printinfo->sampling = XtVaCreateManagedWidget("samplingvalue",
				       asciiTextWidgetClass, printinfo->form,
				       XtNtranslations, translations,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNresize, XawtextResizeWidth,
				       XtNfromVert, printinfo->position,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNwidth, 150,
				       NULL);

    printinfo->printlabel = XtVaCreateManagedWidget("printlabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromVert, printinfo->sampling,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNhorizDistance, 30,
				       XtNvertDistance, 14,
				       NULL);

    printinfo->printcmd = XtVaCreateManagedWidget("printcmd",
				       asciiTextWidgetClass, printinfo->form,
				       XtNtranslations, translations,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNscrollHorizontal, True,
				       XtNfromVert, printinfo->sampling,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNvertDistance, 14,
				       XtNwidth, 150, XtNheight, 40,
				       NULL);
    if (printcmd_str) 
       XtVaSetValues(printinfo->printcmd, XtNstring, printcmd_str, NULL);

#ifdef PRINTCAP
    if (strlen(PRINTCAP)>=1) {
        for(i=0; i<=num_printers; i++) free(printer_names[i]);
        parsePrintcap();
#else
#ifdef LPCCMD
        for(i=0; i<=num_printers; i++) free(printer_names[i]);
        pipe_command(LPCCMD, NULL, ListPrinters, NULL);
#endif
#endif  
        if (num_printers>0) {
           printinfo->printerlist = XtVaCreateManagedWidget("printerlist",
                                       menuButtonWidgetClass, printinfo->form,
				       XtNmenuName, "printermenu",
				       XtNborderWidth, 1,
				       XtNlabel, "?",
				       XtNwidth, 18,
				       XtNheight, 16,
				       XtNfromVert, printinfo->sampling,
				       XtNfromHoriz, printinfo->printcmd,
				       XtNvertDistance, 15,
#ifdef XAW3D
                                       XtNborderWidth, 0,							    
#endif							    
				       NULL);
          printinfo->printermenu = XtVaCreatePopupShell("printermenu",
				       simpleMenuWidgetClass, 
                                       printinfo->printerlist,
				       NULL);
          for (i = 0; i<=num_printers; i++) {
	        sprintf(wname, "%d", i);
	        printinfo->printentry = 
	        XtVaCreateManagedWidget(wname, smeBSBObjectClass,
				       printinfo->printermenu, 
				       XtNlabel, printer_names[i], NULL);
	      XtAddCallback(printinfo->printentry, XtNcallback, printerMenuSelect, NULL);
	  }
	}
#ifdef PRINTCAP       
    }
#endif

    printinfo->printtoggle = XtVaCreateManagedWidget("printtoggle",
				       toggleWidgetClass, printinfo->form,
                                       XtNstate, !printinfo->output,
				       XtNfromVert, printinfo->sampling,
				       XtNfromHoriz, printinfo->printcmd,
				       XtNvertDistance, 
					  (printinfo->printerlist==0)? 28: 37,
				       XtNwidth, 18,
				       NULL);
   
    printinfo->filelabel = XtVaCreateManagedWidget("filelabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromVert, printinfo->printcmd,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNhorizDistance, 30,
				       NULL);

    printinfo->filename = XtVaCreateManagedWidget("filename",
				       asciiTextWidgetClass, printinfo->form,
				       XtNtranslations, translations,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNscrollHorizontal, True,
				       XtNfromVert, printinfo->printcmd,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNwidth, 150, XtNheight, 40,
				       NULL);
    if (canvasfile) {
        XtVaSetValues(printinfo->filename, XtNstring, canvasfile, NULL);
	free(canvasfile);
    }

    printinfo->fileselect = XtVaCreateManagedWidget("fileselect",
				       commandWidgetClass, printinfo->form,
				       XtNlabel, "?",
				       XtNwidth, 18,
				       XtNheight, 16,
				       XtNfromVert, printinfo->printcmd,
				       XtNfromHoriz, printinfo->filename,
                                       XtNvertDistance, 5,
				       NULL);

    printinfo->filetoggle = XtVaCreateManagedWidget("filetoggle",
				       toggleWidgetClass, printinfo->form,
                                       XtNstate, printinfo->output,
				       XtNfromVert, printinfo->fileselect,
				       XtNfromHoriz, printinfo->filename,
				       XtNradioGroup, printinfo->printtoggle,
                                       XtNvertDistance, 5,
				       XtNwidth, 18,
				       NULL);

    printinfo->psviewlabel = XtVaCreateManagedWidget("psviewlabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromVert, printinfo->filename,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNhorizDistance, 30,
				       NULL);

    printinfo->psviewcmd = XtVaCreateManagedWidget("psviewcmd",
				       asciiTextWidgetClass, printinfo->form,
				       XtNtranslations, translations,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNscrollHorizontal, True,
				       XtNfromVert, printinfo->filename,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNwidth, 150, XtNheight, 40,
				       NULL);
   
    printinfo->resultlabel = XtVaCreateManagedWidget("resultlabel",
				       labelWidgetClass, printinfo->form,
				       XtNborderWidth, 0,
				       XtNfromVert, printinfo->psviewcmd,
				       XtNfromHoriz, printinfo->hpplus,
				       XtNvertDistance, 14,
				       XtNhorizDistance, 30,
				       NULL);

    printinfo->printresult = XtVaCreateManagedWidget("printresult",
				       asciiTextWidgetClass, printinfo->form,
				       XtNtranslations, translations,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNscrollHorizontal, True,
				       XtNfromVert, printinfo->psviewcmd,
				       XtNfromHoriz, printinfo->resultlabel,
				       XtNvertDistance, 14,
				       XtNheight, 40,
				       NULL);

    if (psviewcmd_str) 
       XtVaSetValues(printinfo->psviewcmd, XtNstring, psviewcmd_str, NULL);

    XtAddEventHandler(printinfo->shell, StructureNotifyMask, False,
	      (XtEventHandler) printpopupResized, (XtPointer) printinfo);

    XtAddEventHandler(printinfo->form, ExposureMask,
		  False, (XtEventHandler) drawArrows, (XtPointer) printinfo);

    XtAddEventHandler(printinfo->page,
                  ButtonPressMask | ButtonMotionMask | ExposureMask,
		  False, (XtEventHandler)setDrawingPosition, (XtPointer) printinfo);

    XtAddCallback(XtNameToWidget(printinfo->bar, "file.fileMenu.print"),
		  XtNcallback, printCallback, (XtPointer) printinfo);

    XtAddCallback(XtNameToWidget(printinfo->bar, "file.fileMenu.preview"),
		  XtNcallback, previewCallback, (XtPointer) printinfo);

    XtAddCallback(XtNameToWidget(printinfo->bar, "file.fileMenu.close"),
		  XtNcallback, closePrintCallback, (XtPointer) printinfo);

    for (i=0; i<XtNumber(formatMenu); i++) {
        XtAddCallback(formatMenu[i].widget,
		  XtNcallback, formatCallback, (XtPointer) printinfo);
        printinfo->formatChecks[i] = formatMenu[i].widget;
    }

    XtAddCallback(printinfo->wcenter, XtNcallback,
		  (XtCallbackProc) wcenterCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->wplus, XtNcallback,
		  (XtCallbackProc) wplusCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->wpplus, XtNcallback,
		  (XtCallbackProc) wpplusCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->wminus, XtNcallback,
		  (XtCallbackProc) wminusCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->wmminus, XtNcallback,
		  (XtCallbackProc) wmminusCallback, (XtPointer) printinfo);

    XtAddCallback(printinfo->hcenter, XtNcallback,
		  (XtCallbackProc) hcenterCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->hplus, XtNcallback,
		  (XtCallbackProc) hplusCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->hpplus, XtNcallback,
		  (XtCallbackProc) hpplusCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->hminus, XtNcallback,
		  (XtCallbackProc) hminusCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->hmminus, XtNcallback,
		  (XtCallbackProc) hmminusCallback, (XtPointer) printinfo);

    XtAddCallback(printinfo->portraittoggle, XtNcallback,
		  (XtCallbackProc) switchCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->landscapetoggle, XtNcallback,
		  (XtCallbackProc) switchCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->pdftoggle, XtNcallback,
		  (XtCallbackProc) switchCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->printtoggle, XtNcallback,
		  (XtCallbackProc) switchCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->filetoggle, XtNcallback,
		  (XtCallbackProc) switchCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->equal, XtNcallback,
		  (XtCallbackProc) switchCallback, (XtPointer) printinfo);

    XtAddCallback(printinfo->print, XtNcallback,
		  (XtCallbackProc) printCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->preview, XtNcallback,
		  (XtCallbackProc) previewCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->fileselect, XtNcallback,
		  (XtCallbackProc) fileselectCallback, (XtPointer) printinfo);
    XtAddCallback(printinfo->cancel, XtNcallback,
		  (XtCallbackProc) closePrintCallback, (XtPointer) printinfo);
    AddDestroyCallback(printinfo->shell, (DestroyCallbackFunc) closePrintCallback, printinfo);

    XtPopup(printinfo->shell, XtGrabNone);
    XtUnmanageChild(printinfo->form);
    XMapWindow(XtDisplay(printinfo->form), XtWindow(printinfo->form));

    if (new) {
        char *ptr;
	int u, v;
        XtVaGetValues(printinfo->format, XtNstring, &ptr, NULL);
        setPapertype(ptr);
	XtVaGetValues(printinfo->sampling, XtNstring, &ptr, NULL);
	printinfo->wsubdiv = 1;
	printinfo->hsubdiv = 1;
        if (sscanf(ptr, "%d x %d", &u, &v)==2) {
            if (u>=1 && v>=1) {
	        if (u>255) u = 255;
	        if (v>255) v = 255;
	        printinfo->wsubdiv = u;
                printinfo->hsubdiv = v;
	    }
        }
        XtVaGetValues(printinfo->size, XtNstring, &ptr, NULL);
        findPercent(ptr);
    } else {
        char *ptr;
        /* recover values from previous printing session */
        int x = printinfo->x, y = printinfo->y;
        XtVaGetValues(printinfo->formatChecks[printinfo->papertype],
            XtNlabel, &ptr, NULL);
	setPapertype(ptr);
	printinfo->x = x;
	printinfo->y = y;
	setPercent(printinfo);
    }

    XDefineCursor(dpy, XtWindow(printinfo->page), pageCursor);
}

static void 
selectCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    ExternInfo *l = (ExternInfo *) infoArg;
    int i;
    char *str;

    for (i=0; i<XtNumber(externMenu); i++)
        if (l->formatChecks[i] == w) {
            XtVaGetValues(w, XtNlabel, &str, NULL);
            XtVaSetValues(l->formatlabel, XtNlabel, str, NULL);
	    MenuCheckItem(w, True);
	    l->mode = i;
	    break;
	}
}
   
static void 
closeExternCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    ExternInfo *info = (ExternInfo *) infoArg;
    PopdownMenusGlobal();    
    XtDestroyWidget(info->shell);
    info->shell = 0;
}

static void 
viewExternCallback(Widget w, XtPointer infoArg, XtPointer junk2)
{
    ExternInfo *info = (ExternInfo *) infoArg;
    Image *image;
    static char suffix[8];
    char cmd[512], buf[256];
    char *extviewcmd, *tmp, *sep, *file = NULL;
    int i, del, fd;

    XtVaGetValues(info->extview, XtNstring, &extviewcmd, NULL);
    if (!extviewcmd || !*extviewcmd) return;
    info->cmd = (char *)realloc(info->cmd, strlen(extviewcmd)+1);
    strcpy(info->cmd, extviewcmd);

    XtVaGetValues(info->formatChecks[info->mode], XtNlabel, &tmp, NULL);
    strcpy(suffix, tmp);
    i = 0;
    while (suffix[i]) {
       suffix[i] = tolower(suffix[i]);
       i++;
    }
    del = 1;
    sep = strrchr(info->cmd, ' ');
    if (sep) {
        ++sep;
	sprintf(cmd, ".%s", suffix);
	if (strstr(sep, cmd)) {
	file = strdup(sep);
	*sep = '\0';
        del = 0;
	}
    }

    if (!file) {
       if ((tmp = getenv("TMPDIR")) == NULL)
          tmp = P_tmpdir;
        strncpy(buf, tmp, 220);
        buf[220] = '\0';
        strcat(buf, "/XPaint-XXXXXX");
        fd = mkstemp(buf);
        if (fd == -1) return;
        close(fd);
        unlink(buf);
        strcat(buf, ".");
	strcat(buf, suffix);
        file = XtNewString(buf);
    }
    if (del)
        sprintf(cmd, "(%s %s ; rm -f %s) &", info->cmd, file, file);
    else {
        sprintf(cmd, "%s %s &", info->cmd, file);
        *sep = ' ';
    }

    StateSetBusy(True);    
    image = PixmapToImage((Widget)info->paintwidget, 
        info->paintwidget->paint.current.pixmap, info->cmap);
    if (!image) {
        StateSetBusy(False);
	free(file);
	return;
    }

    outputImage = NULL;
    if (info->mode == 0)
        WriteGIF(file, image);
    if (info->mode == 1)
        WritePNGn(file, image);
    if (info->mode == 2)
        WritePNM(file, image);
    if (info->mode == 3)
        WriteXPM(file, image);
    if (info->mode > 0) {
        if (outputImage)
            ImageDelete(outputImage);
        else
            ImageDelete(image);
    }
    StateSetBusy(False);

    system(cmd);
    free(file);
}

void
externpopupResized(Widget w, ExternInfo * l, XConfigureEvent * event, 
                  Boolean * flg)
{
    Dimension width, height, width1, height1;
    int x;

    XtVaGetValues(l->shell, XtNwidth, &width, XtNheight, &height, NULL);
    XResizeWindow(XtDisplay(l->form), XtWindow(l->form), width, height);
    XtVaGetValues(l->extview, XtNx, &x, NULL);
    width1 = width - x - 8;
    if (width1 < 100) width1 = 100;
    height1 = height - 58;
    if (height1 < 40) height1 = 40;
    XtMoveWidget(l->extview, x, 30);
    XtResizeWidget(l->extview, width1, height1, 0);
    XtVaGetValues(l->viewbut, XtNx, &x, NULL);
    XtMoveWidget(l->viewbut, x, height-25);
    XtVaGetValues(l->cancel, XtNx, &x, NULL);
    XtMoveWidget(l->cancel, x, height-25);
}

void 
ExternPopup(Widget w, XtPointer paintArg)
{
    Display *dpy;
    Window root;
    Colormap cmap;
    Position x, y;
    Arg args[6];
    int i, nargs = 0;
    char *str;

    dpy = XtDisplay(w);
    root = RootWindowOfScreen(XtScreen(w));

    if (!externinfo) {
        externinfo = (ExternInfo *) XtMalloc(sizeof(ExternInfo));
	externinfo->mode = -1;
	externinfo->cmd = NULL;
	externinfo->shell = 0;
    }
    if (externinfo->shell) {
        if (externinfo->paintwidget == (PaintWidget)paintArg) {
            RaiseWindow(dpy, XtWindow(externinfo->shell));
            return;
	} else
	    XtDestroyWidget(externinfo->shell);
    }
    externinfo->paintwidget = (PaintWidget)paintArg;

    XtVaGetValues(w, XtNcolormap, &cmap, 
                  XtNx, &x, XtNy, &y, NULL);
    XtSetArg(args[nargs], XtNcolormap, cmap); nargs++; 
    externinfo->cmap = cmap;

    XtSetArg(args[nargs], XtNx,  x+24);  nargs++;
    XtSetArg(args[nargs], XtNy,  y+24);  nargs++;

    externinfo->shell = 
        XtVisCreatePopupShell("extern", topLevelShellWidgetClass,
			      GetShell(w), args, nargs);

    externinfo->form = 
        XtVaCreateManagedWidget("form", formWidgetClass, externinfo->shell, NULL);

    externinfo->bar = 
        MenuBarCreate(externinfo->form, XtNumber(externMenuBar), externMenuBar);

    externinfo->formatlabel = XtVaCreateManagedWidget("formatlabel",
				       labelWidgetClass, externinfo->form,
				       XtNfromHoriz, externinfo->bar,
				       XtNborderWidth, 0,
				       NULL);

    externinfo->extviewlabel = XtVaCreateManagedWidget("extviewlabel",
				       labelWidgetClass, externinfo->form,
				       XtNfromVert, externinfo->bar,
				       XtNvertDistance, 10,
				       XtNborderWidth, 0,
				       NULL);

    if (translations == None) {
	static XtActionsRec act =
	{"print-text-ok", (XtActionProc) textAction};

	XtAppAddActions(XtWidgetToApplicationContext(w), &act, 1);

	translations = XtParseTranslationTable(textTranslations);
    }

    externinfo->extview = XtVaCreateManagedWidget("extviewcmd",
				       asciiTextWidgetClass, externinfo->form,
				       XtNtranslations, translations,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNscrollHorizontal, True,
				       XtNwidth, 150, XtNheight, 40,
				       XtNfromVert, externinfo->bar,
				       XtNvertDistance, 10,
				       XtNfromHoriz, externinfo->extviewlabel,
				       NULL);
    if (externinfo->cmd)
        XtVaSetValues(externinfo->extview, XtNstring, externinfo->cmd, NULL);

    externinfo->viewbut = XtVaCreateManagedWidget("view",
				       commandWidgetClass, externinfo->form,
				       XtNfromVert, externinfo->extview,
				       NULL);

    externinfo->cancel = XtVaCreateManagedWidget("cancel",
				       commandWidgetClass, externinfo->form,
				       XtNfromVert, externinfo->extview,
				       XtNfromHoriz, externinfo->viewbut,
				       NULL);

    for (i=0; i<XtNumber(externMenu); i++) {
        XtAddCallback(externMenu[i].widget,
		  XtNcallback, selectCallback, (XtPointer) externinfo);
        externinfo->formatChecks[i] = externMenu[i].widget;
    }

    if (externinfo->mode == -1) {
        externinfo->mode = 2;
        XtVaGetValues(externinfo->formatlabel, XtNlabel, &str, NULL);
        if (str && *str) {
            char *ptr;
            for (i=0; i<XtNumber(externMenu); i++) {
	        XtVaGetValues(externinfo->formatChecks[i], XtNlabel,&ptr,NULL);
	        if (!strcasecmp(str, ptr)) {
	            externinfo->mode = i;
                    break;
		}
	    }
	}
    }
    XtVaGetValues(externinfo->formatChecks[externinfo->mode], 
                  XtNlabel, &str, NULL);
    XtVaSetValues(externinfo->formatlabel, XtNlabel, str, NULL);
    MenuCheckItem(externinfo->formatChecks[externinfo->mode], True);

    XtAddCallback(externinfo->viewbut, XtNcallback,
		  (XtCallbackProc) viewExternCallback, (XtPointer) externinfo);

    XtAddCallback(externinfo->cancel, XtNcallback,
		  (XtCallbackProc)closeExternCallback, (XtPointer) externinfo);

    AddDestroyCallback(externinfo->shell, (DestroyCallbackFunc) closeExternCallback, 
                  externinfo);

    XtPopup(externinfo->shell, XtGrabNone);
    XtUnmanageChild(externinfo->form);
    XMapWindow(XtDisplay(externinfo->form), XtWindow(externinfo->form));

    XtAddEventHandler(externinfo->shell, StructureNotifyMask, False,
	      (XtEventHandler) externpopupResized, (XtPointer) externinfo);
}

void
checkExternalLink(Widget paint)
{
    PaintWidget pw = (PaintWidget) paint;

    if (printinfo && printinfo->shell && printinfo->paintwidget == pw) {
         XtDestroyWidget(printinfo->shell);
	 printinfo->shell = 0;
    }
    if (externinfo && externinfo->shell && externinfo->paintwidget == pw) {
         XtDestroyWidget(externinfo->shell);
	 externinfo->shell = 0;
    }
}
