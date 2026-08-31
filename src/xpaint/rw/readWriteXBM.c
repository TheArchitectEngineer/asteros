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

/* $Id: readWriteXBM.c,v 1.21 2005/03/20 20:15:34 demailly Exp $ */

#include <stdio.h>
#include "image.h"

#define TRUE 1
#define FALSE 0

/*********************************************************************
*   Code from MIT
*********************************************************************/

/*
   Permission to use, copy, modify, distribute, and sell this software and its
   documentation for any purpose is hereby granted without fee, provided that
   the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation, and that the name of M.I.T. not be used in advertising or
   publicity pertaining to distribution of the software without specific,
   written prior permission.  M.I.T. makes no representations about the
   suitability of this software for any purpose.  It is provided "as is"
   without express or implied warranty.
 */

/*
 *    Code to read bitmaps from disk files. Interprets 
 *	data from X10 and X11 bitmap files and creates
 *	Pixmap representations of files. Returns Pixmap
 *	ID and specifics about image.
 *
 *	Modified for speedup by Jim Becker, changed image
 *	data parsing logic (removed some fscanf()s). 
 *	Aug 5, 1988
 *
 * Note that this file and ../Xmu/RdBitF.c look very similar....  Keep them
 * that way (but don't use common source code so that people can have one 
 * without the other).
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>


#define MAX_SIZE 255
#define True 1
#define False 0

/* shared data for the image read/parse logic */
static short hexTable[256];	/* conversion value */
static int initialized = False;	/* easier to fill in at run time */


/*
 *    Table index for the hex values. Initialized once, first time.
 *	Used for translation value or delimiter significance lookup.
 */
static void initHexTable(void)
{
    /*
     * We build the table at run time for several reasons:
     *
     *	   1.  portable to non-ASCII machines.
     *	   2.  still reentrant since we set the init flag after setting table.
     *	   3.  easier to extend.
     *	   4.  less prone to bugs.
     */
    hexTable['0'] = 0;
    hexTable['1'] = 1;
    hexTable['2'] = 2;
    hexTable['3'] = 3;
    hexTable['4'] = 4;
    hexTable['5'] = 5;
    hexTable['6'] = 6;
    hexTable['7'] = 7;
    hexTable['8'] = 8;
    hexTable['9'] = 9;
    hexTable['A'] = 10;
    hexTable['B'] = 11;
    hexTable['C'] = 12;
    hexTable['D'] = 13;
    hexTable['E'] = 14;
    hexTable['F'] = 15;
    hexTable['a'] = 10;
    hexTable['b'] = 11;
    hexTable['c'] = 12;
    hexTable['d'] = 13;
    hexTable['e'] = 14;
    hexTable['f'] = 15;

    /* delimiters of significance are flagged w/ negative value */
    hexTable[' '] = -1;
    hexTable[','] = -1;
    hexTable['}'] = -1;
    hexTable['\n'] = -1;
    hexTable['\t'] = -1;

    initialized = True;
}

/*
 *    read next hex value in the input stream, return -1 if EOF
 */
static int NextInt(FILE * fstream)
{
    int ch;
    int value = 0;
    int gotone = 0;
    int done = 0;

    /* loop, accumulate hex value until find delimiter	*/
    /* skip any initial delimiters found in read stream */

    while (!done) {
	ch = getc(fstream);
	if (ch == EOF) {
	    value = -1;
	    done++;
	} else {
	    /* trim high bits, check type and accumulate */
	    ch &= 0xff;
	    if (isascii(ch) && isxdigit(ch)) {
		value = (value << 4) + hexTable[ch];
		gotone++;
	    } else if ((hexTable[ch]) < 0 && gotone)
		done++;
	}
    }
    return value;
}


Image *
 ReadXBM(char *file)
{
    Image *image = NULL;
    FILE *fstream;		/* handle on file  */
    unsigned char *data = NULL;	/* working variable */
    char line[MAX_SIZE];	/* input line from file */
    int size;			/* number of bytes of data */
    char name_and_type[MAX_SIZE];	/* an input line */
    char *type;			/* for parsing */
    int value;			/* from an input line */
    int version10p;		/* boolean, old format */
    int padding;		/* to handle alignment */
    int bytes_per_line;		/* per scanline of data */
    unsigned int ww = 0;	/* width */
    unsigned int hh = 0;	/* height */
    int hx = -1;		/* x hotspot */
    int hy = -1;		/* y hotspot */

    /* first time initialization */
    if (initialized == False)
	initHexTable();

    if ((fstream = fopen(file, "r")) == NULL)
	return NULL;

    /* error cleanup and return macro	*/

    while (fgets(line, MAX_SIZE, fstream)) {
	if (strlen(line) == MAX_SIZE - 1) {
	    return NULL;
	}
	if (sscanf(line, "#define %s %d", name_and_type, &value) == 2) {
	    if (!(type = strrchr(name_and_type, '_')))
		type = name_and_type;
	    else
		type++;

	    if (!strcmp("width", type))
		ww = (unsigned int) value;
	    if (!strcmp("height", type))
		hh = (unsigned int) value;
	    if (!strcmp("hot", type)) {
		if (type-- == name_and_type || type-- == name_and_type)
		    continue;
		if (!strcmp("x_hot", type))
		    hx = value;
		if (!strcmp("y_hot", type))
		    hy = value;
	    }
	    continue;
	}
	if (sscanf(line, "static short %s = {", name_and_type) == 1)	/* } */
	    version10p = 1;
	else if (sscanf(line, "static unsigned char %s = {", name_and_type) == 1)	/* } */
	    version10p = 0;
	else if (sscanf(line, "static char %s = {", name_and_type) == 1)	/* } */
	    version10p = 0;
	else
	    continue;

	if (!(type = strrchr(name_and_type, '_')))
	    type = name_and_type;
	else
	    type++;

	if (strcmp("bits[]", type))
	    continue;

	if (!ww || !hh)
	    return NULL;

	if ((ww % 16) && ((ww % 16) < 9) && version10p)
	    padding = 1;
	else
	    padding = 0;

	bytes_per_line = (ww + 7) / 8 + padding;

	if ((image = ImageNewBW(ww, hh)) == NULL)
	    return NULL;

	size = bytes_per_line * hh;

	if (version10p) {
	    unsigned char *ptr;
	    int bytes;
	    int i, x = 0, y = 0;

	    for (bytes = 0, ptr = data; bytes < size; (bytes += 2)) {
		if ((value = NextInt(fstream)) < 0) {
		    ImageDelete(image);
		    return NULL;
		}
		if (!padding || ((bytes + 2) % bytes_per_line))
		    value >>= 8;
		for (i = 0; i < 16 && x < ww; i++, x++)
		    image->data[y * ww + x] = ((value & (1 << i)) != 0) ? 0 : 1;
		if (x == ww) {
		    y++;
		    x = 0;
		}
	    }
	} else {
	    unsigned char *ptr;
	    int bytes;
	    int i, x = 0, y = 0;

	    for (bytes = 0; bytes < size; bytes++, ptr++) {
		if ((value = NextInt(fstream)) < 0) {
		    ImageDelete(image);
		    return NULL;
		}
		for (i = 0; i < 8 && x < ww; i++, x++)
		    image->data[y * ww + x] = ((value & (1 << i)) != 0) ? 0 : 1;
		if (x == ww) {
		    y++;
		    x = 0;
		}
	    }
	}
    }				/* end while */

    fclose(fstream);
    return image;
}

/*********************************************************************
*   End Code from MIT
*********************************************************************/

int WriteXBM(char *file, Image * image)
{
    int x, y, count, idx;
    char name[256], *cp;
    FILE *fd;
    unsigned char val, *dp;

    if ((fd = fopen(file, "w")) == NULL)
	return TRUE;

    if ((cp = strrchr(file, '/')) == NULL)
	strcpy(name, file);
    else
	strcpy(name, cp + 1);
    if ((cp = strrchr(name, '.')) != NULL)
	*cp = '\0';

    fprintf(fd, "#define %s_width %d\n", name, image->width);
    fprintf(fd, "#define %s_height %d\n", name, image->height);
    fprintf(fd, "static unsigned char %s_bits[] = {\n  ", name);

    dp = image->data;
    for (val = count = y = 0; y < image->height; y++) {
	for (idx = x = 0; x < image->width; x++) {
	    val >>= 1;
	    if (image->isBW) {
		val |= *dp++ ? 0 : 0x80;
	    } else if (image->isGrey) {
		val |= (*dp++ > 128) ? 0 : 0x80;
	    } else {
		dp = ImagePixel(image, x, y);
		val |= (((float) dp[0] * 0.4 +
			 (float) dp[1] * 0.4 +
			 (float) dp[2] * 0.2) > 128) ? 0 : 0x80;
	    }
	    idx++;
	    if (idx == 8) {
		idx = 0;
		fprintf(fd, "0x%02x", val);

		if ((++count % 12) == 0)
		    fprintf(fd, ",\n  ");
		else
		    fprintf(fd, ", ");
	    }
	}
	if (idx != 0) {
	    val >>= 8 - idx;
	    fprintf(fd, "0x%02x", val);

	    if ((++count % 12) == 0)
		fprintf(fd, ",\n  ");
	    else
		fprintf(fd, ", ");
	}
    }

    fprintf(fd, "};\n");

    fclose(fd);

    return FALSE;
}

int TestXBM(char *file)
{
    FILE *fd = fopen(file, "r");
    char buf[20];
    int ret = FALSE;

    if (fd == NULL)
	return FALSE;

    fread(buf, sizeof(char), 20, fd);
    fclose(fd);

    ret = (strncmp("#define", buf, 7) == 0);

    return ret;
}
