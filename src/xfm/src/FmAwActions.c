/*---------------------------------------------------------------------------
  Module FmAwActions
  
  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994

  Action procedures for widgets in the application window
---------------------------------------------------------------------------*/

#include <string.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Toggle.h>

#include "Am.h"

/*---------------------------------------------------------------------------
  PRIVATE FUNCTIONS
---------------------------------------------------------------------------*/

/* determine the cursor type for an application */

static Cursor cursorType(char *name, char *directory, char *fname)
{
  if (!*fname)
    return curs[EXEC_CUR];
  else {
    struct stat stats;
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
      return curs[FILE_CUR];
    else if (S_ISDIR(stats.st_mode))
      return curs[DIR_CUR];
    else if (stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
      return curs[EXEC_CUR];
    else
      return curs[FILE_CUR];
  }
}

/*---------------------------------------------------------------------------*/

/* perform a click-drag, returning the widget that the user released the 
   button on */

static Boolean dragging_app = False;

static Widget drag(Widget w, int button,
		   Boolean *on_root_return)
{
  Cursor c;
  XEvent e;
  int i, x, y;
  Window child;

  if (aw.n_selections == 1) {
    for (i=0; !aw.apps[i].selected && i<aw.n_apps;
	 i++);
    c = cursorType(aw.apps[i].name, aw.apps[i].directory, aw.apps[i].fname);
  } else
    c = curs[FILES_CUR];

  XGrabPointer(XtDisplay(w), XtWindow(w), True,
	       EnterWindowMask | LeaveWindowMask | ButtonReleaseMask, 
	       GrabModeAsync, GrabModeAsync, None, c, CurrentTime);

  freeze = dragging_app = True;
  
  for (;;) {
    XtAppNextEvent(app_context, &e);
    switch (e.type) {
    case ButtonPress:        /* Ignore button presses */
      continue;
    case ButtonRelease:      /* Ignore button releases except 2 */
      if (e.xbutton.button != button)
	continue;
      break;
    default:
      XtDispatchEvent(&e);
      continue;
    }
    break;
  }

  XUngrabPointer(XtDisplay(aw.shell),CurrentTime);
  XtDispatchEvent(&e);
  freeze = dragging_app = False;
  
  /* Find out if the drag was released on the root window (child = None) */
  XTranslateCoordinates(XtDisplay(w),e.xbutton.window,e.xbutton.root,
			e.xbutton.x,e.xbutton.y,&x,&y,&child);
  if (child == None)
    *on_root_return = True;
  else
    *on_root_return = False;

  return XtWindowToWidget(XtDisplay(w),e.xbutton.window);
}

/*---------------------------------------------------------------------------
  PUBLIC FUNCTIONS
---------------------------------------------------------------------------*/

int findAppWidget(Widget w)
{
  int i;

  for (i=0; i<aw.n_apps; i++)
    if (aw.apps[i].toggle == w)
      return i;
  return -1;
}

/*---------------------------------------------------------------------------*/

void appPopup(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  Display *dpy;
  Window root, child;
  int x, y, x_win, y_win;
  unsigned int mask;
  int i = findAppWidget(w);

  dpy = XtDisplay(aw.shell);

  XQueryPointer(dpy, XtWindow(w), &root, &child, &x, &y, 
		&x_win, &y_win, &mask);

  /* check whether icon was selected */
  if (child != None) {
    XTranslateCoordinates(dpy, XtWindow(w), child, x_win, y_win,
			  &x_win, &y_win, &child);
    if (child != None) w = XtWindowToWidget(dpy, child);
  }

  i = findAppWidget(w);
  if (i != -1) appSelect(w, event, params, num_params);

  if (i == -1) {
    struct stat stats;
    if (aw.n_selections == 0) {
      grayOut(app_popup_items[3]);
      grayOut(app_popup_items[4]);
      grayOut(app_popup_items[7]);
      grayOut(app_popup_items[10]);
    } else {
      fillIn(app_popup_items[3]);
      fillIn(app_popup_items[4]);
      fillIn(app_popup_items[7]);
      fillIn(app_popup_items[10]);
    }
    if (stat(resources.app_clip, &stats))
      grayOut(app_popup_items[5]);
    else
      fillIn(app_popup_items[5]);

    XQueryPointer(dpy, DefaultRootWindow(dpy), &root, &child, &x, &y, 
		  &x_win, &y_win, &mask);
  
    XtVaSetValues(app_popup_widget, XtNx, (XtArgVal) x, XtNy, (XtArgVal) y,
		  NULL);
  
    XtPopupSpringLoaded(app_popup_widget);
  } else {
    XQueryPointer(dpy, DefaultRootWindow(dpy), &root, &child, &x, &y, 
		  &x_win, &y_win, &mask);
  
    XtVaSetValues(app_popup_widget1, XtNx, (XtArgVal) x, XtNy, (XtArgVal) y,
		  NULL);
  
    XtPopupSpringLoaded(app_popup_widget1);
  }
}  

/*---------------------------------------------------------------------------*/

void appMaybeHighlight(Widget w, XEvent *event, String *params, 
		       Cardinal *num_params)
{
  int i;

  if (dragging) {
    i = findAppWidget(w);
    if (*aw.apps[i].drop_action)
      XtCallActionProc(w, "highlight", event, NULL, 0);
  } else if (dragging_app)
    XtCallActionProc(w, "highlight", event, NULL, 0);
}

/*---------------------------------------------------------------------------*/

void runApp(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  int i;
  char **argv;
  char directory[MAXPATHLEN];

  i = findAppWidget(w);
  strcpy(directory, aw.apps[i].directory);
  if (*directory)
    fnexpand(directory);
  else
    strcpy(directory, user.home);

  if (*aw.apps[i].push_action)
    if (!strcmp(aw.apps[i].push_action, "EDIT"))
      doEdit(directory, aw.apps[i].fname);
    else if (!strcmp(aw.apps[i].push_action, "VIEW"))
      doView(directory, aw.apps[i].fname);
    else if (!strcmp(aw.apps[i].push_action, "OPEN")) {
      int l = strlen(directory);
      if (directory[l-1] != '/')
	directory[l++] = '/';
      strcpy(directory+l, aw.apps[i].fname);
      newFileWindow(directory, resources.default_display_type,
		    False);
    } else if (!strcmp(aw.apps[i].push_action, "LOAD")) {
      int j, l = strlen(directory);
      zzz();
      pushApplicationsFile();
      strcpy(resources.app_file, directory);
      if (resources.app_file[l-1] != '/')
	resources.app_file[l++] = '/';
      strcpy(resources.app_file+l, aw.apps[i].fname);
      for(j=0; j<aw.n_apps; j++)
	freeApplicationResources(&aw.apps[j]);
      XTFREE(aw.apps);
      readApplicationData(resources.app_file);
      updateApplicationDisplay();
      wakeUp();
    } else {
      char *action = varPopup(aw.apps[i].icon_bm, aw.apps[i].push_action);
      if (!action) return;
      if (*aw.apps[i].fname)
	argv = makeArgv2(action, aw.apps[i].fname);
      else
	argv = makeArgv(action);
      executeApplication(user.shell, directory, argv);
      freeArgv(argv);
    }
}

/*---------------------------------------------------------------------------*/

void appBeginDrag(Widget w, XEvent *event, String *params, 
		  Cardinal *num_params)
{
  int i, button;
  Boolean on_root_return, move = True;

  if (*num_params != 2) {
    error("Internal error:","wrong number of parameters to appBeginDrag");
    return;
  }

  button = *params[0] - '0';
  if (!strcmp(params[1],"copy"))
    move = False;

  i =  findAppWidget(w);
  
  if (i != -1) {
    if (!aw.apps[i].selected)
      appSelect(w, event, params, num_params);
  } else
    return;
  
  if (!(w = drag(w,button,&on_root_return)))
    return;
  else if (on_root_return)
    return;

  if (w == aw.icon_box)
    if (move)
      appEndMoveInBox();
    else
      appEndCopyInBox();
  else if ((i = findAppWidget(w)) != -1)
    if (move)
      appEndMove(i);
    else
      appEndCopy(i);
}
  
/*---------------------------------------------------------------------------*/

void appEndMoveInBox(void)
{
  char s[0xff];
  int j, k, l;
  AppRec *apps;

  if (aw.n_selections == 0) return;

  if (resources.confirm_moves) {
    sprintf(s, "Moving %d item%s in", aw.n_selections,
	    aw.n_selections > 1 ? "s" : "" );
    if (!confirm(s, "the application window", ""))
      return;
  }

  apps = (AppRec*) XtMalloc(aw.n_apps*sizeof(AppRec));
  for (j = k = 0, l = aw.n_apps - aw.n_selections; j < aw.n_apps; j++)
    if (aw.apps[j].selected)
      memcpy(&apps[l++], &aw.apps[j], sizeof(AppRec));
    else
      memcpy(&apps[k++], &aw.apps[j], sizeof(AppRec));
  XTFREE(aw.apps);
  aw.apps = apps;
  updateApplicationDisplay();
  writeApplicationData(resources.app_file);
}

/*---------------------------------------------------------------------------*/

void appEndMove(int i)
{
  char s[0xff];
  int j, k, l, m;
  AppRec *apps;

  if (aw.n_selections == 0 || aw.apps[i].selected) return;

  if (resources.confirm_moves) {
    sprintf(s, "Moving %d item%s in", aw.n_selections,
	    aw.n_selections > 1 ? "s" : "" );
    if (!confirm(s, "the application window", ""))
      return;
  }

  apps = (AppRec*) XtMalloc(aw.n_apps*sizeof(AppRec));
  for (j = l = 0; j < i; j++)
    if (!aw.apps[j].selected) l++;
  for (j = k = 0, m = l + aw.n_selections; j < aw.n_apps; j++)
    if (aw.apps[j].selected)
      memcpy(&apps[l++], &aw.apps[j], sizeof(AppRec));
    else if (j < i)
      memcpy(&apps[k++], &aw.apps[j], sizeof(AppRec));
    else
      memcpy(&apps[m++], &aw.apps[j], sizeof(AppRec));
  XTFREE(aw.apps);
  aw.apps = apps;
  updateApplicationDisplay();
  writeApplicationData(resources.app_file);
}

/*---------------------------------------------------------------------------*/

static void copyApps(void)
{
  int j, n_apps = aw.n_apps;

  aw.apps = (AppList) XTREALLOC(aw.apps, (aw.n_apps+aw.n_selections) *
					  sizeof(AppRec));
  for (j=0; j<n_apps; j++)
    if (aw.apps[j].selected) {
      Pixmap icon_bm;
      Boolean loaded = False;
      if (aw.apps[j].loaded)
	if ((icon_bm = readIcon(aw.apps[j].icon,0,0)) == None) {
	  error("Can't read icon for", aw.apps[j].name);
	  icon_bm = defaultIcon(aw.apps[j].name, aw.apps[j].directory,
				aw.apps[j].fname);
	} else
	  loaded = True;
      else
	icon_bm = aw.apps[j].icon_bm;
      aw.apps[aw.n_apps].name = XtNewString(aw.apps[j].name);
      aw.apps[aw.n_apps].directory = XtNewString(aw.apps[j].directory);
      aw.apps[aw.n_apps].fname = XtNewString(aw.apps[j].fname);
      aw.apps[aw.n_apps].icon = XtNewString(aw.apps[j].icon);
      aw.apps[aw.n_apps].push_action = XtNewString(aw.apps[j].push_action);
      aw.apps[aw.n_apps].drop_action = XtNewString(aw.apps[j].drop_action);
      aw.apps[aw.n_apps].icon_bm = icon_bm;
      aw.apps[aw.n_apps].loaded = loaded;
      aw.apps[aw.n_apps].form = aw.apps[aw.n_apps].toggle =
	aw.apps[aw.n_apps].label = NULL;
      aw.n_apps++;
    }
}

/*---------------------------------------------------------------------------*/

void appEndCopyInBox(void)
{
  char s[0xff];
  if (aw.n_selections == 0) return;

  if (resources.confirm_copies) {
    sprintf(s, "Copying %d item%s in", aw.n_selections,
	    aw.n_selections > 1 ? "s" : "" );
    if (!confirm(s, "the application window", ""))
      return;
  }

  copyApps();
  updateApplicationDisplay();
  writeApplicationData(resources.app_file);
}

/*---------------------------------------------------------------------------*/

void appEndCopy(int i)
{
  char s[0xff];
  int n_apps = aw.n_apps;
  AppRec *apps;

  if (aw.n_selections == 0) return;

  if (resources.confirm_copies) {
    sprintf(s, "Copying %d item%s in", aw.n_selections,
	    aw.n_selections > 1 ? "s" : "" );
    if (!confirm(s, "the application window", ""))
      return;
  }

  copyApps();
  apps = (AppRec*) XtMalloc(aw.n_apps * sizeof(AppRec));
  memcpy(apps, aw.apps, i*sizeof(AppRec));
  memcpy(&apps[i], &aw.apps[n_apps], (aw.n_apps-n_apps)*sizeof(AppRec));
  memcpy(&apps[i+aw.n_apps-n_apps], &aw.apps[i], (n_apps-i)*sizeof(AppRec));
  XTFREE(aw.apps);
  aw.apps = apps;
  updateApplicationDisplay();
  writeApplicationData(resources.app_file);
}

/*---------------------------------------------------------------------------*/

void appEndDrag(int i)
{
  char **argv;

  if (*aw.apps[i].drop_action) {
    char *action = varPopup(aw.apps[i].icon_bm, aw.apps[i].drop_action);
    if (!action) return;
    if (*aw.apps[i].fname) {
      int l;
      char path[MAXPATHLEN];
      strcpy(path, aw.apps[i].directory);
      if (*path)
	fnexpand(path);
      else
	strcpy(path, user.home);
      l = strlen(path);
      if (path[l-1] != '/')
	path[l++] = '/';
      strcpy(path+l, aw.apps[i].fname);
      argv = makeArgv2(action, path);
    } else
      argv = makeArgv(action);
    argv = expandArgv(argv);
    executeApplication(user.shell, move_info.fw->directory, argv);
    freeArgv(argv);
  }
}

/*---------------------------------------------------------------------------*/

void appEndDragInBox(void)
{
  int i;

  for (i=0; i<move_info.fw->n_files; i++)
    if (move_info.fw->files[i]->selected) {
      if (S_ISDIR(move_info.fw->files[i]->stats.st_mode)) {
	installApplication(move_info.fw->files[i]->name,
			   move_info.fw->directory,
			   move_info.fw->files[i]->name,
			   move_info.fw->files[i]->type?
			   move_info.fw->files[i]->type->icon:"",
			   "OPEN",
			   "");
      } else if (move_info.fw->files[i]->stats.st_mode &
		 (S_IXUSR | S_IXGRP | S_IXOTH)) {
	char *push_action, *drop_action;
	push_action = move_info.fw->files[i]->name;
	drop_action = (char *)alloca(strlen(push_action)+4);
	strcpy(drop_action, push_action);
	strcat(drop_action, " $*");
	installApplication(move_info.fw->files[i]->name,
			   move_info.fw->directory,
			   "",
			   move_info.fw->files[i]->type?
			   move_info.fw->files[i]->type->icon:"",
			   push_action,
			   drop_action);
      } else if (move_info.fw->files[i]->type) {
	installApplication(move_info.fw->files[i]->name,
			   move_info.fw->directory,
			   move_info.fw->files[i]->name,
			   move_info.fw->files[i]->type->icon,
			   move_info.fw->files[i]->type->push_action,
			   move_info.fw->files[i]->type->drop_action);
      } else {
	installApplication(move_info.fw->files[i]->name,
			   move_info.fw->directory,
			   move_info.fw->files[i]->name,
			   "",
			   "EDIT",
			   "");
      }
    }
  updateApplicationDisplay();
  writeApplicationData(resources.app_file);
}

/*---------------------------------------------------------------------------*/

void appSelect(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  int i, j;
  Pixel back, fore;
  
  j = findAppWidget(w);
  if (j == -1) {
    error("Internal error:", "widget not found in appSelect");
    return;
  }
  
  for (i=0; i<aw.n_apps; i++)
    if (aw.apps[i].selected) {
      XtVaGetValues(aw.apps[i].toggle, XtNbackground, &back, NULL);
      XtVaSetValues(aw.apps[i].toggle, XtNborder, (XtArgVal) back, NULL);
      aw.apps[i].selected = False;
    }

  XtVaGetValues(w, XtNforeground, &fore, NULL);
  XtVaSetValues(w, XtNborder, (XtArgVal) fore, NULL);
  
  aw.apps[j].selected = True;
  aw.n_selections = 1;
}

/*---------------------------------------------------------------------------*/

void appToggle(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  int i;
  Pixel pix;
  
  i = findAppWidget(w);
  if (i == -1) {
    error("Internal error:", "widget not found in appToggle");
    return;
  }
  
  XtVaGetValues(w, aw.apps[i].selected?XtNbackground:XtNforeground, &pix,
		NULL);
  XtVaSetValues(w, XtNborder, (XtArgVal) pix, NULL);
  
  aw.apps[i].selected = !aw.apps[i].selected;
  if (aw.apps[i].selected)
    aw.n_selections++;
  else
    aw.n_selections--;
}

