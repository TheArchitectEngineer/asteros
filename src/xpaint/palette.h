#ifndef __palette_h__
#define __palette_h__

/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, David Koblas.                                     | */
/* | Copyright 1995, 1996 Torsten Martinsen (bullestock@dk-online.dk)  | */
/* |                                                                   | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

/* $Id: palette.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

typedef struct paletteUserList_s {
    Widget widget;
    struct paletteUserList_s *next;
} paletteUserList;

typedef struct {
    int isGrey;
    int isDefault;
    int isMapped;
    int ncolors;
    /*
    **
     */
    int readonly_;
    Display *display;
    Colormap cmap;
    Visual *visual;
    int depth;
    void *htable, *ltable;
    void *ctable;
    Pixel mine;
    /*
    **  TrueColor visual support
     */
    int rShift, gShift, bShift;
    int rRange, gRange, bRange;
    /*
    **  Free color entries
     */
    int nfree;
    void *list;
    /*
    **  Cache the last request value here
     */
    void *last;

    /*
    **  List of all widgets that are using this palette
     */
    paletteUserList *userList;
} Palette;

Palette *PaletteCreate(Widget);
Pixel PaletteAlloc(Palette *, XColor *);
int PaletteAllocN(Palette *, XColor *, int, Pixel *);
Palette *PaletteFindDpy(Display *, Colormap);
Palette *PaletteFind(Widget, Colormap);
XColor *PaletteLookup(Palette *, Pixel);
void PaletteAddUser(Palette *, Widget);
void PaletteSetInvalid(Palette *, Pixel);
void PaletteDelete(Palette *);
Boolean PaletteLookupColor(Palette *, XColor *, Pixel *);
Boolean PaletteSetPixel(Palette *, Pixel, XColor *);
Palette *PaletteGetDefault(Widget);
Pixel PaletteGetUnused(Palette *);

/*
**  Convenience
 */
void PixmapInvert(Widget, Colormap, Pixmap);

#endif				/* __palette_h__ */
