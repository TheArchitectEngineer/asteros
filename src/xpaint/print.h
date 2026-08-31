#ifndef __PRINT_H__
#define __PRINT_H__

/* +-------------------------------------------------------------------+ */
/* | Copyright (C) 2010 JP Demailly (demailly@fourier.ujf-grenoble.fr) | */
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

/* $Id: print.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

typedef struct {
    float wpercent, hpercent;
    int wbbox, hbbox, width, height, rx, ry, wsubdiv, hsubdiv;
    Boolean orient, eps, gray, compress, pdf;
    char *title;
} PageInfo;

#endif
