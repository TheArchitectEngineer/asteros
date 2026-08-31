/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)            | */
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

/* $Id: fontOp.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtWindow XTWINDOW
#endif

#include <math.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>
#include "xpaint.h"
#include "Paint.h"
#include "PaintP.h"
#include "graphic.h"
#include "misc.h"
#include "ops.h"
#include "protocol.h"

static Atom targetAtom, selectionAtom;

#define	DELAY	500

typedef struct {
    unsigned long pixel;
    char * xft_name;
    double x, y;
    char ch[4];
} Replay;

typedef struct {
    Widget w;
    Drawable drawable;
    GC gc, gcx;
    XtIntervalId id;
    Boolean typing;
    Boolean state;
    double startX, startY, curX, curY;
    double rotation, inclination, dilation, linespacing; 
    double font_height, maxadv;
    long pixel;
    XftDraw *draw, *drawp;
    XftColor color;
    XftFont *font;
    char *xft_name;
    Replay * replay;
    int replay_length;
    char *sptr;
    int maxStrLen;
    char *str;
    OpInfo * info;
} LocalInfo;

static void addString(Widget, LocalInfo *, int, char *);

static void 
cursor(LocalInfo * l, Boolean flag)
{
    static int zoom0 = 0;
    int zoom; 
    static int x, y, xp, yp;
    double c, s;

    if (!l->w)return;
    XtVaGetValues(l->w, XtNzoom, &zoom, NULL);
    if (zoom != zoom0) l->state = False;
    zoom0 = zoom;
    
    if (l->state == False) {
        if (!Global.xft_font) return;
        c = cos(Global.xft_rotation);
	s = sin(Global.xft_rotation);
        if (zoom>0) {
	    x = (int)((l->curX + Global.xft_descent * s)*zoom);
	    y = (int)((l->curY + Global.xft_descent * c)*zoom);
	    xp = (int)((l->curX - Global.xft_ascent * s)*zoom);
	    yp = (int)((l->curY - Global.xft_ascent * c)*zoom);
	} else {
	    x = (int)((l->curX + Global.xft_descent * s)/(-zoom));
	    y = (int)((l->curY + Global.xft_descent * c)/(-zoom));
	    xp = (int)((l->curX - Global.xft_ascent * s)/(-zoom));
	    yp = (int)((l->curY - Global.xft_ascent * c)/(-zoom));
	}
    }
    if (flag || l->state) {
	XDrawLine(XtDisplay(l->w), XtWindow(l->w), l->gcx, x, y, xp, yp);
        l->state = !l->state;
    } else
	l->state = False;
}

static void 
flash(LocalInfo * l)
{
    if (!l->w) return;
    cursor(l, True);
    l->id = XtAppAddTimeOut(XtWidgetToApplicationContext(l->w),
		      DELAY, (XtTimerCallbackProc) flash, (XtPointer) l);
}

static void 
gotSelection(Widget w, LocalInfo * l, Atom * selection, Atom * type,
	     XtPointer value, unsigned long *len, int *format)
{
    if (!l->typing)
	return;

    if (len == 0 || value == NULL)
	return;

    switch (*type) {
    case XA_STRING:
	addString(w, l, *len, (char *) value);
	break;
    }
    XtFree((XtPointer) value);
}

#define M_PI_180 M_PI/180

void
FindFont(Display *dpy, LocalInfo *l)
{
    double mat[4];
    char matrix[80];
    char *buf, *ptr;

    if (!l->xft_name) return;
    buf = strdup(l->xft_name);
    if ((ptr = strstr(buf, ":matrix"))) *ptr = '\0';

    if (l->rotation!=0.0 || l->dilation!= 1.0 || l->inclination!=0.0) {
        mat[0] = cos(l->rotation);
        mat[2] = sin(l->rotation);
        mat[1] = l->dilation*(l->inclination*mat[0]-mat[2]);
        mat[3] = l->dilation*(l->inclination*mat[2]+mat[0]);
        sprintf(matrix, ":matrix=%g %g %g %g", mat[0], mat[1], mat[2], mat[3]);
        ptr = (char *)xmalloc(strlen(buf)+strlen(matrix)+1);
        strcpy(ptr, buf);
        strcat(ptr, matrix);
        free(buf);
    } else {
        ptr = buf;
        *matrix = '\0';
    }

    if (strcmp(l->xft_name, ptr)) {
        free(l->xft_name);
        l->xft_name = ptr;
        ptr = strstr(l->xft_name, ":matrix");
        if (ptr) *ptr = '\0';
        if (l->font) XftFontClose(dpy, l->font);
        l->font = XftFontOpenName(dpy, DefaultScreen(dpy), l->xft_name);
        l->font_height = l->font->height * l->dilation;
        l->maxadv = l->font->max_advance_width * l->dilation;     
        if (ptr) {
	    *ptr = ':';
            XftFontClose(dpy, l->font);
            l->font = XftFontOpenName(dpy, DefaultScreen(dpy), l->xft_name);
	}
    } else {
        free(ptr);
        if (!l->font) 
             l->font = XftFontOpenName(dpy, DefaultScreen(dpy), l->xft_name);
    }
}

static void
FreeReplay(LocalInfo *l)
{
    int j, k, found;

    if (!l->replay || !l->replay_length) return;

    for (j=l->replay_length-1; j>=0; j--) {
        found = 0;
        for (k=0; k<j; k++) {
	    if (l->replay[k].xft_name == l->replay[j].xft_name) {
	        found = 1;
                break;
	    }
	}
	if (!found) free(l->replay[j].xft_name);
    }

    if (l->replay) free(l->replay);
    l->replay = NULL;
    l->replay_length = 0;
}

Drawable 
GetLocalInfoDrawable(void *info) 
{
    LocalInfo *l = (LocalInfo *)info;
    return l->drawable;
}

void
WriteText(Widget w, void **info, char * text)
{
    LocalInfo *l;
    Display *dpy = XtDisplay(w);
    XGlyphInfo extents = {};
    XRenderColor xre_color;
    XColor xcol;
    Colormap cmap;
    Pixel fg;
    int m, mp, cmd, bs, zoom=1, found, charlen;
    double rad, dx, dy;
    XRectangle rect;
    char *start, *ptr, *str;

    if (!text) return;
    XtVaGetValues(w, XtNcolormap, &cmap, NULL);

    if (info) {
        if (*info) {
	    l = (LocalInfo *)(* info);
            if (!text) {
	        FreeReplay(l);
                XFreePixmap(dpy, l->drawable);
                XFreeGC(dpy, l->gc);
                if (l->font) XftFontClose(dpy, l->font);
                if (DefaultDepth(dpy, DefaultScreen(dpy))>8)
                    XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
		                 cmap, &l->color);
                free(l->xft_name);
                XftDrawDestroy(l->drawp);
                free(l);
                return;
	    }
        } else {
            l = (LocalInfo *) xmalloc(sizeof(LocalInfo));
            *info = l;
            sscanf(text, "%d %d", &m, &mp);
            l->drawable = XCreatePixmap(dpy, XtWindow(w), m, mp, 
                                        DefaultDepthOfScreen(XtScreen(w)));
            l->gc = XCreateGC(dpy, l->drawable, 0, 0);
            XSetForeground(dpy, l->gc, WhitePixelOfScreen(XtScreen(w)));
            XFillRectangle(dpy, l->drawable, l->gc, 0, 0, m, mp);
            l->startX = l->curX = 50;
            l->startY = l->curY = 80;
            l->replay_length = 0;
            l->replay = NULL;
            l->pixel = 0;
            xre_color.red = 0;
            xre_color.green = 0;
            xre_color.blue = 0;
            xre_color.alpha = 255<<8;
            zoom = -1;
	}
    } else {
        l = (LocalInfo *) GraphicGetData(w);
        if (!l) return;
        if (!l->typing) return;
        cursor(l, False);
        StateSetBusy(True);
        XtVaGetValues(w, XtNforeground, &fg, XtNzoom, &zoom, NULL);
        xcol.pixel = fg;
        xcol.flags = DoRed | DoGreen | DoBlue;
        XQueryColor(dpy, cmap, &xcol);
        xre_color.red = xcol.red;
        xre_color.green = xcol.green;
        xre_color.blue = xcol.blue;
        xre_color.alpha = 255<<8;
        l->draw = XftDrawCreate(XtDisplay(w), XtWindow(w),
                                DefaultVisual(dpy, DefaultScreen(dpy)), cmap);
    }

    if (!info || zoom == -1) {
        l->drawp = XftDrawCreate(dpy, l->drawable,
                                 DefaultVisual(dpy, DefaultScreen(dpy)), cmap);
        XftColorAllocValue(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
		           cmap, &xre_color, &l->color);
        l->rotation = Global.xft_rotation;
        l->font_height = Global.xft_height;
        l->linespacing = Global.xft_linespacing;
        l->inclination = Global.xft_inclination;
        l->dilation = Global.xft_dilation;
        l->maxadv = Global.xft_maxadv;
        l->xft_name = strdup(Global.xft_name);
        l->font = NULL;
        FindFont(dpy, l);
        if (zoom == -1) return;
    }

    if (!l->font) goto terminate;

    start = text;

    while (*start) {
        cmd = 0;
        ptr = start;
        while (*ptr) {
	    if (*ptr == '\\' && *(ptr+1) == '*') {
	        cmd = 1;
                break;
	    }
            if (*ptr == '\\') {
	        ++ptr;
		if (*ptr) ++ptr;
	    } else
		++ptr;
	}
        bs = 0;
        for (str=start; str<ptr; str++) {
	    if (!bs && *str == '\\') {
                bs = 1;
                continue;
	    }
            if (bs && *str == '\n') {
		bs = 0;
                continue;
	    }
            bs = 0;
            m = l->replay_length;
            ++l->replay_length;
            l->replay = 
	        (Replay *) realloc(l->replay, l->replay_length*sizeof(Replay));
            charlen = GetCharLength(str);
            memcpy(l->replay[m].ch, str, charlen);

            if (*str == '\n') {
	        l->startX += l->font_height*sin(l->rotation)*l->linespacing;
	        l->startY += l->font_height*cos(l->rotation)*l->linespacing; 
                l->curX = l->startX;
                l->curY = l->startY;
                l->replay[m].x = l->curX;
                l->replay[m].y = l->curY;
                l->replay[m].xft_name = (char *)xmalloc(2+2*sizeof(double));
                l->replay[m].xft_name[0] = '\0';
                memcpy(l->replay[m].xft_name+2, &l->startX, sizeof(double));
                memcpy(l->replay[m].xft_name+2+sizeof(double), 
                       &l->startY, sizeof(double));
	    } else {
                l->replay[m].x = l->curX;
                l->replay[m].y = l->curY;
                l->replay[m].pixel = l->pixel;
                found = 0;
                for (mp=0; mp<m; mp++) {
		    if (!strcmp(l->replay[mp].xft_name, l->xft_name)) {
		        found = 1;
                        l->replay[m].xft_name = l->replay[mp].xft_name;
                        break;
		    }
		}
                if (!found) l->replay[m].xft_name = strdup(l->xft_name);
                XftTextExtentsUtf8(dpy, l->font,
			        (FcChar8*)str, charlen, (XGlyphInfo*)&extents);
	        if (!info && zoom == 1)
	            XftDrawStringUtf8(l->draw, &l->color, l->font,
			              (int)(l->curX), (int)(l->curY),
                                      (XftChar8*)str, charlen);
	        XftDrawStringUtf8(l->drawp, &l->color, l->font,
			          (int)(l->curX), (int)(l->curY),
                                  (XftChar8*)str, charlen);
		rad = sqrt(extents.xOff*extents.xOff+extents.yOff*extents.yOff);
	        dx = rad * cos(l->rotation);
	        dy = -rad * sin(l->rotation);
                if (!info) {
                    rad = sqrt(l->maxadv*l->maxadv+
                               4*l->font_height*l->font_height);
	            XYtoRECT((int)(l->curX+0.5*(dx-rad)), 
		             (int)(l->curY+0.5*(dy-rad)), 
                             (int)(l->curX+0.5*(dx+rad)), 
                             (int)(l->curY+0.5*(dy+rad)), &rect);
	            PwUpdate(w, &rect, False);
	            UndoGrow(w, rect.x, rect.y);
                    UndoGrow(w, rect.x+rect.width, rect.y+rect.height);
		}
                l->curX += dx;
                l->curY += dy;
	    }
	}
    	if (cmd) {
	    ptr = ptr+2;
            str =  strstr(ptr, "*\\");
            if (!str) break;
	    *str = '\0';
            if (!strncasecmp(ptr, "x:", 2)) {
	        l->curX = atof(ptr+2);
	    } else
            if (!strncasecmp(ptr, "y:", 2)) {
	        l->curY = atof(ptr+2);
	    } else
            if (!strncasecmp(ptr, "+x:", 3)) {
	        l->curX += atof(ptr+3);
	    } else
            if (!strncasecmp(ptr, "+y:", 3)) {
	        l->curY += atof(ptr+3);
	    } else
            if (!strncasecmp(ptr, "sx:", 3)) {
	        l->startX = atof(ptr+3);
	    } else
            if (!strncasecmp(ptr, "sy:", 3)) {
	        l->startY = atof(ptr+3);
	    } else
            if (!strncasecmp(ptr, "+sx:", 4)) {
	        l->startX += atof(ptr+4);
	    } else
            if (!strncasecmp(ptr, "+sy:", 4)) {
	        l->startY += atof(ptr+4);
	    } else
            if (!strncasecmp(ptr, "rotation:", 9)) {
	        l->rotation = atof(ptr+9)*M_PI_180;
                FindFont(dpy, l);
	    } else
            if (!strncasecmp(ptr, "+rotation:", 10)) {
	        l->rotation += atof(ptr+10)*M_PI_180;
                FindFont(dpy, l);
	    } else
            if (!strncasecmp(ptr, "dilation:", 9)) {
	        l->dilation = atof(ptr+9);
                FindFont(dpy, l);
	    } else
            if (!strncasecmp(ptr, "inclination:", 12)) {
	        l->inclination = atof(ptr+12);
                FindFont(dpy, l);
	    } else
            if (!strncasecmp(ptr, "linespacing:", 12)) {
	        l->linespacing = atof(ptr+12);
	    } else
            if (!strncasecmp(ptr, "color:", 6)) {
                if (XAllocNamedColor(dpy, cmap, ptr+6, &xcol, &xcol)!=0)  {
 		    l->pixel = xcol.pixel;
                    xre_color.red = xcol.red;
                    xre_color.green = xcol.green;
                    xre_color.blue = xcol.blue;
                    xre_color.alpha = 255<<8;
                    if (DefaultDepth(dpy, DefaultScreen(dpy))>8)
                        XftColorFree(dpy, 
                                     DefaultVisual(dpy, DefaultScreen(dpy)),
		                     cmap, &l->color);
                    XftColorAllocValue(dpy, 
                                       DefaultVisual(dpy, DefaultScreen(dpy)),
		                       cmap, &xre_color, &l->color);
	        }
	    } else
            if (!strncasecmp(ptr, "font:", 5)) {
	        if (l->xft_name && strncmp(ptr+5, l->xft_name, strlen(ptr+5))) {
                    free(l->xft_name);
                    l->xft_name = strdup(ptr+5);
                    if (l->font) XftFontClose(dpy, l->font);
                    l->font = NULL;
                    FindFont(dpy, l);
		}
	    }
            *str = '*';
            start = str+2;
	} else
            start = ptr;
    }

    if (!info) PwUpdate(w, &rect, True);

 terminate:
    if (!info) {
        if (l->font) XftFontClose(dpy, l->font);
        l->font = NULL;
        if (DefaultDepth(dpy, DefaultScreen(dpy))>8)
            XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
		         cmap, &l->color);
        free(l->xft_name);
        XftDrawDestroy(l->draw);
        XftDrawDestroy(l->drawp);
        StateSetBusy(False);
    }
}

static void 
motion(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
}

static void 
press(Widget w, LocalInfo * l, XButtonEvent * event, OpInfo * info)
{
    if (event->type == ButtonRelease) {
        if (Global.popped_up)
            PopdownMenusGlobal();
        event->type = None;
        return;
    }

    if (event->button == Button3) return;

    if (event->button == Button1) {
	cursor(l, False);

	l->w = w;
	l->typing = True;
	l->gc = info->first_gc;
        l->info = info;
        FreeReplay(l);

	l->startX = l->curX = info->x + Global.xft_descent * sin(Global.xft_rotation) + 0.5;
	l->startY = l->curY = info->y + Global.xft_descent * cos(Global.xft_rotation) + 0.5;

	l->sptr = l->str;

	if (l->id == (XtIntervalId) NULL)
	    flash(l);

	UndoStartPoint(w, info, l->curX, l->curY);
	if (info->surface == opPixmap)
	    l->drawable = info->drawable;

    } else if (event->button == Button2) {
	XtGetSelectionValue(w, selectionAtom, targetAtom,
			    (XtSelectionCallbackProc) gotSelection,
                            (XtPointer) l, event->time);
    }
}

static void 
key(Widget w, LocalInfo * l, XKeyEvent * event, OpInfo * info)
{
    char buf[21];
    KeySym keysym;
    int len;
#ifdef XAW3DXFT
    Status status;
    extern int XftEncoding;
    static XIM xim = None;
    static XIC xic = None;
    static Widget w0 = None;
#endif


    if (!l->w) return;

#ifdef XAW3DXFT
    if (XftEncoding==0) {
        if (!xim) xim = XOpenIM(XtDisplay(w), NULL, NULL, NULL);
        if (w!=w0 && xic) {
            XDestroyIC(xic);
            xic = None;
            w0 = w;
	}
        if (!xic) {
            xic = XCreateIC(xim,
		            XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
		            XNClientWindow, XtWindow(w),
		            XNFocusWindow,  XtWindow(w),
		            (void*)NULL);
            Xutf8ResetIC(xic);
	}
        len = Xutf8LookupString(xic, event, buf, sizeof(buf) - 1, &keysym, &status);
    } else
#endif
        len = XLookupString(event, buf, sizeof(buf) - 1, &keysym, NULL);
    if (len == 0)
	return;

    l->gc = info->first_gc;
    l->drawable = info->drawable;
    addString(w, l, len, buf);
}

static void 
addString(Widget w, LocalInfo * l, int len, char *buf)
{
    Display *dpy = XtDisplay(w);
    PaintWidget pw = (PaintWidget) w;
    XftDraw *draw = NULL, *drawp = NULL;
    XGlyphInfo extents = {};
    XftColor color;
    XftFont *font;
    XRenderColor xre_color;
    Pixmap pixmap;
    XColor xcol;
    Colormap cmap;
    Pixel fg;
    GC gc;
    int i, j, k, charlen, width, height, depth, zoom, found;
    double rad, dx, dy;
    XRectangle rect;
    char *ptr;

    if (len != 0)
	cursor(l, False);
    for (i = 0; i < len; i++) {
	if (l->sptr == l->str + l->maxStrLen - 5) {
	    int delta = l->sptr - l->str;
	    l->maxStrLen += 128;
	    l->str = (char *) XtRealloc((XtPointer) l->str, l->maxStrLen);
	    l->sptr = l->str + delta;
	}
	if (buf[i] == '\n' || buf[i] == '\r') {
	    if (!Global.xft_font || !Global.xft_name) continue;
            j = l->replay_length;
            ++l->replay_length;
            l->replay = 
	        (Replay *) realloc(l->replay, l->replay_length*sizeof(Replay));
            l->replay[j].ch[0] = '\n';
            l->replay[j].x = l->curX;
            l->replay[j].y = l->curY;

	    l->startX += Global.xft_height*sin(Global.xft_rotation)*Global.xft_linespacing;
	    l->startY += Global.xft_height*cos(Global.xft_rotation)*Global.xft_linespacing; 

	    l->curX = l->startX;
	    l->curY = l->startY;
            l->replay[j].xft_name = (char *)xmalloc(2+2*sizeof(double));
            l->replay[j].xft_name[0] = '\0';
            memcpy(l->replay[j].xft_name+2, &l->startX, sizeof(double));
            memcpy(l->replay[j].xft_name+2+sizeof(double), 
                   &l->startY, sizeof(double));
	    *l->sptr++ = '\n';
	} else if (buf[i] == 0x08 || buf[i] == 0x7f) {
            StateSetBusy(True);
            if (l->replay_length == 0) {
	        goto terminate;
	    }
            cursor(l, False);
	    XtVaGetValues(w, XtNcolormap, &cmap, NULL);
            pixmap = pw->paint.undo->alphapix.pixmap;
            GetPixmapWHD(dpy, pixmap, &width, &height, &depth);
            gc = XCreateGC(dpy, l->drawable, 0, 0);
            XCopyArea(dpy, pixmap, l->drawable, gc, 0, 0, width, height, 0, 0);
 	    drawp = XftDrawCreate(XtDisplay(w), l->drawable,
                                  DefaultVisual(dpy, DefaultScreen(dpy)), cmap);
            l->startX = l->replay[0].x;
            l->startY = l->replay[0].y;
            font = NULL;
            ptr = "";
            for (j=0; j<l->replay_length-1; j++) {
	        if (l->replay[j].ch[0] == '\n') {
		    memcpy(&l->startX, l->replay[j].xft_name+2, sizeof(double));
		    memcpy(&l->startY, 
		        l->replay[j].xft_name+2+sizeof(double), sizeof(double));
                    continue;
		}
                xcol.pixel = l->replay[j].pixel;
                xcol.flags = DoRed | DoGreen | DoBlue;
                XQueryColor(dpy, cmap, &xcol);
                xre_color.red = xcol.red;
                xre_color.green = xcol.green;
                xre_color.blue = xcol.blue;
                xre_color.alpha = 255<<8;
                if (!font || strcmp(l->replay[j].xft_name, ptr)) {
		    if (font) XftFontClose(dpy, font);
                    font = XftFontOpenName(dpy, DefaultScreen(dpy), 
                                           l->replay[j].xft_name);
                    ptr = l->replay[j].xft_name;
		}
                if (!font) continue;
                XftColorAllocValue(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
			           cmap, &xre_color, &color);
                charlen = GetCharLength(l->replay[j].ch);
  	        XftDrawStringUtf8(drawp, &color, font,
			       (int)l->replay[j].x, (int)l->replay[j].y,
                               (XftChar8*)l->replay[j].ch, charlen);
                if (DefaultDepth(dpy, DefaultScreen(dpy))>8)
                    XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
			         cmap, &color);
	    }
            if (font) XftFontClose(dpy, font);
  	    j = l->replay_length - 1;
            l->curX = l->replay[j].x;
            l->curY = l->replay[j].y;
            found = 0;
            for (k=0; k<j; k++) {
	        if (l->replay[k].xft_name == l->replay[j].xft_name) {
	            found = 1;
                    break;
		}
	    }
	    if (!found) free(l->replay[j].xft_name);
            l->replay_length = j;
            if (l->replay_length==0) {
	        free(l->replay);
                l->replay = NULL;
            } else
                l->replay = 
	        (Replay *) realloc(l->replay, l->replay_length*sizeof(Replay));
            XftDrawDestroy(drawp);
            XFreeGC(dpy, gc);
            rect.x = 0;
            rect.y = 0;
            rect.width = width;
            rect.height = height;
            PwUpdate(w, &rect, True);
        terminate:
            StateSetBusy(False);
	} else {
	    if (!Global.xft_font || !Global.xft_name) continue;
            /* Fill replay structure for undoing */
            j = l->replay_length;
            ++l->replay_length;
            l->replay = 
	        (Replay *) realloc(l->replay, l->replay_length*sizeof(Replay));

	    XtVaGetValues(w, XtNforeground, &fg, XtNcolormap, &cmap, NULL);
            l->replay[j].pixel = fg;
            found = 0;
            for (k=0; k<j; k++) {
	        if (!strcmp(l->replay[k].xft_name, Global.xft_name)) {
	            l->replay[j].xft_name = l->replay[k].xft_name;
                    found = 1;
                    break;
		}
	    }
	    if (!found) l->replay[j].xft_name = strdup(Global.xft_name);
            l->replay[j].x = l->curX;
            l->replay[j].y = l->curY;
            charlen = GetCharLength(&buf[i]);
            memcpy(l->replay[j].ch, &buf[i], charlen);

            xcol.pixel = fg;
            xcol.flags = DoRed | DoGreen | DoBlue;
            XQueryColor(dpy, cmap, &xcol);
            xre_color.red = xcol.red;
            xre_color.green = xcol.green;
            xre_color.blue = xcol.blue;
            xre_color.alpha = 255<<8;
            XftColorAllocValue(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
			       cmap, &xre_color, &color);
            charlen = GetCharLength(&buf[i]);
	    XftTextExtentsUtf8(dpy, Global.xft_font,
			    (FcChar8*)&buf[i], charlen, (XGlyphInfo*)&extents);
 	    draw = XftDrawCreate(XtDisplay(w), XtWindow(w),
                                 DefaultVisual(dpy, DefaultScreen(dpy)),
				 cmap);
 	    drawp = XftDrawCreate(XtDisplay(w), l->drawable,
                                 DefaultVisual(dpy, DefaultScreen(dpy)),
				 cmap);

  	    XtVaGetValues(w, XtNzoom, &zoom, NULL);
	    if (zoom == 1)
	        XftDrawStringUtf8(draw, &color, Global.xft_font,
			       (int)(l->curX), (int)(l->curY),
                               (XftChar8*)&buf[i], charlen);
	    XftDrawStringUtf8(drawp, &color, Global.xft_font,
			   (int)(l->curX), (int)(l->curY),
                           (XftChar8*)&buf[i], charlen);
            if (DefaultDepth(dpy, DefaultScreen(dpy))>8)
                XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
			     cmap, &color);
 	    XftDrawDestroy(draw);
 	    XftDrawDestroy(drawp);
            rad = sqrt(extents.xOff*extents.xOff+extents.yOff*extents.yOff);
	    dx = rad * cos(Global.xft_rotation);
	    dy = -rad * sin(Global.xft_rotation);
            rad = sqrt(Global.xft_maxadv*Global.xft_maxadv+
                       4*Global.xft_height*Global.xft_height);
	    XYtoRECT((int)(l->curX+0.5*(dx-rad)), 
		     (int)(l->curY+0.5*(dy-rad)), 
                     (int)(l->curX+0.5*(dx+rad)), 
                     (int)(l->curY+0.5*(dy+rad)), &rect);
	    PwUpdate(w, &rect, False);
	    l->curX += dx;
            l->curY += dy;
	    UndoGrow(w, rect.x, rect.y);
            UndoGrow(w, rect.x+rect.width, rect.y+rect.height);

            while (charlen>0) {
	        *l->sptr++ = buf[i++];
                --charlen;
	    }
            --i;
	}
    }
}

/*
**  Those public functions
 */
void *
FontAdd(Widget w)
{
    static int inited = False;
    LocalInfo *l = (LocalInfo *) XtMalloc(sizeof(LocalInfo));

    if (!inited) {
	selectionAtom = XA_PRIMARY;
	targetAtom = XA_STRING;
    }
    l->id = (XtIntervalId) NULL;
    l->w = None;
    l->typing = False;
    l->state = False;
    l->replay = NULL;
    l->replay_length = 0;

    l->maxStrLen = 128;
    l->str = (char *) XtMalloc(l->maxStrLen);
    l->gcx = GetGCX(w);
    setWriteTextSensitive(w, True);

    OpAddEventHandler(w, opPixmap | opWindow, 
                      ButtonPressMask|ButtonReleaseMask, FALSE,
		      (OpEventProc) press, l);
    OpAddEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motion, l);
    OpAddEventHandler(w, opPixmap, KeyPressMask, FALSE, (OpEventProc) key, l);

    SetIBeamCursor(w);

    return l;
}
void 
FontRemove(Widget w, void *p)
{
    LocalInfo *l = (LocalInfo *) p;

    setWriteTextSensitive(w, False);
    OpRemoveEventHandler(w, opPixmap | opWindow, 
                         ButtonPressMask|ButtonReleaseMask, FALSE,
			 (OpEventProc) press, p);
    OpRemoveEventHandler(w, opWindow, PointerMotionMask, FALSE,
		      (OpEventProc) motion, p);
    OpRemoveEventHandler(w, opPixmap, KeyPressMask, FALSE, (OpEventProc) key, p);

    if (l->id != (XtIntervalId) NULL)
	XtRemoveTimeOut(l->id);

    cursor(l, False);
 
    FreeReplay(l);
    XtFree((XtPointer) l->str);
    XtFree((XtPointer) l);
}

void 
FontChanged(Widget w)
{
    LocalInfo *l;

    setFontIcon(w);

    if (CurrentOp->add != FontAdd)
	return;

    l = (LocalInfo *) GraphicGetData(w);

    if (l->id != (XtIntervalId) NULL)
	XtRemoveTimeOut(l->id);

    cursor(l, False);
    XFlush(XtDisplay(w));
    flash(l);
}
