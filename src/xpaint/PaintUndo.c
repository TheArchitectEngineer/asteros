/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)            | */
/* | Copyright 1995, 1996 Torsten Martinsen (bullestock@dk-online.dk)  | */
/* |                                                                   | */
/* | Permission to use, copy, modify, and to distribute this software  | */
/* | and its documentation for any purpose is hereby granted without   | */
/* | fee, provided that the above copyright notice appear in all       | */
/* | copies and that both that copyright notice and this permission    | */
/* | notice appear in supporting documentation.  There is no           | */
/* | representations about the suitability of this software for        | */
/* | any purpose.  This software is provided "as is" without express   | */
/* | or implied warranty.                                              | */
/* |                                                                   | */
/* +-------------------------------------------------------------------+ */

/* $Id: PaintUndo.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <X11/IntrinsicP.h>
#include <stdlib.h>
#include "xpaint.h"
#include "misc.h"
#include "PaintP.h"

/* #define DEBUG */

#define	GET_PW(w)	((PaintWidget)(((PaintWidget)w)->paint.paint == None ? \
				       w : ((PaintWidget)w)->paint.paint))

static AlphaPixmap NULLAP = { None, NULL};

static void UndoPush(PaintWidget pw, AlphaPixmap p);
static void RedoClear(PaintWidget pw);
static void RedoPush(PaintWidget pw, AlphaPixmap p);
static AlphaPixmap UndoPop(PaintWidget pw);
static AlphaPixmap RedoPop(PaintWidget pw);


void
UndoInitialize(PaintWidget pw, int n)
{
    int i;
    pw->paint.nlevels = n;
    pw->paint.undostack = n ? xmalloc(n*sizeof(AlphaPixmap)) : NULL;
    pw->paint.redostack = n ? xmalloc(n*sizeof(AlphaPixmap)) : NULL;
    pw->paint.undobot = pw->paint.undoitems = 0;
    pw->paint.redobot = pw->paint.redoitems = 0;
    for (i=0; i<n; i++) {
        pw->paint.undostack[i].pixmap = None;
        pw->paint.undostack[i].alpha = NULL;
    }
}

static void
RedoClear(PaintWidget pw)
{
    AlphaPixmap p;
    
    do {
        p = RedoPop(pw);
    } while (p.pixmap != None);
}

static void
UndoPush(PaintWidget pw, AlphaPixmap p)
{
    UndoStack * s, * h;

    if (pw->paint.nlevels == 0)
	return;

#ifdef DEBUG    
    printf("push : %d %d\n", p.pixmap, p.alpha);
#endif

    h = pw->paint.head;
    s = h;
    do {
	if (s->alphapix.pixmap == p.pixmap) {
	    s->pushed = True;
	    pw->paint.undo = s;
	    break;
	}
	s = s->next;
    } while (s != h);
    if (pw->paint.undoitems < pw->paint.nlevels)
	pw->paint.undostack[pw->paint.undoitems++] = p;
    else {
	int bot = pw->paint.undobot;
	AlphaPixmap bp;

	bp = pw->paint.undostack[bot];
	s = h;
	do {
	    if (s->alphapix.pixmap == bp.pixmap) {
		s->pushed = False;
		break;
	    }
	    s = s->next;
	} while (s != h);
	pw->paint.undostack[bot++] = p;
	pw->paint.undobot = bot % pw->paint.nlevels;
    }
}

static AlphaPixmap
UndoPop(PaintWidget pw)
{
    AlphaPixmap p;
    UndoStack * s, * h;
    
    if (pw->paint.undoitems == 0)
        return NULLAP;
    --pw->paint.undoitems;
    p = pw->paint.undostack[(pw->paint.undobot + pw->paint.undoitems)
			   % pw->paint.nlevels];
    h = pw->paint.head;
    s = h;
    do {
	if (s->alphapix.pixmap == p.pixmap) {
	    s->pushed = False;
	    break;
	}
	s = s->next;
    } while (s != h);
    return p;
}

static void
RedoPush(PaintWidget pw, AlphaPixmap p)
{
    UndoStack * s, * h;

    if (pw->paint.nlevels == 0)
	return;
    h = pw->paint.head;
    s = h;
    do {
	if (s->alphapix.pixmap == p.pixmap) {
	    s->pushed = True;
	    break;
	}
	s = s->next;
    } while (s != h);
    if (pw->paint.redoitems < pw->paint.nlevels)
	pw->paint.redostack[pw->paint.redoitems++] = p;
    else {
	int bot = pw->paint.redobot;
	AlphaPixmap bp;

	bp = pw->paint.redostack[bot];
	s = h;
	do {
	    if (s->alphapix.pixmap == bp.pixmap) {
		s->pushed = False;
		break;
	    }
	    s = s->next;
	} while (s != h);
	
	pw->paint.redostack[bot++] = p;
	pw->paint.redobot = bot % pw->paint.nlevels;
    }
}

static AlphaPixmap
RedoPop(PaintWidget pw)
{
    AlphaPixmap p;
    UndoStack * s, * h;
    
    if (pw->paint.redoitems == 0)
	return NULLAP;
    --pw->paint.redoitems;
    p = pw->paint.redostack[(pw->paint.redobot + pw->paint.redoitems)
			   % pw->paint.nlevels];
    h = pw->paint.head;
    s = h;
    do {
	if (s->alphapix.pixmap == p.pixmap) {
	    s->pushed = False;
	    break;
	}
	s = s->next;
    } while (s != h);
    return p;
}

void 
Undo(Widget w)
{
    PaintWidget pw = GET_PW(w);
    AlphaPixmap popped;

    popped = UndoPop(pw);
    if (popped.pixmap == None) {
#ifdef ERRORBEEP
	XBell(XtDisplay(w), 100);
#endif
	return;
    }
    RedoPush(pw, pw->paint.current);
    pw->paint.current = popped;
#ifdef DEBUG
    printf("undo : %d %d\n", popped.pixmap, popped.alpha);
#endif
    PwUpdate(w, NULL, True);
}

void 
Redo(Widget w)
{
    PaintWidget pw = GET_PW(w);
    AlphaPixmap popped;

    popped = RedoPop(pw);
    if (popped.pixmap == None) {
#ifdef ERRORBEEP
	XBell(XtDisplay(w), 100);
#endif
	return;
    }
    UndoPush(pw, pw->paint.current);
    pw->paint.current = popped;
    PwUpdate(w, NULL, True);
}

/*
 * Called when starting a drawing operation.
 */
AlphaPixmap
PwUndoStart(Widget wid, XRectangle * rect)
{
    PaintWidget pw = GET_PW(wid);
    UndoStack * s;
    AlphaPixmap cur;
    int x, y, w, h, m;

    pw->paint.dirty = True;

    cur.pixmap = GET_PIXMAP(pw);
    cur.alpha = pw->paint.current.alpha;

    UndoPush(pw, cur);
    RedoClear(pw);
    
    if (rect) {
	x = rect->x;
	y = rect->y;
	w = rect->width;
	h = rect->height;
    } else {
	x = y = 0;
	w = pw->paint.drawWidth;
	h = pw->paint.drawHeight;
    }

    for (s = pw->paint.head; s->next != pw->paint.head; s = s->next) {
	if (s->pushed == False)
	    break;
    }
	
    s->box.x = x;
    s->box.y = y;
    s->box.width = w;
    s->box.height = h;

    XCopyArea(XtDisplay(pw), cur.pixmap, s->alphapix.pixmap,
        pw->paint.gc, 0, 0, pw->paint.drawWidth, pw->paint.drawHeight, 0, 0);

    if (cur.alpha) {
        m = pw->paint.drawWidth * pw->paint.drawHeight;
        if (!s->alphapix.alpha)
	    s->alphapix.alpha = (unsigned char *)XtMalloc(m);
        memcpy(s->alphapix.alpha, cur.alpha, m);
    }

    pw->paint.current = s->alphapix;
    return s->alphapix;
}


void 
UndoStart(Widget w, OpInfo * info)
{
    AlphaPixmap start;
    if (info->surface != opPixmap)
	return;

    start = PwUndoStart(w, NULL);
    info->drawable = start.pixmap;
}


void 
UndoStartPoint(Widget w, OpInfo * info, int x, int y)
{
    PaintWidget pw = GET_PW(w);

    if (info->surface != opPixmap)
	return;

    UndoStart(w, info);

    if (pw->paint.undo == NULL)
	return;

    pw->paint.undo->box.x = x - pw->paint.lineWidth;
    pw->paint.undo->box.y = y - pw->paint.lineWidth;
    pw->paint.undo->box.width = 1 + pw->paint.lineWidth * 2;
    pw->paint.undo->box.height = 1 + pw->paint.lineWidth * 2;
}

void 
UndoGrow(Widget w, int x, int y)
{
    PaintWidget pw = GET_PW(w);
    XRectangle *rect;
    int dx, dy;

#ifdef DEBUG
    printf("point %d %d\n", x, y);
#endif

    if (pw->paint.undo == NULL)
	return;

    rect = &pw->paint.undo->box;

    rect->x += pw->paint.lineWidth;
    rect->y += pw->paint.lineWidth;
    rect->width -= pw->paint.lineWidth * 2 + 1;
    rect->height -= pw->paint.lineWidth * 2 + 1;

    dx = x - rect->x;
    dy = y - rect->y;

    if (dx > 0) {
	rect->width = MAX(rect->width, dx);
    } else {
	rect->width = (int) rect->width - dx;
	rect->x = x;
    }

    if (dy > 0) {
	rect->height = MAX(rect->height, dy);
    } else {
	rect->height = (int) rect->height - dy;
	rect->y = y;
    }

    rect->x -= pw->paint.lineWidth;
    rect->y -= pw->paint.lineWidth;
    rect->width += pw->paint.lineWidth * 2 + 1;
    rect->height += pw->paint.lineWidth * 2 + 1;

#ifdef DEBUG
    printf("undogrow (%d) %d %d [%d %d]\n",
           rect, rect->x, rect->y, rect->width, rect->height);
#endif
}

void 
PwUndoSetRectangle(Widget w, XRectangle * rect)
{
    PaintWidget pw = GET_PW(w);

    if (pw->paint.undo == NULL)
	return;

    pw->paint.undo->box = *rect;
}
void 
PwUndoAddRectangle(Widget w, XRectangle * rect)
{
    PaintWidget pw = GET_PW(w);

    if (pw->paint.undo == NULL)
	return;

    pw->paint.undo->box = *RectUnion(rect, &pw->paint.undo->box);
}

void 
UndoSetRectangle(Widget w, XRectangle * rect)
{
    PaintWidget pw = GET_PW(w);

    if (pw->paint.undo == NULL)
	return;

    PwUndoSetRectangle(w, rect);

    pw->paint.undo->box.x -= pw->paint.lineWidth;
    pw->paint.undo->box.y -= pw->paint.lineWidth;
    pw->paint.undo->box.width += pw->paint.lineWidth * 2 + 1;
    pw->paint.undo->box.height += pw->paint.lineWidth * 2 + 1;
}

void 
UndoStartRectangle(Widget w, OpInfo * info, XRectangle * rect)
{
    UndoStart(w, info);
    UndoSetRectangle(w, rect);
}
