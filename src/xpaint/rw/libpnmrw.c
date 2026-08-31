/* libpnmrw.c - PBM/PGM/PPM read/write library
**
** Copyright (C) 1988, 1989, 1991, 1992 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
 */

/* $Id: libpnmrw.c,v 1.22 2005/03/20 20:15:34 demailly Exp $ */

#include <stdlib.h>
#include <string.h>

#if defined(HAVE_PARAM_H)
#include <sys/param.h>
#endif

#if defined(SVR2) || defined(SVR3) || defined(SVR4) || defined(linux)
#ifndef SYSV
#define SYSV
#endif
#endif
#if ! ( defined(BSD) || defined(SYSV) || defined(MSDOS) )
/* CONFIGURE: If your system is >= 4.2BSD, set the BSD option; if you're a
** System V site, set the SYSV option; and if you're IBM-compatible, set
** MSDOS.  If your compiler is ANSI C, you're probably better off setting
** SYSV - all it affects is string handling.
 */
#define BSD
/* #define SYSV */
/* #define MSDOS */
#endif

#include <stdio.h>
#include "libpnmrw.h"

typedef char *String;
#include "../messages.h"

#ifdef SYSV
#include <string.h>
#else				/*SYSV */
#include <strings.h>
#endif				/*SYSV */

#include <errno.h>

/* Definitions. */

extern void *xmalloc(size_t n);

#define pbm_allocarray( cols, rows ) \
	((bit**) pm_allocarray( cols, rows, sizeof(bit) ))
#define pbm_allocrow( cols ) ((bit*) pm_allocrow( cols, sizeof(bit) ))
#define pbm_freearray( bits, rows ) pm_freearray( (char**) bits, rows )
#define pbm_freerow( bitrow ) pm_freerow( (char*) bitrow )
#define pgm_allocarray( cols, rows ) \
	((gray**) pm_allocarray( cols, rows, sizeof(gray) ))
#define pgm_allocrow( cols ) ((gray*) pm_allocrow( cols, sizeof(gray) ))
#define pgm_freearray( grays, rows ) pm_freearray( (char**) grays, rows )
#define pgm_freerow( grayrow ) pm_freerow( (char*) grayrow )
#define ppm_allocarray( cols, rows ) \
	((pixel**) pm_allocarray( cols, rows, sizeof(pixel) ))
#define ppm_allocrow( cols ) ((pixel*) pm_allocrow( cols, sizeof(pixel) ))
#define ppm_freearray( pixels, rows ) pm_freearray( (char**) pixels, rows )
#define ppm_freerow( pixelrow ) pm_freerow( (char*) pixelrow )


/* Variables. */

static char *progname;


/* Variable-sized arrays. */

char *
pm_allocrow(int cols, int size)
{
    register char *itrow;

    itrow = (char *) xmalloc(cols * size);
    if (itrow == (char *) 0) {
	fprintf(stderr, msgText[OUT_OF_MEMORY_ALLOCATING_A_ROW], progname);
	return (char *) 0;
    }
    return itrow;
}

void
pm_freerow(char *itrow)
{
    free(itrow);
}

char **
pm_allocarray(int cols, int rows, int size)
{
    char **its;
    int i;

    its = (char **) xmalloc(rows * sizeof(char *));
    if (its == (char **) 0) {
	fprintf(stderr, msgText[OUT_OF_MEMORY_ALLOCATING_AN_ARRAY], progname);
	return (char **) 0;
    }
    its[0] = (char *) xmalloc(rows * cols * size);
    if (its[0] == (char *) 0) {
	fprintf(stderr, msgText[OUT_OF_MEMORY_ALLOCATING_AN_ARRAY], progname);
	free((char *) its);
	return (char **) 0;
    }
    for (i = 1; i < rows; ++i)
	its[i] = &(its[0][i * cols * size]);
    return its;
}

void
pm_freearray(char **its, int rows)
{
    free(its[0]);
    free(its);
}


/* File open/close that handles "-" as stdin and checks errors. */

static void
pm_perror(char *reason)
{
#if !defined(__NetBSD__)
#if defined(BSD4_4)
    __const extern char *__const sys_errlist[];
#else
#ifndef __GLIBC__
#ifndef SYS_ERRLIST_DEFINED
#ifdef __CYGWIN__
#  define sys_errlist _sys_errlist
#else
#ifdef MISSING_STRERROR
    extern char *sys_errlist[];
#endif
#endif
#endif
#endif
#endif
#endif
    char *e;

#ifdef MISSING_STRERROR
    e = (char *)sys_errlist[errno];
#else
    e = (char *)strerror(errno);   
#endif

    if (reason != 0 && reason[0] != '\0')
	fprintf(stderr, "%s: %s - %s\n", progname, reason, e);
    else
	fprintf(stderr, "%s: %s\n", progname, e);
}

FILE *
pm_openr(char *name)
{
    FILE *f;

    if (strcmp(name, "-") == 0)
	f = stdin;
    else {
	f = fopen(name, "r");
	if (f == NULL) {
	    pm_perror(name);
	    return (FILE *) 0;
	}
    }
    return f;
}

FILE *
pm_openw(char *name)
{
    FILE *f;

    f = fopen(name, "w");
    if (f == NULL) {
	pm_perror(name);
	return (FILE *) 0;
    }
    return f;
}

int
pm_closer(FILE * f)
{
    if (ferror(f)) {
	fprintf(stderr, msgText[A_FILE_READ_ERROR_OCCURRED_AT_SOME_POINT], 
                        progname);
	return -1;
    }
    if (f != stdin)
	if (fclose(f) != 0) {
	    pm_perror("fclose");
	    return -1;
	}
    return 0;
}

int
pm_closew(FILE * f)
{
    fflush(f);
    if (ferror(f)) {
	fprintf(stderr, msgText[A_FILE_WRITE_ERROR_OCCURRED_AT_SOME_POINT],
		progname);
	return -1;
    }
    if (f != stdout)
	if (fclose(f) != 0) {
	    pm_perror("fclose");
	    return -1;
	}
    return 0;
}

static int
pbm_getc(FILE * file)
{
    register int ich;

    ich = getc(file);
    if (ich == EOF) {
	fprintf(stderr, msgText[READ_ERROR], progname);
	return EOF;
    }
    if (ich == '#') {
	do {
	    ich = getc(file);
	    if (ich == EOF) {
		fprintf(stderr, msgText[READ_ERROR], progname);
		return EOF;
	    }
	}
	while (ich != '\n' && ich != '\r');
    }
    return ich;
}

static bit
pbm_getbit(FILE * file)
{
    register int ich;

    do {
	ich = pbm_getc(file);
	if (ich == EOF)
	    return -1;
    }
    while (ich == ' ' || ich == '\t' || ich == '\n' || ich == '\r');

    if (ich != '0' && ich != '1') {
	fprintf(stderr, msgText[JUNK_IN_FILE_WHERE_BITS_SHOULD_BE], progname);
	return -1;
    }
    return (ich == '1') ? 1 : 0;
}

static int
pbm_readmagicnumber(FILE * file)
{
    int ich1, ich2;

    ich1 = getc(file);
    if (ich1 == EOF) {
	fprintf(stderr, msgText[READ_ERROR_READING_MAGIC_NUMBER], progname);
	return -1;
    }
    ich2 = getc(file);
    if (ich2 == EOF) {
	fprintf(stderr, msgText[READ_ERROR_READING_MAGIC_NUMBER], progname);
	return -1;
    }
    return ich1 * 256 + ich2;
}

static int
pbm_getint(FILE * file)
{
    register char ich;
    register int i;

    do {
	ich = pbm_getc(file);
	if (ich == EOF)
	    return -1;
    }
    while (ich == ' ' || ich == '\t' || ich == '\n' || ich == '\r');

    if (ich < '0' || ich > '9') {
	fprintf(stderr, msgText[JUNK_IN_FILE_WHERE_AN_INTEGER_SHOULD_BE], 
                progname);
	return -1;
    }
    i = 0;
    do {
	i = i * 10 + ich - '0';
	ich = pbm_getc(file);
	if (ich == EOF)
	    return -1;
    }
    while (ich >= '0' && ich <= '9');

    return i;
}

static int
pbm_readpbminitrest(FILE * file, int *colsP, int *rowsP)
{
    /* Read size. */
    *colsP = pbm_getint(file);
    *rowsP = pbm_getint(file);
    if (*colsP == -1 || *rowsP == -1)
	return -1;
    return 0;
}

static int
pbm_getrawbyte(FILE * file)
{
    register int iby;

    iby = getc(file);
    if (iby == EOF) {
	fprintf(stderr, msgText[READ_ERROR], progname);
	return -1;
    }
    return iby;
}

static int
pbm_readpbmrow(FILE * file, bit * bitrow, int cols, int format)
{
    register int col, bitshift, b;
    register int item = 0;
    register bit *bP;

    switch (format) {
    case PBM_FORMAT:
	for (col = 0, bP = bitrow; col < cols; ++col, ++bP) {
	    b = pbm_getbit(file);
	    if (b == -1)
		return -1;
	    *bP = b;
	}
	break;

    case RPBM_FORMAT:
	bitshift = -1;
	for (col = 0, bP = bitrow; col < cols; ++col, ++bP) {
	    if (bitshift == -1) {
		item = pbm_getrawbyte(file);
		if (item == -1)
		    return -1;
		bitshift = 7;
	    }
	    *bP = (item >> bitshift) & 1;
	    --bitshift;
	}
	break;

    default:
	fprintf(stderr, msgText[CANT_HAPPEN], progname);
	return -1;
    }
    return 0;
}

static void
pbm_writepbminit(FILE * file, int cols, int rows, int forceplain)
{
    if (!forceplain)
	fprintf(file, "%c%c\n%d %d\n", PBM_MAGIC1, RPBM_MAGIC2, cols, rows);
    else
	fprintf(file, "%c%c\n%d %d\n", PBM_MAGIC1, PBM_MAGIC2, cols, rows);
}

static void
pbm_writepbmrowraw(FILE * file, bit * bitrow, int cols)
{
    register int col, bitshift;
    register unsigned char item;
    register bit *bP;

    bitshift = 7;
    item = 0;
    for (col = 0, bP = bitrow; col < cols; ++col, ++bP) {
	if (*bP)
	    item += 1 << bitshift;
	--bitshift;
	if (bitshift == -1) {
	    putc(item, file);
	    bitshift = 7;
	    item = 0;
	}
    }
    if (bitshift != 7)
	putc(item, file);
}

static void
pbm_writepbmrowplain(FILE * file, bit * bitrow, int cols)
{
    register int col, charcount;
    register bit *bP;

    charcount = 0;
    for (col = 0, bP = bitrow; col < cols; ++col, ++bP) {
	if (charcount >= 70) {
	    putc('\n', file);
	    charcount = 0;
	}
	putc(*bP ? '1' : '0', file);
	++charcount;
    }
    putc('\n', file);
}

static void
pbm_writepbmrow(FILE * file, bit * bitrow, int cols, int forceplain)
{
    if (!forceplain)
	pbm_writepbmrowraw(file, bitrow, cols);
    else
	pbm_writepbmrowplain(file, bitrow, cols);
}

static int
pgm_readpgminitrest(FILE * file, int *colsP, int *rowsP, gray * maxvalP)
{
    int maxval;

    /* Read size. */
    *colsP = pbm_getint(file);
    *rowsP = pbm_getint(file);
    if (*colsP == -1 || *rowsP == -1)
	return -1;

    /* Read maxval. */
    maxval = pbm_getint(file);
    if (maxval == -1)
	return -1;
    if (maxval > PGM_MAXMAXVAL) {
	fprintf(stderr, msgText[MAXVAL_IS_TOO_LARGE], progname);
	return -1;
    }
    *maxvalP = maxval;
    return 0;
}

static int
pgm_readpgmrow(FILE * file, gray * grayrow, int cols, gray maxval, int format)
{
    register int col, val;
    register gray *gP;

    switch (format) {
    case PGM_FORMAT:
	for (col = 0, gP = grayrow; col < cols; ++col, ++gP) {
	    val = pbm_getint(file);
	    if (val == -1)
		return -1;
	    *gP = val;
	}
	break;

    case RPGM_FORMAT:
	if (fread(grayrow, 1, cols, file) != cols) {
	    fprintf(stderr, msgText[READ_ERROR], progname);
	    return -1;
	}
	break;

    default:
	fprintf(stderr, msgText[CANT_HAPPEN], progname);
	return -1;
    }
    return 0;
}

static void
pgm_writepgminit(FILE * file, int cols, int rows, gray maxval, int forceplain)
{
    if (!forceplain)
	fprintf(
		   file, "%c%c\n%d %d\n%d\n", PGM_MAGIC1, RPGM_MAGIC2,
		   cols, rows, maxval);
    else
	fprintf(
		   file, "%c%c\n%d %d\n%d\n", PGM_MAGIC1, PGM_MAGIC2,
		   cols, rows, maxval);
}

static void
putus(short unsigned int n, FILE * file)
{
    if (n >= 10)
	putus(n / 10, file);
    putc(n % 10 + '0', file);
}

static int
pgm_writepgmrowraw(FILE * file, gray * grayrow, int cols, gray maxval)
{
    if (fwrite(grayrow, 1, cols, file) != cols) {
	fprintf(stderr, msgText[WRITE_ERROR], progname);
	return -1;
    }
    return 0;
}

static int
pgm_writepgmrowplain(FILE * file, gray * grayrow, int cols, gray maxval)
{
    register int col, charcount;
    register gray *gP;

    charcount = 0;
    for (col = 0, gP = grayrow; col < cols; ++col, ++gP) {
	if (charcount >= 65) {
	    putc('\n', file);
	    charcount = 0;
	} else if (charcount > 0) {
	    putc(' ', file);
	    ++charcount;
	}
	putus((unsigned short) *gP, file);
	charcount += 3;
    }
    if (charcount > 0)
	putc('\n', file);
    return 0;
}

static int
pgm_writepgmrow(FILE * file, gray * grayrow, int cols, gray maxval, int forceplain)
{
    if (!forceplain)
	return pgm_writepgmrowraw(file, grayrow, cols, maxval);
    else
	return pgm_writepgmrowplain(file, grayrow, cols, maxval);
}

static int
ppm_readppminitrest(FILE * file, int *colsP, int *rowsP, pixval * maxvalP)
{
    int maxval;

    /* Read size. */
    *colsP = pbm_getint(file);
    *rowsP = pbm_getint(file);
    if (*colsP == -1 || *rowsP == -1)
	return -1;

    /* Read maxval. */
    maxval = pbm_getint(file);
    if (maxval == -1)
	return -1;
    if (maxval > PPM_MAXMAXVAL) {
	fprintf(stderr, msgText[MAXVAL_IS_TOO_LARGE], progname);
	return -1;
    }
    *maxvalP = maxval;
    return 0;
}

static int
ppm_readppmrow(FILE * file, pixel * pixelrow, int cols,
	       pixval maxval, int format)
{
    register int col;
    register pixel *pP;
    register int r, g, b;
    gray *grayrow;
    register gray *gP;

    switch (format) {
    case PPM_FORMAT:
	for (col = 0, pP = pixelrow; col < cols; ++col, ++pP) {
	    r = pbm_getint(file);
	    g = pbm_getint(file);
	    b = pbm_getint(file);
	    if (r == -1 || g == -1 || b == -1)
		return -1;
	    PPM_ASSIGN(*pP, r, g, b);
	}
	break;

    case RPPM_FORMAT:
	grayrow = pgm_allocrow(3 * cols);
	if (grayrow == (gray *) 0)
	    return -1;
	if (fread(grayrow, 1, 3 * cols, file) != 3 * cols) {
	    fprintf(stderr, msgText[READ_ERROR], progname);
	    return -1;
	}
	for (col = 0, gP = grayrow, pP = pixelrow; col < cols; ++col, ++pP) {
	    r = *gP++;
	    g = *gP++;
	    b = *gP++;
	    PPM_ASSIGN(*pP, r, g, b);
	}
	pgm_freerow(grayrow);
	break;

    default:
	fprintf(stderr, msgText[CANT_HAPPEN], progname);
	return -1;
    }
    return 0;
}

static void
ppm_writeppminit(FILE * file, int cols, int rows, pixval maxval, int forceplain)
{
    if (!forceplain)
	fprintf(
		   file, "%c%c\n%d %d\n%d\n", PPM_MAGIC1, RPPM_MAGIC2,
		   cols, rows, maxval);
    else
	fprintf(
		   file, "%c%c\n%d %d\n%d\n", PPM_MAGIC1, PPM_MAGIC2,
		   cols, rows, maxval);
}

static int
ppm_writeppmrowraw(FILE * file, pixel * pixelrow, int cols, pixval maxval)
{
    register int col;
    register pixel *pP;
    gray *grayrow;
    register gray *gP;

    grayrow = pgm_allocrow(3 * cols);
    if (grayrow == (gray *) 0)
	return -1;
    for (col = 0, pP = pixelrow, gP = grayrow; col < cols; ++col, ++pP) {
	*gP++ = PPM_GETR(*pP);
	*gP++ = PPM_GETG(*pP);
	*gP++ = PPM_GETB(*pP);
    }
    if (fwrite(grayrow, 1, 3 * cols, file) != 3 * cols) {
	fprintf(stderr, msgText[WRITE_ERROR], progname);
	return -1;
    }
    pgm_freerow(grayrow);
    return 0;
}

static int
ppm_writeppmrowplain(FILE * file, pixel * pixelrow, int cols, pixval maxval)
{
    register int col, charcount;
    register pixel *pP;
    register pixval val;

    charcount = 0;
    for (col = 0, pP = pixelrow; col < cols; ++col, ++pP) {
	if (charcount >= 65) {
	    putc('\n', file);
	    charcount = 0;
	} else if (charcount > 0) {
	    putc(' ', file);
	    putc(' ', file);
	    charcount += 2;
	}
	val = PPM_GETR(*pP);
	putus(val, file);
	putc(' ', file);
	val = PPM_GETG(*pP);
	putus(val, file);
	putc(' ', file);
	val = PPM_GETB(*pP);
	putus(val, file);
	charcount += 11;
    }
    if (charcount > 0)
	putc('\n', file);
    return 0;
}

static int
ppm_writeppmrow(FILE * file, pixel * pixelrow, int cols,
		pixval maxval, int forceplain)
{
    if (!forceplain)
	return ppm_writeppmrowraw(file, pixelrow, cols, maxval);
    else
	return ppm_writeppmrowplain(file, pixelrow, cols, maxval);
}

void
pnm_init2(char *pn)
{
    /* Save program name. */
    progname = pn;
}

xelval pnm_pbmmaxval = 1;

int
pnm_readpnminit(FILE * file, int *colsP, int *rowsP,
		xelval * maxvalP, int *formatP)
{
    gray gmaxval;

    /* Check magic number. */
    *formatP = pbm_readmagicnumber(file);
    if (*formatP == -1)
	return -1;
    switch (PNM_FORMAT_TYPE(*formatP)) {
    case PPM_TYPE:
	if (ppm_readppminitrest(file, colsP, rowsP, (pixval *) maxvalP) < 0)
	    return -1;
	break;

    case PGM_TYPE:
	if (pgm_readpgminitrest(file, colsP, rowsP, &gmaxval) < 0)
	    return -1;
	*maxvalP = (xelval) gmaxval;
	break;

    case PBM_TYPE:
	if (pbm_readpbminitrest(file, colsP, rowsP) < 0)
	    return -1;
	*maxvalP = pnm_pbmmaxval;
	break;

    default:
	fprintf(
	  stderr, msgText[BAD_MAGIC_NUMBER_NOT_A_PPM_PGM_OR_PBM_FILE],
		  progname);
	return -1;
    }
    return 0;
}

int
pnm_readpnmrow(FILE * file, xel * xelrow, int cols, xelval maxval, int format)
{
    register int col;
    register xel *xP;
    gray *grayrow;
    register gray *gP;
    bit *bitrow;
    register bit *bP;

    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
	if (ppm_readppmrow(file, (pixel *) xelrow, cols,
			   (pixval) maxval, format) < 0)
	    return -1;
	break;

    case PGM_TYPE:
	grayrow = pgm_allocrow(cols);
	if (grayrow == (gray *) 0)
	    return -1;
	if (pgm_readpgmrow(file, grayrow, cols, (gray) maxval, format) < 0)
	    return -1;
	for (col = 0, xP = xelrow, gP = grayrow; col < cols; ++col, ++xP, ++gP)
	    PNM_ASSIGN1(*xP, *gP);
	pgm_freerow(grayrow);
	break;

    case PBM_TYPE:
	bitrow = pbm_allocrow(cols);
	if (bitrow == (bit *) 0)
	    return -1;
	if (pbm_readpbmrow(file, bitrow, cols, format) < 0) {
	    pbm_freerow(bitrow);
	    return -1;
	}
	for (col = 0, xP = xelrow, bP = bitrow; col < cols; ++col, ++xP, ++bP)
	    PNM_ASSIGN1(*xP, *bP == PBM_BLACK ? 0 : pnm_pbmmaxval);
	pbm_freerow(bitrow);
	break;

    default:
	fprintf(stderr, msgText[CANT_HAPPEN], progname);
	return -1;
    }
    return 0;
}

xel **
pnm_readpnm(FILE * file, int *colsP, int *rowsP, xelval * maxvalP, int *formatP)
{
    xel **xels;
    int row;

    if (pnm_readpnminit(file, colsP, rowsP, maxvalP, formatP) < 0)
	return (xel **) 0;

    xels = pnm_allocarray(*colsP, *rowsP);
    if (xels == (xel **) 0)
	return (xel **) 0;

    for (row = 0; row < *rowsP; ++row)
	if (pnm_readpnmrow(file, xels[row], *colsP, *maxvalP, *formatP) < 0) {
	    pnm_freearray(xels, *rowsP);
	    return (xel **) 0;
	}
    return xels;
}

int
pnm_writepnminit(FILE * file, int cols, int rows, xelval maxval,
		 int format, int forceplain)
{
    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
	ppm_writeppminit(file, cols, rows, (pixval) maxval, forceplain);
	break;

    case PGM_TYPE:
	pgm_writepgminit(file, cols, rows, (gray) maxval, forceplain);
	break;

    case PBM_TYPE:
	pbm_writepbminit(file, cols, rows, forceplain);
	break;

    default:
	fprintf(stderr, msgText[CANT_HAPPEN], progname);
	return -1;
    }
    return 0;
}

int
pnm_writepnmrow(FILE * file, xel * xelrow, int cols,
		xelval maxval, int format, int forceplain)
{
    register int col;
    register xel *xP;
    gray *grayrow;
    register gray *gP;
    bit *bitrow;
    register bit *bP;

    switch (PNM_FORMAT_TYPE(format)) {
    case PPM_TYPE:
	if (ppm_writeppmrow(file, (pixel *) xelrow, cols,
			    (pixval) maxval, forceplain) < 0)
	    return -1;
	break;

    case PGM_TYPE:
	grayrow = pgm_allocrow(cols);
	if (grayrow == (gray *) 0)
	    return -1;
	for (col = 0, gP = grayrow, xP = xelrow; col < cols; ++col, ++gP, ++xP)
	    *gP = PNM_GET1(*xP);
	if (pgm_writepgmrow(file, grayrow, cols,
			    (gray) maxval, forceplain) < 0) {
	    pgm_freerow(grayrow);
	    return -1;
	}
	pgm_freerow(grayrow);
	break;

    case PBM_TYPE:
	bitrow = pbm_allocrow(cols);
	if (bitrow == (bit *) 0)
	    return -1;
	for (col = 0, bP = bitrow, xP = xelrow; col < cols; ++col, ++bP, ++xP)
	    *bP = PNM_GET1(*xP) == 0 ? PBM_BLACK : PBM_WHITE;
	pbm_writepbmrow(file, bitrow, cols, forceplain);
	pbm_freerow(bitrow);
	break;

    default:
	fprintf(stderr, msgText[CANT_HAPPEN], progname);
	return -1;
    }
    return 0;
}

int
pnm_writepnm(FILE * file, xel ** xels, int cols, int rows,
	     xelval maxval, int format, int forceplain)
{
    int row;

    if (pnm_writepnminit(file, cols, rows, maxval, format, forceplain) < 0)
	return -1;

    for (row = 0; row < rows; ++row)
	if (pnm_writepnmrow(file, xels[row], cols,
			    maxval, format, forceplain) < 0)
	    return -1;
    return 0;
}


/* Colormap stuff. */

#define HASH_SIZE 20023

#define ppm_hashpixel(p) ((((long) PPM_GETR(p)*33023 + \
			    (long) PPM_GETG(p)*30013 + \
			    (long) PPM_GETB(p)*27011) & 0x7fffffff) % HASH_SIZE)

colorhist_vector
ppm_computecolorhist(pixel ** pixels, int cols, int rows,
		     int maxcolors, int *colorsP)
{
    colorhash_table cht;
    colorhist_vector chv;

    cht = ppm_computecolorhash(pixels, cols, rows, maxcolors, colorsP);
    if (cht == (colorhash_table) 0)
	return (colorhist_vector) 0;
    chv = ppm_colorhashtocolorhist(cht, maxcolors);
    ppm_freecolorhash(cht);
    return chv;
}

void
ppm_addtocolorhist(colorhist_vector chv, int *colorsP,
		   int maxcolors, pixel * colorP, int value, int position)
{
    int i, j;

    /* Search colorhist for the color. */
    for (i = 0; i < *colorsP; ++i)
	if (PPM_EQUAL(chv[i].color, *colorP)) {
	    /* Found it - move to new slot. */
	    if (position > i) {
		for (j = i; j < position; ++j)
		    chv[j] = chv[j + 1];
	    } else if (position < i) {
		for (j = i; j > position; --j)
		    chv[j] = chv[j - 1];
	    }
	    chv[position].color = *colorP;
	    chv[position].value = value;
	    return;
	}
    if (*colorsP < maxcolors) {
	/* Didn't find it, but there's room to add it; so do so. */
	for (i = *colorsP; i > position; --i)
	    chv[i] = chv[i - 1];
	chv[position].color = *colorP;
	chv[position].value = value;
	++(*colorsP);
    }
}

colorhash_table
ppm_computecolorhash(pixel ** pixels, int cols, int rows,
		     int maxcolors, int *colorsP)
{
    colorhash_table cht;
    register pixel *pP;
    colorhist_list chl;
    int col, row, hash;

    cht = ppm_alloccolorhash();
    if (cht == (colorhash_table) 0)
	return (colorhash_table) 0;
    *colorsP = 0;

    /* Go through the entire image, building a hash table of colors. */
    for (row = 0; row < rows; ++row)
	for (col = 0, pP = pixels[row]; col < cols; ++col, ++pP) {
	    hash = ppm_hashpixel(*pP);
	    for (chl = cht[hash]; chl != (colorhist_list) 0; chl = chl->next)
		if (PPM_EQUAL(chl->ch.color, *pP))
		    break;
	    if (chl != (colorhist_list) 0)
		++(chl->ch.value);
	    else {
		if (++(*colorsP) > maxcolors) {
		    ppm_freecolorhash(cht);
		    return (colorhash_table) 0;
		}
		chl = (colorhist_list)
		    xmalloc(sizeof(struct colorhist_list_item));
		if (chl == 0) {
		    fprintf(stderr,
			    msgText[OUT_OF_MEMORY_COMPUTING_HASH_TABLE],
			    progname);
		    ppm_freecolorhash(cht);
		    return (colorhash_table) 0;
		}
		chl->ch.color = *pP;
		chl->ch.value = 1;
		chl->next = cht[hash];
		cht[hash] = chl;
	    }
	}

    return cht;
}

colorhash_table
ppm_alloccolorhash(void)
{
    colorhash_table cht;
    int i;

    cht = (colorhash_table) xmalloc(HASH_SIZE * sizeof(colorhist_list));
    if (cht == 0) {
	fprintf(stderr, msgText[OUT_OF_MEMORY_ALLOCATING_HASH_TABLE], 
                progname);
	return (colorhash_table) 0;
    }
    for (i = 0; i < HASH_SIZE; ++i)
	cht[i] = (colorhist_list) 0;

    return cht;
}

int
ppm_addtocolorhash(colorhash_table cht, pixel * colorP, int value)
{
    register int hash;
    register colorhist_list chl;

    chl = (colorhist_list) xmalloc(sizeof(struct colorhist_list_item));
    if (chl == 0)
	return -1;
    hash = ppm_hashpixel(*colorP);
    chl->ch.color = *colorP;
    chl->ch.value = value;
    chl->next = cht[hash];
    cht[hash] = chl;
    return 0;
}

colorhist_vector
ppm_colorhashtocolorhist(colorhash_table cht, int maxcolors)
{
    colorhist_vector chv;
    colorhist_list chl;
    int i, j;

    /* Now collate the hash table into a simple colorhist array. */
    chv = (colorhist_vector) xmalloc(maxcolors * sizeof(struct colorhist_item));
    /* (Leave room for expansion by caller.) */
    if (chv == (colorhist_vector) 0) {
	fprintf(stderr, msgText[OUT_OF_MEMORY_GENERATING_HISTOGRAM], progname);
	return (colorhist_vector) 0;
    }
    /* Loop through the hash table. */
    j = 0;
    for (i = 0; i < HASH_SIZE; ++i)
	for (chl = cht[i]; chl != (colorhist_list) 0; chl = chl->next) {
	    /* Add the new entry. */
	    chv[j] = chl->ch;
	    ++j;
	}

    /* All done. */
    return chv;
}

colorhash_table
ppm_colorhisttocolorhash(colorhist_vector chv, int colors)
{
    colorhash_table cht;
    int i, hash;
    pixel color;
    colorhist_list chl;

    cht = ppm_alloccolorhash();
    if (cht == (colorhash_table) 0)
	return (colorhash_table) 0;

    for (i = 0; i < colors; ++i) {
	color = chv[i].color;
	hash = ppm_hashpixel(color);
	for (chl = cht[hash]; chl != (colorhist_list) 0; chl = chl->next)
	    if (PPM_EQUAL(chl->ch.color, color)) {
		fprintf(stderr, "%s %s - %d %d %d\n", progname,
		      msgText[SAME_COLOR_FOUND_TWICE],
		      PPM_GETR(color), PPM_GETG(color), PPM_GETB(color));
		ppm_freecolorhash(cht);
		return (colorhash_table) 0;
	    }
	chl = (colorhist_list) xmalloc(sizeof(struct colorhist_list_item));
	if (chl == (colorhist_list) 0) {
	    fprintf(stderr, msgText[COLORHIST_OUT_OF_MEMORY], progname);
	    ppm_freecolorhash(cht);
	    return (colorhash_table) 0;
	}
	chl->ch.color = color;
	chl->ch.value = i;
	chl->next = cht[hash];
	cht[hash] = chl;
    }

    return cht;
}

int
ppm_lookupcolor(colorhash_table cht, pixel * colorP)
{
    int hash;
    colorhist_list chl;

    hash = ppm_hashpixel(*colorP);
    for (chl = cht[hash]; chl != (colorhist_list) 0; chl = chl->next)
	if (PPM_EQUAL(chl->ch.color, *colorP))
	    return chl->ch.value;

    return -1;
}

void
ppm_freecolorhist(colorhist_vector chv)
{
    free((char *) chv);
}

void
ppm_freecolorhash(colorhash_table cht)
{
    int i;
    colorhist_list chl, chlnext;

    for (i = 0; i < HASH_SIZE; ++i)
	for (chl = cht[i]; chl != (colorhist_list) 0; chl = chlnext) {
	    chlnext = chl->next;
	    free((char *) chl);
	}
    free((char *) cht);
}
