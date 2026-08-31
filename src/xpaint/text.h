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

/* $Id: text.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

typedef struct {
    char *title;
    int nprompt;
    struct textPromptInfo {
	char *prompt;		/* displayed prompt */
	int len;		/* maximum string length */
	char *str;		/* displayed initial string */
	char *rstr;		/* returned string value */
	Widget w;		/* internal -- don't use */
    } *prompts;
} TextPromptInfo;

Widget TextPrompt(Widget w, char *name, TextPromptInfo * prompt,
	      XtCallbackProc okProc, XtCallbackProc nokProc, void *data);
void TextPromptPixmap(Widget w, char *name, TextPromptInfo * prompt,
	       XtCallbackProc okProc, XtCallbackProc nokProc, void *data,
		      Pixmap pix);
