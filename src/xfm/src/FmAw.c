/*-----------------------------------------------------------------------------
  FmAw.c
  
  (c) Simon Marlow 1990-1993
  (c) Albert Graef 1994

  modified 7-1997 by strauman@sun6hft.ee.tu-berlin.de to add
  different enhancements (see README-NEW).

  Functions & data for creating & maintaining  the application window
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <time.h>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Label.h>

#include "Am.h"

#define APPWIDTH 300

#ifdef ENHANCE_TRANSLATIONS
#undef XtNtranslations
#define XtNtranslations ""
#endif

/*-----------------------------------------------------------------------------
  PUBLIC DATA 
-----------------------------------------------------------------------------*/
AppWindowRec aw;
Widget *aw_button_items,
app_popup_widget, *app_popup_items, app_popup_widget1;
int n_appst = 0;
char **appst = NULL;


/*-----------------------------------------------------------------------------
  STATIC DATA 
-----------------------------------------------------------------------------*/

static MenuItemRec app_popup_menu[] = {
  { "install app", "Install...", appInstallAppCb },
  { "install group", "Install group...", appInstallGroupCb },
  { "line1", NULL, NULL },
  { "cut", "Cut", appCutCb },
  { "copy", "Copy", appCopyCb },
  { "paste", "Paste", appPasteCb },
  { "line2", NULL, NULL },
  { "delete", "Delete", appRemoveCb },
  { "line3", NULL, NULL },
  { "select all", "Select all", appSelectAllCb },
  { "deselect all", "Deselect all", appDeselectCb },
  { "line4", NULL, NULL },
  { "about", "About xfm...", aboutCb },
  { "line5", NULL, NULL },
  { "quit", "Quit", appCloseCb },
};

static MenuItemRec app_popup_menu1[] = {
  { "edit", "Edit...", appEditCb },
  { "line1", NULL, NULL },
  { "cut", "Cut", appCutCb },
  { "copy", "Copy", appCopyCb },
  { "line2", NULL, NULL },
  { "delete", "Delete", appRemoveCb },
};

/*-----------------------------------------------------------------------------
  Widget Argument lists
-----------------------------------------------------------------------------*/

static Arg form_args[] = {
  { XtNdefaultDistance, 0 }
};

static Arg viewport_args[] = {
  { XtNfromVert, (XtArgVal) NULL },
  { XtNwidth, APPWIDTH },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainBottom },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainRight },
  { XtNallowVert, (XtArgVal) True },
};

static Arg button_box_args[] = {
  { XtNfromVert, (XtArgVal) NULL },
  { XtNtop, XtChainBottom },
  { XtNbottom, XtChainBottom },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainLeft },
};

static Arg icon_box_args[] = {
  { XtNwidth, 0 },
  { XtNtranslations, (XtArgVal) NULL }
};

static Arg icon_form_args[] = {
  { XtNdefaultDistance, 0 },
  { XtNwidth, 0 }
};

static Arg icon_toggle_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNbitmap, (XtArgVal) NULL },
  { XtNtranslations, (XtArgVal) NULL },
  { XtNwidth, 0 },
  { XtNheight, 0 },
  { XtNforeground, (XtArgVal) 0 },
};

static Arg icon_label_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNlabel, (XtArgVal) NULL },
  { XtNfont, (XtArgVal) NULL },
  { XtNwidth, 0 },
  { XtNinternalWidth, 0 },
  { XtNinternalHeight, 0 }
};

/*-----------------------------------------------------------------------------
  Translation tables
-----------------------------------------------------------------------------*/

#ifndef ENHANCE_TRANSLATIONS

static char app_translations[] = "\
    <Enter>             : appMaybeHighlight()\n\
    <Leave>             : unhighlight()\n\
    <Btn1Up>(2)         : runApp()\n\
    <Btn1Down>,<Btn1Up> : appSelect()\n\
    <Btn1Down>,<Leave>  : appBeginDrag(1,move)\n\
    <Btn2Down>,<Btn2Up> : appToggle()\n\
    <Btn2Down>,<Leave>  : appBeginDrag(2,copy)\n";

static char iconbox_translations[] = "\
    <Btn2Up>            : dummy()\n\
    <Btn3Up>            : dummy()\n\
    <Btn3Down>          : appPopup()\n";

#endif

/*-----------------------------------------------------------------------------
  Action tables
-----------------------------------------------------------------------------*/
static void CatchEntryLeave2(Widget,XtPointer,XEvent*,Boolean *ctd);

static void dummy(Widget w, XEvent *event, String *params, 
		       Cardinal *num_params) {}

static XtActionsRec app_actions[] = {
  { "appMaybeHighlight", appMaybeHighlight },
  { "runApp", runApp },
  { "appSelect", appSelect },
  { "appToggle", appToggle },
  { "appPopup", appPopup },
  { "appBeginDrag", appBeginDrag },
  { "dummy", dummy }
};

/*-----------------------------------------------------------------------------
  Button Data
-----------------------------------------------------------------------------*/

static ButtonRec aw_buttons[] = {
  { "back", "Back", (FmCallbackProc *) appBackCb },
  { "main", "Main", (FmCallbackProc *) appMainCb },
  { "reload", "Reload", (FmCallbackProc *) appLoadCb },
  { "open", "File window", appOpenCb },
};


/*-----------------------------------------------------------------------------
  PRIVATE FUNCTIONS
-----------------------------------------------------------------------------*/

static int longestName()
{
  int i, l, longest = 0;

  for (i=0; i<aw.n_apps; i++)
    if ((l = XTextWidth(resources.icon_font, aw.apps[i].name, 
			strlen(aw.apps[i].name))) > longest)
      longest = l;
  return longest;
}


/*-----------------------------------------------------------------------------
  PUBLIC FUNCTIONS
-----------------------------------------------------------------------------*/

int parseApp(FILE *fp, char **name, char **directory, char **fname,
	     char **icon, char **push_action, char **drop_action)
{
  static char s[MAXAPPSTRINGLEN];
  int l;

 start:
  if (feof(fp)||!fgets(s, MAXAPPSTRINGLEN, fp))
    return 0;
  l = strlen(s);
  if (s[l-1] == '\n')
    s[--l] = '\0';
  if (!l || *s == '#')
    goto start;
  if (!(*name = split(s, ':')))
    return -1;
  if (!(*directory = split(NULL, ':')))
    return -1;
  if (!(*fname = split(NULL, ':')))
    return -1;
  if (!(*icon = split(NULL, ':')))
    return -1;
  if (!(*push_action = split(NULL, ':')))
    return -1;
  if (!(*drop_action = split(NULL, ':')))
    return -1;
  return l;
}

/*---------------------------------------------------------------------------*/

/* determine the default icon of an application */

Pixmap defaultIcon(char *name, char *directory, char *fname)
{
  if (!*fname)
    return bm[EXEC_BM];
  else {
    struct stat stats;
    Boolean sym_link;
    int l = *directory?strlen(directory):strlen(user.home);
    char *path = (char *)alloca(2+strlen(fname)+l);
    
    strcpy(path, *directory?directory:user.home);

    if (l) {
      if (path[l-1] != '/')
	path[l++] = '/';
      strcpy(path+l, fname);
    } else
      strcpy(path, fname);

    if (lstat(path, &stats))
      return bm[FILE_BM];
    else if (S_ISLNK(stats.st_mode)) {
      sym_link = True;
      stat(path, &stats);
    } else
      sym_link = False;

    if (S_ISLNK(stats.st_mode))
      return bm[BLACKHOLE_BM];
    else if (S_ISDIR(stats.st_mode))
      if (sym_link)
	return bm[DIRLNK_BM];
      else if (!strcmp(name, ".."))
	return bm[UPDIR_BM];
      else
	return bm[DIR_BM];
    else if (stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
      if (sym_link)
	return bm[EXECLNK_BM];
      else
	return bm[EXEC_BM];
    else if (sym_link)
      return bm[SYMLNK_BM];
    else
      return bm[FILE_BM];
  }
}

/*-------------------------------------------------------------------------*/

void createApplicationWindow()
{
#ifndef ENHANCE_TRANSLATIONS
  XtTranslations t;
#endif

  /* Add new actions and parse the translation tables */
  XtAppAddActions(app_context, app_actions, XtNumber(app_actions));

#ifndef ENHANCE_TRANSLATIONS
  t = XtParseTranslationTable(app_translations);
  icon_toggle_args[3].value = (XtArgVal) t;

  t = XtParseTranslationTable(iconbox_translations);
  icon_box_args[1].value = (XtArgVal) t;
#endif

  icon_label_args[3].value = (XtArgVal) resources.icon_font;

  /* create the install popups */
  createInstallPopups();

  /* create the menus */
  app_popup_items = createFloatingMenu("app popup", 
				       app_popup_menu,
				       XtNumber(app_popup_menu),
				       4, aw.shell,
				       NULL, &app_popup_widget);
  createFloatingMenu("app popup 1", app_popup_menu1,
		     XtNumber(app_popup_menu1), 4, aw.shell,
		     NULL, &app_popup_widget1);

  XtRegisterGrabAction(appPopup, True, ButtonPressMask | ButtonReleaseMask,
		       GrabModeAsync, GrabModeAsync);

  /* create the form */
#ifndef ENHANCE_TRANSLATIONS
  aw.form = XtCreateManagedWidget("form", formWidgetClass, aw.shell,
#else
  aw.form = XtCreateManagedWidget("awform", formWidgetClass, aw.shell,
#endif
				  form_args, XtNumber(form_args) );
  
  /* create the viewport */
  aw.viewport = XtCreateManagedWidget("viewport", viewportWidgetClass,
    aw.form, viewport_args, XtNumber(viewport_args) );

  /* create button box */
  button_box_args[0].value = (XtArgVal) aw.viewport;
  aw.button_box = XtCreateManagedWidget("button box", boxWidgetClass,
					aw.form, button_box_args, 
					XtNumber(button_box_args) );
  aw_button_items = createButtons(aw_buttons, XtNumber(aw_buttons),
				  aw.button_box, NULL);

  aw.n_selections = 0;
}

/*---------------------------------------------------------------------------*/

void createApplicationDisplay()
{
  int i;
  Dimension width;

  for (i=0; i<aw.n_apps; i++)
    aw.apps[i].selected = False;
  aw.n_selections = 0;
  
  XtVaGetValues(aw.viewport, XtNwidth, &width, NULL);
  icon_box_args[0].value = (XtArgVal) width;

  aw.icon_box = XtCreateWidget("icon box", boxWidgetClass,
    aw.viewport, icon_box_args, XtNumber(icon_box_args) );

  if (aw.n_apps == 0)
    XtVaCreateManagedWidget("label", labelWidgetClass, aw.icon_box,
			    XtNlabel, "No configured applications",
			    XtNfont, resources.label_font, NULL);
  else {
    width = longestName();
    if (width < resources.app_icon_width)
      width = resources.app_icon_width;
    icon_form_args[1].value = (XtArgVal) width;
    icon_label_args[4].value = (XtArgVal) width;
    icon_toggle_args[4].value = (XtArgVal) width;
    icon_toggle_args[5].value = (XtArgVal) resources.app_icon_height;
#ifdef ENHANCE_SELECTION
    icon_toggle_args[6].value = (XtArgVal) resources.highlight_pixel;
#endif
    
    for (i=0; i < aw.n_apps; i++) {
      Pixel back;
      aw.apps[i].form = XtCreateManagedWidget(aw.apps[i].name,
					      formWidgetClass, aw.icon_box,
					      icon_form_args,
					      XtNumber(icon_form_args) );
      icon_toggle_args[2].value = aw.apps[i].icon_bm;
      aw.apps[i].toggle = XtCreateManagedWidget("icon", toggleWidgetClass,
						aw.apps[i].form,
						icon_toggle_args,
						XtNumber(icon_toggle_args) );
      XtVaGetValues(aw.apps[i].toggle, XtNbackground, &back, NULL);
      XtVaSetValues(aw.apps[i].toggle, XtNborder, (XtArgVal) back, NULL);

/*
    RBW Test stuff...
*/
XtInsertEventHandler(aw.apps[i].toggle,
                     EnterWindowMask|LeaveWindowMask,
                     False,
                     (XtEventHandler)CatchEntryLeave2,
                     (XtPointer)0,
                     XtListHead);


      icon_label_args[1].value = (XtArgVal) aw.apps[i].toggle;
      icon_label_args[2].value = (XtArgVal) aw.apps[i].name;
      aw.apps[i].label = XtCreateManagedWidget("label", labelWidgetClass,
					       aw.apps[i].form,
					       icon_label_args,
					       XtNumber(icon_label_args) );
    };
  }

  if (n_appst > 0)
    fillIn(aw_button_items[0]);
  else
    grayOut(aw_button_items[0]);
  XtManageChild(aw.icon_box);
}

/*---------------------------------------------------------------------------*/

void updateApplicationDisplay()
{
  XtDestroyWidget(aw.icon_box);
  createApplicationDisplay();
  setApplicationWindowName();
}

/*---------------------------------------------------------------------------*/

void setApplicationWindowName()
{
  char app_file_name[MAXPATHLEN], *p;
  if ((p = strrchr(resources.app_file, '/')))
    strcpy(app_file_name, p+1);
  else
    strcpy(app_file_name, resources.app_file);
  XSetIconName(XtDisplay(aw.shell), XtWindow(aw.shell), app_file_name);
  XStoreName(XtDisplay(aw.shell), XtWindow(aw.shell), app_file_name);
}

/*---------------------------------------------------------------------------*/

void readApplicationBitmaps()
{
  int i;

  for (i=0; i<aw.n_apps; i++) {
    aw.apps[i].loaded = False;
    if (!aw.apps[i].icon[0])
      aw.apps[i].icon_bm = defaultIcon(aw.apps[i].name, aw.apps[i].directory,
				       aw.apps[i].fname);
    else if ((aw.apps[i].icon_bm = readIcon(aw.apps[i].icon,0,0)) == None) {
      fprintf(stderr, "%s: can't read icon for application %s\n",
	      progname, aw.apps[i].name);
      aw.apps[i].icon_bm = defaultIcon(aw.apps[i].name, aw.apps[i].directory,
				       aw.apps[i].fname);
    } else
      aw.apps[i].loaded = True;
  }
}

/*---------------------------------------------------------------------------*/

void readApplicationData(String path)
{
  FILE *fp;
  char *name, *directory, *fname, *icon, *push_action, *drop_action;
  char s[MAXAPPSTRINGLEN];
  int i, p;
  
  aw.n_apps = 0;
  aw.apps = NULL;
  
  if (!(fp = fopen(path, "r"))) return;

  for (i=0; (p = parseApp(fp, &name, &directory, &fname, &icon, &push_action,
			  &drop_action)) > 0; i++) {
    aw.apps = (AppList) XTREALLOC(aw.apps, (i+1)*sizeof(AppRec) );
    aw.apps[i].name = XtNewString(strparse(s, name, "\\:"));
    aw.apps[i].directory = XtNewString(strparse(s, directory, "\\:"));
    aw.apps[i].fname = XtNewString(strparse(s, fname, "\\:"));
    aw.apps[i].icon = XtNewString(strparse(s, icon, "\\:"));
    aw.apps[i].push_action = XtNewString(strparse(s, push_action, "\\:"));
    aw.apps[i].drop_action = XtNewString(strparse(s, drop_action, "\\:"));
  }

  if (p == -1)
    error("Error in applications file", "");

  aw.n_apps = i;
  
  if (fclose(fp))
    sysError("Error reading applications file:");

  readApplicationBitmaps();
}

/*---------------------------------------------------------------------------*/

int writeApplicationData(String path)
{
  FILE *fp;
  int i;
  char name[2*MAXAPPSTRINGLEN], directory[2*MAXAPPSTRINGLEN],
    fname[2*MAXAPPSTRINGLEN], icon[2*MAXAPPSTRINGLEN],
    push_action[2*MAXAPPSTRINGLEN], drop_action[2*MAXAPPSTRINGLEN];
  
  if (! (fp = fopen(path, "w") )) {
    sysError("Error writing applications file:");
    return -1;
  }

  fprintf(fp, "#XFM\n");

  for (i=0; i < aw.n_apps; i++) {
    expand(name, aw.apps[i].name, "\\:");
    expand(directory, aw.apps[i].directory, "\\:");
    expand(fname, aw.apps[i].fname, "\\:");
    expand(icon, aw.apps[i].icon, "\\:");
    expand(push_action, aw.apps[i].push_action, "\\:");
    expand(drop_action, aw.apps[i].drop_action, "\\:");
    fprintf(fp, "%s:%s:%s:%s:%s:%s\n", name, directory, fname, icon,
	    push_action, drop_action);
  }
  
  if (fclose(fp)) {
    sysError("Error writing applications file:");
    return -1;
  }

  return 0;
}

/*---------------------------------------------------------------------------*/

void freeApplicationResources(AppRec *app)
{
  if (app->loaded)
    XFreePixmap(XtDisplay(aw.shell), app->icon_bm);
  XTFREE(app->name);
  XTFREE(app->directory);
  XTFREE(app->fname);
  XTFREE(app->icon);
  XTFREE(app->push_action);
  XTFREE(app->drop_action);
}

/*---------------------------------------------------------------------------*/

void installApplication(char *name, char *directory, char *fname, char *icon,
			char *push_action, char *drop_action)
{
  int i;
  Pixmap icon_bm;
  Boolean loaded = False;

  if (!*icon)
    icon_bm = defaultIcon(name, directory, fname);
  else if ((icon_bm = readIcon(icon,0,0)) == None) {
    error("Can't read icon for", name);
    icon_bm = defaultIcon(name, directory, fname);
  } else
    loaded = True;

  i = aw.n_apps++;
  aw.apps = (AppList) XTREALLOC(aw.apps, aw.n_apps * sizeof(AppRec));

  aw.apps[i].name = XtNewString(name);
  aw.apps[i].directory = XtNewString(directory);
  aw.apps[i].fname = XtNewString(fname);
  aw.apps[i].icon = XtNewString(icon);
  aw.apps[i].push_action = XtNewString(push_action);
  aw.apps[i].drop_action = XtNewString(drop_action);
  aw.apps[i].icon_bm = icon_bm;
  aw.apps[i].loaded = loaded;
  aw.apps[i].form = aw.apps[i].toggle = aw.apps[i].label = NULL;
}

/*---------------------------------------------------------------------------*/

void replaceApplication(AppRec *app, char *name, char *directory, char *fname,
			char *icon, char *push_action, char *drop_action)
{
  Pixmap icon_bm;
  Boolean loaded = False;

  if (!*icon)
    icon_bm = defaultIcon(name, directory, fname);
  else if ((icon_bm = readIcon(icon,0,0)) == None) {
    error("Can't read icon for", name);
    icon_bm = defaultIcon(name, directory, fname);
  } else
    loaded = True;

  freeApplicationResources(app);

  app->name = XtNewString(name);
  app->directory = XtNewString(directory);
  app->fname = XtNewString(fname);
  app->icon = XtNewString(icon);
  app->push_action = XtNewString(push_action);
  app->drop_action = XtNewString(drop_action);
  app->icon_bm = icon_bm;
  app->loaded = loaded;
  app->form = app->toggle = app->label = NULL;
}

/*---------------------------------------------------------------------------*/

void pushApplicationsFile()
{
  int i = n_appst++;

  appst = (char **) XTREALLOC(appst, n_appst * sizeof(char *));
  appst[i] = XtNewString(resources.app_file);
}

/*---------------------------------------------------------------------------*/

void popApplicationsFile()
{
  if (n_appst <= 0) return;
  strcpy(resources.app_file, appst[--n_appst]);
  XTFREE(appst[n_appst]);
  appst = (char **) XTREALLOC(appst, n_appst * sizeof(char *));
}

/*---------------------------------------------------------------------------*/

void clearApplicationsStack()
{
  int i;
  for (i = 0; i < n_appst; i++)
    XTFREE(appst[i]);
  XTFREE(appst);
  appst = NULL;
  n_appst = 0;
}


/*
    RBW Test stuff...
*/


void CatchEntryLeave2(Widget w, XtPointer cld, XEvent* ev, Boolean *ctd)
{
/*
    RBW - eat extraneous LeaveNotify/EnterNotify events caused by some window
    managers when a ButtonPress happens, so that the Xt translation mechanism
    for double-clicks doesn't get confused.
*/
if (ev->xcrossing.mode == NotifyGrab || ev->xcrossing.mode == NotifyUngrab)  {
  *ctd = False;  /* Eat this event */
  }  else  {
  *ctd = True;  /* Don't eat this event */
  }
return;
}



