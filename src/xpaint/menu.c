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

/* $Id: menu.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <stdio.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include "xaw_incdir/MenuButton.h"
#include "xaw_incdir/SimpleMenu.h"
#include "xaw_incdir/SmeBSB.h"
#include "xaw_incdir/SmeLine.h"
#include "xaw_incdir/Form.h"

#include "Paint.h"
#include "menu.h"
#include "misc.h"
#include "xpaint.h"

#include "bitmaps/xbm/checkmark.xbm"
#include "bitmaps/xbm/pullright.xbm"

#define MAX_GROUPS	6

extern int w_edit_paste;

typedef struct group_s {
    int size;
    Widget group[16];
    struct group_s *next;
} GroupList;

typedef struct rightlist_s {
    char * name;
    Widget widget;
    Widget shell;
    GroupList * grouplist;
    struct rightlist_s *next;
} RightList;

static GroupList *groupHead = NULL;
static RightList *rightHead = NULL;
static Pixmap checkBitmap = None, pullBitmap = None;
static int transitional = 0;

static void popupMainAction(Widget, XEvent *, String *, Cardinal *);
static void pullRightAction(Widget, XEvent *, String *, Cardinal *);

static GroupList *
findGroup(Widget w)
{
    GroupList *cur = groupHead;
    int i;

    if (w == None)
	return NULL;

    while (cur != NULL) {
	for (i = 0; i < cur->size; i++)
	  if (cur->group[i] == w)
		return cur;
	cur = cur->next;
    }
    return NULL;
}

static void 
initMenu(Widget w)
{
    static XtActionsRec act[] = {
        {"popup-main-menu", (XtActionProc) popupMainAction},
        {"popup-menu-pullright", (XtActionProc) pullRightAction}
    };

    if (checkBitmap != None)
	return;

    checkBitmap = XCreateBitmapFromData(XtDisplay(w),
					DefaultRootWindow(XtDisplay(w)),
					(char *) checkmark_bits,
				        checkmark_width, checkmark_height);
    pullBitmap = XCreateBitmapFromData(XtDisplay(w),
				       DefaultRootWindow(XtDisplay(w)),
				       (char *) pullright_bits,
				       pullright_width, pullright_height);

    XtAppAddActions(XtWidgetToApplicationContext(w), act, 2);
}

static void 
destroy_string(Widget w, XtPointer data, XtPointer junk)
{
    XtFree((XtPointer) data);
}

static void 
destroy_group(Widget w, XtPointer data, XtPointer junk)
{
    GroupList *c, *cp;

    if (!data) return;

    c = groupHead;
    if (!c) return;

    if (c == (GroupList *)data) {
        groupHead = c->next;
        XtFree((XtPointer) c);
        return;
    }

 iter_group:
    cp = c->next;
    if (!cp) return;
    if (cp == (GroupList *)data) {
         c->next = cp->next;
         XtFree((XtPointer) cp);
         return;
    } else {
         c = cp;
         if (c) goto iter_group;
    }
}

static void 
destroy_right(Widget w, XtPointer data, XtPointer junk)
{
    RightList *c, *cp;

    if (!data) return;

    c = rightHead;
    if (!c) return;

    if (c == (RightList *)data) {
        if (c->name) XtFree((XtPointer) c->name);
        rightHead = c->next;
        XtFree((XtPointer) c);
        return;
    }

 iter_right:
    cp = c->next;
    if (!cp) return;
    if (cp == (RightList *)data) {
         if (cp->name) XtFree((XtPointer) cp->name);
         c->next = cp->next;
         XtFree((XtPointer) cp);
         return;
    } else {
         c = cp;
         if (c) goto iter_right;
    }
}

/*
int
getTransitionalMenu()
{
    return transitional;
}

void
setTransitionalMenu()
{
    transitional = 1;
}
*/

void
PopdownMenusGlobal()
{
    if (Global.popped_up != None) {
        if (XtIsRealized(Global.popped_up))
            XtPopdown(Global.popped_up);
        Global.popped_up = None;
    }
    Global.popped_parent = None;
    transitional = 0;
}

static void
PopdownEscape(Widget w, XEvent * event, String * params, Cardinal * nparams)
{
    transitional = 0;
    if (XtIsRealized(w)) XtPopdown(w);
    if (w != Global.popped_up) PopdownMenusGlobal();
    Global.popped_up = None;
    Global.popped_parent = None;
}

static void
PopdownAll(Widget w, XEvent * event, String * params, Cardinal * nparams)
{
    if (transitional) {
        transitional = 0;
        return;
    }
    if (XtIsRealized(w)) XtPopdown(w);
    if (w != Global.popped_up) PopdownMenusGlobal();
}

/*
static void
Popdown(Widget w, XEvent * event, String * params, Cardinal * nparams)
{
    if (!transitional) {
        XtPopdown(w);
        if (w == Global.popped_up) Global.popped_up = None;
    }
    transitional = 0;
}
*/

static void 
PopdownChild(Widget w, XEvent * event, String * params, Cardinal * nparams)
{
    Position y;

    XtVaGetValues(w, XtNy, &y, NULL);
    y = event->xbutton.y_root-y;

    if (event->type == ButtonRelease && y>0) {
        if (!transitional) {
	    XtPopdown(w);
            if (w == Global.popped_up) Global.popped_up = None;
	}
        transitional = 0;
    }
}

void SetFocusOn(Widget w)
{
  /* No longer useful ? */
  /*
    XWindowAttributes win_attributes;
    if (!w) return;
    if (!XtIsRealized(w)) return;
    XGetWindowAttributes(XtDisplay(w), XtWindow(w), &win_attributes);
    if (win_attributes.map_state==IsViewable)
        XSetInputFocus(XtDisplay(w), XtWindow(w), RevertToPointerRoot, CurrentTime);
  */
}

static void 
HighlightChild(Widget w, XEvent * event, String * params, Cardinal * nparams)
{
    Position x, y;
    Dimension wi, he;

    x = y = 0;
    XtVaGetValues(w, XtNx, &x, XtNy, &y, XtNwidth, &wi, XtNheight, &he, NULL);
    x = event->xbutton.x_root-x;
    y = event->xbutton.y_root-y;

    if (event->type == ButtonPress || event->type == MotionNotify) {
        if (x>=0 && x<=wi && y>=0 && y<=he)
            XtCallActionProc(w, "highlight", event, params, 0);
        else
            XtCallActionProc(w, "unhighlight", event, params, 0);
        SetFocusOn(w);
    }
    SetFocusOn(Global.popped_up);
}

static void 
popupMainAction(Widget w, XEvent * event, String * params, Cardinal * nparams)
{
    String paramseq[] = { "popup-menu" };
    WidgetList wlist;
    Widget editmenu;
    Position x, y;
    Dimension width;
    int zoom;
    Boolean locked;

    if (strcmp(XtName(w), "paint") || !IsMenuBarVisible(w) || IsFullMenuSet(w)) {
      mainpopup:
        transitional = 0;
        XtCallActionProc(w, "MenuPopup", event, paramseq, 1);
        routine = XtName(w);
	return;
    }

    XtVaGetValues(w, XtNlocked, &locked, NULL);
    if (locked) return;
    PopdownMenusGlobal();
    wlist = None;
    XtVaGetValues(w, XtNmenuwidgets, &wlist, NULL);
    if (!wlist || !wlist[0]) goto mainpopup;
    XtVaGetValues(w, XtNzoom, &zoom, NULL);
    if (zoom>0)
        XtTranslateCoords(w, zoom*event->xbutton.x, zoom*event->xbutton.y, &x, &y);
    else
        XtTranslateCoords(w, event->xbutton.x/(-zoom), event->xbutton.y/(-zoom), &x, &y);
    editmenu = GetShell(wlist[w_edit_paste]);
    XtVaGetValues(editmenu, XtNwidth, &width, NULL);
    XtPopup(editmenu, XtGrabNone);
    routine = XtName(editmenu);
    XtMoveWidget(editmenu, x-width/2, y-8);
    XtCallActionProc(editmenu, "unhighlight", event, NULL, 0);
    XFlush(XtDisplay(editmenu));
    SetFocusOn(editmenu);
    Global.popped_up = editmenu;
    transitional = 1;
}

static void 
pullRightAction(Widget w, XEvent * event, String * params, Cardinal * nparams)
{
    Dimension width, height;
    int x, y;
    RightList *cur;
    Widget cw = XawSimpleMenuGetActiveEntry(w);

    if (cw == None) return ;

    if (event->type == MotionNotify) {
        x = event->xmotion.x;
        y = event->xmotion.y;
    } else 
    if(event->type == ButtonPress) {
        x = event->xbutton.x;
        y = event->xbutton.y;
    } else
	return;


    XtVaGetValues(w, XtNwidth, &width, XtNheight, &height, NULL);
    if (x < 0 || x >= width || y < 0 || y >= height)
	return;

    /*
    **  Only the icon part of the menu is sensitive to motion pulls
     */
    if (event->type == MotionNotify && x < width - 28)
	return;

    for (cur = rightHead; cur != NULL && cur->widget != cw; cur = cur->next);

    if (cur == NULL)
	return;

    x = event->xmotion.x_root-16;
    y = event->xmotion.y_root-20;
    XtVaSetValues(cur->shell, XtNx, x, XtNy, y, NULL);

    PopdownMenusGlobal();
    XtPopup(cur->shell, XtGrabNone);
    routine = XtName(cur->shell);
    SetFocusOn(cur->shell);
    Global.popped_up = cur->shell;
    transitional = 1;
}

static void
closeparent(Widget w, XtPointer data, XtPointer junk)
{
    if (w && (w = XtParent(w))) {
        if (XtIsRealized(w) && strstr(XtName(w), "-right")) {
            XtPopdown(w);
            if (w == Global.popped_up) Global.popped_up = None;
	}
        w = XtParent(w);
        if (w && XtIsRealized(w) && strstr(XtName(w), "popup-menu")) {
            XtPopdown(w);
            if (w == Global.popped_up) Global.popped_up = None;
	}
    }
    Global.popped_parent = None;
}

static void 
MenuHandler(Widget w, XtPointer l, XEvent * event, Boolean * flg)
{
    Dimension u, v;
    Position x, y;
    Boolean visible;
    XWindowAttributes win_attributes;
    KeySym keysym;
    char buf[4];
    int len;

    if (!w || !XtIsRealized(w)) return;
    XtVaGetValues(w, XtNwidth, &u, NULL);

    if (event->type==KeyPress || event->type == KeyRelease) {
        len = XLookupString((XKeyEvent *)event, 
                            buf, sizeof(buf) - 1, &keysym, NULL);
        if (keysym == XK_Escape)
            PopdownMenusGlobal();
    }

    if (event->type==ButtonPress) {
        if (Global.popped_up && l!=Global.popped_up)
            XtPopdown(Global.popped_up);
        XtPopup(l, XtGrabNone);
        routine = XtName(l);
        Global.popped_up = l;
        SetFocusOn(l);        
        return;
    }

    if (event->type==LeaveNotify && 
           (event->xcrossing.x<0 ||event->xcrossing.x>=u || 
            event->xcrossing.y<0)) {
        if (XtIsRealized(l)) {
            XGetWindowAttributes(XtDisplay((Widget)l), 
                                 XtWindow((Widget)l), &win_attributes);
            visible = (win_attributes.map_state==IsViewable);
	} else
	    visible = 0;
        if (visible) {
	    if (XtIsRealized(l)) XtPopdown(l);
            Global.popped_parent = XtParent(w);
	}
    }
    if (event->type==EnterNotify) {
        if (Global.popped_up && XtIsRealized(Global.popped_up) &&
            l!=Global.popped_up) {
	    XtPopdown(Global.popped_up);
	    Global.popped_up = None;
	}
        if (Global.popped_parent==XtParent(w)) {
            XtVaGetValues(w, XtNx, &x, XtNy, &y, 
                          XtNwidth, &u, XtNheight, &v, NULL);
            XtMoveWidget(l, event->xcrossing.x_root-event->xcrossing.x, 
                         event->xcrossing.y_root-event->xcrossing.y+v);
            XtPopup(l, XtGrabNone);
            routine = XtName(l);
            SetFocusOn(l);
            Global.popped_up = l;
	}
    }
}

static void 
createItem(Widget parent, PaintMenuItem * item,
	   Widget groups[MAX_GROUPS])
{
    int grp = -1, i;
    Widget entry;
    int nargs = 0;
    Arg args[4];
    RightList *cur;
    String nm;

    if (item->name[0] == '\0') {
	entry = XtVaCreateManagedWidget("separator",
					smeLineObjectClass, parent,
					NULL);
    } else if (item->flags & MF_CHECK) {
	entry = XtVaCreateManagedWidget(item->name,
					smeBSBObjectClass, parent,
				        XtNleftMargin, checkmark_width + 4,
					NULL);
    } else if (item->right != NULL && item->nright != 0) {

	entry = XtVaCreateManagedWidget(item->name,
					smeBSBObjectClass, parent,
					XtNrightMargin, pullright_width+8,
					XtNrightBitmap, pullBitmap,
					NULL);

	nm = (String) XtMalloc(strlen(XtName(parent)) +
			       strlen(item->name) + 16);
	sprintf(nm, "%s-right", item->name);

	item->rightShell = XtVisCreatePopupShell(nm,
						 simpleMenuWidgetClass, parent,
						 args, nargs);

	for (i = 0; i < item->nright; i++)
	    createItem(item->rightShell, &item->right[i], groups);

	cur = XtNew(RightList);
        memset(cur, 0, sizeof(RightList));
	cur->shell = item->rightShell;
	cur->widget = entry;
	cur->next = rightHead;
        cur->name = nm;
	rightHead = cur;
	XtAddCallback(item->rightShell, XtNdestroyCallback,
		      destroy_right, (XtPointer) cur);
    } else {
	entry = XtVaCreateManagedWidget(item->name,
					smeBSBObjectClass, parent,
					NULL);
    }

    if (item->flags & MF_GROUP1)
	grp = 0;
    else if (item->flags & MF_GROUP2)
	grp = 1;
    else if (item->flags & MF_GROUP3)
	grp = 2;
    else if (item->flags & MF_GROUP4)
	grp = 3;
    else if (item->flags & MF_GROUP5)
	grp = 4;

    if (grp != -1) {
	GroupList *c = findGroup(groups[grp]);
	if (c == NULL) {
	    /* XXX GroupList entry leaked */
	    c = XtNew(GroupList);
            memset(c, 0, sizeof(GroupList));
	    c->next = groupHead;
	    groupHead = c;
	    c->size = 0;
	    groups[grp] = entry;
  	    XtAddCallback(entry, XtNdestroyCallback, destroy_group, 
                          (XtPointer)c);
	}
	c->group[c->size++] = entry;
    }
    if ((item->flags & MF_CHECKON) == MF_CHECKON)
	XtVaSetValues(entry, XtNleftBitmap, checkBitmap, NULL);
    if (item->callback != NULL)
	XtAddCallback(entry, XtNcallback,
		      (XtCallbackProc) item->callback, item->data);

    XtAddCallback(entry, XtNcallback,
		      (XtCallbackProc) closeparent, item->data);

    item->widget = entry;
}

Widget
MenuBarCreate(Widget parent, int nbar, PaintMenuBar bar[])
{
    int list, item;
    Widget button = None, menu;
    Widget prevButton = None;
    char menuPopupName[80];
    int i;
    Widget groups[MAX_GROUPS];
    Arg args[4];
    int nargs;
    static XtActionsRec act[4] = {
        {"popdown-all",  (XtActionProc) PopdownAll},
        {"escape",  (XtActionProc) PopdownEscape},
        {"popdown-child",  (XtActionProc) PopdownChild},
        {"highlight-child",  (XtActionProc) HighlightChild}
    };

    initMenu(parent);

    /*
    **  If there is more than one entry in this bar
    **    reparent it.
     */
    if (nbar > 1)
	parent = XtVaCreateManagedWidget("menu",
					 formWidgetClass, parent,
					 XtNborderWidth, 0,
					 XtNbottom, XtChainTop,
					 XtNright, XtChainLeft,
					 XtNleft, XtChainLeft,
					 NULL);

    for (list = 0; list < nbar; list++) {
	char *nm;
	strcpy(menuPopupName, bar[list].name);
	strcat(menuPopupName, "Menu");

	nm = XtNewString(menuPopupName);

	button = XtVaCreateManagedWidget(bar[list].name,
					 menuButtonWidgetClass, parent,
					 XtNmenuName, nm,
					 XtNfromHoriz, prevButton,
#ifdef XAW3D					 
					 XtNhorizDistance, 2,
					 XtNborderWidth, 0,
#else					 
					 XtNhorizDistance, 3,
					 XtNborderWidth, 1,
#endif					 
					 NULL);

	XtAddCallback(button, XtNdestroyCallback, destroy_string, 
                      (XtPointer) nm);

	prevButton = button;

        nargs = 0;
	menu = XtVisCreatePopupShell(menuPopupName,
				     simpleMenuWidgetClass, button,
				     args, nargs);

        XtAddEventHandler(button, 
                      ButtonPressMask | KeyPressMask | KeyReleaseMask |
                      EnterWindowMask | LeaveWindowMask, False,
                      (XtEventHandler) MenuHandler, (XtPointer) menu);
        XtAppAddActions(XtWidgetToApplicationContext(button), act, 4);

	bar[list].widget = menu;

	for (i = 0; i < MAX_GROUPS; i++)
	    groups[i] = None;

	for (item = 0; item < bar[list].nitems; item++)
	    createItem(menu, &bar[list].items[item], groups);
    }

    return (nbar > 1) ? parent : button;
}

Widget
MenuPopupCreate(Widget parent, char *name, int nitems, PaintMenuItem items[])
{
    static XtTranslations trans = None;
    Widget groups[MAX_GROUPS];
    Widget menu;
    Arg args[4]; /* XtVisCreatePopupShell modified args internally */
    int i, nargs = 0;
        
    for (i = 0; i < MAX_GROUPS; i++)
	groups[i] = None;

    initMenu(parent);

    menu = XtVisCreatePopupShell(name,
				 simpleMenuWidgetClass, parent,
				 args, nargs);

    if (trans == None)
	trans = XtParseTranslationTable(
	"<Key>Escape: MenuPopdown(popup-menu)\n"
	"<Btn3Down>: XawPositionSimpleMenu(popup-menu) popup-main-menu()");

    XtOverrideTranslations(parent, trans);

    for (i = 0; i < nitems; i++)
	createItem(menu, &items[i], groups);

    return menu;
}

void 
MenuCheckItem(Widget w, Boolean flag)
{
    GroupList *c;
    int i;

    if (w == None)
	return;

    if ((c = findGroup(w)) != NULL) {
	for (i = 0; i < c->size; i++) {
	    if (c->group[i])
	    XtVaSetValues(c->group[i], XtNleftBitmap, None, NULL);
	}
    }
    XtVaSetValues(w, XtNleftBitmap, flag ? checkBitmap : None, NULL);
}

Boolean
IsItemChecked(Widget w)
{
    Pixmap p;

    if (w == None)
	return False;

    XtVaGetValues(w, XtNleftBitmap, &p, NULL);
    return ( p != None);
}
