/* +-------------------------------------------------------------------+ */
/* | Copyright 1993, Jean-Pierre Demailly			       | */
/* | <demailly@fourier.ujf-grenoble.fr>				       | */
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

/* $Id: readWriteLXP.c,v 1.14 2005/03/20 20:15:34 demailly Exp $ */

#define MIN(a,b)       (((a) < (b)) ? (a) : (b))

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include "xpaint.h"
#include "image.h"
#include "rwTable.h"
#include "libpnmrw.h"

#if defined(sco) || defined(__CYGWIN__)
#include <time.h>
#else
#include <time.h>
#include <sys/time.h>
#endif

#include <string.h>
#include <X11/Intrinsic.h>

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

extern void * xmalloc(size_t n);
extern FILE * openTemp(char **np);
extern void removeTemp(void);
extern Image * ReadScriptC(char *file);
extern int WritePNGn(char *file, Image *image);
extern int writeMagic(char *file, Image *image);
extern void AddFileToGlobalList(char * file);

static char tmpdir[256];
static int width0=0, height0=0;

static char *Basename(char *file)
{
    char *ptr;
    if (!file) return NULL;
    ptr = strrchr(file, '/');
    if (!ptr) return file;
    return ptr+1;
}

char * ArchiveFile(char * file)
{
    static char path[256];
    struct stat buf;
    Image *image;
    Pixmap pix;
    Colormap cmap;
    Visual *visual;
    Screen *screen;
    GC gc;

    sprintf(path, "%s/%s", tmpdir, file);
    if (stat(path, &buf)!=0) {
        if (width0==0 || height0==0) {
	    width0 = Global.default_width;
	    height0 = Global.default_height;
	}
        screen = XtScreen(Global.toplevel);
	visual = DefaultVisualOfScreen(screen);
	cmap = XCreateColormap(XtDisplay(Global.toplevel), 
		    RootWindowOfScreen(screen), visual, AllocNone);
        pix = XCreatePixmap(Global.display, XtWindow(Global.toplevel), 
                            width0, height0, 
                            DefaultDepthOfScreen(screen));
        gc = XCreateGC(Global.display, pix, 0, 0);
        XSetForeground(Global.display, gc, WhitePixelOfScreen(screen));
        XFillRectangle(Global.display, pix, gc, 0, 0, width0, height0);
        image = PixmapToImage(Global.toplevel, pix, cmap);
        writeMagic(path, image);
        ImageDelete(image);
        XFreePixmap(Global.display, pix);
        XFreeColormap(Global.display, cmap);
        XFreeGC(Global.display, gc);
    }
    AddFileToGlobalList(path);
    return path;
}

void * LoadLayers(char ** files)
{
    static Image **image = NULL;
    int i;
    width0 = 0;
    height0 = 0;
    i = 0;
    while (files[i]) {
        image = (Image **)realloc(image, (i+1)*sizeof(Image *));
        image[i] = ImageFromFile(ArchiveFile(files[i]));
        if (width0==0 && image[i]) {
	    width0 = image[i]->width;
	    height0 = image[i]->height;
	}
        ++i;
    }
    return (void *)image;
}

/* Test LXP format */
/* LXP is just a plain X.tar.gz archive containing a single directory ./
 * and a C file image.c which may combine any number of other files
 * (images, other .c files of .h headers)
 */
int
TestLXP(char *file)
{
    char header[12];
    FILE *fp = fopen(file, "rb");

    if (!fp)
        return 0;

    fread(header, 1, 8, fp);
    fclose(fp);
    if (!strncmp(header, "\037\213", 2))
        return 1;
    else
        return 0;
}

/* read LXP format */
Image *
ReadLXP(char *file)
{
    Image *image;
    char buf[2048];
    char *home;

    if (!TestLXP(file)) return NULL;

    home = getenv("HOME");
    if (!home) return NULL;
    
    sprintf(tmpdir, "%s/.xpaint/tmp/%s_files", home, Basename(file));
    sprintf(buf, "mkdir -p %s ; ln -s -f %s %s ; cd %s ; tar xvfz %s",
            tmpdir, file, tmpdir, tmpdir, file);
    system(buf);
    sprintf(buf, "%s/image.c", tmpdir);
    AddFileToGlobalList(buf);

    image = ReadScriptC(buf);
    return image;
}

int WriteLXP(char *file, Image * image)
{
    char buf[8192];
    char *home;

    home = getenv("HOME");
    if (!home) return 0;
    
    sprintf(tmpdir, "%s/.xpaint/tmp/%s_dir", home, Basename(file));
    sprintf(buf, "mkdir -p \"%s\"", tmpdir);
    system(buf); 
    sprintf(buf, "%s/file.png", tmpdir);
    WritePNGn(buf, image);
    sprintf(buf, "cp -p -f %s/XPaintIcon.xpm \"%s/XPaintIcon.xpm\"", 
            SHAREDIR, tmpdir);
    system(buf);
    sprintf(buf, "cp -p -f %s/c_scripts/templates/image.c \"%s/image.c\" ; ( cd \"%s\" ; tar cvfz ../\"%s\" . ) ; mv -f \"%s/../%s\" \"%s\" ; rm -rf \"%s\"", 
            SHAREDIR, tmpdir, tmpdir, Basename(file), tmpdir, 
            Basename(file), file, tmpdir);
    system(buf);
    return 0;
}
