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

/* $Id: palette.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtScreen XTSCREEN
#endif

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include "palette.h"
#include "hash.h"
#include "messages.h"
#include "misc.h"
#include "image.h"
#include "xpaint.h"

typedef struct list_s {
    Display *dpy;
    Colormap cmap;
    Palette *map;
    struct list_s *next;
} PaletteList;

typedef struct Col_s {
    XColor color;
    Boolean used;
    Boolean invalid;
} Col;

#define HASH_SIZE 128
#define HASH(c)   	  ((int)((c).red + (c).green + (c).blue) % HASH_SIZE)
#define HASH_PIXEL(c)     ((c) % HASH_SIZE)

static PaletteList *cmapList = NULL;

static int 
readCMP(void *cca, void *ccb)
{
    XColor *ca = &((Col *) cca)->color, *cb = &((Col *) ccb)->color;

    if (ca->red != cb->red)
	return ca->red < cb->red ? -1 : 1;
    if (ca->green != cb->green)
	return ca->green < cb->green ? -1 : 1;
    if (ca->blue != cb->blue)
	return ca->blue < cb->blue ? -1 : 1;
    return 0;
}

static int 
cmpPixel(void *cca, void *ccb)
{
    if (((Col *) cca)->color.pixel == ((Col *) ccb)->color.pixel)
	return 0;
    return (((Col *) cca)->color.pixel < ((Col *) ccb)->color.pixel) ? -1 : 1;
}

static void 
freeFunc(void *junk)
{
    /*
    **  Nop free func
     */
}

static void 
entryUnlink(Palette * map, Col * node)
{
    node->used = True;
    node->invalid = False;
    map->nfree--;
}


#define STEP	256

static Palette *
paletteNew(Widget w, Boolean useDefault)
{
    int i, v;
    Display *dpy = XtDisplay(w);
    Screen *screen = XtScreen(w);
    Colormap rcmap = DefaultColormapOfScreen(screen);
    Palette *map = XtNew(Palette);
    Col *ctable;
    Visual *visual = NULL;
    PaletteList *new;
    int end, depth = -1;
    Boolean flg = False;

    PopdownMenusGlobal();

    XtVaGetValues(w, XtNvisual, &visual, XtNdepth, &depth, NULL);
    if (visual == NULL || useDefault)
	visual = DefaultVisualOfScreen(screen);
    if (depth <= 0 || useDefault)
	depth = DefaultDepthOfScreen(screen);

    map->htable = NULL;
    map->ltable = NULL;
    map->display = dpy;
    map->htable = HashCreate(readCMP, freeFunc, HASH_SIZE);
    map->ltable = HashCreate(cmpPixel, freeFunc, HASH_SIZE);
    map->list = NULL;
    map->last = NULL;
    map->isMapped = visual->class != TrueColor;
    map->isGrey = (visual->class == StaticGray || visual->class == GrayScale);
    map->ncolors = visual->map_entries;
    map->nfree = 0;
    map->ctable = NULL;
    map->visual = visual;
    map->depth = depth;
    map->userList = NULL;
    map->isDefault = False;
    map->mine = None;

    switch (visual->class) {
    case TrueColor:
	map->rShift = 0;
	map->gShift = 0;
	map->bShift = 0;
	map->rRange = 1;
	map->gRange = 1;
	map->bRange = 1;
	for (v = visual->red_mask; (v & 1) == 0; v >>= 1)
	    map->rShift++;
	for (; (v & 1) == 1; v >>= 1)
	    map->rRange <<= 1;
	for (v = visual->green_mask; (v & 1) == 0; v >>= 1)
	    map->gShift++;
	for (; (v & 1) == 1; v >>= 1)
	    map->gRange <<= 1;
	for (v = visual->blue_mask; (v & 1) == 0; v >>= 1)
	    map->bShift++;
	for (; (v & 1) == 1; v >>= 1)
	    map->bRange <<= 1;
    case StaticGray:
    case StaticColor:
	map->readonly_ = True;
	map->cmap = XCreateColormap(dpy, RootWindowOfScreen(screen),
				    visual, AllocNone);
	map->isDefault = False;
	goto addlist;
    default:
	map->readonly_ = False;
	if (useDefault) {
	    map->cmap = rcmap;
	    map->isDefault = True;
	} else {
	    map->cmap = XCreateColormap(dpy, RootWindowOfScreen(screen),
					visual, AllocAll);
	}
	break;
    }

    ctable = (Col *) XtCalloc(sizeof(Col), visual->map_entries);
    map->ctable = ctable;
    end = CellsOfScreen(screen);
    for (i = 0; i < visual->map_entries; i += STEP) {
	XColor xcol[STEP];
	int cnt = visual->map_entries - i;
	int j, d;

	if (cnt > STEP)
	    cnt = STEP;

	for (j = 0; j < cnt; j++) {
	    Col *c = &ctable[i + j];

	    c->color.pixel = i + j;
	    c->color.flags = DoRed | DoGreen | DoBlue;
	    xcol[j].pixel = i + j;
	    xcol[j].flags = DoRed | DoGreen | DoBlue;

	    c->used = False;
	    c->invalid = False;
	}

	if (i >= end)
	    d = 0;
	else
	    d = MIN(end - i, cnt);

	if (i < end)
	    XQueryColors(dpy, rcmap, xcol, d);
	if (!flg) {
	    for (j = d; j < cnt; j++) {
		xcol[j].flags = DoRed | DoGreen | DoBlue;
		xcol[j].red = xcol[j].green = xcol[j].blue = 0xffff;
	    }
	    if (d == 0)
		flg = True;
	}
	if (!map->isDefault)
	    XStoreColors(dpy, map->cmap, xcol, cnt);

	for (j = 0; j < cnt; j++) {
	    Col *c = &ctable[i + j];

	    c->color.red = xcol[j].red & 0xff00;
	    c->color.green = xcol[j].green & 0xff00;
	    c->color.blue = xcol[j].blue & 0xff00;
	    HashAdd(map->htable, HASH(c->color), c);
	    HashAdd(map->ltable, HASH_PIXEL(c->color.pixel), c);
	}
    }

    map->nfree = visual->map_entries;

    if (!map->isDefault) {
	Boolean got = False;

	got = !map->isMapped;
	for (i = 0; i < visual->map_entries; i++) {
	    if (ctable[i].color.pixel == BlackPixelOfScreen(screen)) {
		entryUnlink(map, &ctable[i]);
	    } else if (ctable[i].color.pixel == WhitePixelOfScreen(screen)) {
		entryUnlink(map, &ctable[i]);
	    } else if (!got) {
		map->mine = i;
		entryUnlink(map, &ctable[i]);
		got = True;
	    }
	}
    }
  addlist:
    new = (PaletteList *) XtMalloc(sizeof(PaletteList));
    new->dpy = XtDisplay(w);
    new->cmap = map->cmap;
    new->next = cmapList;
    new->map = map;
    cmapList = new;

    return map;
}

Palette *
PaletteCreate(Widget w)
{
    return paletteNew(GetShell(w), False);
}

Palette *
PaletteGetDefault(Widget w)
{
    static Palette *defMap = NULL;

    if (defMap == NULL)
	defMap = paletteNew(GetShell(w), True);

    return defMap;
}

static Palette *
paletteGetBW(void)
{
    static Palette *map = NULL;
    Col *ctable;

    if (map != NULL)
	return map;

    map = XtNew(Palette);

    map->display = NULL;
    map->htable = HashCreate(readCMP, freeFunc, HASH_SIZE);
    map->ltable = HashCreate(cmpPixel, freeFunc, HASH_SIZE);
    map->list = NULL;
    map->last = NULL;
    map->isMapped = True;
    map->isGrey = True;
    map->ncolors = 2;
    map->nfree = 0;
    map->ctable = NULL;
    map->visual = None;		/* X */
    map->depth = 1;
    map->userList = NULL;
    map->isDefault = False;
    map->mine = None;
    map->cmap = None;

    ctable = (Col *) XtCalloc(sizeof(Col), map->ncolors);
    map->ctable = ctable;

    ctable[0].color.pixel = 0;
    ctable[0].color.flags = DoRed | DoGreen | DoBlue;
    ctable[0].color.red = 0xffff;
    ctable[0].color.green = 0xffff;
    ctable[0].color.blue = 0xffff;
    ctable[0].used = True;
    ctable[0].invalid = False;

    ctable[1].color.pixel = 1;
    ctable[1].color.flags = DoRed | DoGreen | DoBlue;
    ctable[1].color.red = 0x0000;
    ctable[1].color.green = 0x0000;
    ctable[1].color.blue = 0x0000;
    ctable[1].used = True;
    ctable[1].invalid = False;

    HashAdd(map->htable, HASH(ctable[0].color), &ctable[0]);
    HashAdd(map->htable, HASH(ctable[1].color), &ctable[1]);
    HashAdd(map->ltable, HASH_PIXEL(ctable[0].color.pixel), &ctable[0]);
    HashAdd(map->ltable, HASH_PIXEL(ctable[1].color.pixel), &ctable[1]);

    return map;
}

static void 
addColor(Palette * map, XColor * color)
{
    Col *node = NULL, *n, *cptr;
    int i;


    if (map->readonly_) {
	/*
	**  The temporary color is needed since alloc
	**  will change the pixel values.
	 */
	XColor newc;
	newc = *color;
	node = XtNew(Col);

	newc.flags = DoRed | DoGreen | DoBlue;
	XAllocColor(map->display, map->cmap, &newc);
	node->color.pixel = color->pixel = newc.pixel;
    } else {
	if (map->nfree == 0) {
	    unsigned int d, curDif = ~0;
	    int rd, gd, bd;
	    /*
	    **  All free colors used up -- use closest match.
	     */
	    for (i = 0, cptr = (Col *) map->ctable; i < map->ncolors; i++, cptr++) {
		rd = (cptr->color.red >> 8) - (color->red >> 8);
		gd = (cptr->color.green >> 8) - (color->green >> 8);
		bd = (cptr->color.blue >> 8) - (color->blue >> 8);
		d = rd * rd + gd * gd + bd * bd;
		if (d < curDif) {
		    node = cptr;
		    curDif = d;
		}
	    }
	    color->pixel = node->color.pixel;
	    return;
	}
	/* Find first free entry -- we know it's there. */
	for (i = 0, node = (Col *) map->ctable;
	     (i < map->ncolors) && node->used; i++, node++);
	entryUnlink(map, node);	/* mark as used */

	color->pixel = node->color.pixel;
	n = HashFind(map->ltable, HASH_PIXEL(node->color.pixel), node);
	HashRemove(map->ltable, HASH_PIXEL(node->color.pixel), n);
	HashRemove(map->htable, HASH(*color), node);
    }

    node->used = True;
    node->invalid = False;
    node->color.red = color->red & 0xff00;
    node->color.green = color->green & 0xff00;
    node->color.blue = color->blue & 0xff00;
    node->color.flags = DoRed | DoGreen | DoBlue;
    if (!map->readonly_)
	XStoreColor(map->display, map->cmap, &node->color);
    HashAdd(map->htable, HASH(*color), node);
    HashAdd(map->ltable, HASH_PIXEL(color->pixel), node);
}

/*
 * Try to allocate the specified XColor in the colormap.
 * If the color is already in the palette, use that instead.
 * Return the corresponding Pixel value in 'list'.
 */
static Col *
allocN(Palette * map, XColor * color, Pixel * list)
{
    Col c, *node;

    c.color.red = color->red & 0xff00;
    c.color.green = color->green & 0xff00;
    c.color.blue = color->blue & 0xff00;

    if ((node = HashFind(map->htable, HASH(c.color), &c)) == NULL) {
	addColor(map, color);
	*list = color->pixel;
    } else {
	/*
	**  It must have been allocated in the previous
	**   pass, or by the above.
	 */
	if (!node->used)
	    entryUnlink(map, node);
	*list = node->color.pixel;
    }

    return node;
}

int 
PaletteAllocN(Palette * map, XColor * color, int ncolor, Pixel * list)
{
    Boolean *flg = XtCalloc(sizeof(Boolean), ncolor);
    Col c, *node;
    int i;
    Boolean newMine = False;

    if (!map->isMapped) {
	for (i = 0; i < ncolor; i++) {
	    unsigned int r, g, b;

	    r = (color[i].red * map->rRange) >> 16;
	    g = (color[i].green * map->gRange) >> 16;
	    b = (color[i].blue * map->bRange) >> 16;
	    list[i] = (r << map->rShift) | (g << map->gShift) | (b << map->bShift);
	}
	return 0;
    }
    for (i = 0; i < ncolor; i++) {
	Col c;

	c.color.red = color[i].red & 0xff00;
	c.color.green = color[i].green & 0xff00;
	c.color.blue = color[i].blue & 0xff00;

	if ((node = HashFind(map->htable, HASH(c.color), &c)) != NULL) {
	    flg[i] = True;
	    /*
	    **  Match found, if the entry hasn't been "alloced"
	    **   yet, mark it so, and remove it from the "free" list.
	     */
	    if (!node->used)
		entryUnlink(map, node);
	    list[i] = node->color.pixel;
	    if (list[i] == map->mine)
		newMine = True;
	} else {
	    flg[i] = False;
	}
    }

    for (i = 0; i < ncolor; i++) {
	if (flg[i])
	    continue;

	c.color.red = color[i].red & 0xff00;
	c.color.green = color[i].green & 0xff00;
	c.color.blue = color[i].blue & 0xff00;

	if ((node = HashFind(map->htable, HASH(c.color), &c)) == NULL) {
	    addColor(map, &color[i]);
	    list[i] = color[i].pixel;
	} else {
	    /*
	    **  It must have been allocated in the previous
	    **   pass, or by the above.
	     */
	    list[i] = node->color.pixel;
	}
	if (list[i] == map->mine)
	    newMine = True;
    }

    XtFree((XtPointer) flg);

    if (newMine && map->ctable != NULL) {
	Col *cptr = (Col *) map->ctable;

	for (i = 0; i < map->ncolors; i++, cptr++) {
	    if (cptr->used)
		continue;
	    map->mine = cptr->color.pixel;
	    break;
	}
    }
    return 0;
}

/*
 * Return the Pixel corresponding to (or closest to) the RGB
 * values specified in 'color'.
 * If no match is found in the existing palette, try to allocate
 * a new color.
 */
Pixel
PaletteAlloc(Palette * map, XColor * color)
{
    if (!map->isMapped) {
	unsigned int r, g, b;

	r = (color->red * map->rRange) >> 16;
	g = (color->green * map->gRange) >> 16;
	b = (color->blue * map->bRange) >> 16;
	return (r << map->rShift) | (g << map->gShift) | (b << map->bShift);
    }
    if (map->last != NULL) {
	XColor *lc = (XColor *) map->last;

	if (lc->red == color->red &&
	    lc->green == color->green &&
	    lc->blue == color->blue)
	    return color->pixel = lc->pixel;
    }
    map->last = allocN(map, color, &color->pixel);

    return color->pixel;
}

/*
**  Given a Pixel value on the specified map return the
**   RGB value.
**  On non-TrueColor displays, the returned value is actually a pointer
**   into map->ctable[].
**
**   Special case "TrueColor" since it is just computed.
 */
XColor *
PaletteLookup(Palette * map, Pixel pix)
{
    if (map->isMapped) {
	Col col;
	Col *c;
	col.color.pixel = pix;

	if ((c = (Col *) HashFind(map->ltable, HASH_PIXEL(pix), &col)) == NULL) {
	    fprintf(stderr,"%s", msgText[SHOULDNT_HAPPEN]);
	    return NULL;
	}
	if (c->invalid) {
	    HashRemove(map->htable, HASH(c->color), c);
	    XQueryColor(map->display, map->cmap, &c->color);
	    c->color.red &= 0xff00;
	    c->color.green &= 0xff00;
	    c->color.blue &= 0xff00;
	    HashAdd(map->htable, HASH(c->color), c);
	    c->invalid = False;
	}
	return &c->color;
    } else {
	static XColor xc;

	xc.red = (pix >> map->rShift) & (map->rRange - 1);
	xc.green = (pix >> map->gShift) & (map->gRange - 1);
	xc.blue = (pix >> map->bShift) & (map->bRange - 1);

	xc.red *= 65536 / map->rRange;
	xc.green *= 65536 / map->gRange;
	xc.blue *= 65536 / map->bRange;

	return &xc;
    }
}

Boolean
PaletteLookupColor(Palette * map, XColor * col, Pixel * pxl)
{
    Col c, *node;

    if (!map->isMapped) {
	unsigned int r, g, b;

	r = (col->red * map->rRange) >> 16;
	g = (col->green * map->gRange) >> 16;
	b = (col->blue * map->bRange) >> 16;
	*pxl = (r << map->rShift) | (g << map->gShift) | (b << map->bShift);
	return True;
    }
    c.color.red = col->red & 0xff00;
    c.color.green = col->green & 0xff00;
    c.color.blue = col->blue & 0xff00;

    if ((node = HashFind(map->htable, HASH(c.color), &c)) != NULL) {
	*pxl = node->color.pixel;
	return True;
    }
    return False;
}

Pixel
PaletteGetUnused(Palette * map)
{
    return map->mine;
}

/*
**  Change the RGB value of a pixel
 */
void 
PaletteSetInvalid(Palette * map, Pixel pix)
{
    Col col, *c;

    if (!map->isMapped || map->readonly_)
	return;

    col.color.pixel = pix;

    if ((c = (Col *) HashFind(map->ltable, HASH_PIXEL(pix), &col)) == NULL)
	return;

    c->invalid = True;
}

/*
**  This will fail if on a read only colormap
 */
Boolean
PaletteSetPixel(Palette * map, Pixel pixel, XColor * xcol)
{
    Col col, *c;

    if (map->readonly_)
	return False;

    if (map->isDefault)
	return False;

    xcol->pixel = pixel;
    xcol->flags = DoRed | DoGreen | DoBlue;
    XStoreColor(map->display, map->cmap, xcol);

    col.color.pixel = pixel;

    if ((c = (Col *) HashFind(map->ltable, HASH_PIXEL(pixel), &col)) != NULL)
	c->invalid = True;

    return True;
}

Palette *
PaletteFindDpy(Display * dpy, Colormap cmap)
{
    PaletteList *cur;

    if (cmap == -1)
	return paletteGetBW();

    for (cur = cmapList; cur != NULL; cur = cur->next)
	if (cur->cmap == cmap && cur->dpy == dpy)
	    return cur->map;

    return NULL;
}

Palette *
PaletteFind(Widget w, Colormap cmap)
{
    return PaletteFindDpy(XtDisplay(w), cmap);
}

static void 
paletteAddUserDestroy(Widget w, Palette * map, XtPointer junk)
{
    paletteUserList *cur = map->userList;
    paletteUserList **prev = &map->userList;

    while (cur != NULL && cur->widget != w) {
	prev = &cur->next;
	cur = cur->next;
    }

    if (cur == NULL)
	return;

    *prev = cur->next;
    XtFree((XtPointer) cur);
}

void 
PaletteDelete(Palette * map)
{
    PaletteList *cur, **prev;
    paletteUserList *ul, *nx;

    HashDestroy(map->htable);
    HashDestroy(map->ltable);
    if (map->ctable != NULL)
	XtFree((XtPointer) map->ctable);

    for (cur = cmapList, prev = &cmapList; cur != NULL && cur->map != map;
	 prev = &cur->next, cur = cur->next);

    if (cur != NULL)
	*prev = cur->next;

    ul = map->userList;
    while (ul != NULL) {
	nx = ul->next;
	XtRemoveCallback(ul->widget, XtNdestroyCallback,
			 (XtCallbackProc) paletteAddUserDestroy,
			 (XtPointer) map);
	XtFree((XtPointer) ul);
	ul = nx;
    }

    XtFree((XtPointer) map);
    XtFree((XtPointer) cur);
}

void 
PaletteAddUser(Palette * map, Widget w)
{
    paletteUserList *n = XtNew(paletteUserList);

    n->widget = w;
    n->next = map->userList;
    map->userList = n;
    XtAddCallback(w, XtNdestroyCallback,
		  (XtCallbackProc) paletteAddUserDestroy, (XtPointer) map);
}
