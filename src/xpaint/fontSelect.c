/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)	       | */
/* |								       | */
/* | Permission to use, copy, modify, and to distribute this software  | */
/* | and its documentation for any purpose is hereby granted without   | */
/* | fee, provided that the above copyright notice appear in all       | */
/* | copies and that both that copyright notice and this permission    | */
/* | notice appear in supporting documentation.	 There is no	       | */
/* | representations about the suitability of this software for	       | */
/* | any purpose.  this software is provided "as is" without express   | */
/* | or implied warranty.					       | */
/* |								       | */
/* +-------------------------------------------------------------------+ */

/* $Id: fontSelect.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <math.h>
#include <stdio.h>
#include <locale.h>
#include <ctype.h>
#include <math.h>

#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xos.h>
#include <X11/Xft/Xft.h>

#include "xaw_incdir/Form.h"
#include "xaw_incdir/Paned.h"
#include "xaw_incdir/List.h"
#include "xaw_incdir/AsciiText.h"
#include "xaw_incdir/Command.h"
#include "xaw_incdir/Viewport.h"
#include "xaw_incdir/Scrollbar.h"
#include "xaw_incdir/Toggle.h"

#include "Paint.h"
#include "menu.h"

#define M_PI_180 M_PI/180

#define XftNameUnparseAlloc FcNameUnparse
#define DEFAULT_XFT_TEXT "\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\n\
abcdefghijklmnopqrstuvwxyz\n\
0123456789-+*/=<>.,:;?!&~#(){}[]'`\"^@$\\\\\n"

/*
**  swiped from X11/Xfuncproto.h
**   since qsort() may or may not be defined with a constant sub-function
 */
#ifndef _Xconst
#if __STDC__ || defined(__cplusplus) || defined(c_plusplus) || (FUNCPROTO&4)
#define _Xconst const
#else
#define _Xconst
#endif
#endif				/* _Xconst */

#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#endif

#include "xpaint.h"
#include "messages.h"
#include "misc.h"
#include "operation.h"
#include "ops.h"
#include "graphic.h"
#include "protocol.h"

#if defined(XAWPLAIN) || defined(XAW3D)
#define BORDERWIDTH 1
#else
#define BORDERWIDTH 0
#endif

extern void  removeTempFile(void);

typedef struct {
    int num_fonts, num_families, num_weights;
    char *font_select;
    char *weight_select;
    char **font_desc;
    char **font_family;
    char **weight_list;
    XftFont * xft_font;
    char * xft_text;
    char * xft_name;
    double xft_size, xft_height, xft_ascent, xft_descent, xft_maxadv;
    double xft_rotation, xft_inclination, xft_dilation, xft_linespacing;
    Pixmap pixmap;
    Widget shell, form1, form2, subform;
    Widget vport0, vport1, vport2, sample, paint;
    Widget family, familyLabel;
    Widget weight, weightLabel;
    Widget pointBar, pointSelect, pointSelectLabel;
    Widget rotation, rotationLabel;
    Widget dilation, dilationLabel;
    Widget linespacing, linespacingLabel;
    Widget inclination, inclinationLabel;
    Widget editButton, applyButton, doneButton, menubar;
} arg_t;

static arg_t *theArg = NULL;

static int 
strqsortcmp(char **a, char **b)
{
    if (**a == ':') return 1;
    if (**b == ':') return -1;
    return strcasecmp(*a, *b);
}

int GetCharLength(char *buf)
{
    int j;
    char c = buf[0];
    if (c & 0x80) 
        c = c<<1;
    else 
        return 1;
    j = 1;
    do {
        ++j;
        c = c<<1;
    } while ((c & 0x80) && j <= 4);
    return j;
}

void
FreeTypeDrawString(Display *dpy, arg_t * arg,
                   XftDraw *draw, XftColor * color,
                   char *buf, int len, int x, int y, double r)
{
    int j, charlen;
    double dx=0.5, dy=0.5, a, u, v;
    XGlyphInfo extents = {};

    u = cos(r);
    v = sin(r);

    for (j=0; j<len; j++) {
        charlen = GetCharLength(buf+j);
        XftDrawStringUtf8(draw, color, arg->xft_font,
		       x+((int)dx)+ v*arg->xft_ascent, 
                       y+((int)dy)+ u*arg->xft_ascent, 
                       (XftChar8*)(buf+j), charlen);
        XftTextExtentsUtf8(dpy, arg->xft_font,
			(XftChar8*)(buf+j), charlen, 
                        (XGlyphInfo*)&extents);
        a = sqrt(extents.xOff*extents.xOff+extents.yOff*extents.yOff);
        dx += a * u;
        dy -= a * v;
        j += charlen-1;
    }
}

static void 
setSamplePixmap(arg_t * arg)
{
    XtVaSetValues(arg->sample, XtNbackgroundPixmap, None, NULL);
    if (arg->pixmap)
        XtVaSetValues(arg->sample, XtNbackgroundPixmap, arg->pixmap, NULL);
    /* force refresh  */
    XUnmapWindow(XtDisplay(arg->sample), XtWindow(arg->sample));
    XMapWindow(XtDisplay(arg->sample), XtWindow(arg->sample));
}

static void
cleanSamplePixmap(arg_t * arg)
{
    Display * dpy = XtDisplay(arg->shell);
    GC gc;
    int w, h, d;
    if (arg->pixmap) {
        GetPixmapWHD(dpy, arg->pixmap, &w, &h, &d);
        gc = XCreateGC(dpy, arg->pixmap, 0, NULL);
        XSetForeground(dpy, gc, WhitePixelOfScreen(XtScreen(arg->shell)));
        XFillRectangle(dpy, arg->pixmap, gc, 0, 0, w, h);
        XFreeGC(dpy, gc);
    }
    setSamplePixmap(arg);
}

static void editCallback(Widget w, XtPointer argArg, XtPointer junk);

static void string_extract(char **str, char **ret)
{
    char *ptr;
    int bs, k;
 
    ptr = *str;
    k = 0;
    bs = 0;
    *ret = (char *)realloc(*ret, 1);
    (*ret)[0] = '\0';

    while (*ptr) {
	if (!bs && *ptr == '\\' && *(ptr+1) == '*') {
            ptr =  strstr(ptr+2, "*\\");
            if (!ptr)
                ptr =  strchr(ptr+2, '\n');
            if (!ptr) break;
            ptr += 2;
	}
        if (bs) {
	    if (*ptr == '\n') {
	        ++ptr;
                bs = 0;
	    }
	} else {
	    if (*ptr == '\n') {
	        ++ptr;
                break;
	    }
        }
        if (!bs && *ptr == '\\') {
	    bs = 1;
            ++ptr;
            continue;
	}
        bs = 0;
	(*ret)[k] = *ptr;
	++k;
        *ret = (char *)realloc(*ret, k+1);
        (*ret)[k] = '\0';
        ++ptr;
    }
    *str = ptr;
}

static void 
setXftFont(arg_t * arg)
{
    Display *dpy = XtDisplay(arg->shell);
    XGlyphInfo extents = {};
    Colormap cmap;
    GC gc;
    Dimension width, height;
    Boolean bool;

    XftColor color;
    XftDraw *draw = NULL;
    XRenderColor xre_color;
    char matrix[80];
    char *ptr, *str;
    double r=0.0, i=0.0, d=1.0, ls=1.0, hd;
    double mat[4];
    int j, x, y, w, x0, x1, x2, y0, y1, y2;

    if (!arg->font_select) {
        cleanSamplePixmap(arg);
        return;
    }

    XtVaGetValues(arg->shell, XtNcolormap, &cmap, NULL);
    XtVaGetValues(arg->vport2, XtNwidth, &width, XtNheight, &height, NULL);
    XtVaGetValues(arg->pointSelect, XtNstring, &ptr, NULL);
    if (ptr) {
        arg->xft_size = atof(ptr);
        if (arg->xft_size<1.0) arg->xft_size = 1.0;
        if (arg->xft_size>300.0) arg->xft_size = 300.0;
    } else
        arg->xft_size = 18.0;
    sprintf(matrix, "%g", arg->xft_size);
    x = strlen(matrix);

    XtVaGetValues(arg->rotation, XtNstring, &ptr, NULL);
    if (ptr) r = atof(ptr) * M_PI_180;
    arg->xft_rotation = r;

    XtVaGetValues(arg->dilation, XtNstring, &ptr, NULL);
    if (ptr) d = atof(ptr);
    arg->xft_dilation = d;

    XtVaGetValues(arg->linespacing, XtNstring, &ptr, NULL);
    if (ptr) ls = atof(ptr);
    arg->xft_linespacing = ls;

    XtVaGetValues(arg->inclination, XtNstring, &ptr, NULL);
    if (ptr)
        i = arg->xft_inclination = atof(ptr);

    if (r!=0.0 || d!= 1.0 || i!=0.0) {
        mat[0] = cos(r);
        mat[2] = sin(r);
        mat[1] = d*(i*mat[0]-mat[2]);
        mat[3] = d*(i*mat[2]+mat[0]);
        sprintf(matrix, ":matrix=%g %g %g %g",
		mat[0], mat[1], mat[2], mat[3]);
    } else {
        *matrix = '\0';
        mat[0] = mat[3] = 1.0;
        mat[1] = mat[2] = 0.0;
    }

    ptr = strchr(arg->font_select, '(');
    if (ptr && ptr>=arg->font_select+2) *(ptr-2) = '\0';

    if (arg->weight_select) { 
        j = strlen(arg->font_select)+strlen(arg->weight_select)+
            strlen(matrix)+x+9;
        arg->xft_name = realloc(arg->xft_name, j);
        sprintf(arg->xft_name, 
                "%s-%g:style=%s", arg->font_select, arg->xft_size, 
		arg->weight_select);
    } else {
        j = strlen(arg->font_select)+strlen(matrix)+x+2;
        arg->xft_name = realloc(arg->xft_name, j);
        sprintf(arg->xft_name, "%s-%g", arg->font_select, arg->xft_size);
    }

    if (arg->xft_font) XftFontClose(dpy, arg->xft_font);
    arg->xft_font = XftFontOpenName(dpy, DefaultScreen(dpy), arg->xft_name);

    if (arg->xft_font) {
        arg->xft_height = arg->xft_font->height * d;
        arg->xft_ascent = arg->xft_font->ascent * d;
        arg->xft_descent = arg->xft_font->descent * d;
        arg->xft_maxadv = arg->xft_font->max_advance_width;     
    }

    if (*matrix) {
        if (arg->weight_select) {
            sprintf(arg->xft_name, 
                    "%s-%g:style=%s%s", arg->font_select, arg->xft_size, 
		    arg->weight_select, matrix);
        } else
            sprintf(arg->xft_name, 
                    "%s-%g%s", arg->font_select, arg->xft_size, matrix);
        if (arg->xft_font) XftFontClose(dpy, arg->xft_font);
        arg->xft_font = 
            XftFontOpenName(dpy, DefaultScreen(dpy), arg->xft_name);
    }

    if (ptr) *(ptr-2) = ' ';

    if (!arg->xft_font) {
        cleanSamplePixmap(arg);
        return;
    }

    XtVaGetValues(arg->editButton, XtNstate, &bool, NULL);
    if (bool) {
        XtVaSetValues(arg->editButton, XtNstate, False, NULL);
        editCallback(arg->editButton, arg, NULL);
    }

    w = arg->xft_maxadv;
    hd = arg->xft_height*ls;

    x1 = x2 = y1 = y2 = 0;
    x = (int)(0.5+3*hd*mat[2]);
    y = (int)(0.5+3*hd*mat[0]);
    if (x<=x1) x1 = x; if (x>=x2) x2 = x;
    if (y<=y1) y1 = y; if (y>=y2) y2 = y;

    j = 0;

    ptr = arg->xft_text;
    str = NULL;
    while (ptr && *ptr) {
        string_extract(&ptr, &str);
        XftTextExtents8(dpy, arg->xft_font,
		        (XftChar8*)str, strlen(str), 
                        (XGlyphInfo*)&extents);
        x = extents.xOff + (int)(0.5+(j-0.2)*hd*mat[2]);
        y = extents.yOff + (int)(0.5+(j-0.2)*hd*mat[0]);
	if (x<=x1) x1 = x; if (x>=x2) x2 = x;
	if (y<=y1) y1 = y; if (y>=y2) y2 = y;
        x = extents.xOff + (int)(0.5+(j+1.2)*hd*mat[2]);
        y = extents.yOff + (int)(0.5+(j+1.2)*hd*mat[0]);
	if (x<=x1) x1 = x; if (x>=x2) x2 = x;
	if (y<=y1) y1 = y; if (y>=y2) y2 = y;
        ++j;
    }

    if ((j=x2-x1+6+w/4)>= width) width = j;
    if ((j=y2-y1+6+((int)arg->xft_height*0.2501))>= height) height = j;

    XtResizeWidget(arg->sample, width, height, 0);
    XtVaSetValues(arg->sample, XtNwidth, width, XtNheight, height, NULL);
    if (arg->pixmap) XFreePixmap(dpy, arg->pixmap);
    arg->pixmap = XCreatePixmap(dpy, XtWindow(arg->sample),
                   width, height,    
                   DefaultDepthOfScreen(XtScreen(arg->shell)));
    gc = XCreateGC(dpy, arg->pixmap, 0, NULL);
    XSetForeground(dpy, gc, WhitePixelOfScreen(XtScreen(arg->shell)));
    XFillRectangle(dpy, arg->pixmap, gc, 0, 0, width, height);
    xre_color.red = 0;
    xre_color.green = 0;
    xre_color.blue = 0;
    xre_color.alpha = 255<<8;
    XftColorAllocValue(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
			       cmap, &xre_color, &color);
    draw = XftDrawCreate(dpy, arg->pixmap,
                         DefaultVisual(dpy, DefaultScreen(dpy)), cmap);

    x0 = 3+w/8;
    y0 = 3+(int) arg->xft_height/8;
    if (x1<0) x0 += -x1;
    if (y1<0) y0 += -y1;

    j = 0;

    ptr = arg->xft_text;
    while (ptr && *ptr) {
        string_extract(&ptr, &str);
        x = x0 + (int)(0.5+j*hd*mat[2]);
        y = y0 + (int)(0.5+j*hd*mat[0]);     
        FreeTypeDrawString(dpy, arg, draw, &color, str, strlen(str), x, y, r);
        ++j;
    }
    free(str);

    setSamplePixmap(arg);
    XFreeGC(dpy, gc);
    XftDrawDestroy(draw);
}

static void 
barCallback(Widget w, arg_t * arg, float *percent)
{
    int npts;
    char val[8];
    npts = (int) (428*(*percent));
    if (npts<=100) npts = 1+npts/5;
    else
    if (npts<=200) npts = npts/2-28;
    else
    npts = npts-128;
    sprintf(val, "%d", npts);
    XtVaSetValues(arg->pointSelect, XtNstring, val, NULL);
    setXftFont(arg);
}

static void
setBar(arg_t * arg, double npts)
{
    float percent; 
    char val[8];

    sprintf(val, "%g", npts); 
    XtVaSetValues(arg->pointSelect, XtNstring, val, NULL);

    if (npts<=1.0) npts = 1.0;

    if (npts<=21.0)
        percent = 5*(npts-1)/428.0;
    else
    if (npts<=72.0)
        percent = 2*(npts+28)/428.0;
    else
        percent = (npts+128)/428.0;
    if (percent>=1.0) percent = 1.00001;

    XawScrollbarSetThumb(arg->pointBar, percent, -1.0);
    setXftFont(arg);
}

static void 
scrollCallback(Widget w, arg_t * arg, XtPointer position)
{
    char *ptr;
    double npts;

    XtVaGetValues(arg->pointSelect, XtNstring, &ptr, NULL);
    npts = atof(ptr);
    if ((long)position>0) {
        npts += 1.0;
        if (npts>=300.0) npts = 300.0;
    } else {
        npts -= 1.0;
        if (npts<=1.0) npts = 1.0;
    }
    setBar(arg, npts);
}

static void 
applySetCallback(Widget paint, Arg * argArg)
{
    FontChanged(paint);
}

static void 
applyCallback(Widget w, XtPointer argArg, XtPointer junk)
{
    Display *dpy = XtDisplay(w);
    arg_t *arg = (arg_t *) argArg;
    XftFont * font;
    Boolean bool;
    char *str;

    XtVaGetValues(arg->editButton, XtNstate, &bool, NULL);
    if (bool)
        XtVaGetValues(arg->sample, XtNstring, &str, NULL);
    else
        str = arg->xft_text;
    if (Global.xft_text) free(Global.xft_text);
    Global.xft_text = strdup(str);

    setXftFont(arg);
    if (!arg->xft_font) return;

    /* reopen same font under different XftFont struct chunk */
    font = XftFontOpenName(dpy, DefaultScreen(dpy), arg->xft_name);
    if (!font) {
	Notice(Global.toplevel, msgText[UNABLE_TO_LOAD_REQUESTED_FONT]);
	return;
    }
    if (Global.xft_font) 
        XftFontClose(dpy, (XftFont *)Global.xft_font);
    Global.xft_font = font;
    if (Global.xft_name) 
        free(Global.xft_name);
    Global.xft_name = strdup(arg->xft_name);
    Global.xft_size = arg->xft_size;
    Global.xft_height = arg->xft_height;
    Global.xft_ascent = arg->xft_ascent;
    Global.xft_descent = arg->xft_descent;
    Global.xft_maxadv = arg->xft_maxadv;
    Global.xft_rotation = arg->xft_rotation;
    Global.xft_dilation = arg->xft_dilation;
    Global.xft_inclination = arg->xft_inclination;
    Global.xft_linespacing = arg->xft_linespacing;
    GraphicAll((GraphicAllProc) applySetCallback, NULL);
}

static void 
closeCallback(Widget w, XtPointer argArg, XtPointer junk)
{
    arg_t *arg = (arg_t *) argArg;
    PopdownMenusGlobal();
    XtPopdown(arg->shell);
}

static void 
unselectCallback(Widget w, XtPointer argArg, XEvent * event, Boolean * flg)
{
    arg_t *arg = (arg_t *) argArg;

    if (!arg->font_select) return;
    if (event->type != ButtonPress) return;
    if (event->xbutton.button != 1) return;

    if (!strcmp(XtName(w), "font")) {
        if (arg->font_select) free(arg->font_select);
        arg->font_select = NULL;
        if (!arg->pixmap) return;
    }
    if (!strcmp(XtName(w), "weight")) {
        if (arg->weight_select) free(arg->weight_select);
	arg->weight_select = NULL;
    }
    setXftFont(arg);
}

static void 
listCallback(Widget w, XtPointer argArg, XtPointer itemArg)
{
    arg_t *arg = (arg_t *) argArg;
    XawListReturnStruct *item = (XawListReturnStruct *) itemArg;
    char *ptr1, *ptr2;
    int j, k, l;

    if (!strcmp(XtName(w), "font")) {
        if (!item || !item->string) {
	    if (arg->font_select) free(arg->font_select);
	    arg->font_select = NULL;
            cleanSamplePixmap(arg);
            return;       
	}
	if (arg->font_select) free(arg->font_select);
        arg->font_select = strdup(item->string);

        for (j=0; j<arg->num_weights; j++)
	    free(arg->weight_list[j]);
        ptr1 = strchr(arg->font_select, '(');
        if (ptr1) *(ptr1-2) = '\0';
        l = strlen(arg->font_select);
        if (ptr1) *(ptr1-2) = ' ';
        k = 0;
        for (j=0; j<arg->num_fonts; j++) {
	  if (!strncasecmp(arg->font_desc[j], arg->font_select, l)) {
	        ptr2 = strrchr(arg->font_desc[j], ':');
                if (ptr2) {
                    arg->weight_list = 
		       (char **)realloc(arg->weight_list, (k+1)*sizeof(char *));
                    /* hack to put light, medium, normal (et al) on top */
                    if (strncasecmp(ptr2+1, "light", 5) &&
                        strncasecmp(ptr2+1, "medium", 6) &&
                        strncasecmp(ptr2+1, "normal", 6) &&
                        strncasecmp(ptr2+1, "regular", 7))
		        arg->weight_list[k] = strdup(ptr2+1);
                    else {
		        arg->weight_list[k] = strdup(ptr2);
                        arg->weight_list[k][0] = '\1';
		    }
                    k++;
		}
	    }
	}

        qsort(arg->weight_list, k, sizeof(char *),
          (int (*)(_Xconst void *, _Xconst void *)) strqsortcmp);
        j = 1;
        while (j<k) {
	    if (!strcasecmp(arg->weight_list[j], arg->weight_list[j-1])) {
	        free(arg->weight_list[j]);
                --k;
                for (l=j; l<k; l++) 
		    arg->weight_list[l] = arg->weight_list[l+1];
	    } else
	        j++;
	}
        /* remove hack (first character = 1) */
        for (j=0; j<k; j++)
	    if (arg->weight_list[j][0] == '\1')
	        for (l=0; l<strlen(arg->weight_list[j]); l++)
		    arg->weight_list[j][l] = arg->weight_list[j][l+1];
        arg->num_weights = k;
        XawListChange(arg->weight, arg->weight_list, k, 0, True);
	if (arg->weight_select) free(arg->weight_select);
        arg->weight_select = NULL;
    }
    if (!strcmp(XtName(w), "weight")) {
        if (arg->weight_select) free(arg->weight_select);
	arg->weight_select = NULL;
        if (!item || !item->string) {
	    arg->weight_select = NULL;
	} else
            arg->weight_select = strdup(item->string);
      
    }
    setXftFont(arg);
}

void
fontboxResized(Widget w, arg_t * arg, XConfigureEvent * event, Boolean * flg)
{
    Display *dpy = XtDisplay(w);
    Dimension width, height, height1, height2, width2;
    Position x, y;
    char *val;
    double m = 18.0;

    XtVaGetValues(arg->shell, XtNwidth, &width, XtNheight, &height, NULL);
    XtVaGetValues(arg->form1, XtNheight, &height1, NULL);
    if (width<360) width = 360;
    if (height1<150) height1 = 150;
    XtResizeWidget(arg->shell, width, height, 0);
    XtResizeWidget(arg->form1, width, height1, 0);
    height1 -= 32;
   
    XtResizeWidget(arg->familyLabel, 150, 20, 0);
    XtVaSetValues(arg->familyLabel, XtNwidth, 150, XtNheight, 20, NULL);   
    XtMoveWidget(arg->familyLabel, -2, 4);

    XtMoveWidget(arg->vport0, 4, 27);
    XtResizeWidget(arg->vport0, width-218, height1, BORDERWIDTH);
    XtVaSetValues(arg->vport0, XtNwidth, width-218, NULL);

    XtResizeWidget(arg->weightLabel, 150, 20, 0);
    XtVaSetValues(arg->weightLabel, XtNwidth, 150, XtNheight, 20, NULL);   
    XtMoveWidget(arg->weightLabel, width-220, 4);

    XtMoveWidget(arg->vport1, width-209, 27);
    XtResizeWidget(arg->vport1, 205, height1, BORDERWIDTH);
    XtVaSetValues(arg->vport1, XtNwidth, 205, NULL);   

#ifdef XAW3D   
    height2 = height-height1-162;
#else
    height2 = height-height1-158;
#endif
    if (height2<4) height2 = 4;

    XtResizeWidget(arg->vport2, width-8, height2, BORDERWIDTH);
    XtVaSetValues(arg->vport2, XtNwidth, width-8, NULL);
    XtResizeWidget(arg->sample, width-8, height2, 0);
    XtVaGetValues(arg->editButton, XtNwidth, &width2, NULL);
    XtResizeWidget(arg->editButton, width2, 22, 0);

    XtResizeWidget(arg->subform, width, 60, 0);
    XtMoveWidget(arg->pointSelectLabel, 4, 8);
    XtVaGetValues(arg->form2, XtNheight, &height2, NULL);
    y = height2-25;
    XtMoveWidget(arg->editButton, 4, y);
    XtVaGetValues(arg->applyButton, XtNx, &x, NULL);
    XtMoveWidget(arg->applyButton, x, y);
    XtVaGetValues(arg->menubar, XtNx, &x, NULL);
    XtMoveWidget(arg->menubar, x, y-4);
    XtMoveWidget(arg->doneButton, 304, y);

    XtResizeWidget(arg->pointBar, 350, 20, 0);
    XtMoveWidget(arg->pointBar, 150, 8);
    XtMoveWidget(arg->pointSelect, 506, 8);

    XtMoveWidget(arg->rotationLabel, 4, 38);
    XtMoveWidget(arg->rotation, 90, 38);

    XtMoveWidget(arg->inclinationLabel, 164, 38);
    XtMoveWidget(arg->inclination, 250, 38);

    XtMoveWidget(arg->dilationLabel, 324, 38);
    XtMoveWidget(arg->dilation, 410, 38);

    XtMoveWidget(arg->linespacingLabel, 484, 38);
    XtMoveWidget(arg->linespacing, 570, 38);

    XtMoveWidget(arg->vport2, 4, 76);

    XtVaGetValues(arg->pointSelect, XtNstring, &val, NULL);
    if (val) m = atof(val);
    if (m<=2.0001) m = 2.0001;
    if (30*m>=width) width = (int) (30*m);
    if (5*m>=height2-10) height2 = (int) (5*m+10);
    XtVaSetValues(arg->sample, XtNwidth, width, XtNheight, height2, NULL);
    if (!arg->pixmap) {
        arg->pixmap = XCreatePixmap(dpy, XtWindow(arg->sample),
                           width, height2,    
                           DefaultDepthOfScreen(XtScreen(arg->shell)));
        cleanSamplePixmap(arg);
    }


    if (!XPending(XtDisplay(w)))
        setXftFont(arg);
}

static void 
inputDataAction(Widget w, XEvent * event, String * prms, Cardinal * nprms)
{
    char *val;

    if (theArg == NULL)
	return;

    XtVaGetValues(theArg->pointSelect, XtNstring, &val, NULL);
    if (val && *val) setBar(theArg, atoi(val));

    if (!XtIsManaged(theArg->pointSelect))
	return;
    
    setXftFont(theArg);
    fontboxResized(w, theArg, (XConfigureEvent *)event, NULL);
}

static PaintMenuItem fileMenu[] =
{
#define FILE_LOAD 0
    MI_SIMPLE("load"),
#define FILE_SAVE 1
    MI_SIMPLE("save"),
#define FILE_SAVEAS 2
    MI_SIMPLE("saveas"),
#define FILE_EDIT 3
    MI_SIMPLE("editor"),
};

static PaintMenuItem editMenu[] =
{
#define EDIT_DEFAULT 0
    MI_SIMPLE("default"),
#define EDIT_ERASE 1
    MI_SIMPLE("erase"),
                /* 2 */
    MI_SEPARATOR(),  
#define EDIT_FONT 3
    MI_SIMPLE("\\* font: ?*\\"),
#define EDIT_COLOR 4
    MI_SIMPLE("\\* color: ?*\\"),
#define EDIT_ROTATION 5
    MI_SIMPLE("\\* rotation: ?*\\"),
#define EDIT_PLUS_ROTATION 6
    MI_SIMPLE("\\* +rotation: ?*\\"),
#define EDIT_INCLINATION 7
    MI_SIMPLE("\\* inclination: ?*\\"),
#define EDIT_DILATION 8
    MI_SIMPLE("\\* dilation: ?*\\"),
#define EDIT_LINESPACING 9
    MI_SIMPLE("\\* linespacing: ?*\\"),
#define EDIT_X 10
    MI_SIMPLE("\\* x: ?*\\"),
#define EDIT_Y 11
    MI_SIMPLE("\\* y: ?*\\"),
#define EDIT_PLUS_X 12
    MI_SIMPLE("\\* +x: ?*\\"),
#define EDIT_PLUS_Y 13
    MI_SIMPLE("\\* +y: ?*\\"),
#define EDIT_SX 14
    MI_SIMPLE("\\* sx: ?*\\"),
#define EDIT_SY 15
    MI_SIMPLE("\\* sy: ?*\\"),
#define EDIT_PLUS_SX 16
    MI_SIMPLE("\\* +sx: ?*\\"),
#define EDIT_PLUS_SY 17
    MI_SIMPLE("\\* +sy: ?*\\"),
#define EDIT_NOOP 18
};

static PaintMenuBar textMenuBar[] =
{
    {None, "text_file", XtNumber(fileMenu), fileMenu},
    {None, "text_sample", XtNumber(editMenu), editMenu}
};

static void
popupHandler(Widget w, arg_t * argArg, XEvent * event, Boolean * flag)
{
    if (Global.popped_up) {
        if (event->type == ButtonRelease || event->type == ButtonPress)
            PopdownMenusGlobal();
        event->type = None;       
    }
}

static void 
loadFileCallbackOK(Widget w, XtPointer argArg, char *file)
{
   arg_t *arg = (arg_t *) argArg;
   FILE *fd;
   static char *str;
   char buf[2048];
   int l;

   if (!file || !*file) return;
   fd = fopen(file, "r");
   if (!fd) {
      Notice(w, msgText[UNABLE_TO_OPEN_FILE], file);
      return;
   }
   /* XtVaSetValues(arg->textname, XtNlabel, file, NULL); */
   str = xmalloc(2);
   *str = '\0';
   l = 0;
   while (fgets(buf, 2040, fd)) {
      l += strlen(buf);
      str = realloc(str, l+2);
      strcat(str, buf);
   }
   fclose(fd);
   XtVaSetValues(arg->sample, XtNstring, str, NULL);
}

static void
loadFileCallback(Widget w, XtPointer argArg, XtPointer junk)
{
   char buf[256];
   *buf = '\0';
   if (getcwd(buf, 256)) strcat(buf, "/");
   GetFileName(GetShell(w), BROWSER_SIMPLEREAD, 
               buf, (XtCallbackProc) loadFileCallbackOK, argArg);
}

static void 
saveFileCallbackOK(Widget w, XtPointer argArg, char *file)
{
   arg_t *arg = (arg_t *) argArg;
   FILE *fd;
   char *ptr, *str;
 
   if (!file || !*file) return;
   fd = fopen(file, "w");
   if (!fd) {
      Notice(w, msgText[UNABLE_TO_OPEN_FILE], file);
      return;
   }

   /* XtVaSetValues(arg->textname, XtNlabel, file, NULL); */
   XtVaGetValues(arg->sample, XtNstring, &str, NULL);
  
   if (!str || !*str) {
      fclose(fd);
      return;
   }
   ptr = str;
   while (*ptr) {
      fputc(*ptr, fd);
      ++ptr;
   }
   fclose(fd);
}

static void
saveFileCallback(Widget w, XtPointer argArg, XtPointer junk)
{
   char buf[256];
   *buf = '\0';
   if (getcwd(buf, 256)) strcat(buf, "/");
   GetFileName(GetShell(w), BROWSER_SIMPLESAVE, 
               buf, (XtCallbackProc) saveFileCallbackOK, argArg);
}

static void 
defaultTextCallback(Widget w, XtPointer argArg, XtPointer junk)
{
    arg_t *arg = (arg_t *) argArg;
    XtVaSetValues(arg->sample, XtNstring, DEFAULT_XFT_TEXT, NULL);
}

static void 
eraseTextCallback(Widget w, XtPointer argArg, XtPointer junk)
{
    arg_t *arg = (arg_t *) argArg;
    XtVaSetValues(arg->sample, XtNstring, "", NULL);
}

static void 
insertTextCallback(Widget w, XtPointer argArg, XtPointer ptr)
{
    arg_t *arg = (arg_t *) argArg;
    char buf[256];
    int j;
    XawTextPosition p;
    XawTextBlock b;

    for (j=EDIT_FONT; j<=EDIT_NOOP; j++)
        if (editMenu[j].widget == w) break;
    
    switch(j) {
        case EDIT_FONT:
             sprintf(buf, "\\*font:%s*\\", 
		     (arg->xft_name)? arg->xft_name:"Liberation-18");
             break;
        case EDIT_COLOR:
             strcpy(buf, "\\*color:#2367ab*\\");
             break;
        case EDIT_ROTATION:
	     sprintf(buf, "\\*rotation:%g*\\", arg->xft_rotation);
             break;
        case EDIT_PLUS_ROTATION:
             sprintf(buf, "\\*+rotation:%g*\\", arg->xft_rotation);
             break;
        case EDIT_INCLINATION:
             sprintf(buf, "\\*inclination:%g*\\", arg->xft_inclination);
             break;
        case EDIT_DILATION:
             sprintf(buf, "\\*dilation:%g*\\", arg->xft_dilation);
             break;
        case EDIT_LINESPACING:
             sprintf(buf, "\\*linespacing:%g*\\", arg->xft_linespacing);
             break;
        case EDIT_X:
             strcpy(buf, "\\*x:50*\\");
             break;
        case EDIT_Y:
             strcpy(buf, "\\*y:80*\\");
             break;
        case EDIT_PLUS_X:
             strcpy(buf, "\\*+x:50*\\");
             break;
        case EDIT_PLUS_Y:
             strcpy(buf, "\\*+y:80*\\");
             break;
        case EDIT_SX:
             strcpy(buf, "\\*sx:50*\\");
             break;
        case EDIT_SY:
             strcpy(buf, "\\*sy:80*\\");
             break;
        case EDIT_PLUS_SX:
             strcpy(buf, "\\*+sx:50*\\");
             break;
        case EDIT_PLUS_SY:
             strcpy(buf, "\\*+sy:80*\\");
             break;
        default:
	     return;
    }
    p = XawTextGetInsertionPoint(arg->sample);
    b.firstPos = 0;
    b.length = strlen(buf);
    b.ptr = buf;
    b.format = 0;
    XawTextReplace(arg->sample, p, p, &b);
}

static void
saveasFileCallback(Widget w, XtPointer argArg, XtPointer junk)
{
   char buf[256];
   *buf = '\0';
   if (getcwd(buf, 256)) strcat(buf, "/");
   GetFileName(GetShell(w), BROWSER_SIMPLESAVE, 
               buf, (XtCallbackProc) saveFileCallbackOK, argArg);
}

static void
externCallback(Widget w, XtPointer argArg, XtPointer junk)
{
   arg_t *arg = (arg_t *) argArg;
   FILE *fd;
   int l;
   char *tmp, *ptr, *str;
   char name[256];
   char buf[2048];

   /* write buffer to file */
   PopdownMenusGlobal();
   fd = openTempFile(&tmp);
   fclose(fd);
   sprintf(name, "%s.c", tmp);
   removeTempFile();
   fd = fopen(name, "w");
   if (!fd) {
      Notice(w, msgText[UNABLE_TO_OPEN_FILE], name);
      return;
   }
   XtVaGetValues(arg->sample, XtNstring, &str, NULL);
   if (str && *str) {
      ptr = str;
      while (*ptr) {
         fputc(*ptr, fd);
         ++ptr;
      }
   }
   fclose(fd);

   /* call external editor */
   sprintf(buf, "%s %s", EDITOR, name);
   system(buf);

   /* Now copy edited file into buffer */
   fd = fopen(name, "r");
   if (!fd) {
      Notice(w, msgText[UNABLE_TO_OPEN_FILE], name);
      return;
   }

   str = xmalloc(2);
   *str = '\0';
   l = 0;
   while (fgets(buf, 2040, fd)) {
      l += strlen(buf);
      str = realloc(str, l+2);
      strcat(str, buf);
   }
   fclose(fd);
   unlink(name);
   XtVaSetValues(arg->sample, XtNstring, str, NULL);
}

static void
editCallback(Widget w, XtPointer argArg, XtPointer junk)
{
    arg_t *arg = (arg_t *) argArg;
    Boolean bool;
    Dimension width;
    char *str;

    XtVaGetValues(w, XtNstate, &bool, XtNwidth, &width, NULL);
    XtResizeWidget(arg->editButton, width, 22, 0);

    if (bool) {
        cleanSamplePixmap(arg);
        XtVaSetValues(arg->sample,
                      XtNeditType, XawtextEdit,
                      XtNresizable, True,
                      XtNwidth, 620,
                      XtNheight, 300,
	              XtNdisplayCaret, True,
		      XtNstring, arg->xft_text, NULL);
        free(arg->xft_text);
        arg->xft_text = NULL;
        XMapWindow(XtDisplay(arg->menubar), XtWindow(arg->menubar));
    } else {
        cleanSamplePixmap(arg);
        XtVaGetValues(arg->sample, XtNstring, &str, NULL);
        if (str) {
            if (arg->xft_text) free(arg->xft_text);
            arg->xft_text = strdup(str);
	}
        XtVaSetValues(arg->sample,
                      XtNeditType, XawtextEdit,
                      XtNresizable, True,
                      XtNwidth, 620,
                      XtNheight, 300,
	              XtNdisplayCaret, False,
		      XtNstring, "", NULL);
        XUnmapWindow(XtDisplay(arg->menubar), XtWindow(arg->menubar));
        setXftFont(arg);
    }
}

void 
FontSelect(Widget w, Widget paint)
{
    Display *dpy = XtDisplay(w);
    XftFontSet	*fs;
    XftChar8 *str;
    XftChar8 *foundry;
    XftChar8 *style;

    static XtActionsRec dataAct =
      {"input-data-ok", (XtActionProc) inputDataAction};
    static Widget shell = None;
    static arg_t *arg;

    Widget pane, form1, form2;
    Widget editButton, applyButton, doneButton, label;
    char *ptr1, *ptr2;
    int j, k, l, lmax;

    PopdownMenusGlobal();

    if (shell != None) {
	XtPopup(shell, XtGrabNone);
	RaiseWindow(XtDisplay(shell), XtWindow(shell));
        XtSetMinSizeHints(shell, 360, 480);
	return;
    }

    StateSetBusyWatch(True);
    arg = XtNew(arg_t);
    memset(arg, 0, sizeof(arg_t));

    /*
    **	Init the world
     */
    fs = XftListFonts(dpy, DefaultScreen(dpy), 0,
                      XFT_FAMILY, XFT_FOUNDRY, XFT_STYLE, (char *)0);
    arg->font_desc = (char **) xmalloc(fs->nfont * sizeof(char *));
    lmax = 0;
    for (j = 0; j <fs->nfont; j++) {
        /* Caution : this allocates str */
        str = XftNameUnparseAlloc(fs->fonts[j]);
        /* Search for first alternative of family name in str */
        ptr1 = strchr(str, ':');
        if (ptr1) *ptr1 = '\0';
        ptr2 = strchr(str, ',');
        if (ptr2) *ptr2 = '\0';
        /* length of family name */
        k = strlen(str);
        if (ptr1) *ptr1 = ':';
        if (ptr2) *ptr2 = ',';
	if (XftPatternGetString(fs->fonts[j], XFT_FOUNDRY, 0, &foundry) !=
            XftResultMatch)
	    foundry = "unknown";
	if (XftPatternGetString(fs->fonts[j], XFT_STYLE, 0, &style) != 
            XftResultMatch)
	    style = "";
        l = k+strlen(foundry)+strlen(style)+3;
        if (l>=lmax) lmax = l;
        arg->font_desc[j] = (char *) xmalloc(l);
        /* copy family name */
        strncpy(arg->font_desc[j], str, k);
        sprintf(arg->font_desc[j]+k, ":%s:%s", foundry, style);
        free(str);
    }

    qsort(arg->font_desc, fs->nfont, sizeof(char *),
          (int (*)(_Xconst void *, _Xconst void *)) strqsortcmp);

    k = 0;
    arg->font_family = (char **) xmalloc(fs->nfont * sizeof(char *));

    for (j = 0; j<fs->nfont; j++) {
        if (arg->font_desc[j][0] == ':') break;
        ++arg->num_fonts;
        ptr1 = strchr(arg->font_desc[j], ':');
        if (ptr1) { 
            *ptr1 = '\0';
            ptr2 = strchr(ptr1+1, ':');
	} else
	    ptr2 = NULL;
        if (ptr2) *ptr2 = '\0';
        l = strlen(arg->font_desc[j]);
        if (k==0 ||
            strncasecmp(arg->font_desc[j], arg->font_family[k-1], l)) {
	  if (ptr1 && strcmp(ptr1+1, "unknown")) {
                arg->font_family[k] = 
	            (char *) xmalloc(l+strlen(ptr1+1)+5);
                sprintf(arg->font_family[k], "%s  (%s)", 
                       arg->font_desc[j], ptr1+1);
	    } else
	        arg->font_family[k] = strdup(arg->font_desc[j]);
            ++k;
	}
        if (ptr1) *ptr1 = ':';
        if (ptr2) *ptr2 = ':';
    }

    arg->num_families = k;
    XftFontSetDestroy(fs);

    Global.xft_text = strdup(DEFAULT_XFT_TEXT);

    /*
    **	Init the widgets
     */

    shell = XtVaCreatePopupShell("fontSelect",
				 topLevelShellWidgetClass, Global.toplevel,
				 NULL);
    arg->shell = shell;

    pane = XtVaCreateManagedWidget("pane",
				   panedWidgetClass, arg->shell,
				   XtNborderWidth, 0,
				   NULL);

    label = XtVaCreateManagedWidget("title",
				    labelWidgetClass, pane,
	                            XtNlabel, msgText[FONT_SELECT_DESIRED_PROPERTIES],
				    XtNborderWidth, 0,
				    XtNshowGrip, False,
				    NULL);

    form1 = XtVaCreateManagedWidget("form",
				   formWidgetClass, pane,
				   XtNborderWidth, 0,
				   XtNwidth, 600, XtNheight, 200,
				   NULL);
    arg->form1 = form1;

    /*
    **	lists of items to select
     */

    arg->familyLabel = XtVaCreateManagedWidget("familyLabel",
				    labelWidgetClass, form1,
				    XtNborderWidth, 0,
				    XtNtop, XtChainTop,
				    XtNbottom, XtChainTop,
				    NULL);

    arg->vport0 = XtVaCreateManagedWidget("vport",
				    viewportWidgetClass, form1,
				    XtNwidth, 420, XtNheight, 200,
				    XtNuseBottom, True,
				    XtNuseRight, True,
				    XtNforceBars, False,
				    XtNallowHoriz, False,
				    XtNallowVert, True,
				    XtNfromVert, arg->familyLabel,
				    XtNtop, XtChainTop,
				    NULL);

    arg->family = XtVaCreateManagedWidget("font",
					  listWidgetClass, arg->vport0,
					  XtNverticalList, True,
					  XtNforceColumns, True,
					  XtNdefaultColumns, 1,
					  XtNnumberStrings, 0,
					  NULL);

    arg->weightLabel = XtVaCreateWidget("weightLabel",
				    labelWidgetClass, form1,
				    XtNborderWidth, 0,
				    XtNfromHoriz, arg->vport0,
				    XtNtop, XtChainTop,
				    XtNbottom, XtChainTop,
				    NULL);

    arg->vport1 = XtVaCreateManagedWidget("vport",
				    viewportWidgetClass, form1,
				    XtNwidth, 220, XtNheight, 200,
				    XtNuseBottom, True,
				    XtNuseRight, True,
				    XtNforceBars, False,
				    XtNallowHoriz, False,
				    XtNallowVert, True,
				    XtNfromHoriz, arg->vport0,
				    XtNfromVert, arg->weightLabel,
				    XtNtop, XtChainTop,
				    NULL);

    arg->weight = XtVaCreateManagedWidget("weight",
					  listWidgetClass, arg->vport1,
					  XtNverticalList, True,
					  XtNforceColumns, True,
					  XtNdefaultColumns, 1,
					  XtNnumberStrings, 0,
					  NULL);

    /*
    **	The text area and buttons
     */

    form2 = XtVaCreateManagedWidget("form2",
				   formWidgetClass, pane,
				   XtNborderWidth, 0,
				   NULL);
    arg->form2 = form2;

    arg->subform = XtVaCreateManagedWidget("subForm",
				    formWidgetClass, form2,
				    XtNborderWidth, 0,
				    XtNtop, XtChainTop,
				    XtNbottom, XtChainTop,
				    XtNwidth, 600,
				    XtNheight, 60,
				    NULL);

    XtAppAddActions(XtWidgetToApplicationContext(arg->subform), &dataAct, 1);

    arg->pointSelectLabel = XtVaCreateWidget("pointSelectLabel",
				   labelWidgetClass, arg->subform,
				   XtNborderWidth, 0,
				   XtNleft, XtChainLeft,
				   XtNright, XtChainLeft,
				   NULL);

    arg->pointBar = XtVaCreateManagedWidget("pointBar", 
                                   scrollbarWidgetClass, arg->subform,
				   XtNorientation, XtorientHorizontal,
				   XtNwidth, 328, XtNheight, 20,
				   XtNfromHoriz, arg->pointSelectLabel,
				   XtNleft, XtChainLeft,
				   XtNright, XtChainLeft,
				   NULL);

    arg->pointSelect = XtVaCreateWidget("pointSelect",
					asciiTextWidgetClass, arg->subform,
					XtNleft, XtChainLeft,
					XtNright, XtChainRight,
					XtNfromHoriz, arg->pointBar,
					XtNhorizDistance, 10,
                                        XtNstring, "18",
					XtNeditType, XawtextEdit,
					XtNwrap, XawtextWrapNever,
                                        XtNwidth, 64,
					XtNlength, 8,
					XtNtranslations,
					XtParseTranslationTable("#override\n\
					<Key>Return: input-data-ok()\n\
					<Key>Linefeed: input-data-ok()\n\
					Ctrl<Key>M: input-data-ok()\n\
					Ctrl<Key>J: input-data-ok()\n"),
					NULL);

    arg->rotationLabel = XtVaCreateWidget("rotationLabel",
				          labelWidgetClass, arg->subform,
				          XtNborderWidth, 0,
				          XtNfromVert, arg->pointSelect,
				          XtNleft, XtChainLeft,
				          XtNright, XtChainLeft,
				          NULL);

    arg->rotation = XtVaCreateWidget("rotation",
					asciiTextWidgetClass, arg->subform,
				        XtNfromVert, arg->pointSelect,
	                                XtNfromHoriz, arg->rotationLabel,
					XtNleft, XtChainLeft,
					XtNright, XtChainRight,
					XtNhorizDistance, 10,
                                        XtNstring, "0",
					XtNeditType, XawtextEdit,
					XtNwrap, XawtextWrapNever,
                                        XtNwidth, 64,
					XtNlength, 8,
					XtNtranslations,
					XtParseTranslationTable("#override\n\
					<Key>Return: input-data-ok()\n\
					<Key>Linefeed: input-data-ok()\n\
					Ctrl<Key>M: input-data-ok()\n\
					Ctrl<Key>J: input-data-ok()\n"),
					NULL);

    arg->inclinationLabel = XtVaCreateWidget("inclinationLabel",
				   labelWidgetClass, arg->subform,
				   XtNborderWidth, 0,
				   XtNfromHoriz, arg->rotation,
				   XtNfromVert, arg->pointSelect,
				   XtNhorizDistance, 20,
				   XtNleft, XtChainLeft,
				   XtNright, XtChainLeft,
				   NULL);

    arg->inclination = XtVaCreateWidget("inclination",
					asciiTextWidgetClass, arg->subform,
				        XtNfromVert, arg->pointSelect,
	                                XtNfromHoriz, arg->inclinationLabel,
					XtNleft, XtChainLeft,
					XtNright, XtChainRight,
					XtNhorizDistance, 10,
                                        XtNstring, "0",
					XtNeditType, XawtextEdit,
					XtNwrap, XawtextWrapNever,
                                        XtNwidth, 64,
					XtNlength, 8,
					XtNtranslations,
					XtParseTranslationTable("#override\n\
					<Key>Return: input-data-ok()\n\
					<Key>Linefeed: input-data-ok()\n\
					Ctrl<Key>M: input-data-ok()\n\
					Ctrl<Key>J: input-data-ok()\n"),
					NULL);

    arg->linespacingLabel = XtVaCreateWidget("linespacingLabel",
				   labelWidgetClass, arg->subform,
				   XtNborderWidth, 0,
				   XtNfromHoriz, arg->inclination,
				   XtNfromVert, arg->pointSelect,
				   XtNhorizDistance, 20,
				   XtNleft, XtChainLeft,
				   XtNright, XtChainLeft,
				   NULL);

    arg->linespacing = XtVaCreateWidget("linespacing",
					asciiTextWidgetClass, arg->subform,
				        XtNfromVert, arg->pointSelect,
	                                XtNfromHoriz, arg->linespacingLabel,
					XtNleft, XtChainLeft,
					XtNright, XtChainRight,
					XtNhorizDistance, 10,
                                        XtNstring, "1",
					XtNeditType, XawtextEdit,
					XtNwrap, XawtextWrapNever,
                                        XtNwidth, 64,
					XtNlength, 8,
					XtNtranslations,
					XtParseTranslationTable("#override\n\
					<Key>Return: input-data-ok()\n\
					<Key>Linefeed: input-data-ok()\n\
					Ctrl<Key>M: input-data-ok()\n\
					Ctrl<Key>J: input-data-ok()\n"),
					NULL);

    arg->dilationLabel = XtVaCreateWidget("dilationLabel",
				   labelWidgetClass, arg->subform,
				   XtNborderWidth, 0,
				   XtNfromHoriz, arg->linespacing,
				   XtNfromVert, arg->pointSelect,
				   XtNhorizDistance, 20,
				   XtNleft, XtChainLeft,
				   XtNright, XtChainLeft,
				   NULL);

    arg->dilation = XtVaCreateWidget("dilation",
					asciiTextWidgetClass, arg->subform,
				        XtNfromVert, arg->pointSelect,
	                                XtNfromHoriz, arg->dilationLabel,
					XtNleft, XtChainLeft,
					XtNright, XtChainRight,
					XtNhorizDistance, 10,
                                        XtNstring, "1",
					XtNeditType, XawtextEdit,
					XtNwrap, XawtextWrapNever,
                                        XtNwidth, 64,
					XtNlength, 8,
					XtNtranslations,
					XtParseTranslationTable("#override\n\
					<Key>Return: input-data-ok()\n\
					<Key>Linefeed: input-data-ok()\n\
					Ctrl<Key>M: input-data-ok()\n\
					Ctrl<Key>J: input-data-ok()\n"),
					NULL);

    arg->vport2 = XtVaCreateManagedWidget("vport",
				    viewportWidgetClass, form2,
				    XtNheight, 160,
				    XtNallowResize, True,
				    XtNuseRight, True,
				    XtNforceBars, False,
				    XtNallowHoriz, True,
				    XtNallowVert, True,
				    XtNfromVert, arg->rotation,
				    XtNtop, XtChainTop,
				    XtNbottom, XtChainBottom,
				    NULL);

    arg->sample = XtVaCreateManagedWidget("sample",
					  asciiTextWidgetClass, arg->vport2,
					  XtNallowResize, True,
                                          XtNeditType, XawtextEdit,
                                          XtNresizable, True,
                                          XtNwidth, 620,
                                          XtNheight, 300,
	                                  XtNdisplayCaret, True,
		                          XtNstring, Global.xft_text, NULL);

    editButton = XtVaCreateManagedWidget("edittoggle",
					 toggleWidgetClass, form2,
                                         XtNstate, True,
					 XtNfromVert, arg->vport2,
                                         XtNvertDistance, 3,
					 XtNtop, XtChainBottom,
					 XtNbottom, XtChainBottom,
					 XtNright, XtChainLeft,
					 XtNleft, XtChainLeft,
					 NULL);
    arg->editButton = editButton;

    applyButton = XtVaCreateManagedWidget("apply",
					  commandWidgetClass, form2,
					  XtNfromVert, arg->vport2,
                                          XtNheight, 22,
                                          XtNvertDistance, 3,
					  XtNfromHoriz, editButton,
					  XtNtop, XtChainBottom,
					  XtNbottom, XtChainBottom,
					  XtNright, XtChainLeft,
					  XtNleft, XtChainLeft,
					  NULL);
    arg->applyButton = applyButton;

    doneButton = XtVaCreateManagedWidget("done",
					 commandWidgetClass, form2,
					 XtNfromVert, arg->vport2,
                                         XtNvertDistance, 3,
					 XtNfromHoriz, applyButton,
					 XtNtop, XtChainBottom,
					 XtNbottom, XtChainBottom,
					 XtNright, XtChainLeft,
					 XtNleft, XtChainLeft,
					 NULL);
    arg->doneButton = doneButton;

    arg->menubar = MenuBarCreate(form2, XtNumber(textMenuBar), textMenuBar);
    XtVaSetValues(arg->menubar, XtNfromVert, arg->vport2,
		                XtNfromHoriz, applyButton,
		                XtNvertDistance, 3,
		                XtNhorizDistance, 50,
				XtNtop, XtChainBottom,
				XtNbottom, XtChainBottom,
			        XtNright, XtChainLeft,
				XtNleft, XtChainLeft,
                                NULL);

    arg->paint = paint;
    arg->font_select = NULL;
    arg->xft_name = NULL;
    arg->xft_font = NULL;
    arg->xft_text = NULL;
    arg->weight_select = NULL;
    arg->pixmap = None;
    arg->xft_dilation = 1.0;
    arg->xft_linespacing = 1.0;

    arg->weight_list = (char **)xmalloc(sizeof(char*));
    arg->num_weights = 1;
    arg->weight_list[0] = strdup("Regular");
    theArg = arg;
    XawListChange(arg->family, arg->font_family, arg->num_families, 0, True);
    XawListChange(arg->weight, arg->weight_list, 1, 0, True);
    XawScrollbarSetThumb(arg->pointBar, 0.2125, -1.0);

    XtPopup(arg->shell, XtGrabNone);
    XFlush(dpy);
    XtSetMinSizeHints(arg->shell, 360, 480);
    StateSetBusyWatch(False);
    XtVaGetValues(arg->shell, XtNtitle, &ptr1, NULL);
    StoreName(arg->shell, ptr1);

    XtUnmanageChild(arg->menubar);
    XtUnmanageChild(arg->familyLabel);
    XMapWindow(dpy, XtWindow(arg->familyLabel));
    XtUnmanageChild(arg->weightLabel);
    XMapWindow(dpy, XtWindow(arg->weightLabel));

    XtUnmanageChild(XtParent(arg->pointSelect));
    XMapWindow(dpy, XtWindow(XtParent(arg->pointSelect)));
    XtUnmanageChild(arg->applyButton);

    XMapWindow(dpy, XtWindow(arg->pointSelectLabel));
    XMapWindow(dpy, XtWindow(arg->pointBar));
    XMapWindow(dpy, XtWindow(arg->pointSelect));
    XMapWindow(dpy, XtWindow(arg->rotationLabel));
    XMapWindow(dpy, XtWindow(arg->rotation));
    XMapWindow(dpy, XtWindow(arg->inclinationLabel));
    XMapWindow(dpy, XtWindow(arg->inclination));
    XMapWindow(dpy, XtWindow(arg->dilationLabel));
    XMapWindow(dpy, XtWindow(arg->dilation));
    XMapWindow(dpy, XtWindow(arg->linespacingLabel));
    XMapWindow(dpy, XtWindow(arg->linespacing));
    XMapWindow(dpy, XtWindow(arg->applyButton));
    XMapWindow(dpy, XtWindow(arg->menubar));

    XtUnmanageChild(arg->vport2);
    XMapWindow(dpy, XtWindow(arg->vport2));
    XFlush(dpy);

    XtAddCallback(arg->family, XtNcallback, listCallback, (XtPointer) arg);
    XtAddCallback(arg->weight, XtNcallback, listCallback, (XtPointer) arg);
 
    XtAddEventHandler(arg->family, ButtonPressMask, False,
		      (XtEventHandler) mousewheelScroll, (XtPointer) 1);
    XtAddEventHandler(arg->weight, ButtonPressMask, False,
		      (XtEventHandler) mousewheelScroll, (XtPointer) 1);
    XtAddEventHandler(arg->sample, ButtonPressMask, False,
		      (XtEventHandler) mousewheelScroll, (XtPointer) 1);

    XtInsertRawEventHandler(arg->family, ButtonPressMask, False,
			    (XtEventHandler) unselectCallback, (XtPointer) arg,
                            XtListHead);
    XtInsertRawEventHandler(arg->weight, ButtonPressMask, False,
			    (XtEventHandler) unselectCallback, (XtPointer) arg,
                            XtListHead);

    XtAddCallback(arg->pointBar, XtNjumpProc,
		  (XtCallbackProc) barCallback, (XtPointer) arg);
    XtAddCallback(arg->pointBar, XtNscrollProc,
		  (XtCallbackProc) scrollCallback, (XtPointer) arg);

    XtAddEventHandler(arg->pointSelect, 
                      ButtonPressMask|ButtonReleaseMask, False,
                      (XtEventHandler) clickFocusCallback, (XtPointer) arg->shell);
    XtAddEventHandler(arg->rotation,
                      ButtonPressMask|ButtonReleaseMask, False,
                      (XtEventHandler) clickFocusCallback, (XtPointer) arg->shell);
    XtAddEventHandler(arg->dilation,
                      ButtonPressMask|ButtonReleaseMask, False,
                      (XtEventHandler) clickFocusCallback, (XtPointer) arg->shell);
    XtAddEventHandler(arg->inclination,
                      ButtonPressMask|ButtonReleaseMask, False,
                      (XtEventHandler) clickFocusCallback, (XtPointer) arg->shell);
    XtAddEventHandler(arg->linespacing,
                      ButtonPressMask|ButtonReleaseMask, False,
                      (XtEventHandler) clickFocusCallback, (XtPointer) arg->shell);

    XtAddEventHandler(arg->sample, 
                      ButtonPressMask|ButtonReleaseMask, False,
                      (XtEventHandler) clickFocusCallback, (XtPointer) arg->shell);
    XtInsertRawEventHandler(arg->sample, 
			    ButtonPressMask | ButtonReleaseMask,
                            False, (XtEventHandler) popupHandler, arg, 
                            XtListHead);

    XtAddCallback(editButton, XtNcallback, editCallback, (XtPointer) arg);
    XtAddCallback(applyButton, XtNcallback, applyCallback, (XtPointer) arg);
    XtAddCallback(doneButton, XtNcallback, closeCallback, (XtPointer) arg);
    AddDestroyCallback(arg->shell, (DestroyCallbackFunc) closeCallback, arg);

    XtAddCallback(fileMenu[FILE_LOAD].widget, XtNcallback, 
		     (XtCallbackProc) loadFileCallback, (XtPointer) arg);
    XtAddCallback(fileMenu[FILE_SAVE].widget, XtNcallback, 
		     (XtCallbackProc) saveFileCallback, (XtPointer) arg);
    XtAddCallback(fileMenu[FILE_SAVEAS].widget, XtNcallback, 
		     (XtCallbackProc) saveasFileCallback, (XtPointer) arg);
    XtAddCallback(fileMenu[FILE_EDIT].widget, XtNcallback, 
		     (XtCallbackProc) externCallback, (XtPointer) arg);

    XtAddCallback(editMenu[EDIT_DEFAULT].widget, XtNcallback, 
		     (XtCallbackProc) defaultTextCallback, (XtPointer) arg);
    XtAddCallback(editMenu[EDIT_ERASE].widget, XtNcallback, 
		     (XtCallbackProc) eraseTextCallback, (XtPointer) arg);

    for (j=EDIT_FONT; j<=EDIT_PLUS_Y; j++)
    XtAddCallback(editMenu[j].widget, XtNcallback, 
		     (XtCallbackProc) insertTextCallback, (XtPointer) arg);

    XtAddEventHandler(arg->shell, StructureNotifyMask, False,
		      (XtEventHandler) fontboxResized, (XtPointer) arg);
}

void *
setDefaultGlobalFont(Display *dpy, char *name)
{
    char *ptr1, *ptr2, *str;
    XftFont *font;
    Global.xft_size = 18;
    font = XftFontOpenName(dpy, DefaultScreen(dpy), name);
    if (!font) {
	Notice(Global.toplevel, msgText[UNABLE_TO_LOAD_REQUESTED_FONT]);
	return NULL;
    }
    if (Global.xft_name)
        free(Global.xft_name);
    Global.xft_name = strdup(name);
    if (Global.xft_font)
        XftFontClose(dpy, (XftFont *)Global.xft_font);
    Global.xft_font = (void *) font;
    Global.xft_height = font->height;
    Global.xft_ascent = font->ascent;
    Global.xft_descent = font->descent;
    Global.xft_maxadv = font->max_advance_width;
    Global.xft_rotation = 0;
    Global.xft_dilation = 1.0;
    Global.xft_inclination = 0.0;
    Global.xft_linespacing = 1.0;
    str = strdup(name);
    ptr1 = strchr(str, '-');
    if (ptr1) {
        ++ptr1;
        ptr2 = strchr(ptr1, ':');
        if (ptr2) *ptr2 = '\0';
        Global.xft_size = atoi(ptr1);
    } else
        /* guess size from height */
        Global.xft_size = (int)(Global.xft_height * 0.643);
    free(str);
    return (void *) font;
}
