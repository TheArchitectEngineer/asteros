/* +-------------------------------------------------------------------+ */
/* | Copyright 1993, David Koblas (koblas@netcom.com)		       | */
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

/* $Id: rwTable.c,v 1.22 2005/03/20 20:15:34 demailly Exp $ */

#if defined(HAVE_PARAM_H)
#include <sys/param.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include "image.h"
#include "rwTable.h"
#include <string.h>
#include <errno.h>

#ifdef MISSING_STDARG_H
#include <varargs.h>
#else
#include <stdarg.h>
#endif

typedef char *String;
#include "../messages.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

typedef struct {
    char *name;
    RWreadFunc read;
    RWwriteFunc write;
    RWtestFunc test;
} ImageTypes;

static char RWtableMsg[512];

/*
**  Define all the read/write functions here.
**    [ unfortunately most compilers won't take
**	the "variable" name version of these. ]
 */

extern Image *ReadBMP(char *);
extern Image *ReadXBM(char *);
extern Image *ReadLXP(char *);
extern Image *ReadXWD(char *);
extern Image *ReadPNM(char *);
extern Image *ReadPNG(char *);
extern Image *ReadGIF(char *);
extern Image *ReadSGI(char *);
extern Image *ReadXPM(char *);
extern Image *ReadTIFF(char *);
extern Image *ReadJPEG(char *);
extern Image *ReadICO(char *);
extern Image *ReadScriptC(char *);
extern Image *ReadPS_(char *);
extern int WriteBMP(char *, Image *);
extern int WriteXBM(char *, Image *);
extern int WriteLXP(char *, Image *);
extern int WriteXWD(char *, Image *);
extern int WritePNM(char *, Image *);
extern int WriteXPM(char *, Image *);
extern int WritePNGn(char *, Image *);
extern int WritePNGi(char *, Image *);
extern int WriteGIF(char *, Image *);
extern int WriteSGI(char *, Image *);
extern int WriteTIFF(char *, Image *);
extern int WriteJPEG(char *, Image *);
extern int WriteICO(char *, Image *);
extern int WritePDF(char *, Image *);
extern int WritePS(char *, Image *);
extern int TestBMP(char *);
extern int TestICO(char *);
extern int TestPNM(char *);
extern int TestLXP(char *);
extern int TestXWD(char *);
extern int TestXBM(char *);
extern int TestXPM(char *);
extern int TestPNG(char *);
extern int TestGIF(char *);
extern int TestSGI(char *);
extern int TestTIFF(char *);
extern int TestJPEG(char *);
extern int TestScriptC(char *);
extern int TestPS_(char *);
extern int TestTEX(char *);

#define DEF_READ_ENTRY	0
#define DEF_WRITE_ENTRY	0

/* GRR 960219:  added PNG, alphabetized image types: */
static ImageTypes RWtable[] =
{
    {"Auto_Detect",      readMagic, writeMagic, NULL },
#ifdef HAVE_PNG   /* ReadPNG does all PNG files; no need for two entries */
    {"PNG_Format",       ReadPNG,   WritePNGn, TestPNG },
    {"PNG_Interlaced",	 NULL,      WritePNGi, TestPNG },
#endif
    {"GIF_Format",       ReadGIF,   WriteGIF,  TestGIF },
#ifdef HAVE_JPEG
    {"JPEG_Format",      ReadJPEG,  WriteJPEG, TestJPEG},
#endif
#ifdef HAVE_TIFF
    {"TIFF_Format",      ReadTIFF,  WriteTIFF, TestTIFF},
#endif
    {"BMP_Format",       ReadBMP,   WriteBMP,  TestBMP },
    {"ICO_Format",       ReadICO,   WriteICO,  TestICO },
    {"PPM_Format",       ReadPNM,   WritePNM,  TestPNM },
#ifdef HAVE_SGI
    {"SGI_Format",       ReadSGI,   WriteSGI,  TestSGI },
#endif
    {"XBM_Format",       ReadXBM,   WriteXBM,  TestXBM },
    {"XPM_Format",       ReadXPM,   WriteXPM,  TestXPM },
    {"XWD_Format",       ReadXWD,   WriteXWD,  TestXWD },
    {"LXP_Format",       ReadLXP,   WriteLXP,  TestLXP },
    {"PS_Format",        ReadPS_,   WritePS,   TestPS_ },
    {"PDF_Format",       ReadPS_,   WritePDF,  TestPS_ },
    {"TEX_Format",       ReadPS_,   NULL,      TestPS_ },
    {"CSC_Format",       ReadScriptC,   NULL,  TestScriptC }
};

#define	FMT_NUMBER	(sizeof(RWtable) / sizeof(RWtable[0]))

static char *readList[FMT_NUMBER + 1];
static char *writeList[FMT_NUMBER + 1];

/*
**  Special reader that uses the above information.
 */
static char *usedMagicReader = NULL;
int file_isSpecialImage;
int file_numpages;
int file_force = 1;
int file_bbox = 1;
int file_transparent = 0;
int file_specified_zoom = 0;

RWwriteFunc
RWtableGetWriterFromSuffix(char *suffix)
{
    int i;
    if (!suffix || !*suffix) return WritePNGn;
    if (!strcasecmp(suffix, "C")) suffix = "CSC";
    if (!strcasecmp(suffix, "JPG")) suffix = "JPEG";

    for (i = 1; i < FMT_NUMBER; i++) {
      if (!strncasecmp(RWtable[i].name, suffix, strlen(suffix)) && 
          RWtable[i].write)
	    return RWtable[i].write;
    }
    /* default to WritePNG otherwise */
    return WritePNGn;
}

Image *
readMagic(char *file)
{
    int i;

    errno = 0;
    file_isSpecialImage = 0;
    file_transparent = 0;
    file_numpages = 1;
    file_specified_zoom = 0;

    for (i = 0; i < FMT_NUMBER; i++) {
	if (RWtable[i].read == NULL || RWtable[i].test == NULL)
	    continue;
	if (!RWtable[i].test(file))
	    continue; 
        usedMagicReader = RWtable[i].name;
	return RWtable[i].read(file);
    }

    if (errno == 0)
	RWSetMsg(msgText[UNKNOWN_IMAGE_FORMAT]);

    return NULL;
}

int 
writeMagic(char *file, Image *image)
{
    RWwriteFunc proc;
    char *ptr;
    ptr = strrchr(file, '.');
    if (ptr) {
        ++ptr;
        proc = RWtableGetWriterFromSuffix(ptr);
    } else
        proc = WritePNGn;
    return proc(file, image);
}

void *
RWtableGetReaderID()
{
    return (void *) usedMagicReader;
}


/*
**  Give a name, return an "opaque" handle to some information
 */
void *
RWtableGetEntry(char *name)
{
    int i;

    for (i = 0; i < FMT_NUMBER; i++)
	if (strcmp(name, RWtable[i].name) == 0)
	    return (void *) &RWtable[i];
    return NULL;
}

char *
RWtableGetId(void *v)
{
    ImageTypes *entry = (ImageTypes *) v;

    if (entry == NULL)
	return NULL;

    return entry->name;
}

RWreadFunc
RWtableGetReader(void *entry)
{
    RWtableMsg[0] = '\0';

    if (entry == NULL)
	return RWtable[DEF_READ_ENTRY].read;

    return ((ImageTypes *) entry)->read;
}

RWwriteFunc
RWtableGetWriter(void *entry)
{
    RWtableMsg[0] = '\0';

    if (entry == NULL)
	return RWtable[DEF_WRITE_ENTRY].write;

    return ((ImageTypes *) entry)->write;
}

char **
RWtableGetReaderList()
{
    static int done = FALSE;
    int i, idx = 0;

    if (!done) {
	for (i = 0; i < FMT_NUMBER; i++)
	    if (RWtable[i].read != NULL)
		readList[idx++] = RWtable[i].name;
	readList[idx++] = NULL;
	done = TRUE;
    }
    return readList;
}

char **
RWtableGetWriterList()
{
    static int done = FALSE;
    int i, idx = 0;

    if (!done) {
	for (i = 0; i < FMT_NUMBER; i++)
	    if (RWtable[i].write != NULL)
		writeList[idx++] = RWtable[i].name;
	writeList[idx++] = NULL;
	done = TRUE;
    }
    return writeList;
}

char *
RWGetMsg()
{
#ifndef __NetBSD__
#if defined(BSD4_4)
    __const extern char *__const sys_errlist[];
#else
#ifndef __GLIBC__
#ifndef SYS_ERRLIST_DEFINED
#ifdef __CYGWIN__
#  define sys_errlist _sys_errlist
#else
    extern char *sys_errlist[];
#endif
#endif
#endif
#endif
#endif

    if (RWtableMsg[0] == '\0') {
	if (errno == 0)
	    return "";
#if defined(__STDC__) && !defined(MISSING_STRERROR)
	return strerror(errno);
#else
	return sys_errlist[errno];
#endif
    }
    return RWtableMsg;
}

#ifdef MISSING_STDARG_H
void RWSetMsg(va_alist)
va_dcl
{
    va_list ap;
    char *fmt;

    va_start(ap);
    fmt = va_arg(ap, char *);
    vsprintf(RWtableMsg, fmt, ap);
}

#else
void RWSetMsg(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsprintf(RWtableMsg, fmt, ap);
}
#endif

Image *
ImageFromFile(char *file)
{
    return readMagic(file);
}
