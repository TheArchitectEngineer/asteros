/*-----------------------------------------------------------------------------
  Module FmUtils.c

  (c) Simon Marlow 1990-1993
  (c) Albert Graef 1994

  - default values for parameters added in varPopup() by Brian King
    (ender@ee.WPI.EDU), integrated Mar 24 1995, AG

  modified 7-1997 by strauman@sun6hft.ee.tu-berlin.de to add
  different enhancements (see README-NEW).


  General utility functions for creating menus, buttons, questions,
  and functions for desensetising and 'ticking' menu entries.
-----------------------------------------------------------------------------*/

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Box.h>
#ifdef ENHANCE_TXT_FIELD
#include "TextField.h"
#include "FocusForm.h"
#else
#include <X11/Xaw/AsciiText.h>
#endif

#include "Am.h"
#include "Fm.h"

#define PADDING 20
#define TEXT_WIDTH 350

#define kDefaultValueMarker "--" /* Marker to denote default value */
#define kDefaultValue ""         /* Default Value to use if none specified */

/*-----------------------------------------------------------------------------
  STATIC DATA
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
  Widget Argument Lists
-----------------------------------------------------------------------------*/

static Arg shell_args[] = {
  { XtNtitle, (XtArgVal) NULL }
};

static Arg form_args[] = {
  { XtNdefaultDistance, PADDING }
};

static Arg bitmap_args[]  = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNbitmap, (XtArgVal) NULL },
  { XtNtop, (XtArgVal) XtChainTop },
  { XtNbottom, (XtArgVal) XtChainTop },
  { XtNleft, (XtArgVal) XtChainLeft },
  { XtNright, (XtArgVal) XtChainLeft }
};

static Arg label_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNlabel, (XtArgVal) NULL },
  { XtNwidth, (XtArgVal) 0 },
  { XtNfont, (XtArgVal) NULL },
  { XtNjustify, XtJustifyRight },
  { XtNinternalWidth, (XtArgVal) 0 },
  { XtNinternalHeight, (XtArgVal) 0 },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainLeft }
};

static Arg text_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNstring, (XtArgVal) NULL },
  { XtNlength, (XtArgVal) NULL },
  { XtNwidth, (XtArgVal) TEXT_WIDTH },
  { XtNfont, (XtArgVal) NULL },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainRight },
#ifndef ENHANCE_TXT_FIELD
  { XtNeditType, XawtextEdit },
  { XtNtype, XawAsciiString },
#else
  { XtNfocusInterest, True },
#endif
  { XtNuseStringInPlace, (XtArgVal) True },
};

static Arg button_box_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainLeft }
};

static Arg button_args[] = {
  { XtNlabel, (XtArgVal) NULL },
  { XtNfont, (XtArgVal) NULL }
};

static Arg menu_button_args[] = {
  { XtNlabel, (XtArgVal) NULL },
  { XtNfont, (XtArgVal) NULL }
};

static Arg menu_item_args[] = {
  { XtNlabel, (XtArgVal) NULL },
  { XtNfont, (XtArgVal) NULL },
  { XtNleftMargin , (XtArgVal) 0 }
};

/*-----------------------------------------------------------------------------
  PUBLIC FUNCTIONS
-----------------------------------------------------------------------------*/

void initUtils()
{
  button_args[1].value = (XtArgVal) resources.button_font;
  menu_button_args[1].value = (XtArgVal) resources.button_font;
  menu_item_args[1].value = (XtArgVal) resources.menu_font;
  label_args[4].value = (XtArgVal) resources.label_font;
  text_args[5].value = (XtArgVal) resources.cell_font;
}

/*****************************************************************************/
/* Function: createFloatingMenu                                              */
/* Arguments: menu_name   :  The menu widget name                            */
/*            items       :  Items to put in menu                            */
/*            n_items     :  Number of items                                 */
/*            left_margin :  left_margin in pixels (in case ticks are needed */
/*            parent      :  The parent widget to use                        */
/*            client_data :  Client data to be returned by any callback      */
/*            menu_widget :  returns the menu widget                         */
/*                                                                           */
/* Create a popup menu with the specified attributes and place in it the     */
/* specifed items; return the list of item widgets                           */
/*****************************************************************************/

Widget *createFloatingMenu(String menu_name,
			   MenuItemRec *items, Cardinal n_items, 
			   Dimension left_margin, Widget parent, 
			   XtPointer client_data,
			   Widget *menu_widget)
{
  int i;
  Widget *item_widgets;
  
  item_widgets = (Widget *) XtMalloc(n_items * sizeof(Widget));

  *menu_widget = XtCreatePopupShell(menu_name, simpleMenuWidgetClass,
				    parent, NULL, 0 );
    
  menu_item_args[2].value = (XtArgVal) left_margin;

  for (i = 0; i < n_items; i++) {
    if (items[i].callback == NULL)
      XtCreateManagedWidget(items[i].item_name, smeLineObjectClass,
			    *menu_widget, NULL, 0 );
    else {
      menu_item_args[0].value = (XtArgVal) items[i].item_label;
      item_widgets[i] = XtCreateManagedWidget(items[i].item_name,
					      smeBSBObjectClass, *menu_widget,
					      menu_item_args, 
					      XtNumber(menu_item_args));
      XtAddCallback(item_widgets[i], XtNcallback,
		    (XtCallbackProc) items[i].callback, client_data );
    }
  }

  return item_widgets;
}

/*****************************************************************************/
/* Function: createMenu                                                      */
/* Arguments: menu_name   :  The menu widget name                            */
/*            menu_label  :  The label for the menu button                   */
/*            items       :  Items to put in menu                            */
/*            n_items     :  Number of items                                 */
/*            left_margin :  left_margin in pixels (in case ticks are needed */
/*            parent      :  The parent widget to use                        */
/*            client_data :  Client data to be returned by any callback      */
/*                                                                           */
/* Create a menu with the specified attributes and place in it the           */
/* specifed items                                                            */
/*****************************************************************************/

Widget *createMenu(String menu_name, String menu_label, MenuItemList items,
		   Cardinal n_items, Dimension left_margin,
		   Widget parent, XtPointer client_data)
{
  int i;
  Widget menu_widget, button_widget, *item_widgets;
  
  item_widgets = (Widget *) XtMalloc(n_items * sizeof(Widget));

  menu_button_args[0].value = (XtArgVal) menu_label;
  button_widget = XtCreateManagedWidget(menu_name, menuButtonWidgetClass,
    parent, menu_button_args, XtNumber(menu_button_args));
  menu_widget = XtCreatePopupShell( "menu", simpleMenuWidgetClass,
      button_widget, NULL, 0 );
    
  menu_item_args[2].value = (XtArgVal) left_margin;

  for (i = 0; i < n_items; i++) {
    if (items[i].callback == NULL)
      XtCreateManagedWidget(items[i].item_name, smeLineObjectClass,
			    menu_widget, NULL, 0 );
    else {
      menu_item_args[0].value = (XtArgVal) items[i].item_label;
      item_widgets[i] = XtCreateManagedWidget(items[i].item_name,
					      smeBSBObjectClass, menu_widget,
					      menu_item_args, 
					      XtNumber(menu_item_args));
      XtAddCallback(item_widgets[i], XtNcallback, 
		    (XtCallbackProc) items[i].callback, client_data );
    }
  }

  return item_widgets;
}

/*****************************************************************************/
/* Function: createButtons                                                   */
/* Arguments: buttons     :  The list of buttons to create                   */
/*            n_buttons   :  Number of buttons                               */
/*            parent      : The parent widget to use                         */
/*            client data :  Client data returned by all buttons             */
/*                                                                           */
/* Create a set of buttons (usually in a box) with the attributes specified  */
/*****************************************************************************/


Widget *createButtons(ButtonList buttons, Cardinal n_buttons, Widget parent,
		   XtPointer client_data)
{
  int i;
  Widget *button_widgets;
  
  button_widgets = (Widget *) XtMalloc(n_buttons * sizeof(Widget));

  for (i = 0; i < n_buttons; i++) {
    button_args[0].value = (XtArgVal) buttons[i].button_label;
    button_widgets[i] = XtCreateManagedWidget(buttons[i].button_name,
      commandWidgetClass, parent, button_args, XtNumber(button_args));
    XtAddCallback(button_widgets[i], XtNcallback, 
		  (XtCallbackProc) buttons[i].callback, client_data );
  }

  return button_widgets;
}

/*****************************************************************************/
/* Function: createPopupQuestions                                            */
/* Arguments: name        :  The widget name for the shell                   */
/*            title       :  The title of the popup window                   */
/*            bitmap      :  A bitmap to display to the left of the box      */
/*            questions   :  A list of questions to use                      */
/*            n_questions :  Number of questions                             */
/*            buttons     :  A set of buttons to put at the bottom           */
/*            n_buttons   :  Number of buttons                               */
/*            dflt_button :  Default button to use when the popup is closed  */
/*                           (WM_DELETE_WINDOW message)                      */
/*                                                                           */
/* Create a popup questionaire with a bitmap to the left (or none), several  */
/* questions (each consisting of a label and a text area) to the right of    */
/* the bitmap, and a set of buttons underneath all this.                     */
/*****************************************************************************/

Widget createPopupQuestions(String name, String title, Pixmap bitmap,
			    QuestionList questions, Cardinal n_questions,
			    ButtonList buttons, Cardinal n_buttons,
			    Cardinal dflt_button)
{
  int i, l;
  Widget form_widget, box_widget, bitmap_widget = NULL, shell,
    vert = NULL, horiz = NULL;
#ifdef ENHANCE_POP_ACCEL
  Widget *button_widgets;
#ifndef ENHANCE_TRANSLATIONS
  int flag;
#endif
#endif
  /* create popup shell */
  shell_args[0].value = (XtArgVal) title;
  shell = XtCreatePopupShell(name, transientShellWidgetClass, aw.shell,
			     shell_args, XtNumber(shell_args) );

  /* create form */
  form_args[0].value = PADDING;
#ifdef ENHANCE_TXT_FIELD
  form_widget = XtCreateManagedWidget("popup form", focusFormWidgetClass, shell, 
#else
  form_widget = XtCreateManagedWidget("popup form", formWidgetClass, shell, 
#endif
				      form_args, XtNumber(form_args) );

  /* create bitmap */
  if (bitmap != None) {
    bitmap_args[2].value = (XtArgVal) bitmap;
    bitmap_widget = XtCreateManagedWidget("bitmap", labelWidgetClass,
	  form_widget, bitmap_args, XtNumber(bitmap_args));
  }

  label_args[5].value = XtJustifyRight;

  /* Find width of label */
  label_args[3].value = (XtArgVal) 0;
  for (i=0; i<n_questions; i++) {
    l = XTextWidth(resources.label_font, questions[i].label, 
		   strlen(questions[i].label));
    if (l > label_args[3].value)
      label_args[3].value = l;
  }

  for (i = 0; i<n_questions; i++) {
    label_args[0].value = (XtArgVal) bitmap_widget;
    label_args[1].value = (XtArgVal) vert;
    label_args[2].value = (XtArgVal) questions[i].label;
    horiz = XtCreateManagedWidget("label", labelWidgetClass, form_widget,
					 label_args, XtNumber(label_args));
    if (n_questions == 1) {
      text_args[0].value = (XtArgVal) bitmap_widget;
      text_args[1].value = (XtArgVal) horiz;
    }
    else {
      text_args[0].value = (XtArgVal) horiz;
      text_args[1].value = (XtArgVal) vert;
    }      
    text_args[2].value = (XtArgVal) questions[i].value;
    text_args[3].value = (XtArgVal) questions[i].length;
    vert = questions[i].widget = XtCreateManagedWidget("text", 
#ifdef ENHANCE_TXT_FIELD
	   textFieldWidgetClass, form_widget, text_args, XtNumber(text_args));
#else
	   asciiTextWidgetClass, form_widget, text_args, XtNumber(text_args));
#endif
  }

  if (buttons != NULL) {
    button_box_args[0].value = (XtArgVal) NULL;
    button_box_args[1].value = (XtArgVal) vert;
    box_widget = XtCreateManagedWidget("button box", boxWidgetClass, 
	        form_widget, button_box_args, XtNumber(button_box_args));
#ifdef ENHANCE_POP_ACCEL
    button_widgets =
#endif
    createButtons(buttons, n_buttons, box_widget, NULL);

#ifdef ENHANCE_POP_ACCEL
    for (i = 0; i<n_questions; i++) {
    	XtInstallAllAccelerators(questions[i].widget,form_widget);
    }
    XtInstallAllAccelerators(form_widget,form_widget);
    XtInstallAllAccelerators(shell,form_widget);
#ifndef ENHANCE_TRANSLATIONS
    for (i=0;i<n_buttons;i++) {
	/* This allows the user to trigger the buttons
	 * by hitting keys anywhere in the form
	 * provided translations and accelerators are set
	 * correctly in the resource file
	 */
      flag=0;
      if (!strcmp(buttons[i].name,"ok")) flag=1; 
      else if (!strcmp(buttons[i].name,"cancel")) flag=2;
      if (flag) {
	XtVaSetValues(button_widgets[i],
	  XtNaccelerators,
	  (flag==1?
	    "<Key>Return:set()notify()unset()\n <Key>Linefeed:set()notify()unset()\n":
	    "<ClientMessage>WM_PROTOCOLS:set()notify()unset()\n<Key>Escape:set()notify()unset()\n"),
	  NULL);
      }
    }
#endif
#endif
  }

  XtRealizeWidget(shell);
  setWMProps(shell);

  return shell;
}

/*****************************************************************************/
/* Function: fillIn                                                          */
/* Arguments: w : The widget to fill in                                      */
/*                                                                           */
/* sensitize a menu entry                                                    */
/*****************************************************************************/

void fillIn(Widget w)
{
  XtVaSetValues(w, XtNsensitive, (XtArgVal) True, NULL);
}

/*****************************************************************************/
/* Function: grayOut                                                         */
/* Arguments: w : the widget to gray out                                     */
/*                                                                           */
/* desensitises a menu entry                                                 */
/*****************************************************************************/

void grayOut(Widget w)
{
  XtVaSetValues(w, XtNsensitive, (XtArgVal) False, NULL);
}

/*****************************************************************************/
/* Function: tick                                                            */
/* Arguments: w : the widget to tick                                         */
/*                                                                           */
/* place a tick to the left of the specifed menu entry                       */
/*****************************************************************************/

void tick(Widget w)
{
  XtVaSetValues(w, XtNleftBitmap, (XtArgVal) bm[TICK_BM], NULL);
}

/*****************************************************************************/
/* Function: notick                                                          */
/* Arguments: w : the widget                                                 */
/*                                                                           */
/* remove a tick from a menu entry                                           */
/*****************************************************************************/

void noTick(Widget w)
{
  XtVaSetValues(w, XtNleftBitmap, (XtArgVal) bm[NOTICK_BM], NULL);
}

/*****************************************************************************/
/* Function: popupByCursor                                                   */
/* Arguments: shell       :  the shell to popup                              */
/*            grab_kind   :  parameter passed to XtPopup                     */
/*                                                                           */
/* Try to popup a shell by the cursor, make sure it fits on the screen       */
/*****************************************************************************/

void popupByCursor(Widget shell, XtGrabKind grab_kind)
{
  char *geom;
  Display *dpy;
  Screen *scr;
  Window root, child;
  int x, y, x_win, y_win, scr_width, scr_height;
  Dimension width, height;
  unsigned int mask;

  XtVaGetValues(shell, XtNgeometry, &geom, NULL);

  if (!geom || !(strchr(geom, '+') || strchr(geom, '-'))) {
    dpy = XtDisplay(aw.shell);
    scr = XtScreen(aw.shell);
    scr_width = WidthOfScreen(scr);
    scr_height = HeightOfScreen(scr);
  
    XQueryPointer(dpy, DefaultRootWindow(dpy), &root, &child, &x, &y, 
		  &x_win, &y_win, &mask);

    XtVaGetValues(shell, XtNwidth, &width, XtNheight, &height, NULL);

    x -= width/2;
    y -= height/2;

    if (x + width > scr_width)
      x = scr_width - width;
    else if (x < 0)
      x = 0;

    if (y + height > scr_height)
      y = scr_height - height;
    else if (y < 0)
      y = 0;

    XtVaSetValues(shell, XtNx, (XtArgVal) x, XtNy, (XtArgVal) y, NULL);
  }

  XtPopup(shell, grab_kind);
}

/*---------------------------------------------------------------------------*/

void zzz(void)
{
  FileWindowRec *fw;
  Display *dpy = XtDisplay(aw.shell);

  for (fw = file_windows; fw; fw = fw->next)
    XDefineCursor(dpy, XtWindow(fw->viewport), curs[WATCH_CUR]);

  if (resources.appmgr)
    XDefineCursor(dpy, XtWindow(aw.shell), curs[WATCH_CUR]);

  XFlush(dpy);
}

/*---------------------------------------------------------------------------*/

void wakeUp(void)
{
  FileWindowRec *fw;
  Display *dpy = XtDisplay(aw.shell);

  for (fw = file_windows; fw; fw = fw->next)
    XUndefineCursor(dpy, XtWindow(fw->viewport));

  if (resources.appmgr)
    XUndefineCursor(dpy, XtWindow(aw.shell));
}

/*---------------------------------------------------------------------------*/

#define MAXVARSTRINGLEN MAXPATHLEN

static enum { DontKnow, Ok, Cancel } dialog_flag;
static Widget dialog;

static void dialogOkCb(Widget w, XtPointer client_data, XtPointer call_data)
{
  XtPopdown(dialog);
  dialog_flag = Ok;
}

/*---------------------------------------------------------------------------*/

static void dialogCancelCb(Widget w, XtPointer client_data, 
			   XtPointer call_data)
{
  XtPopdown(dialog);
  dialog_flag = Cancel;
}

/*---------------------------------------------------------------------------*/

static ButtonRec dialog_buttons[] = {
  { "ok", "Ok", (FmCallbackProc *) dialogOkCb },
  { "cancel", "Cancel", (FmCallbackProc *) dialogCancelCb }
};

char *varPopup(Pixmap icon_bm, char *action)
{
  static char *act = NULL;
  char *act1 = (char *)alloca(strlen(action)+1), *s, *t;
  char *str = (char *)alloca(strlen(action)+1);
  char **acts = NULL, **vars = NULL;
  int n_acts = 0, n_vars = 0;
  char *def_val;
  char **vals = NULL;

  if (act) XTFREE(act);
  act = NULL;
  strcpy(act1, action);

  for (s = split(act1, '%'); s; s = split(NULL, '%')) {
    acts = (char **)XTREALLOC(acts, (n_acts+1)*sizeof(char *));
    acts[n_acts++] = XtNewString(strparse(str, s, "\\%"));
    if (t = split(NULL, '%')) {
      vars = (char **)XTREALLOC(vars, (n_vars+1)*sizeof(char *));
      vars[n_vars] = XtNewString(strparse(str, t, "\\%"));
      /* Check string for default value character */
      vals = (char **)XTREALLOC(vals, (n_vars+1)*sizeof(char *));
      vals[n_vars] = (char *)XtMalloc(MAXVARSTRINGLEN);
      if ((def_val = strstr(vars[n_vars], kDefaultValueMarker)) == NULL) {
	strcpy(vals[n_vars++], kDefaultValue);
      } else {
	def_val[0] = '\0'; /* Separate label and default value */
	strcpy(vals[n_vars++], def_val + strlen(kDefaultValueMarker));
      }
    } else
      break;
  }

  if (n_vars) {
    QuestionRec *dialog_questions;
    int i, l;
    XEvent e;

    dialog_questions = (QuestionRec *)XtMalloc(n_vars*sizeof(QuestionRec));
    for (i = 0; i < n_vars; i++) {
      dialog_questions[i].label = vars[i];
      dialog_questions[i].value = vals[i];
      dialog_questions[i].length = MAXVARSTRINGLEN;
      dialog_questions[i].widget = NULL;
    }

    dialog = createPopupQuestions("dialog", "Parameter Dialog", icon_bm,
				  dialog_questions, n_vars, dialog_buttons,
				  XtNumber(dialog_buttons),
				  XtNumber(dialog_buttons)-1);
    popupByCursor(dialog, XtGrabExclusive);

    dialog_flag = DontKnow;

    do {
      XtAppNextEvent(app_context, &e);
      XtDispatchEvent(&e);
    } while (dialog_flag == DontKnow);

    if (dialog_flag == Ok)
      for (l = i = 0; i < n_acts; i++) {
	int l1 = strlen(acts[i]), l2 = i<n_vars?strlen(vals[i]):0;
	act = (char *)XTREALLOC(act, l+l1+l2+1);
	strcpy(act+l, acts[i]);
	if (l2) strcpy(act+l+l1, vals[i]);
	l += l1+l2;
      }

    XTFREE(dialog_questions);
    for (i = 0; i < n_acts; i++)
      XTFREE(acts[i]);
    for (i = 0; i < n_vars; i++)
      XTFREE(vars[i]);
    for (i = 0; i < n_vars; i++)
      XTFREE(vals[i]);
    XTFREE(acts); XTFREE(vars); XTFREE(vals);
    XtDestroyWidget(dialog);
    return act;
  } else {
    if (n_acts) XTFREE(acts);
    return action;
  }
}

/*---------------------------------------------------------------------------*/

/* variant of createPopupQuestions which shows some lines of left-justified
   text below a pixmap */

static Widget createPopupText(String name, String title, Pixmap bitmap,
			      String *lines, Cardinal n_lines,
			      ButtonList buttons, Cardinal n_buttons,
			      Cardinal dflt_button)
{
  int i, l;
  Widget form_widget, box_widget, bitmap_widget = NULL, shell,
    vert = NULL;
#ifdef ENHANCE_POP_ACCEL
  Widget *button_widgets;
#ifndef ENHANCE_TRANSLATIONS
  int flag;
#endif
#endif
  /* create popup shell */
  shell_args[0].value = (XtArgVal) title;
  shell = XtCreatePopupShell(name, transientShellWidgetClass, aw.shell,
			     shell_args, XtNumber(shell_args) );

  /* create form */
  form_args[0].value = 5;
#ifdef ENHANCE_TXT_FIELD
  form_widget = XtCreateManagedWidget("popup form", focusFormWidgetClass, shell, 
#else
  form_widget = XtCreateManagedWidget("popup form", formWidgetClass, shell, 
#endif
				      form_args, XtNumber(form_args) );
  /* create bitmap */
  if (bitmap != None) {
    bitmap_args[2].value = (XtArgVal) bitmap;
    vert = bitmap_widget = XtCreateManagedWidget("bitmap", labelWidgetClass,
	  form_widget, bitmap_args, XtNumber(bitmap_args));
  }

  label_args[5].value = XtJustifyLeft;

  /* Find width of label */
  label_args[3].value = (XtArgVal) 0;
  for (i=0; i<n_lines; i++) {
    l = XTextWidth(resources.label_font, lines[i], 
		   strlen(lines[i]));
    if (l > label_args[3].value)
      label_args[3].value = l;
  }

  for (i = 0; i<n_lines; i++) {
    label_args[0].value = (XtArgVal) NULL;
    label_args[1].value = (XtArgVal) vert;
    label_args[2].value = (XtArgVal) lines[i];
    vert = XtCreateManagedWidget("label", labelWidgetClass, form_widget,
				 label_args, XtNumber(label_args));
  }

  if (buttons != NULL) {
    button_box_args[0].value = (XtArgVal) NULL;
    button_box_args[1].value = (XtArgVal) vert;
    box_widget = XtCreateManagedWidget("button box", boxWidgetClass, 
	        form_widget, button_box_args, XtNumber(button_box_args));
#ifdef ENHANCE_POP_ACCEL
    button_widgets =
#endif
    createButtons(buttons, n_buttons, box_widget, NULL);

#ifdef ENHANCE_POP_ACCEL
    XtInstallAllAccelerators(form_widget,form_widget);
    XtInstallAllAccelerators(shell,form_widget);
#ifndef ENHANCE_TRANSLATIONS
    for (i=0;i<n_buttons;i++) {
	/* This allows the user to trigger the buttons
	 * by hitting keys anywhere in the form
	 * provided translations and accelerators are set
	 * correctly in the resource file
	 */
      flag=0;
      if (!strcmp(buttons[i].name,"ok")) flag=1; 
      else if (!strcmp(buttons[i].name,"cancel")) flag=2;
      if (flag) {
	XtVaSetValues(button_widgets[i],
	  XtNaccelerators,
	  (flag==1?
	    "<Key>Return:set()notify()unset()\n <Key>Linefeed:set()notify()unset()\n":
	    "<Key>Escape:set()notify()unset()\n"),
	  NULL);
      }
    }
#endif
#endif
  }

  XtRealizeWidget(shell);
  setWMProps(shell);

  return shell;
}

/*---------------------------------------------------------------------------*/

static Widget about = None;
static Pixmap logo_bm;

static void aboutOkCb(Widget w, XtPointer client_data, 
		      XtPointer call_data)
{
  XtPopdown(about);
}

/*---------------------------------------------------------------------------*/

static ButtonRec about_buttons[] = {
  { "ok", "Ok", (FmCallbackProc *) aboutOkCb },
};

static char ver_str[250];
static String about_text[] = {
  ver_str,
  "Written by Simon Marlow, Albert Graef, Till Straumann, et al",
  "Copying policy: GPL Version 2 or later",
};

#include "FmVersion.h"

void aboutPopup(void)
{
  if (about == None) {
    sprintf(ver_str, "This is xfm version %s.%s", XFMVERSION,XFMMINORVERSION);
    logo_bm = readIcon("XfmLogo.xpm", 0, 0);
    about = createPopupText("about", "About xfm", logo_bm,
			    about_text, XtNumber(about_text), about_buttons,
			    XtNumber(about_buttons),
			    XtNumber(about_buttons)-1);
  }
  if (!XtIsManaged(about))
    popupByCursor(about, XtGrabNone);
}

/* set the neccessary WM properties */
void setWMProps(Widget shell)
{
if (!XtIsShell(shell) || !XtIsRealized(shell)) {
	String msg=XtMalloc(100);
	sprintf(msg,"setWMProps(%s): need a realized shell",XtName(shell));
	abortXfm(msg);
	XtFree(msg); /* not really needed */
}
if (!XtIsApplicationShell(shell)) {
	char *empty="";
	XSetWMProtocols(XtDisplay(shell), XtWindow(shell), &wm_delete_window, 1);
	/* clear the WM_COMMAND property; work around some kwm problems */
	XSetCommand(XtDisplay(shell),XtWindow(shell),&empty,1);
} else {
	Atom atoms[2];
	/* set the SAVE_YOURSELF protocol as well */
	atoms[0]=wm_delete_window; atoms[1]=wm_save_yourself;
	XSetWMProtocols(XtDisplay(shell), XtWindow(shell), atoms, XtNumber(atoms));
	/* is the KWM_SAVE_YOURSELF atom known ? */
	if (None != kwm_save_yourself) {
		long val=0x1;
		/* seems that kwm is running; if we set KWM_SAVE_YOURSELF to 0x1, 
		 * our session will also be restored if we ran `xfm -filemgr' :-)
		 */
		XChangeProperty(XtDisplay(shell),XtWindow(shell),
						kwm_save_yourself, kwm_save_yourself,
						32,
						PropModeReplace,
						(unsigned char *)&val,1);
	}
}
}
