/* +-------------------------------------------------------------------+ */
/* | Copyright 1993, David Koblas (koblas@netcom.com)                  | */
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

/* $Id: help.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#include <stdio.h>
#include <ctype.h>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>

#include "xaw_incdir/Command.h"
#include "xaw_incdir/Toggle.h"
#include "xaw_incdir/Form.h"
#include "xaw_incdir/Label.h"
#include "xaw_incdir/List.h"
#include "xaw_incdir/AsciiText.h"
#include "xaw_incdir/Text.h"
#ifdef XAW3D
#include "X11/IntrinsicP.h"
#include "xaw_incdir/TextP.h"
#include "xaw_incdir/SmeThreeD.h"
#endif
#include "xaw_incdir/Viewport.h"
#include "xaw_incdir/Scrollbar.h"

#include "xpaint.h"
#include "Paint.h"
#include "misc.h"
#include "protocol.h"

#define TAB_DISTANCE	4

extern String *helpText;
extern int helpNum;

typedef struct HelpInfo_s {
    char *name, *topic;
    int index;
    int start, stop;
    struct HelpInfo_s *next;
} HelpInfo;

typedef struct {
    Widget shell;
    int curTopic;
    Widget topicList, topicPort;
    Widget textText, textTitle;
    Widget nextButton, prevButton;
    Widget scrollbar;
    int ninfo;
    String *topics;
    HelpInfo *info;
} LocalInfo;

/*
**  Build hierarchical help information structures.
 */

/*
 * If the string 'pat' matches the beginning of 'line', skip
 * any following spaces and return the following word.
 */
char *
matchGet(char *line, char *pat)
{
    static char buf[256];
    int len = strlen(pat);
    char *sp, *ep;

    if (strncmp(line, pat, len) != 0)
	return NULL;

    for (sp = line + len; isspace(*sp); sp++);
    for (ep = sp; isalnum(*ep); ep++);
    strncpy(buf, sp, ep - sp);
    buf[ep - sp] = '\0';

    return buf;
}

static HelpInfo *
buildInfo(int *count)
{
    Boolean nc, flg;
    int depth = 0, stop = -1;
    int i, j;
    char parts[10][20];
    HelpInfo *head = NULL, **nxt = &head, *cur = NULL;
    char *cp, *tp = NULL;
    int idx = 0;

    if (count != NULL)
	*count = 0;

    for (i = 0; i < helpNum; i++) {
	flg = nc = False;
	if ((cp = matchGet(helpText[i], "#BEGIN")) != NULL) {
	    int argc;
	    char *argv[128];
	    char buf[256];

	    nc = True;
	    strcpy(buf, helpText[i]);	/* Need R&W buffer */
	    StrToArgv(buf, &argc, argv);
	    strcpy(parts[depth], argv[1]);
	    tp = (argc > 2) ? argv[2] : argv[1];
	} else if ((cp = matchGet(helpText[i], "#PUSH")) != NULL) {
	    depth++;
	    flg = True;
	} else if ((cp = matchGet(helpText[i], "#POP")) != NULL) {
	    if (depth > 0)
		depth--;
	    flg = True;
	} else if ((cp = matchGet(helpText[i], "#NL")) != NULL) {
	    stop = i;
	} else {
	    stop = i;
	}

	if (nc) {		/* saw #BEGIN */
	    int len;

	    if (cur != NULL) {
		cur->stop = stop + 1;
		*nxt = cur;
		nxt = &cur->next;
	    }
	    cur = XtNew(HelpInfo);
	    cur->index = idx++;
	    cur->start = i + 1;
	    cur->next = NULL;

	    if (count != NULL)
		(*count)++;

	    len = 0;
	    for (j = 0; j <= depth; j++)
		len += strlen(parts[j]) + 1;
	    cur->name = (char *) XtMalloc(len + 1);
	    cur->name[0] = '\0';
	    for (j = 0; j <= depth; j++) {
		strcat(cur->name, parts[j]);
		strcat(cur->name, ".");
	    }
	    cur->name[strlen(cur->name) - 1] = '\0';
	    cur->topic = (char *) XtMalloc(strlen(tp) + 7 + 2 * depth);
	    for (j = 0; j < depth * 2; j++)
		cur->topic[j] = ' ';
	    cur->topic[j] = '\0';
	    strcat(cur->topic, tp);
	    strcat(cur->topic, "  ");
	} else if (flg) {
	    if (cur->start == i)
		cur->start++;
	}
    }

    if (cur != NULL) {
	cur->stop = i;
	*nxt = cur;
	nxt = &cur->next;
    }
    return head;
}


static void 
doneCB(Widget w, XtPointer lArg, XtPointer junk)
{
    LocalInfo *l = (LocalInfo *) lArg;

    XtPopdown(l->shell);
}

static char *
buildText(HelpInfo * cur)
{
    int i, len;
    char *txt;
    char *tp, *cp;

    len = 0;
    for (i = cur->start; i < cur->stop; i++)
	len += strlen(helpText[i]) + 2;

    tp = txt = (char *) XtCalloc(len + 8, sizeof(char));
    for (i = cur->start; i < cur->stop; i++) {
	/*
	**  Line of all whitespace is a paragraph break.
	 */
	for (cp = helpText[i]; *cp != '\0'; cp++)
	    if (!isspace(*cp))
		break;
	if (*cp == '\0') {
	    *tp++ = '\n';
	    *tp++ = '\n';
	    continue;
	}
	if (strncmp(helpText[i], "#NL", 3) == 0) {
	    *tp++ = '\n';
	    continue;
	}
	for (cp = helpText[i]; *cp != '\0'; *tp++ = *cp++);
	*tp++ = ' ';
    }
    *tp++ = '\n';
    *tp = '\0';

    return txt;
}

static void 
display(LocalInfo * l, HelpInfo * cur)
{
    char *txt;
    float self, top, shown;

    if (cur == NULL)
	cur = l->info;

    l->curTopic = cur->index;

    txt = buildText(cur);

    XtVaSetValues(l->textText, XtNstring, txt, NULL);
    XtVaSetValues(l->textTitle, XtNlabel, cur->topic, NULL);

    XtFree((XtPointer) txt);

    XtVaSetValues(l->nextButton, XtNsensitive, (cur->next != NULL), NULL);
    XtVaSetValues(l->prevButton, XtNsensitive, (cur->index != 0), NULL);

    XtCallActionProc(l->textText, "beginning-of-file", NULL, NULL, 0);

    /*
    **  Now position the scrollbar to be visible
     */

    XtVaGetValues(l->scrollbar, XtNtopOfThumb, &top,
		  XtNshown, &shown,
		  NULL);
    self = (float) cur->index / (float) l->ninfo;
    if (self < top || self > top + shown) {
	top = self - shown / 2.0;
	if (top < 0.0)
	    top = 0.0;
	if (top + shown > 1.0)
	    top = 1.0 - shown;

	/*
	**  Scrollbar doesn't notify on a SetThumb()
	**    So we must do it
	 */
	XawScrollbarSetThumb(l->scrollbar, top, -1.0);
	XtCallCallbacks(l->scrollbar, XtNjumpProc, (XtPointer) & top);
    }
}

static void 
topicCB(Widget w, LocalInfo * l, XawListReturnStruct * list)
{
    HelpInfo *cur = l->info;
    int i;

    for (i = 0; i < list->list_index; i++)
	cur = cur->next;

    display(l, cur);
}
static void 
downCB(Widget w, LocalInfo * l, XtPointer junk)
{
    XtCallActionProc(l->textText, "next-page", NULL, NULL, 0);
}
static void 
upCB(Widget w, LocalInfo * l, XtPointer junk)
{
    XtCallActionProc(l->textText, "previous-page", NULL, NULL, 0);
}
static void 
prevCB(Widget w, LocalInfo * l, XtPointer junk)
{
    XawListReturnStruct lrs;

    if (l->curTopic == 0)
	return;

    lrs.list_index = l->curTopic - 1;
    XawListHighlight(l->topicList, lrs.list_index);
    topicCB(w, l, &lrs);
}
static void 
nextCB(Widget w, LocalInfo * l, XtPointer junk)
{
    XawListReturnStruct lrs;

    if (l->curTopic == l->ninfo - 1)
	return;

    lrs.list_index = l->curTopic + 1;
    XawListHighlight(l->topicList, lrs.list_index);
    topicCB(w, l, &lrs);
}

static LocalInfo *
buildPopup(LocalInfo * l, Widget parent)
{
    Widget shell, form;
    Widget title;
    Widget topicList, topicPort;
    Widget textTitle, textList;
    Widget done, prev, next;
    Widget pgDown, pgUp;

    if (l == NULL) {
	HelpInfo *cur;
	int i;

	l = XtNew(LocalInfo);
	l->info = buildInfo(&l->ninfo);

	l->topics = (String *) XtCalloc(l->ninfo + 1, sizeof(String));
	for (i = 0, cur = l->info; cur != NULL; cur = cur->next, i++)
	    l->topics[i] = cur->topic;
	l->topics[i] = NULL;
	l->shell = None;
    }
    if (l->shell != None)
	return l;

    shell = XtVaCreatePopupShell("helpDialog",
			         topLevelShellWidgetClass, Global.toplevel,
				 NULL);

    form = XtVaCreateManagedWidget("form",
				   formWidgetClass, shell,
				   XtNborderWidth, 0,
				   NULL);

    title = XtVaCreateManagedWidget("title",
				    labelWidgetClass, form,
				    XtNborderWidth, 0,
				    XtNtop, XtChainTop,
				    XtNbottom, XtChainTop,
				    XtNleft, XtChainLeft,
				    XtNright, XtChainLeft,
				    NULL);

    /*
    **
     */
    topicPort = XtVaCreateManagedWidget("topicPort",
					viewportWidgetClass, form,
					XtNtop, XtChainTop,
					XtNbottom, XtChainBottom,
					XtNleft, XtChainLeft,
					XtNright, XtChainLeft,
					XtNfromVert, title,
					XtNallowVert, True,
					XtNforceBars, True,
					NULL);
    topicList = XtVaCreateManagedWidget("topic",
					listWidgetClass, topicPort,
					XtNverticalList, True,
					XtNforceColumns, True,
					XtNdefaultColumns, 1,
					NULL);

    /*
    **
     */
    textTitle = XtVaCreateManagedWidget("textTitle",
					labelWidgetClass, form,
					XtNborderWidth, 0,
					XtNfromHoriz, topicPort,
					XtNfromVert, title,
					XtNleft, XtChainLeft,
					XtNright, XtChainRight,
					XtNtop, XtChainTop,
					XtNbottom, XtChainTop,
					XtNresize, False,
					NULL);

    textList = XtVaCreateManagedWidget("textText",
				       asciiTextWidgetClass, form,
				       XtNwrap, XawtextWrapWord,
				       XtNtop, XtChainTop,
				       XtNbottom, XtChainBottom,
				       XtNleft, XtChainLeft,
				       XtNright, XtChainRight,
				       XtNfromVert, textTitle,
				       XtNfromHoriz, topicPort,
				       XtNscrollVertical, XawtextScrollAlways,
				       XtNdisplayCaret, False,
				       NULL);
    {
	Widget sink;
	int tabs[10];
	int i;

	for (i = 0; i < XtNumber(tabs); i++)
	    tabs[i] = i * TAB_DISTANCE;

	XtVaGetValues(textList, XtNtextSink, &sink, NULL);

	XawTextSinkSetTabs(sink, XtNumber(tabs), tabs);
    }

    /*
    **
     */
    done = XtVaCreateManagedWidget("done",
				   commandWidgetClass, form,
				   XtNfromVert, textList,
				   XtNtop, XtChainBottom,
				   XtNbottom, XtChainBottom,
				   XtNleft, XtChainLeft,
				   XtNright, XtChainLeft,
				   NULL);
    next = XtVaCreateManagedWidget("next",
				   commandWidgetClass, form,
				   XtNfromVert, textList,
				   XtNfromHoriz, done,
				   XtNtop, XtChainBottom,
				   XtNbottom, XtChainBottom,
				   XtNleft, XtChainLeft,
				   XtNright, XtChainLeft,
				   NULL);
    prev = XtVaCreateManagedWidget("prev",
				   commandWidgetClass, form,
				   XtNfromVert, textList,
				   XtNfromHoriz, next,
				   XtNtop, XtChainBottom,
				   XtNbottom, XtChainBottom,
				   XtNleft, XtChainLeft,
				   XtNright, XtChainLeft,
				   NULL);
    pgDown = XtVaCreateManagedWidget("down",
				     commandWidgetClass, form,
				     XtNfromVert, textList,
				     XtNfromHoriz, topicPort,
				     XtNtop, XtChainBottom,
				     XtNbottom, XtChainBottom,
				     XtNleft, XtChainLeft,
				     XtNright, XtChainLeft,
				     NULL);
    pgUp = XtVaCreateManagedWidget("up",
				   commandWidgetClass, form,
				   XtNfromVert, textList,
				   XtNfromHoriz, pgDown,
				   XtNtop, XtChainBottom,
				   XtNbottom, XtChainBottom,
				   XtNleft, XtChainLeft,
				   XtNright, XtChainLeft,
				   NULL);

    l->shell = shell;
    l->textText = textList;
    l->textTitle = textTitle;
    l->topicPort = topicPort;
    l->topicList = topicList;
    l->nextButton = next;
    l->prevButton = prev;
    l->scrollbar = XtNameToWidget(topicPort, "vertical");

    XawListChange(topicList, l->topics, 0, 0, True);

    XtAddCallback(pgDown, XtNcallback, (XtCallbackProc) downCB, (XtPointer) l);
    XtAddCallback(pgUp, XtNcallback, (XtCallbackProc) upCB, (XtPointer) l);
    XtAddCallback(next, XtNcallback, (XtCallbackProc) nextCB, (XtPointer) l);
    XtAddCallback(prev, XtNcallback, (XtCallbackProc) prevCB, (XtPointer) l);

    XtAddCallback(topicList, XtNcallback,
		  (XtCallbackProc) topicCB, (XtPointer) l);
    XtAddEventHandler(topicList, ButtonPressMask, False,
		      (XtEventHandler) mousewheelScroll, (XtPointer) 1);
    XtAddEventHandler(textList, ButtonPressMask, False,
		      (XtEventHandler) mousewheelScroll, (XtPointer) NULL);

#ifdef XAW3D_OLD
    /* no longer needed - and sometimes segfaults ... */
    {
    Pixel pix;
    XtVaGetValues(form, XtNbackground, &pix, NULL);
    XtVaSetValues(((TextWidget)textList)->text.vbar, XtNbackground,
                  pix, NULL);
    }
#endif   

    XtAddCallback(done, XtNcallback, doneCB, (XtPointer) l);
    AddDestroyCallback(shell, (DestroyCallbackFunc) doneCB, (XtPointer) l);

    return l;
}

void 
HelpDialog(Widget parent, String name)
{
    static LocalInfo *l = NULL;
    int i;
    HelpInfo *cur;
    char *str;

    PopdownMenusGlobal();
    if (!helpText) return;
    l = buildPopup(l, Global.toplevel);

    for (i = 0, cur = l->info; cur != NULL; cur = cur->next, i++)
	if (strcmp(name, cur->name) == 0)
	    break;

    if (!XtIsRealized(l->shell))
	display(l, cur);

    XawListHighlight(l->topicList, cur == NULL ? 0 : i);

    XtPopup(l->shell, XtGrabNone);
    XtVaGetValues(l->shell, XtNtitle, &str, NULL);
    StoreName(l->shell, str);

    display(l, cur);
    RaiseWindow(XtDisplay(l->shell), XtWindow(l->shell));
    XtSetMinSizeHints(l->shell, 466, 160);
}

/*
 * This is only called from usage().
 */
void 
HelpTextOutput(FILE * fd, String name)
{
    char *txt;
    int col, wlen, i;
    char *tp, *wstart = NULL;
    HelpInfo *head, *cur;

    if (!helpText) return;
    head = buildInfo(NULL);

    for (i = 0, cur = head; cur != NULL; cur = cur->next, i++)
	if (strcmp(name, cur->name) == 0)
	    break;

    if (cur == NULL)
	return;

    txt = buildText(cur);

    col = wlen = 0;
    for (tp = txt; *tp != '\0'; tp++) {
	if (isspace(*tp) || *tp == '\n') {
	    for (i = 0; i < wlen; i++, wstart++)
		putc(*wstart, fd);
	    col += wlen;
	    wlen = 0;
	    if (*tp == '\t') {
		do {
		    putc(' ', fd);
		    col++;
		}
		while (col % TAB_DISTANCE != 0);
	    } else if (*tp == '\n') {
		putc(*tp, fd);
		col = 0;
	    } else {
		putc(*tp, fd);
		col++;
	    }

	    if (col > 75) {
		putc('\n', fd);
		col = 0;
	    }
	} else if (wlen != 0) {
	    if (col != 0 && (wlen + col > 75)) {
		putc('\n', fd);
		col = 0;
	    }
	    wlen++;
	} else {
	    wlen = 1;
	    wstart = tp;
	}
    }
    for (i = 0; i < wlen; i++, wstart++)
	putc(*wstart, fd);
    if (col != 0)
	putc('\n', fd);

    XtFree((XtPointer) txt);
}

static void 
helpAction(Widget w, XEvent * event, String * prms, Cardinal * nprms)
{
    if (*nprms != 1) {
	fprintf(stderr, "Help called with wrong number of params\n");
	return;
    }
    HelpDialog(w, prms[0]);
}

void 
HelpInit(Widget top)
{
    static XtActionsRec act =
    {"PaintHelp", (XtActionProc) helpAction};

    XtAppAddActions(XtWidgetToApplicationContext(top), &act, 1);
}
