/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)	       | */
/* | Copyright 1995, 1996 Torsten Martinsen (bullestock@dk-online.dk)  | */
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

/* $Id: fileBrowser.c,v 1.18 2005/03/20 20:15:32 demailly Exp $ */

#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>

#include "xaw_incdir/Dialog.h"
#include "xaw_incdir/Command.h"
#include "xaw_incdir/Toggle.h"
#include "xaw_incdir/Form.h"
#include "xaw_incdir/Label.h"
#include "xaw_incdir/List.h"
#include "xaw_incdir/AsciiText.h"
#include "xaw_incdir/Text.h"
#include "xaw_incdir/Viewport.h"
#include "xaw_incdir/Scrollbar.h"

#include "Paint.h"
#include "PaintP.h"
#include "xpaint.h"
#include "messages.h"
#include "misc.h"
#include "image.h"
#include "text.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(SYSV) || defined(SVR4) || defined(__CYGWIN__) || defined(__VMS )
#include <dirent.h>
#else
#include <sys/dir.h>
#endif
#include <pwd.h>

#include "rw/rwTable.h"
#include "graphic.h"
#include "protocol.h"

#define MAX_PATH	1024
#define NUM_FORMATS END_IMAGE_FORMATS-IMAGE_FORMATS

#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#endif

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

typedef struct {
    Widget shell, pane, name, program, cclog;
    int mode;
    char *scriptfile;
} CScriptInfo;

static void *lastId = NULL;

extern int file_isSpecialImage;
extern int file_transparent;
extern int file_numpages;
extern int file_force;
extern int file_bbox;
extern int file_specified_zoom;

void *
GetFileNameGetLastId()
{
    if (lastId == NULL)
	return RWtableGetReaderID();
    return lastId;
}

/*
**  "Std" Save functions
 */

/*
 * This function is called by all image save functions.
 * If 'flag' is True, save the entire image; otherwise, save the region.
 */
static void
okSaveCallback(Widget paint, char *file, Boolean flag, RWwriteFunc f)
{
    PaintWidget pw = (PaintWidget) paint;
    Pixmap pix, mask = None;
    Colormap cmap;
    Image *image;

    StateSetBusy(True);

    if (flag) {
	PwGetPixmap(paint, &pix, NULL, NULL);
    } else {
	if (!PwRegionGet(paint, &pix, &mask)) {
	    Notice(paint, msgText[UNABLE_TO_GET_REGION]);
            goto terminate;
	}
    }
    XtVaGetValues(paint, XtNcolormap, &cmap, NULL);

    if ((image = PixmapToImage(paint, pix, cmap)) == NULL) {
	Notice(paint, msgText[UNABLE_TO_CREATE_IMAGE_FOR_SAVING]);
        goto terminate;
    }
    if (mask != None)
	PixmapToImageMask(paint, image, mask);

    if (pw->paint.current.alpha) {
        image->alpha = (unsigned char *)xmalloc(image->width*image->height);
        memcpy(image->alpha, pw->paint.current.alpha, image->width*image->height);
    }
    image->refCount++;

    if (f(file, image)) {
	Notice(paint, msgText[ERROR_SAVING_FILE], RWGetMsg());
    } else if (flag) {
	XtVaSetValues(paint, XtNdirty, False, NULL);
	if (pw->paint.filename != NULL)
	    free(pw->paint.filename);
	pw->paint.filename = xmalloc(strlen(file) + 1);
	strcpy(pw->paint.filename, file);
	EnableRevert(paint);
    }
    image->refCount--;
    ImageDelete(image);
 terminate:
    StateSetBusy(False);
}

static void
okOverwriteCallback(Widget shell, XtPointer data, XtPointer junk)
{
    void **ptr = data;
    okSaveCallback(shell, (char *)ptr[0], ptr[1]?True:False, 
                   (RWwriteFunc)ptr[2]);
}

static void
noOverwriteCallback(Widget shell, XtPointer data, XtPointer junk)
{
}

static void 
stdSaveCommonCallback(Widget paint, char *file, Boolean flag, RWwriteFunc f)
{
    char msg[512];
    static void *ptr[3];
    struct stat buf;

    if (*file == '\0') {
	Notice(paint, msgText[NO_FILE_NAME_SUPPLIED]);
	return;
    }
    if (!stat(file, &buf)) {
        sprintf(msg, msgText[OVERWRITE_FILE], file);
	ptr[0] = file;
	ptr[1] = (void *)((long)flag);
	ptr[2] = (void *)f;
        AlertBox(paint, msg, okOverwriteCallback, 
                             noOverwriteCallback, (XtPointer)ptr);
	return;
    }
    okSaveCallback(paint, file, flag, f);
}

static void 
saveRegionFileCallback(Widget paint, XtPointer str, XtPointer func)
{
    stdSaveCommonCallback(paint, (char *) str, False, (RWwriteFunc) func);
}

static void 
saveFileCallback(Widget paint, XtPointer str, XtPointer func)
{
    stdSaveCommonCallback(paint, (char *) str, True, (RWwriteFunc) func);
    StoreName(GetShell(paint), str);
}

void
StdSaveRegionFile(Widget w, XtPointer paintArg, XtPointer junk)
{
    Widget paint = (Widget) paintArg;

    if (PwRegionGet(paint, NULL, NULL))
	GetFileName(paint, BROWSER_SAVE, NULL, saveRegionFileCallback, NULL);
    else
	Notice(paint, msgText[NO_REGION_SELECTED_PRESENTLY]);
}

void 
StdSaveAsFile(Widget w, XtPointer paintArg, XtPointer junk)
{
    Widget paint = (Widget) paintArg;
    String name;
    String nm = XtName(GetShell(paint));

    XtVaGetValues(GetShell(paint), XtNtitle, &name, NULL);

    if (strcmp(name, msgText[DEFAULT_TITLE]) == 0 || strcmp(nm, name) == 0)
	name = NULL;

    GetFileName(paint, BROWSER_SAVE, name, saveFileCallback, NULL);
}

void 
StdSaveFile(Widget w, XtPointer paintArg, XtPointer junk)
{
    Widget paint = (Widget) paintArg;
    String name = NULL;
    String nm = XtName(GetShell(paint));
    RWwriteFunc f = NULL;
    void *id;

    if (strcmp(nm, "Canvas") == 0) {
	XtVaGetValues(GetShell(paint), XtNtitle, &name, NULL);

	if (strcmp(name, msgText[DEFAULT_TITLE]) == 0 || strcmp(nm, name) == 0)
	    name = NULL;
    }

    if (name != NULL) {
	if ((id = getArgType(paint)) != NULL) {
	    f = (RWwriteFunc) RWtableGetWriter(id);
	} else if ((id = GraphicGetReaderId(paint)) != NULL) {
	    f = (RWwriteFunc) RWtableGetWriter(RWtableGetEntry(id));
	}
	if (f != NULL) {
	    stdSaveCommonCallback(paint, name, True, f);
	    return;
	}
    }
    GetFileName(paint, BROWSER_SAVE, name, saveFileCallback, NULL);
}

/*
**
 */
void *
ReadMagic(char *file)
{
    RWreadFunc f = RWtableGetReader(RWtableGetEntry(MAGIC_READER));

    return (void *) f(file);
}

/*
**  The code begins
 */
typedef struct arg_s {
    XtCallbackProc okFunc;
    void *type;
    Boolean isRead, isSimple, isPopped, isToRefresh;
    Widget w;
    XtPointer closure;
    char dirname[MAX_PATH];
    Widget shell, list, browser, parent, 
           form, title_w, name, vport, 
           home, root, dot, hidden, ongoing, cwd_w, info, ok, cancel,
           refresh, delete, edit, create,
           zoomlabel, zoom, dpilabel, dpi, 
           bboxlabel, bboxleft, bbox, bboxright,
           pagelabel, page, pagetotal, pageleft, pageLeft, pageright, pageRight;
    Widget format[NUM_FORMATS+2];
    int first, numformat, oldwidth;
    /*
    **	Use for caching, list purposes
     */
    int browserType;
    struct arg_s *next;
} arg_t;

static arg_t *argList = NULL;
static XtTranslations trans = None, toglt = None;

static void 
fileTypeCallback(Widget w, XtPointer argArg, XtPointer junk)
{
    arg_t *arg = (arg_t *) argArg;
    String nm = XtName(w);

    arg->type = RWtableGetEntry(nm);
}

static void 
emptyList(arg_t * arg)
{
    String *strs;
    int i, n;

    if (arg->first) {
	arg->first = False;
	return;
    }
    XtVaGetValues(arg->list, XtNnumberStrings, &n, XtNlist, &strs, NULL);

    for (i = 0; i < n; i++)
	XtFree((XtPointer) strs[i]);
    XtFree((XtPointer) strs);
}

static void
browserResized(Widget w, arg_t * l, XConfigureEvent * event, Boolean * flag)
{
    Dimension width, height, width2, width3;
    Position x, y;
    int i, j, width1, height1, dw = 0, im;
    Widget *widget;

    XtVaGetValues(XtParent(l->parent), 
		  XtNwidth, &width, XtNheight, &height, NULL);

    if (l->oldwidth!= 0) dw = (int)width - l->oldwidth;   
    l->oldwidth = (int)width;
    XtVaGetValues(l->ok, XtNwidth, &width2, NULL);
    i = width2;
    XtVaGetValues(l->cancel, XtNwidth, &width2, NULL);
    i += width2 + 35; 
    for (j=0; j<l->numformat; j++) {
        XtVaGetValues(l->format[j], XtNwidth, &width3, NULL);
        width3 += 25;
        if (width3>i) i = width3;
    }

    width1 = (int)width - i; if (width1<60) width1 = 60;
    height1 = (int)height - 120;
    if (l->zoom) height1 -= 100;
    if (l->home) height1 -= 27;
    if (height1<60) height1 = 60;
    
    XtResizeWidget(l->browser, width, height, 0);
    XtResizeWidget(l->parent, width, height, 0);   
    if (l->zoom) 
        XResizeWindow(XtDisplay(l->parent), XtWindow(l->parent), 
        width, height);
    XtVaSetValues(l->browser, XtNwidth, width, XtNheight, height, NULL);
    XtVaSetValues(l->parent, XtNwidth, width, XtNheight, height, NULL);   
    XtResizeWidget(l->name, width1-2, 20, 1);
    XtResizeWidget(l->form, width1 + 10, height, 0);
    if (Global.numfiles) {
        XtVaGetValues(l->list, XtNlongest, &i, NULL);
        XtVaGetValues(l->list, XtNwidth, &width2, NULL);
        if (width2 > i+25) {
            width2 = i+25;
            XtVaSetValues(l->list, XtNwidth, width2, NULL);
	}
    }
    XtResizeWidget(l->vport, width1, height1, 1);     
    XtVaSetValues(l->vport, XtNwidth, width1, XtNheight, height1, NULL);
    XtVaGetValues(l->ok, XtNx, &x, XtNy, &y, NULL);
    if (dw) XtMoveWidget(l->ok, x+dw, y);
    XtVaGetValues(l->cancel, XtNx, &x, XtNy, &y, NULL);  
    if (dw) XtMoveWidget(l->cancel, x+dw, y);   
    if (dw) for (i=0; i<l->numformat; i++) {
        XtVaGetValues(l->format[i], XtNx, &x, XtNy, &y, NULL);  
        XtMoveWidget(l->format[i], x+dw, y);   
    }
    XtVaGetValues(l->cwd_w, XtNx, &x, NULL);
    if (l->home) {
        XtMoveWidget(l->home, 5, height-84);
        XtMoveWidget(l->root, 29, height-84);
        XtMoveWidget(l->dot, 53, height-84);
        XtMoveWidget(l->hidden, 77, height-84);
        if (l->ongoing) {
            XtVaGetValues(l->hidden, XtNwidth, &width1, NULL);
            XtMoveWidget(l->ongoing, 80+width1, height-84);
	}
        XtMoveWidget(l->cwd_w, x, height-52);
        XtMoveWidget(l->info, x, height-30);
        return;
    }

    XtMoveWidget(l->cwd_w, x, height-158);
    XtMoveWidget(l->info, x, height-134);

    im = &l->pageRight - &l->refresh;
    widget = &l->refresh;
    for (i=0; i<=im; i++) {
        XtVaGetValues(widget[i], XtNx, &x, NULL);
        if (i==0) j = 5-x;
        XtMoveWidget(widget[i], x+j, height - 
                     ((i<=3)? 99: ((i<=11)?67:35)));
    }
}

static char *
doDirname(arg_t * arg, char *file)
{
    static char newPath[MAX_PATH];
    char *cp;

    if (file == NULL) {
#ifdef NOSTDHDRS
        (void) getwd(newPath);
#else
	(void) getcwd(newPath, sizeof(newPath));
#endif
	return newPath;
    }
    if (*file == '~') {
	struct passwd *pw;

	file++;
	if (*file == '/' || *file == '\0') {
	    pw = getpwuid(getuid());
	} else {
	    char buf[80], *bp = buf;

	    while (*file != '/' && *file != '\0')
		*bp++ = *file++;
	    *bp = '\0';
	    pw = getpwnam(buf);
	}
	if (pw == NULL)
	    return NULL;
	while (*file == '/')
	    file++;
	strcpy(newPath, pw->pw_dir);
    } else if (*file == '/') {
	file++;
	newPath[0] = '\0';
    } else {
	strcpy(newPath, arg->dirname);
    }
#ifndef __VMS
    if (strcmp(newPath, "/") != 0)
	strcat(newPath, "/");
#endif
    while (*file != '\0') {
	char *ep;

	if ((ep = strchr(file, '/')) == NULL)
	    ep = file + strlen(file);
	if (strncmp(file, "./", 2) == 0 || strcmp(file, ".") == 0)
	    goto bottom;
	if (strncmp(file, "../", 3) == 0 || strcmp(file, "..") == 0) {
	    /*
	    **	First strip trailing '/'
	     */
	    char *cp = newPath + strlen(newPath) - 1;
	    if (cp != newPath)
		*cp = '\0';
	    cp = strrchr(newPath, '/');
	    if (cp == newPath)
		strcpy(newPath, "/");
	    else
		*cp = '\0';
	    goto bottom;
	}
	strncat(newPath, file, ep - file);
      bottom:
	file = ep;
	if (*file == '/')
	    strcat(newPath, "/");
	while (*file == '/')
	    file++;
    }

    /*
    **	Strip any trailing '/'s
     */
    cp = newPath + strlen(newPath) - 1;
    if (*cp == '/' && cp != newPath)
	*cp = '\0';

    return newPath;
}

static int 
strqsortcmp(char **a, char **b)
{
    String astr = *a;
    String bstr = *b;
    String aend = astr + strlen(astr) - 1;
    String bend = bstr + strlen(bstr) - 1;

    if (strncmp(astr, "../", 3) == 0)
	return -1;
    if (strncmp(bstr, "../", 3) == 0)
	return 1;

    if (*aend == '/' && *bend != '/')
	return -1;
    if (*aend != '/' && *bend == '/')
	return 1;
    return strcmp(astr, bstr);
}

static void 
setCWD(arg_t * arg, char *dir)
{
    DIR *dirp;
#if defined(SYSV) || defined(SVR4) || defined(__alpha) || defined(__CYGWIN__)
    struct dirent *e;
#else
    struct direct *e;
#endif
    int count = 0, i = 0;
    int dirCount = 0, fileCount = 0;
    String *list;
    char fileStr[MAX_PATH], *filePtr;
    static char infoStr[256];
    struct stat statbuf;
    Widget sb;
    Boolean state;

    if (arg->browserType == BROWSER_LOADED) {
        emptyList(arg);
        fileCount = Global.numfiles;
        list = (String *) XtCalloc(sizeof(String *), fileCount+2);
        for (i=0; i<fileCount; i++) {
            filePtr = Global.loadedfiles[i];
            if (*filePtr == '/') {
	        list[i] = XtMalloc(sizeof(char) * (strlen(filePtr) + 2));
	        strcpy(list[i], Global.loadedfiles[i]);
            } else {
	        list[i] = XtMalloc(sizeof(char) * 
				 (strlen(filePtr) + strlen(arg->dirname)+3));
	        sprintf(list[i], "%s/%s", arg->dirname, Global.loadedfiles[i]);
	    }
        }
        if (fileCount == 0)
	    list[0] = strdup("");
	XtVaSetValues(arg->cwd_w, XtNlabel, msgText[LIST_LOADED_FILES], NULL);
        sprintf(infoStr, "%d %s", fileCount, msgText[FILES]);
        i = fileCount;
        if (Global.numfiles<=1) 
	    XtVaSetValues(arg->name, XtNstring, list[0], NULL);
        goto fill;
    }

    if (dir == NULL)
	dir = arg->dirname;

    if (stat(dir, &statbuf) < 0 || (statbuf.st_mode & S_IFDIR) == 0)
	return;

    if ((dirp = opendir(dir)) == NULL)
	return;

    StateSetBusyWatch(True);

    while (readdir(dirp) != NULL)
	count++;
    rewinddir(dirp);

    list = (String *) XtCalloc(sizeof(String *), count + 1);

    strcpy(fileStr, dir == NULL ? arg->dirname : dir);
    filePtr = fileStr + strlen(fileStr);
    *filePtr++ = '/';
    if (arg->hidden) 
        XtVaGetValues(arg->hidden, XtNstate, &state, NULL);
    else
        state = True;
    while ((e = readdir(dirp)) != NULL) {
	struct stat statbuf;
	char *nm = e->d_name;

	if (nm[0] == '.') {
	    /*
	    **	Skip '.'
	     */
	    if (nm[1] == '\0')
		continue;
	    if (nm[1] == '.' && nm[2] == '\0') {
	        list[i++] = XtNewString(msgText[GO_UP_ONE_DIRECTORY_LEVEL]);
		continue;
	    }
            if (!state) continue;
	}
	strcpy(filePtr, nm);
	if (stat(fileStr, &statbuf) < 0) {
	    list[i++] = XtNewString(nm);
	    continue;
	}
	if ((statbuf.st_mode & S_IFDIR) != 0) {
	    list[i] = XtMalloc(sizeof(char) * strlen(nm) + 2);
	    strcpy(list[i], nm);
	    strcat(list[i], "/");
	    i++;
	    dirCount++;
	    continue;
	}
	/*
	**  Now only if it is a file.
	 */
#ifdef  __EMX__
	{
#else
	if ((statbuf.st_mode & (S_IFMT & ~S_IFLNK)) == 0) {
#endif
	    list[i++] = XtNewString(nm);
	    fileCount++;
	}
    }

    closedir(dirp);

    emptyList(arg);

    qsort(list, i, sizeof(String),
	  (int (*)(_Xconst void *, _Xconst void *)) strqsortcmp);

    if (dir != NULL) {
	strcpy(arg->dirname, dir);
	XtVaSetValues(arg->cwd_w, XtNlabel, dir, NULL);
    }

    sprintf(infoStr, "%d %s, %d %s", 
        dirCount, msgText[DIRECTORIES], fileCount, msgText[FILES]);

  fill:

    XawListChange(arg->list, list, i, 0, True);
    XtVaSetValues(arg->info, XtNlabel, infoStr, NULL);

    if (arg->browserType != BROWSER_LOADED || arg->isToRefresh) {
        XtVaSetValues(arg->name, XtNstring, "", NULL);
        if ((sb = XtNameToWidget(arg->vport, "vertical")) != None) {
	    float top = 0.0;
	    XawScrollbarSetThumb(sb, top, -1.0);
	    XtCallCallbacks(sb, XtNjumpProc, (XtPointer) & top);
	}
        StateSetBusyWatch(False);
    }
}

static void
Refresh(Widget w, XEvent * event, String * params, Cardinal * nparams)
{
    arg_t *arg;

    for (arg = argList; arg != NULL; arg = arg->next)
	if (w == arg->name) break;
    if (!arg) return;
    setCWD(arg, NULL);
    browserResized(arg->parent, arg, NULL, False); 
}

static XtActionsRec act[1] = {
        {"refresh",  (XtActionProc) Refresh}
};

static void 
okCallback(Widget w, XtPointer argArg, XtPointer junk)
{
    arg_t *arg = (arg_t *) argArg;
    String str;
    RWreadFunc f;
    struct stat statbuf;
    char *file;
    char *cp;
    char *nm;
    int zoom = Global.default_zoom, zoom_prev = Global.default_zoom;
    Boolean ongoing;
    static char buf[20];
    static int np = 0;

    ongoing = False;
    if (arg->ongoing)
        XtVaGetValues(arg->ongoing, XtNstate, &ongoing, NULL);

    XtVaGetValues(arg->name, XtNstring, &str, NULL);
    if (str == NULL || *str == '\0') {
	XawListReturnStruct *lr = XawListShowCurrent(arg->list);

	if (lr->list_index == XAW_LIST_NONE)
	    return;
	str = lr->string;
    }
    /*
    **	Got a valid string, check to see if it is a directory
    **	  if not try and read/write the file.
     */
    if ((file = doDirname(arg, str)) == NULL) {
	Notice(w, msgText[NO_SUCH_FILE_OR_DIRECTORY], str);
	return;
    }
    if (stat(file, &statbuf) >= 0 && (statbuf.st_mode & S_IFDIR) != 0) {
	setCWD(arg, file);
        browserResized(arg->parent, arg, NULL, False); 
	return;
    }
    if ((cp = strrchr(file, '/')) != NULL) {
	*cp = '\0';
	if (stat(file, &statbuf) >= 0 && (statbuf.st_mode & S_IFDIR) != 0 &&
            !ongoing) {
	    if (arg->browserType != BROWSER_LOADED || arg->isToRefresh) 
                setCWD(arg, file);
            if (arg->browserType != BROWSER_LOADED)
                browserResized(arg->parent, arg, NULL, False);
	}
	*cp = '/';
    }
    arg->isPopped = False;
    arg->isToRefresh = False;
    arg->oldwidth = 0;

    if (arg->browserType == BROWSER_LOADED) {
        char *dpistr, *pagestr, *zoomstr, *bboxstr;
        Widget paint = GetNonDirtyCanvas();
        XtVaGetValues(arg->dpi, XtNstring, &dpistr, NULL);
        XtVaGetValues(arg->page, XtNstring, &pagestr, NULL);
        XtVaGetValues(arg->zoom, XtNstring, &zoomstr, NULL);
        XtVaGetValues(arg->bbox, XtNstring, &bboxstr, NULL);
        Global.dpi = atof(dpistr);
        Global.numpage = atoi(pagestr);
        file_bbox = abs(atoi(bboxstr))%4;
        zoom = StrToZoom(zoomstr);
        if (zoom>32) zoom = 32;
        if (zoom<-9) zoom = -9;
        if (zoom==0) zoom = Global.default_zoom;
        if (Global.dpi<1.0) Global.dpi = 1.0;
        if (Global.numpage<=1) Global.numpage = 1;
        if (paint) {
	    XtVaSetValues(paint, XtNzoom, zoom, NULL);
	    loadPrescribedFile(paint, file);
            setZoomButtonLabel(paint, zoom);
            XtVaSetValues(arg->zoom, XtNstring, ZoomToStr(zoom), NULL);
            if (file_numpages != np) {
                sprintf(buf, "/ %d", file_numpages);
                XtVaSetValues(arg->pagetotal, XtNlabel, buf, NULL);
                np = file_numpages;
            }
            return;
	} else
            XtVaSetValues(arg->zoom, XtNstring, ZoomToStr(zoom), NULL);
    } else {
	if (ongoing) {
            Widget paint = GetNonDirtyCanvas();
            if (paint) {
	        if (file_specified_zoom)
		    zoom = file_specified_zoom;
                else
	            XtVaGetValues(paint, XtNzoom, &zoom, NULL);
                Global.default_zoom = zoom;
	        loadPrescribedFile(paint, file);
                AddFileToGlobalList(file);
                setZoomButtonLabel(paint, zoom);
                Global.default_zoom = zoom_prev;
                return;
	    }
        } else
            XtPopdown(GetShell(w));
    }

    if (arg->isSimple) {
        if (arg->okFunc != NULL)
	    arg->okFunc(arg->w, (XtPointer) arg->closure, (XtPointer) file);
	return;
    }

    if (arg->isRead)
	f = (RWreadFunc) RWtableGetReader(arg->type);
    else
	f = (RWreadFunc) RWtableGetWriter(arg->type);

    if (arg->type != NULL &&
	strcmp(nm = RWtableGetId(arg->type), MAGIC_READER) != 0)
        lastId = (void *) nm;
    else
	lastId = NULL;

    if (arg->okFunc != NULL) {
        zoom_prev = Global.default_zoom;
        Global.default_zoom = zoom;
        StateSetBusy(True);
	if (!arg->isRead) {
	    arg->okFunc(arg->w, file, (XtPointer) f);
            AddFileToGlobalList(file);
	} else {
	    Image *image = f(file);
	    if (file_specified_zoom)
	        Global.default_zoom = file_specified_zoom;
	    if (image == NULL)
		Notice(w, msgText[UNABLE_TO_OPEN_INPUT_FILE], file, 
                          RWGetMsg());
	    else
		arg->okFunc(arg->w, (XtPointer) file, (XtPointer) image);
            if (Global.numfiles)
                setCWD(arg, NULL);
            browserResized(arg->parent, arg, NULL, False);
	}
        StateSetBusy(False);
        Global.default_zoom = zoom_prev;
    }
}

static void 
pageCallback(Widget w, XtPointer argArg, XtPointer junk)
{
    arg_t *arg = (arg_t *) argArg;
    char *value;
    static char buf[20];
    int p, p0;

    if (!file_isSpecialImage) return;

    XtVaGetValues(arg->page, XtNstring, &value, NULL);
    p = p0 = atoi(value);
    if (w == arg->pageLeft) p -=10;
    if (w == arg->pageleft) p -=1;
    if (w == arg->pageright) p +=1;
    if (w == arg->pageRight) p +=10;
    if (p <= 0) p = 1;
    if (p > file_numpages) p = file_numpages;
    Global.numpage = p;
    sprintf(buf, "%d", p);
    XtVaSetValues(arg->page, XtNstring, buf, NULL);
    if (p==p0) return;
    file_force = 0;
    okCallback(w, argArg, junk);
}

static void
bboxCallback(Widget w, XtPointer argArg, XtPointer junk)
{
    arg_t *arg = (arg_t *) argArg;
    char name[2];
    if (strstr(XtName(w), "xright"))
        file_bbox = (file_bbox+1)%4;
    else
        file_bbox = (file_bbox+3)%4;
    name[0] = '0'+file_bbox;
    name[1] = '\0';
    XtVaSetValues(arg->bbox, XtNstring, name, NULL);
    okCallback(w, argArg, junk);
}

static void 
cancelCallback(Widget w, XtPointer argArg, XtPointer junk)
{
    arg_t *arg = (arg_t *) argArg;
    arg->isPopped = False;
    arg->oldwidth = 0;   
    XtPopdown(GetShell(w));
}

static void 
homeCallback(Widget w, XtPointer argArg, XtPointer itemArg)
{
    arg_t *arg = (arg_t *) argArg;
    char *home;
 
    home = getenv("HOME");
    if (!home) return;
    strcpy(arg->dirname, home);
    setCWD(arg, NULL);
    browserResized(arg->parent, arg, NULL, False); 
}

static void 
rootCallback(Widget w, XtPointer argArg, XtPointer itemArg)
{
    arg_t *arg = (arg_t *) argArg;
    strcpy(arg->dirname, "/");
    setCWD(arg, NULL);
    browserResized(arg->parent, arg, NULL, False); 
}

static void 
dotCallback(Widget w, XtPointer argArg, XtPointer itemArg)
{
    arg_t *arg = (arg_t *) argArg;
#ifdef NOSTDHDRS
    (void) getwd(arg->dirname);
#else
    (void) getcwd(arg->dirname, MAX_PATH);
#endif
    setCWD(arg, NULL);
    browserResized(arg->parent, arg, NULL, False); 
}

static void 
refreshCallback(Widget w, XtPointer argArg, XtPointer itemArg)
{
    arg_t *arg = (arg_t *) argArg;
    if (Global.numfiles || w==arg->hidden)
        setCWD(arg, NULL);
    browserResized(arg->parent, arg, NULL, False); 
}

static void 
fileDeleteOkCallback(Widget w, XtPointer argArg,  XtPointer infoArg)
{
    arg_t *arg = (arg_t *) argArg;
    char *name;
    int i, exists;
    struct stat buf;

    XtVaGetValues(arg->name, XtNstring, &name, NULL);
    if (!name || !*name) return;
    if (stat(name, &buf)) 
        Notice(w, msgText[FILE_DOES_NOT_EXIST], name);
    else {
        exists = 0;
        for (i=0; i<Global.numfiles; i++)
	    if (!strcmp(name, Global.loadedfiles[i])) {
	        exists = 1;
	        break;
	    }
        if (exists) {
            unlink(name);
            RemoveFileFromGlobalList(name);
	} else {
            Notice(w, msgText[FILE_NOT_IN_THE_LIST], name);
  	    return;
	}
    }
    if (Global.numfiles)
        setCWD(arg, NULL);
    browserResized(arg->parent, arg, NULL, False); 
}

static void 
exitDeleteCallback(Widget paint, XtPointer junk, XtPointer junk2)
{
}

static void 
fileDeleteCallback(Widget w, XtPointer argArg,  XtPointer junk2)
{
    arg_t *arg = (arg_t *) argArg;
    char *name = NULL;
    char title[300];

    XtVaGetValues(arg->name, XtNstring, &name, NULL);
    if (!name || !*name) return;
    sprintf(title, "%s : %s", msgText[REMOVE_FILE], name);

    AlertBox(w, title, fileDeleteOkCallback, exitDeleteCallback, arg);
}

static void 
editCallback(Widget bar, XtPointer argArg, XtPointer itemArg)
{
    arg_t *arg = (arg_t *) argArg;
    char *file;
    CScriptInfo * info;
    FILE *fd;
    char buf[2048];
    char *text;
    int l;

    if (!arg) return;
    XtVaGetValues(arg->name, XtNstring, &file, NULL);
    if (!file || !*file) return;
    info = (CScriptInfo *) ScriptEditor(Global.toplevel, None);
    fd = fopen(file, "r");
    if (!fd) {
        Notice(Global.toplevel, msgText[UNABLE_TO_OPEN_FILE], file);
        return;
    }
    info->scriptfile = file;
    XtVaSetValues(info->name, XtNlabel, file, NULL);
    text = xmalloc(2);
    *text = '\0';
    l = 0;
    while (fgets(buf, 2040, fd)) {
        l += strlen(buf);
        text = realloc(text, l+2);
        strcat(text, buf);
    }
    fclose(fd);
    XtVaSetValues(info->program, XtNstring, text, NULL);
}

static void 
createLXPCallback(Widget w, XtPointer argArg, XtPointer fileArg)
{
    arg_t *arg = (arg_t *) argArg;
    char *file = (char *) fileArg;
    char *script, *dir, *ptr;
    char buf[2048];

    XtVaGetValues(arg->name, XtNstring, &script, NULL);
    if (!script || !*script) return;
    dir = strdup(script);
    ptr = strrchr(dir, '/');

    sprintf(buf, "cd %s ; tar cvfz %s .\n", dir, file);
    free(dir);
    (void) system(buf);
    AddFileToGlobalList(file);
    setCWD(arg, NULL);
    browserResized(arg->parent, arg, NULL, False);
}

static void 
createCallback(Widget w, XtPointer argArg, XtPointer itemArg)
{
    arg_t *arg = (arg_t *) argArg;
    char *script, *ptr, *dir;

    if (!arg) return;
    XtVaGetValues(arg->name, XtNstring, &script, NULL);
    if (!script || !*script) return;
    dir = strdup(script);
    ptr = strrchr(dir, '/');
    if (!ptr) return;
    *ptr = '\0';
    ++ptr;
    if (strcmp(ptr, "image.c")) return;
    GetFileName(w, BROWSER_SIMPLESAVE, NULL, createLXPCallback, arg);
    free(dir);
}

static void 
listCallback(Widget bar, XtPointer argArg, XtPointer itemArg)
{
    XawListReturnStruct *item = (XawListReturnStruct *) itemArg;
    arg_t *arg = (arg_t *) argArg;
    String str, label;

    if (strncmp(item->string, "../", 3) == 0)
	label = "../";
    else
	label = item->string;

    XtVaGetValues(arg->name, XtNstring, &str, NULL);

    if (strcmp(str, label) == 0) {
        file_force = 1;
        if (!arg->okFunc)
            arg->okFunc = GraphicOpenFile;
	okCallback(bar, argArg, NULL);
    } else {
        if (arg->browserType == BROWSER_LOADED) {
            static char value[2];
            Global.numpage = 1;
            *value = '1'; value[1] = '\0';
            XtVaSetValues(arg->page, XtNstring, value, NULL);
	}
	XtVaSetValues(arg->name, XtNstring, label, NULL);
    }
}
   
static Widget
buildBrowser(Widget parent, char *title, arg_t * arg, 
             int width, int height)
{
    Widget form;
    Widget title_w, name, list, vport, cwd, info;

    PopdownMenusGlobal();

    arg->parent = parent;

    if (!trans) {
	trans = XtParseTranslationTable("#override\n\
						 <Key>Return: no-op()\n\
						 <Key>Linefeed: no-op()\n\
						 Ctrl<Key>L: refresh()\n\
						 Ctrl<Key>M: no-op()\n\
						 Ctrl<Key>J: no-op()\n");
	toglt = XtParseTranslationTable("<BtnDown>,<BtnUp>: set() notify()");
    }
#ifdef NOSTDHDRS
    (void) getwd(arg->dirname);
#else
    (void) getcwd(arg->dirname, sizeof(arg->dirname));
#endif

    form = XtVaCreateManagedWidget("browser",
				   formWidgetClass, parent,
				   XtNborderWidth, 0,
				   XtNright, XtChainRight,
				   XtNleft, XtChainLeft,
				   XtNtop, XtChainTop,
				   NULL);
    arg->form = form;

    title_w = XtVaCreateManagedWidget("title",
				      labelWidgetClass, form,
				      XtNborderWidth, 0,
				      XtNlabel, title,
				      XtNtop, XtChainTop,
				      XtNbottom, XtChainTop,
				      XtNjustify, XtJustifyLeft,
				      NULL);
    arg->title_w = title_w;

    name = XtVaCreateManagedWidget("name",
				   asciiTextWidgetClass, form,
				   XtNfromVert, title_w,
				   XtNeditType, XawtextEdit,
				   XtNwrap, XawtextWrapNever,
				   XtNresize, XawtextResizeWidth,
				   XtNtranslations, trans,
				   XtNbottom, XtChainTop,
				   XtNtop, XtChainTop,
				   XtNwidth, width,
				   NULL);
    arg->name = name;
    XtAppAddActions(XtWidgetToApplicationContext(arg->name), act, 1);

    vport = XtVaCreateManagedWidget("vport",
				    viewportWidgetClass, form,
				    XtNfromVert, name,
				    XtNtop, XtChainTop,
				    XtNbottom, XtChainBottom,
				    XtNallowVert, True,
				    XtNallowHoriz, True,
				    XtNuseBottom, True,
				    XtNuseRight, True,
				    XtNwidth, width,
				    XtNheight, height,
				    NULL);
    arg->vport = vport;

    list = XtVaCreateManagedWidget("files",
				   listWidgetClass, vport,
				   XtNborderWidth, 1,
				   XtNverticalList, True,
				   XtNforceBars, False,
				   XtNforceColumns, True,
				   XtNdefaultColumns, 1,
				   XtNnumberStrings, 0,
				   NULL);
    arg->list = list;

    if (arg->browserType != BROWSER_LOADED) {
        arg->home = XtVaCreateManagedWidget("home",
				  commandWidgetClass, form,
				  XtNlabel, "~",
                                  XtNfromVert, vport,
                                  XtNhorizDistance, 4,
                                  XtNvertDistance, 0,
                                  XtNwidth, 20,
                                  XtNheight, 20,
				  NULL);
        arg->root = XtVaCreateManagedWidget("root",
				  commandWidgetClass, form,
				  XtNlabel, "/",
                                  XtNfromVert, vport,
                                  XtNfromHoriz, arg->home,
                                  XtNhorizDistance, 3,
                                  XtNvertDistance, 0,
                                  XtNwidth, 20,
                                  XtNheight, 20,
				  NULL);
        arg->dot = XtVaCreateManagedWidget("dot",
				  commandWidgetClass, form,
				  XtNlabel, ".",
                                  XtNfromVert, vport,
                                  XtNfromHoriz, arg->root,
                                  XtNhorizDistance, 3,
                                  XtNvertDistance, 0,
                                  XtNwidth, 20,
                                  XtNheight, 20,
				  NULL);
        arg->hidden = XtVaCreateManagedWidget("hidden",
				  toggleWidgetClass, form,
                                  XtNfromVert, vport,
                                  XtNfromHoriz, arg->dot,
                                  XtNhorizDistance, 3,
                                  XtNvertDistance, 0,
                                  XtNheight, 20,
				  NULL);
        if (arg->browserType <= BROWSER_MULTIREAD)
            arg->ongoing = XtVaCreateManagedWidget("ongoing",
				  toggleWidgetClass, form,
                                  XtNfromVert, vport,
                                  XtNfromHoriz, arg->hidden,
                                  XtNhorizDistance, 3,
                                  XtNvertDistance, 0,
                                  XtNheight, 20,
				  XtNstate, (arg->browserType==BROWSER_MULTIREAD),
				  NULL);
        else
	    arg->ongoing = None;
    }

    cwd = XtVaCreateManagedWidget("cwd",
				  labelWidgetClass, form,
				  XtNlabel, arg->dirname,
				  XtNborderWidth, 0,
				  XtNfromVert, 
				     ((arg->home)?arg->home:vport),
				  XtNtop, XtChainBottom,
				  XtNbottom, XtChainBottom,
				  XtNjustify, XtJustifyLeft,
				  XtNresizable, True,
				  NULL);
    arg->cwd_w = cwd;
   
    info = XtVaCreateManagedWidget("info",
				   labelWidgetClass, form,
				   XtNborderWidth, 0,
				   XtNfromVert, cwd,
				   XtNtop, XtChainBottom,
				   XtNbottom, XtChainBottom,
				   XtNjustify, XtJustifyLeft,
				   NULL);
    arg->info = info;

    if (arg->home) {
        XtAddCallback(arg->home, XtNcallback, 
                      homeCallback, (XtPointer) arg);
        XtAddCallback(arg->root, XtNcallback, 
                      rootCallback, (XtPointer) arg);
        XtAddCallback(arg->dot, XtNcallback, 
                      dotCallback, (XtPointer) arg);
        XtAddCallback(arg->hidden, XtNcallback, 
                      refreshCallback, (XtPointer) arg);
    }
    XtAddCallback(list, XtNcallback, listCallback, (XtPointer) arg);
    XtAddEventHandler(list, ButtonPressMask, False,
		      (XtEventHandler) mousewheelScroll, (XtPointer) 1);

    arg->first = True;

    if (arg->browserType != BROWSER_LOADED) XtSetKeyboardFocus(form, arg->name);

    setCWD(arg, doDirname(arg, NULL));
    return form;
}

static Widget
buildOpenBrowser(Widget w, arg_t * arg, int width, int height, int mode)
{
    Widget shell, browser, okButton, cancelButton, form;
    Widget toggle, firstToggle = None;
    XtAccelerators accel;
    XtTranslations toglt;
    static char value[20];
    int i, j, k;
    char *name_format;
    char **list;
    Arg args[4];
    int nargs = 0;
    
    
    shell = XtVisCreatePopupShell("filebrowser",
				  ((mode==BROWSER_LOADED)?
				   topLevelShellWidgetClass:
                                   transientShellWidgetClass),
                                  GetToplevel(w),
				  args, nargs);
    arg->shell = shell;
    form = XtVaCreateManagedWidget("form",
				   formWidgetClass, shell,
				   XtNborderWidth, 0,
				   NULL);
    browser = buildBrowser(form, msgText[OPEN_FILE_NAMED], arg, width, height);
    arg->browser = browser;

    accel = XtParseAcceleratorTable("#override\n\
					 <Key>Return: set() notify() unset()\n\
					 <Key>Linefeed: set() notify() unset()");

    okButton = XtVaCreateManagedWidget("ok",
				       commandWidgetClass, form,
				       XtNfromHoriz, browser,
				       XtNaccelerators, accel,
				       XtNbottom, XtChainTop,
				       XtNleft, XtChainRight,
				       XtNright, XtChainRight,
				       NULL);
    arg->ok = okButton;
    cancelButton = XtVaCreateManagedWidget("cancel",
					   commandWidgetClass, form,
					   XtNfromHoriz, okButton,
					   XtNbottom, XtChainTop,
					   XtNleft, XtChainRight,
					   XtNright, XtChainRight,
					   NULL);
    arg->cancel = cancelButton;

    toggle = okButton;
    toglt = XtParseTranslationTable("<BtnDown>,<BtnUp>: set() notify()");

    list = RWtableGetReaderList();

    j = 0;
    k = 0;
    for (i = 0; list[i] != NULL; i++) {
        while (strncmp(list[i], msgText[IMAGE_FORMATS+j], strlen(list[i])) &&
	       j<NUM_FORMATS)
	    ++j;
        if (j == NUM_FORMATS) {
 	    fprintf(stderr, "Logic error: file-format string \"%s\" not found "
 	      "in msgText\n", list[i]);
 	    name_format = "[unknown]";
 	} else {
 	    name_format = index(msgText[IMAGE_FORMATS+j], ' ');
 	    if (name_format)
 	       ++name_format;
 	    else
 	       name_format = list[i];
  	}
	if (j<NUM_FORMATS) ++j;

	toggle = XtVaCreateManagedWidget(list[i],
					 toggleWidgetClass, form,
					 XtNtranslations, toglt,
					 XtNradioGroup, firstToggle,
					 XtNfromVert, toggle,
					 XtNfromHoriz, browser,
					 XtNlabel, name_format,
					 XtNtop, XtChainBottom,
					 XtNbottom, XtChainBottom,
					 XtNleft, XtChainRight,
					 XtNright, XtChainRight,
					 NULL);
        arg->format[k] = toggle;
        k++;

	if (firstToggle == None) {
	    arg->type = NULL;
	    XtVaSetValues(toggle, XtNstate, True,
			  XtNvertDistance, 29, NULL);
	    firstToggle = toggle;
	}
	XtAddCallback(toggle, XtNcallback, fileTypeCallback, (XtPointer) arg);
    }

    arg->isSimple = False;
    arg->isRead = True;
    arg->numformat = k;

    if (arg->browserType == BROWSER_LOADED) {
        arg->refresh = XtVaCreateManagedWidget("refresh",
				       commandWidgetClass, browser,
				       XtNborderWidth, 0,
                                       XtNfromVert, arg->info,
				       XtNvertDistance, 10,
				       XtNhorizDistance, 10,
				       NULL);

        arg->delete = XtVaCreateManagedWidget("delete",
				       commandWidgetClass, browser,
				       XtNborderWidth, 0,
                                       XtNfromVert, arg->info,
                                       XtNfromHoriz, arg->refresh,
				       XtNvertDistance, 10,
				       XtNhorizDistance, 10,
				       NULL);

        arg->edit = XtVaCreateManagedWidget("edit",
				       commandWidgetClass, browser,
				       XtNborderWidth, 0,
                                       XtNfromHoriz, arg->delete,
                                       XtNfromVert, arg->info,
				       XtNvertDistance, 10,
				       XtNhorizDistance, 10,
				       NULL);

        arg->create = XtVaCreateManagedWidget("create",
				       commandWidgetClass, browser,
				       XtNborderWidth, 0,
                                       XtNfromHoriz, arg->edit,
                                       XtNfromVert, arg->info,
				       XtNvertDistance, 10,
				       XtNhorizDistance, 10,
				       NULL);

        arg->zoomlabel = XtVaCreateManagedWidget("zoom",
				       labelWidgetClass, browser,
				       XtNborderWidth, 0,
                                       XtNfromVert, arg->refresh,
				       XtNvertDistance, 10,
				       XtNhorizDistance, 10,
				       NULL);

        arg->zoom = XtVaCreateManagedWidget("zoomW",
				       asciiTextWidgetClass, browser,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNresize, XawtextResizeWidth,
				       XtNtranslations, trans,
                                       XtNfromVert, arg->refresh,
				       XtNvertDistance, 10,
                                       XtNfromHoriz, arg->zoomlabel,
				       XtNhorizDistance, 10,
				       XtNwidth, 50,
				       XtNheight, 20,
				       NULL);
        XtVaSetValues(arg->zoom, XtNstring, ZoomToStr(Global.default_zoom),
                      NULL);

        arg->dpilabel = XtVaCreateManagedWidget("dpi",
				       labelWidgetClass, browser,
				       XtNborderWidth, 0,
                                       XtNfromVert, arg->refresh,
                                       XtNfromHoriz, arg->zoom,
				       XtNvertDistance, 10,
				       XtNhorizDistance, 10,
				       NULL);

        arg->dpi = XtVaCreateManagedWidget("dpiW",
				       asciiTextWidgetClass, browser,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNresize, XawtextResizeWidth,
				       XtNtranslations, trans,
                                       XtNfromVert, arg->refresh,
				       XtNvertDistance, 10,
                                       XtNfromHoriz, arg->dpilabel,
				       XtNhorizDistance, 10,
				       XtNwidth, 50,
				       XtNheight, 20,
				       NULL);
        sprintf(value, "%g", Global.dpi);
        XtVaSetValues(arg->dpi, XtNstring, value, NULL);

        arg->bboxlabel = XtVaCreateManagedWidget("BBox",
				       labelWidgetClass, browser,
				       XtNborderWidth, 0,
                                       XtNheight, 20,
                                       XtNfromVert, arg->refresh,
				       XtNvertDistance, 7,
                                       XtNfromHoriz, arg->dpi,
				       XtNhorizDistance, 10,
				       NULL);

        arg->bboxleft = XtVaCreateManagedWidget("xxleft",
				       commandWidgetClass, browser,
                                       XtNfromVert, arg->refresh,
				       XtNvertDistance, 7,
				       XtNfromHoriz, arg->bboxlabel,
				       XtNhorizDistance, 3,
				       XtNheight, 20,
				       XtNwidth, 18,
				       NULL);

        arg->bbox = XtVaCreateManagedWidget("bbox",
				       asciiTextWidgetClass, browser,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNresize, XawtextResizeWidth,
				       XtNtranslations, trans,
                                       XtNfromVert, arg->refresh,
				       XtNvertDistance, 7,
                                       XtNfromHoriz, arg->bboxleft,
				       XtNhorizDistance, 3,
				       XtNwidth, 18,
				       NULL);
        sprintf(value, "%d", file_bbox);
        XtVaSetValues(arg->bbox, XtNstring, value, NULL);

        arg->bboxright = XtVaCreateManagedWidget("xxright",
				       commandWidgetClass, browser,
                                       XtNfromVert, arg->refresh,
				       XtNvertDistance, 7,
				       XtNfromHoriz, arg->bbox,
				       XtNhorizDistance, 3,
				       XtNheight, 20,
				       XtNwidth, 18,
				       NULL);

        arg->pagelabel = XtVaCreateManagedWidget("page",
				       labelWidgetClass, browser,
				       XtNborderWidth, 0,
                                       XtNfromVert, arg->dpi,
				       XtNvertDistance, 7,
				       XtNhorizDistance, 10,
				       NULL);

        sprintf(value, "/%d", file_numpages);
        arg->pagetotal = XtVaCreateManagedWidget(value,
				       labelWidgetClass, browser,
				       XtNborderWidth, 0,
				       XtNwidth, 31,
                                       XtNfromVert, arg->dpi,
				       XtNvertDistance, 7,
				       XtNfromHoriz, arg->pagelabel,
				       XtNhorizDistance, 0,
				       NULL);

        arg->pageLeft = XtVaCreateManagedWidget("xxleft",
				       commandWidgetClass, browser,
                                       XtNfromVert, arg->dpi,
				       XtNvertDistance, 7,
				       XtNfromHoriz, arg->pagetotal,
				       XtNhorizDistance, 10,
				       XtNheight, 20,
				       XtNwidth, 18,
				       NULL);

        arg->pageleft = XtVaCreateManagedWidget("xleft",
				       commandWidgetClass, browser,
                                       XtNfromVert, arg->dpi,
				       XtNvertDistance, 7,
				       XtNfromHoriz, arg->pageLeft,
				       XtNhorizDistance, 3,
				       XtNheight, 20,
				       XtNwidth, 18,
				       NULL);

        arg->page = XtVaCreateManagedWidget("pageW",
				       asciiTextWidgetClass, browser,
 				       XtNeditType, XawtextEdit,
				       XtNwrap, XawtextWrapNever,
				       XtNresize, XawtextResizeWidth,
				       XtNtranslations, trans,
                                       XtNfromVert, arg->dpi,
				       XtNvertDistance, 7,
                                       XtNfromHoriz, arg->pageleft,
				       XtNhorizDistance, 3,
				       XtNwidth, 48,
				       NULL);

        arg->pageright = XtVaCreateManagedWidget("xright",
				       commandWidgetClass, browser,
                                       XtNfromVert, arg->dpi,
				       XtNvertDistance, 7,
				       XtNfromHoriz, arg->page,
				       XtNhorizDistance, 3,
				       XtNheight, 20,
				       XtNwidth, 18,
				       NULL);

        arg->pageRight = XtVaCreateManagedWidget("xxright",
				       commandWidgetClass, browser,
                                       XtNfromVert, arg->dpi,
				       XtNvertDistance, 7,
				       XtNfromHoriz, arg->pageright,
				       XtNhorizDistance, 3,
				       XtNheight, 20,
				       XtNwidth, 18,
				       NULL);

        sprintf(value, "%d", Global.numpage);
        XtVaSetValues(arg->page, XtNstring, value, NULL);

        XtInstallAccelerators(arg->page, okButton);
        XtInstallAccelerators(arg->dpi, okButton);
        XtInstallAccelerators(arg->zoom, okButton);
        XtInstallAccelerators(arg->bbox, okButton);

        XtAddCallback(arg->pageLeft, XtNcallback,
                      pageCallback, (XtPointer) arg);
        XtAddCallback(arg->pageleft, XtNcallback,
                      pageCallback, (XtPointer) arg);
        XtAddCallback(arg->pageright, XtNcallback,
                      pageCallback, (XtPointer) arg);
        XtAddCallback(arg->pageRight, XtNcallback,
                      pageCallback, (XtPointer) arg);
        XtAddCallback(arg->bboxleft, XtNcallback, 
                      bboxCallback, (XtPointer) arg);
        XtAddCallback(arg->bboxright, XtNcallback, 
                      bboxCallback, (XtPointer) arg);
        XtAddCallback(arg->refresh, XtNcallback,
                      refreshCallback, (XtPointer) arg);
        XtAddCallback(arg->delete, XtNcallback,
                      fileDeleteCallback, (XtPointer) arg);
        XtAddCallback(arg->edit, XtNcallback,
                      editCallback, (XtPointer) arg);
        XtAddCallback(arg->create, XtNcallback,
                      createCallback, (XtPointer) arg);
    }

    XtAddCallback(okButton, XtNcallback, okCallback, (XtPointer) arg);
    XtAddCallback(cancelButton, XtNcallback, cancelCallback, (XtPointer) arg);
    AddDestroyCallback(shell,
		       (DestroyCallbackFunc) cancelCallback, (XtPointer) arg);

    if (arg->browserType != BROWSER_LOADED)
        XtSetKeyboardFocus(form, arg->name);
    XtInstallAccelerators(arg->name, okButton);

    return shell;
}

static Widget
buildSaveBrowser(Widget w, arg_t * arg)
{
    Widget shell, browser, okButton, cancelButton, form;
    Widget toggle, firstToggle = None;
    XtAccelerators accel;

    XtTranslations toglt;
    int i, j, k;
    char *name_format;
    char **list;
    /* char *rdr = NULL; */
    Arg args[4];
    int nargs=0;
    
    shell = XtVisCreatePopupShell("filebrowser",
			          transientShellWidgetClass, GetToplevel(w),
				  args, nargs);
    arg->shell = shell;
    form = XtVaCreateManagedWidget("form",
				   formWidgetClass, shell,
				   XtNborderWidth, 0,
				   NULL);
    browser = buildBrowser(form, msgText[SAVE_IN_FILE], arg, 250, 380);
    arg->browser = browser;
   
    accel = XtParseAcceleratorTable("#override\n\
					 <Key>Return: set() notify() unset()\n\
					 <Key>Linefeed: set() notify() unset()");

    okButton = XtVaCreateManagedWidget("ok",
				       commandWidgetClass, form,
				       XtNfromHoriz, browser,
				       XtNaccelerators, accel,
				       XtNbottom, XtChainTop,
				       NULL);
    arg->ok = okButton;      
    cancelButton = XtVaCreateManagedWidget("cancel",
					   commandWidgetClass, form,
					   XtNfromHoriz, okButton,
					   XtNbottom, XtChainTop,
					   NULL);
    arg->cancel = cancelButton;

    toggle = okButton;
    toglt = XtParseTranslationTable("<BtnDown>,<BtnUp>: set() notify()");
    list = RWtableGetWriterList();

    /*
    if (RWtableGetWriter(GraphicGetReaderId(w)) != NULL) {
	rdr = (char *) GraphicGetReaderId(w);
    }
    */

    j = 0;
    k = 0;
    for (i = 0; list[i] != NULL; i++) {
        while (strncmp(list[i], msgText[IMAGE_FORMATS+j], strlen(list[i]))
	       && j<NUM_FORMATS) ++j;
        if (j == NUM_FORMATS) {
	    fprintf(stderr, 
              "Logic error: file-format string \"%s\" not found in msgText\n", 
	      list[i]);
	    name_format = "[unknown]";
	} else { 
	    name_format = list[i];
	}
	name_format = index(msgText[IMAGE_FORMATS+j], ' ');
	if (name_format) 
	    ++name_format;
	else
	    name_format = list[i];
	if (j<NUM_FORMATS) ++j;

	toggle = XtVaCreateManagedWidget(list[i],
					 toggleWidgetClass, form,
					 XtNlabel, name_format,
					 XtNtranslations, toglt,
					 XtNradioGroup, firstToggle,
					 XtNfromVert, toggle,
					 XtNfromHoriz, browser,
					 XtNtop, XtChainBottom,
					 XtNbottom, XtChainBottom,
					 NULL);
        arg->format[k] = toggle;
        k++;

	if (firstToggle == None) {
	    arg->type = NULL;
	    XtVaSetValues(toggle, XtNvertDistance, 29, NULL);
	    firstToggle = toggle;
	    /*
	    if (rdr == NULL)
	    */
	        XtVaSetValues(toggle, XtNstate, True, NULL);
	}

	/*
	if (rdr != NULL && strcmp(rdr, list[i]) == 0) {
	    arg->type = RWtableGetEntry(list[i]);
	    XtVaSetValues(toggle, XtNstate, True, NULL);
	}
	*/
	XtAddCallback(toggle, XtNcallback, fileTypeCallback, (XtPointer) arg);
    }

    arg->isSimple = False;
    arg->isRead = False;
    arg->numformat = k;

    XtAddCallback(okButton, XtNcallback, okCallback, (XtPointer) arg);
    XtAddCallback(cancelButton, XtNcallback, cancelCallback, (XtPointer) arg);
    AddDestroyCallback(shell,
		       (DestroyCallbackFunc) cancelCallback, (XtPointer) arg);

    XtSetKeyboardFocus(form, arg->name);
    XtInstallAccelerators(arg->name, okButton);

    return shell;
}

static Widget
buildSimpleBrowser(Widget w, arg_t * arg, Boolean isSave)
{
    Widget shell, browser, okButton, cancelButton, form;
    XtAccelerators accel;
    Arg args[4];
    int nargs = 0;

    shell = XtVisCreatePopupShell("filebrowser",
				  transientShellWidgetClass, GetToplevel(w),
				  args, nargs);
    arg->shell = shell;
    form = XtVaCreateManagedWidget("form",
				   formWidgetClass, shell,
				   XtNborderWidth, 0,
				   NULL);
    browser = buildBrowser(form, isSave ? 
			   msgText[SAVE_IN_FILE] : msgText[LOAD_FROM_FILE],
			   arg, 250, 380);
    arg->browser = browser;   

    accel = XtParseAcceleratorTable("#override\n\
					 <Key>Return: set() notify() unset()\n\
					 <Key>Linefeed: set() notify() unset()");

    okButton = XtVaCreateManagedWidget("ok",
				       commandWidgetClass, form,
				       XtNfromHoriz, browser,
				       XtNaccelerators, accel,
				       XtNbottom, XtChainTop,
				       NULL);
    arg->ok = okButton;
    cancelButton = XtVaCreateManagedWidget("cancel",
					   commandWidgetClass, form,
					   XtNfromHoriz, okButton,
					   XtNbottom, XtChainTop,
					   NULL);
    arg->cancel = cancelButton;   

    arg->isSimple = True;
    arg->numformat = 0;

    XtAddCallback(okButton, XtNcallback, okCallback, (XtPointer) arg);
    XtAddCallback(cancelButton, XtNcallback, cancelCallback, (XtPointer) arg);
    AddDestroyCallback(shell,
		       (DestroyCallbackFunc) cancelCallback, (XtPointer) arg);

    XtSetKeyboardFocus(form, arg->name);
    XtInstallAccelerators(arg->name, okButton);

    return shell;
}

/*
**  
**
 */

static void 
freeArg(Widget w, arg_t * arg)
{
    arg_t *c = argList, **pp = &argList;
    
    while (c != arg && c != NULL) {
	pp = &c->next;
	c = c->next;
    }

    *pp = arg->next;

    XtDestroyWidget(GetShell(arg->name));
    XtFree((XtPointer) arg);
}

void *
getArgType(Widget w)
{
    arg_t *cur;

    w = GetShell(w);

    for (cur = argList; cur != NULL; cur = cur->next)
	if (cur->parent == w &&
	    (cur->browserType >= BROWSER_READ &&
	     cur->browserType <= BROWSER_SAVE))
	    break;

    if (cur == NULL)
	return NULL;
    return cur->type;
}


static arg_t *
getArg(Widget w, Boolean type, Boolean * built)
{
    Widget shell, p = GetShell(w);
    arg_t *cur;

    *built = False;

    for (cur = argList; cur != NULL; cur = cur->next)
	if (p == cur->parent && cur->browserType == type)
	    return cur;

    cur = XtNew(arg_t);
    memset(cur, 0, sizeof(arg_t));
    cur->parent = p;
    cur->browserType = type;

    switch (type) {
    case BROWSER_READ:
        shell = buildOpenBrowser(w, cur, 250, 380, BROWSER_READ);
	break;
    case BROWSER_MULTIREAD:
        shell = buildOpenBrowser(w, cur, 250, 380, BROWSER_MULTIREAD);
        break;
    case BROWSER_LOADED:
        shell = buildOpenBrowser(w, cur, 340, 300, BROWSER_LOADED);
        break;
    case BROWSER_SAVE:
        shell = buildSaveBrowser(w, cur);
	break;
    case BROWSER_SIMPLEREAD:
	shell = buildSimpleBrowser(w, cur, False);
	break;
    case BROWSER_SIMPLESAVE:
	shell = buildSimpleBrowser(w, cur, True);
	break;
    }

    cur->next = argList;	/* Add cur to front of list */
    argList = cur;

    XtAddCallback(p, XtNdestroyCallback,
		  (XtCallbackProc) freeArg, (XtPointer) cur);

    *built = True;

    return cur;
}

void 
GetFileName(Widget w, int type, char *def, XtCallbackProc okFunc, XtPointer data)
{
    arg_t *arg;
    Boolean built;
    Position x, y;
    Widget shell, *widget;
    int i, j, im;

    if (type == BROWSER_LOADED) {
        /* if (!Global.numfiles) return; */
        /* Check whether "loaded files" browser is already popped up */
        for (arg = argList; arg != NULL; arg = arg->next)
	    if (arg->browserType == BROWSER_LOADED) {
                XWindowAttributes win_attributes;
                XGetWindowAttributes(XtDisplay(arg->bbox), 
                                     XtWindow(arg->bbox), &win_attributes);
                if (win_attributes.map_state==IsViewable) return;
	    }
        i = 416;
        j = 439;
    } else {
        i = 366;
        j = 500;
    }
    XtVaGetValues(GetShell(w), XtNx, &x, XtNy, &y, NULL);
    x += 24; y += 24;
    if (x<24) x=24;
    if (x>WidthOfScreen(XtScreen(w))-i)
       x=WidthOfScreen(XtScreen(w))-i;
    if (y<24) y=24;
    if (y>HeightOfScreen(XtScreen(w))-j)
       y=HeightOfScreen(XtScreen(w))-j;

    arg = getArg(w, type, &built);
    arg->closure = data;

    XtVaSetValues(GetShell(arg->name), XtNx, x, XtNy, y, NULL);

    if (def != NULL && *def != '\0') {
	char *cp, dirname[MAX_PATH];
	strcpy(dirname, def);
	cp = strrchr(dirname, '/');

	if (cp != NULL && cp != dirname)
	    *cp++ = '\0';
	else if (cp == NULL)
	    cp = dirname;

        /* This browser has not been used before, init it. */
	if (built | Global.explore)	
	    setCWD(arg, doDirname(arg, dirname));

	XtVaSetValues(arg->name, XtNstring, cp,
		      XtNinsertPosition, strlen(cp),
		      NULL);
    } else if (!built) {
	/*
	**  This browser has been used before, rescan directory.
	 */
	setCWD(arg, NULL);
    }
    arg->okFunc = okFunc;
    arg->w = w;

    arg->isPopped = True;
    arg->oldwidth = 0;   
    shell = GetShell(arg->name);

    XtPopup(shell, XtGrabNone);
    XtVaGetValues(shell, XtNtitle, &def, NULL);
    StoreName(shell, def);

    if (arg->bbox)
        im = &arg->pageRight - &arg->browser;
    else
        im = &arg->cancel - &arg->browser;

    widget = &arg->browser;
    for (i=im; i>=0; i--) if (widget[i])
        XtUnmanageChild(widget[i]);
    for (i=0; i<arg->numformat; i++) if (arg->format[i])
        XtUnmanageChild(arg->format[i]);
    for (i=0; i<=im; i++) if (widget[i])
        XMapWindow(XtDisplay(arg->parent), XtWindow(widget[i]));
    for (i=0; i<arg->numformat; i++) if (arg->format[i])
        XMapWindow(XtDisplay(arg->format[i]), XtWindow(arg->format[i]));

    XtAddEventHandler(XtParent(arg->parent), StructureNotifyMask, False,
		      (XtEventHandler) browserResized, (XtPointer) arg);
    XFlush(XtDisplay(arg->browser));
    XSetInputFocus(XtDisplay(arg->browser), XtWindow(GetToplevel(arg->browser)),
                   RevertToParent, CurrentTime);
}
