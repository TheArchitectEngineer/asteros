/* +-------------------------------------------------------------------+ */
/* | Copyright 1990, 1991, 1993 David Koblas.			       | */
/* | Copyright 1996 Torsten Martinsen.				       | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.	       | */
/* +-------------------------------------------------------------------+ */

/* $Id: readScriptC.c,v 1.8 2009/09/16 14:15:34 demailly Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <dlfcn.h>
#include "image.h"
#include "rwTable.h"
#include <sys/types.h>
#include <sys/stat.h>

extern int file_isSpecialImage;
extern int file_transparent;

extern char * GetShareDir();
extern void *dl_filter;
extern void *dl_proc;

int
TestScriptC(char *file)
{
    FILE *fd = fopen(file, "r");
    char buf[25];
    int ret = 0;

    if (fd == NULL)
	return 0;

    if (fread(buf, 23, 1, fd) != 0) {
      if (!strncasecmp(buf, "/* Xpaint-image */", 18)) ret = 1;
      if (!strncasecmp(buf, "/* Xpaint-filter */", 19)) ret = 2;
      if (!strncasecmp(buf, "/* Xpaint-procedure */", 22)) ret = 3;
    }
    fclose(fd);

    return ret;
}

/* Read, compile, link and execute a C-script to produce an image */

Image *
ReadScriptC(char *file)
{
    Image * image;
    char cmd[512];
    char radix[256];
    char *ptr;
    int ret;
    void *dl_handle;
    Image * (* proc)();
    struct stat buf;

    if (!file || !*file) return NULL;

    ret  = TestScriptC(file);
    ptr = strrchr(file, '/');
    if (ptr) ++ptr; else ptr = file;
    strncpy(radix, ptr, 255);
    radix[255] = '\0';
    ptr = strrchr(radix, '.');
    if (ptr) *ptr = '\0';

    /* compile C script */
    sprintf(cmd, "gcc -fPIC -I%s/include -I/usr/include/X11 "
                 "-c %s -o /tmp/%s.o ; "
                 "gcc -fpic -shared -Wl,-soname,%s.so /tmp/%s.o -o /tmp/%s.so\n",
	    	 GetShareDir(),
                 file, radix, radix, radix, radix);
    system(cmd);

    sprintf(cmd, "/tmp/%s.o", radix);
    unlink(cmd); 
    sprintf(cmd, "/tmp/%s.so", radix);

    if (stat(cmd, &buf)==-1) {
    error:
       fprintf(stderr, "Compilation of C-script failed !!\n");
       return NULL;
    }

    switch(ret) {
    case 1:
       dl_handle = dlopen(cmd, RTLD_LAZY);
       if (!dl_handle) goto error;
       unlink(cmd);
       proc = dlsym(dl_handle, "ImageCreate");
       if (proc) {
          fprintf(stderr, "Executing C-script ImageCreate() ... \n");
          image = proc();
          if (image) {
             if (image->alpha) {
	        file_isSpecialImage = 1;
	        file_transparent = 1;
	     }
	  } else
             fprintf(stderr, "C-script procedure created invalid image !!\n");
       } else {
          image = NULL;
          fprintf(stderr, 
             "C-script didn't include valid ImageCreate() procedure !!\n");
       }
       dlclose(dl_handle);
       return image;
    case 2:
       if (dl_filter) dlclose(dl_filter);
       dl_filter = dlopen(cmd, RTLD_LAZY);
       if (!dl_filter) goto error;
       unlink(cmd);
       return NULL;
    case 3:
       if (dl_proc) dlclose(dl_proc);
       dl_proc = dlopen(cmd, RTLD_LAZY);
       if (!dl_proc) goto error;
       unlink(cmd);
       proc = dlsym(dl_proc, "PaintProcedure");
       if (!proc)
          fprintf(stderr, 
            "C-script didn't include valid PaintProcedure() procedure !!\n");
       else {
          fprintf(stderr, 
            "Executing C-script PaintProcedure() ...\n");
	  proc();
       }
    default:
       return NULL;
    }
}
