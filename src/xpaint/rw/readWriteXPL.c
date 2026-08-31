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
#include <sys/stat.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include "xpaint.h"
#include "image.h"
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

static char tmpdir[256];

char * ArchiveFile(char * file)
{
    static char buf[256];
    sprintf(buf, "%s/%s", tmpdir, file);
    AddFileToGlobalList(buf);
    return buf;
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
    FILE *fp;

    if (!TestLXP(file)) return NULL;

    home = getenv("HOME");
    if (!home) return NULL;
    
    sprintf(tmpdir, "%s/.xpaint/tmp/%s_files", home, basename(file));
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
    return 0;
}
