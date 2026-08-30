/*---------------------------------------------------------------------------
  Module FmFwActions

  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994

  modified 7-1997 by strauman@sun6hft.ee.tu-berlin.de to add
  different enhancements (see README-NEW).

  Action procedures for widgets in a file window
---------------------------------------------------------------------------*/


#include <stdio.h>
#include <unistd.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Toggle.h>
#include "FileList.h"

#include "Am.h"
#include "Fm.h"

#ifdef ENHANCE_SELECTION
#include "FmSelection.h"
#endif

/*---------------------------------------------------------------------------
  PUBLIC DATA
---------------------------------------------------------------------------*/

MoveInfo move_info;
Boolean dragging = False;
Cursor drag_cursor, cache_cursor;

/*---------------------------------------------------------------------------
  STATIC DATA
---------------------------------------------------------------------------*/

static char from[MAXPATHLEN], to[MAXPATHLEN];
static int fromi, toi;

/*---------------------------------------------------------------------------
  PRIVATE FUNCTIONS
---------------------------------------------------------------------------*/

static int findWidget(Widget w, FileWindowRec **fw_ret, XEvent *ev)
{
  int i;
  FileWindowRec *fw;

  for (fw = file_windows; fw; fw = fw->next) {
    if (fw->icon_box == w || fw->label == w) {
      *fw_ret = fw;
      if (XtIsSubclass(w,fileListWidgetClass)) {
	return FileListFindEntry((FileListWidget)w,ev) ;
      }
      return -1;
    }
    for (i = 0; i < fw->n_files; i++)
      if (fw->files[i]->icon.toggle == w) {
	*fw_ret = fw;
	return i;
      }
  }
  *fw_ret = NULL;
  return 0;
}

/*---------------------------------------------------------------------------*/

static int moveFiles()
{
  int i, n_moved = 0;

  if (from[fromi-1] != '/') {
    from[fromi++] = '/';
    from[fromi] = '\0';
  }

  if (to[toi-1] != '/') {
    to[toi++] = '/';
    to[toi] = '\0';
  }

  for (i=0; i < move_info.fw->n_files; i++)
    if (move_info.fw->files[i]->selected) {
      if (!strcmp(move_info.fw->files[i]->name, ".") ||
	  !strcmp(move_info.fw->files[i]->name, "..")) {
	error("Cannot move . or ..", "");
	continue;
      }
      strcpy(from+fromi, move_info.fw->files[i]->name);
      strcpy(to+toi, move_info.fw->files[i]->name);
      if (exists(to) && resources.confirm_overwrite) {
	char s[0xff];
	sprintf(s, "Move: file %s already exists at destination",
		move_info.fw->files[i]->name);
	if (!confirm(s, "Overwrite?", ""))
	  if (aborted)
	    break;
	  else
	    continue;
      }
      if (rmove(from,to)) {
	char s[0xff];
	sprintf(s, "Error moving %s:", move_info.fw->files[i]->name);
	sysError(s);
      } else
	n_moved++;
    }

  return n_moved;
}

/*---------------------------------------------------------------------------*/

static int copyFiles()
{
  int i, n_copied = 0;

  if (from[fromi-1] != '/') {
    from[fromi++] = '/';
    from[fromi] = '\0';
  }

  if (to[toi-1] != '/') {
    to[toi++] = '/';
    to[toi] = '\0';
  }

  for (i=0; i < move_info.fw->n_files; i++)
    if (move_info.fw->files[i]->selected) {
      if (!strcmp(move_info.fw->files[i]->name, ".") ||
	  !strcmp(move_info.fw->files[i]->name, "..")) {
	error("Cannot copy . or ..", "");
	continue;
      }
      strcpy(from+fromi, move_info.fw->files[i]->name);
      strcpy(to+toi, move_info.fw->files[i]->name);
      if (exists(to) && resources.confirm_overwrite) {
	char s[0xff];
	sprintf(s, "Copy: file %s already exists at destination",
		move_info.fw->files[i]->name);
	if (!confirm(s, "Overwrite?", ""))
	  if (aborted)
	    break;
	  else
	    continue;
      }
      if (rcopy(from,to)) {
	char s[0xff];
	sprintf(s, "Error copying %s:", move_info.fw->files[i]->name);
	sysError(s);
      } else
	n_copied++;
    }

  return n_copied;
}

/*---------------------------------------------------------------------------*/

static int setupMoveCopy(FileWindowRec *fw, int i)
{
  strcpy(from, move_info.fw->directory);

  if (i != -1 && S_ISDIR(fw->files[i]->stats.st_mode)
      && strcmp(fw->files[i]->name, ".")) {
    if (chdir(fw->directory) || chdir(fw->files[i]->name) || !getwd(to)) {
      sysError("System error:");
      return 0;
    }
  } else
    strcpy(to, fw->directory);
    
  toi = strlen(to);
  fromi = strlen(from);

  return 1;
}

/*---------------------------------------------------------------------------*/

static void fileEndMove(FileWindowRec *fw, int i)
{
  char fromdir[MAXPATHLEN], todir[MAXPATHLEN];

  if (!setupMoveCopy(fw, i)) return;
  strcpy(fromdir, from);
  strcpy(todir, to);

  if (!strcmp(from, to)) {
    error("Move:", "Source and destination are identical");
    return;
  }

  if (access(to, W_OK)) {
    error("No write access to this directory","");
    return;
  }

  freeze = True;

  if (resources.confirm_moves) {
    char s1[0xff], s2[0xff], s3[0xff];
    sprintf(s1, "Moving %d item%c", move_info.fw->n_selections,
	    move_info.fw->n_selections > 1 ? 's' : ' ');
    sprintf(s2, "from: %s", from);
    sprintf(s3, "to: %s", to);
    if (!confirm(s1, s2, s3))
      goto out;
  }

  if (moveFiles()) {
    markForUpdate(fromdir); markForUpdate(todir);
    intUpdate();
  }

 out:
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void fileEndCopy(FileWindowRec *fw, int i)
{
  char todir[MAXPATHLEN];

  if (!setupMoveCopy(fw, i)) return;
  strcpy(todir, to);

  if (!strcmp(from, to)) {
    error("Copy:", "Source and destination are identical");
    return;
  }

  if (access(to, W_OK)) {
    error("No write access to this directory","");
    return;
  }

  freeze = True;

  if (resources.confirm_copies) {
    char s1[0xff], s2[0xff], s3[0xff];
    sprintf(s1, "Copying %d item%c", move_info.fw->n_selections,
	    move_info.fw->n_selections > 1 ? 's' : ' ');
    sprintf(s2, "from: %s", from);
    sprintf(s3, "to: %s", to);
    if (!confirm(s1, s2, s3))
      goto out;
  }

  if (copyFiles()) {
    markForUpdate(todir);
    intUpdate();
  }

 out:
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void fileEndExec(FileWindowRec *fw, int i)
{
  int l;
  char *path;
  char **argv;

  l = strlen(fw->directory);
  path = (char *)alloca(l+strlen(fw->files[i]->name)+2);
  strcpy(path, fw->directory);
  if (path[l-1] != '/')
    path[l++] = '/';
  strcpy(path+l, fw->files[i]->name);

  argv = (char **) XtMalloc(2 * sizeof(char *));
  argv[0] = XtNewString(path);
  argv[1] = NULL;

  argv = expandArgv(argv);
  executeApplication(path, move_info.fw->directory, argv);
  freeArgv(argv);
}

/*---------------------------------------------------------------------------*/

static void fileEndAction(FileWindowRec *fw, int i)
{
  int l;
  char *path;
  char **argv;
  char *action = varPopup(fw->files[i]->type->icon_bm,
			  fw->files[i]->type->drop_action);

  if (!action) return;

  l = strlen(fw->directory);
  path = (char *)alloca(l+strlen(fw->files[i]->name)+2);
  strcpy(path, fw->directory);
  if (path[l-1] != '/')
    path[l++] = '/';
  strcpy(path+l, fw->files[i]->name);

  argv = makeArgv2(action, path);
  argv = expandArgv(argv);
  executeApplication(user.shell, move_info.fw->directory, argv);
  freeArgv(argv);
}

/*---------------------------------------------------------------------------*/

/* perform a click-drag, returning the widget that the user released the 
   button on */

static Widget drag(Widget w, int button, FileWindowRec *fw,
		   Boolean *on_root_return, XEvent *event_return)
{
  Cursor c;
  int i, x, y;
  Window child;

  move_info.fw = fw;
  
  if (fw->n_selections == 1) {
    for (i=0; !fw->files[i]->selected && i<fw->n_files;
	 i++);
    if (S_ISDIR(fw->files[i]->stats.st_mode)) {
      move_info.type = Directory;
      c = curs[DIR_CUR];
    }
    else if (S_ISLNK(fw->files[i]->stats.st_mode)) {
      move_info.type = SingleFile;
      c = curs[FILE_CUR];
    }
    else if (fw->files[i]->stats.st_mode & 
	     (S_IXUSR | S_IXGRP | S_IXOTH)) {
      move_info.type = Executable;
      c = curs[EXEC_CUR];
    }
    else {
      move_info.type = SingleFile;
      c = curs[FILE_CUR];
    }
  }
  else {
    move_info.type = MultipleFiles;
    c = curs[FILES_CUR];
  }
  cache_cursor=drag_cursor = c;

  XGrabPointer(XtDisplay(w), XtWindow(w), True,
	       EnterWindowMask | LeaveWindowMask | ButtonReleaseMask, 
	       GrabModeAsync, GrabModeAsync, None, c, CurrentTime);

  move_info.dragged_from = w;
  freeze = dragging = True;
  
  for (;;) {
    XtAppNextEvent(app_context, event_return);
    switch (event_return->type) {
    case ButtonPress:        /* Ignore button presses */
      continue;
    case ButtonRelease:      /* Ignore button releases except 2 */
      if (event_return->xbutton.button != button)
	continue;
      break;
    default:
      XtDispatchEvent(event_return);
      continue;
    }
    break;
  }

  XUngrabPointer(XtDisplay(aw.shell),CurrentTime);
  cache_cursor=(Cursor)-1;

  XtDispatchEvent(event_return);
  freeze = dragging = False;
  
  /* Find out if the drag was released on the root window (child = None) */
  XTranslateCoordinates(XtDisplay(w),
	event_return->xbutton.window,
	event_return->xbutton.root,
	event_return->xbutton.x,
	event_return->xbutton.y,
	&x,&y,&child);
  if (child == None)
    *on_root_return = True;
  else
    *on_root_return = False;

  return XtWindowToWidget(XtDisplay(w),event_return->xbutton.window);
}

/*---------------------------------------------------------------------------
  PUBLIC FUNCTIONS
---------------------------------------------------------------------------*/

void filePopup(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  int i;
  FileWindowRec *fw;
  Display *dpy;
  Window root, child;
  int x, y, x_win, y_win;
  unsigned int mask;

  i = findWidget(w, &fw, event);
  if (i==-1 || !fw) {
/*
    error("Internal error:", "widget not found in filePopup");
*/
    return;
  }

  popup_fw = fw;
  fileSelect(w, event, params, num_params);

  if (S_ISLNK(fw->files[i]->stats.st_mode)) {
    grayOut(file_popup_items[0]);
    grayOut(file_popup_items[1]);
  } else {
    fillIn(file_popup_items[0]);
    fillIn(file_popup_items[1]);
  }

  dpy = XtDisplay(aw.shell);
  
  XQueryPointer(dpy, DefaultRootWindow(dpy), &root, &child, &x, &y, 
		&x_win, &y_win, &mask);
  
  XtVaSetValues(file_popup_widget, XtNx, (XtArgVal) x, XtNy, (XtArgVal) y,
		NULL);
  
  XtPopupSpringLoaded(file_popup_widget);
}  

/*---------------------------------------------------------------------------*/

void dirPopup(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  int i;
  FileWindowRec *fw;
  Display *dpy;
  Window root, child;
  int x, y, x_win, y_win;
  unsigned int mask;

  i = findWidget(w, &fw, event);
  if (i==-1 || !fw) {
/*
    error("Internal error:", "widget not found in dirPopup");
*/
    return;
  }

  popup_fw = fw;
  fileSelect(w, event, params, num_params);

  if (!strcmp(fw->files[i]->name, ".") ||
      !strcmp(fw->files[i]->name, "..")) {
    grayOut(dir_popup_items[2]);
    grayOut(dir_popup_items[4]);
    grayOut(dir_popup_items[5]);
    grayOut(dir_popup_items[8]);
  } else {
    fillIn(dir_popup_items[2]);
    fillIn(dir_popup_items[4]);
    fillIn(dir_popup_items[5]);
    fillIn(dir_popup_items[8]);
  }

  dpy = XtDisplay(aw.shell);
  
  XQueryPointer(dpy, DefaultRootWindow(dpy), &root, &child, &x, &y, 
		&x_win, &y_win, &mask);
  
  XtVaSetValues(dir_popup_widget, XtNx, (XtArgVal) x, XtNy, (XtArgVal) y,
		NULL);
  
  XtPopupSpringLoaded(dir_popup_widget);
}  

/*---------------------------------------------------------------------------*/

void fileToggle(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  int i;
  
  FileWindowRec *fw;
  Pixel pix;
  int   onewid;

  i = findWidget(w, &fw, event);
  if (i==-1 || !fw) {
/*
    error("Internal error:", "widget not found in fileToggle");
*/
    return;
  }
    
  if (!(onewid=XtIsSubclass(w,fileListWidgetClass))) {
    XtVaGetValues(w, fw->files[i]->selected?XtNbackground:XtNforeground, &pix,
		NULL);
    XtVaSetValues(w, XtNborder, (XtArgVal) pix, NULL);
  }

  if (fw->files[i]->selected) {
    fw->files[i]->selected = False;
    fw->n_selections--;
    fw->n_bytes_selected -= fw->files[i]->stats.st_size;
  }
  else {
    fw->files[i]->selected = True;
    fw->n_selections++;
    fw->n_bytes_selected += fw->files[i]->stats.st_size;
  }
  if (onewid) FileListRefreshItem((FileListWidget)w,i);
  updateStatus(fw);
#ifdef ENHANCE_SELECTION
  FmOwnSelection(fw,CurrentTime);
#endif
}

/*---------------------------------------------------------------------------*/

void fileSelect(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  int i, j;
  int onewid=XtIsSubclass(w,fileListWidgetClass);
  FileWindowRec *fw;
  Pixel back, fore;

  j = findWidget(w, &fw, event);
  if (j==-1 || !fw) {
/*
    error("Internal error:", "widget not found in fileSelect");
*/
    return;
  }
  
  for (i=0; i<fw->n_files; i++)
    if (fw->files[i]->selected) {
     fw->files[i]->selected = False;
     if(!onewid) {
      XtCallActionProc(fw->files[i]->icon.toggle, "unset", event, NULL, 0);
      XtVaGetValues(fw->files[i]->icon.toggle, XtNbackground, &back, NULL);
      XtVaSetValues(fw->files[i]->icon.toggle, XtNborder, (XtArgVal) back,
		    NULL);
     } else FileListRefreshItem((FileListWidget)w,i);
    }

  if (!onewid) {
    XtVaGetValues(w, XtNforeground, &fore, NULL);
    XtVaSetValues(w, XtNborder, (XtArgVal) fore, NULL);
  }

  fw->files[j]->selected = True;
  fw->n_selections = 1;
  fw->n_bytes_selected = fw->files[j]->stats.st_size;
  updateStatus(fw);
  if (onewid) FileListRefreshItem((FileListWidget)w,j);
#ifdef ENHANCE_SELECTION
  FmOwnSelection(fw,CurrentTime);
#endif
}

/*---------------------------------------------------------------------------*/

void fileHighlight(Widget w, XEvent *event, String *params, 
		   Cardinal *num_params)
{
  int i;
  FileWindowRec *fw;
  Cursor	c;

  if (!dragging)
    return;

  i = findWidget(w,&fw, event);
  if (!fw) {
/*
    error("Internal error:","Widget not found in fileHighlight");
*/
    return;
  }

  if ((i == -1 && !permission(&fw->stats,W_OK)) || 
      (i != -1 && S_ISDIR(fw->files[i]->stats.st_mode) && 
      !permission(&fw->files[i]->stats, W_OK))) c=curs[NOENTRY_CUR];
  else					        c=drag_cursor;

  if (c!=cache_cursor) {
    XChangeActivePointerGrab(XtDisplay(w),
		       EnterWindowMask | LeaveWindowMask | ButtonReleaseMask, 
		       c, CurrentTime);
    cache_cursor=c;
  }

  if ( i != -1 && (fw != move_info.fw || !fw->files[i]->selected))
    XtCallActionProc(w, "highlight", event, NULL, 0);
}

/*---------------------------------------------------------------------------*/

void trackCursor(Widget w, XEvent *event, String *params,
		 Cardinal *num_params)
{
  FileWindowRec *fw;
  Cursor	c;

  if (!dragging) return;

  c=drag_cursor;

  if (event->type!=LeaveNotify) {
  fw=file_windows;
  while(fw) {
    if (w==fw->viewport || w==fw->icon_box ||
        w==fw->label || XtParent(w)==fw->viewport) {
          if (!permission(&fw->stats,W_OK)) c=curs[NOENTRY_CUR];
	  break;
    }
    fw=fw->next;
  }
  }
  if (c!=cache_cursor) {
    XChangeActivePointerGrab(XtDisplay(w),
		       EnterWindowMask | LeaveWindowMask | ButtonReleaseMask, 
		       c, CurrentTime);
    cache_cursor=c;
  }
  
}

/*---------------------------------------------------------------------------*/

void fileMaybeHighlight(Widget w, XEvent *event, String *params, 
			Cardinal *num_params)
{
  int i;
  FileWindowRec *fw;

  if (!dragging)
    return;

  i = findWidget(w,&fw, event);
  if (!fw) {
/*
    error("Internal error:","Widget not found in fileMaybeHighlight");
*/
    return;
  }

  if (i != -1 && (fw != move_info.fw || !fw->files[i]->selected) &&
      fw->files[i]->type && *fw->files[i]->type->drop_action) {
    if (cache_cursor!=drag_cursor) {
      XChangeActivePointerGrab(XtDisplay(w),
		       EnterWindowMask | LeaveWindowMask | ButtonReleaseMask, 
		       drag_cursor, CurrentTime);
      cache_cursor=drag_cursor;
    }
    XtCallActionProc(w, "highlight", event, NULL, 0);
  }
}

/*---------------------------------------------------------------------------*/

void resetCursor(Widget w, XEvent *event, String *params,Cardinal *num_params)
{
  int i;
  Cursor c;
  FileWindowRec *fw;
  i = findWidget(w,&fw, event);
  if (!fw) {
    return;
  }

  if (i != -1) 
      XtCallActionProc(w, "unhighlight", event, NULL, 0);

  if (!dragging)
    return;

  c=((!permission(&fw->stats,W_OK))?curs[NOENTRY_CUR]:drag_cursor);

  if (c!=cache_cursor) {
    XChangeActivePointerGrab(XtDisplay(w),
		      EnterWindowMask | LeaveWindowMask | ButtonReleaseMask, 
		      c,CurrentTime);
    cache_cursor=c;
  }
}


/*---------------------------------------------------------------------------*/

void fileRefresh(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
  FileWindowRec *fw;
  
  findWidget(w, &fw, event);
  if (fw) 
#ifdef ENHANCE_SCROLL
    updateFileDisplay(fw,True);
#else
    updateFileDisplay(fw);
#endif
}

/*---------------------------------------------------------------------------*/

void fileOpenDir(Widget w, XEvent *event, String *params,Cardinal *num_params)
{
  FileWindowRec *fw;
  int i;
  char path[MAXPATHLEN];
#ifdef ENHANCE_SCROLL
  Boolean keep_position;
#endif

  i = findWidget(w, &fw, event);
  if (i==-1 || !fw)
    return;
  if (chdir(fw->directory) || chdir(fw->files[i]->name))
    sysError("Can't open folder:");
  else if (!getwd(path))
    sysError("System error:");
  else {
#ifdef ENHANCE_SCROLL
    if (!(keep_position=(!strcmp(fw->directory,path))))
#endif
    strcpy(fw->directory, path);
#ifdef ENHANCE_SCROLL
    updateFileDisplay(fw,keep_position);
#else
    updateFileDisplay(fw);
#endif
  }
}

/*---------------------------------------------------------------------------*/

void fileBeginDrag(Widget w, XEvent *event, String *params, 
		   Cardinal *num_params)
{
  XEvent drag_end_event;
  int i, button;
  FileWindowRec *fw;
  Boolean root, move = True;

  if (*num_params != 2) {
    error("Internal error:","wrong number of parameters to fileBeginDrag");
    return;
  }

  button = *params[0] - '0';
  if (!strcmp(params[1],"copy"))
    move = False;

  i =  findWidget(w, &fw, event);
  if (!fw) {
/*
    error("Internal error:","widget not found in fileBeginDrag");
*/
    return;
  }
  
  if (i != -1) {
    if (!fw->files[i]->selected)
      fileSelect(w, event, params, num_params);
  } else
    return;
  
  if (!(w = drag(w,button,fw,&root,&drag_end_event)))
    return;

  /* Open directories if dragged onto the root window */
  if (root && move_info.type == Directory) {
    fileOpenCb(NULL,move_info.fw,NULL);
    return;
  }

  i = findWidget(w,&fw, &drag_end_event);

  /* First see whether the target is the application window */
  if (!fw) {
    if (w == aw.icon_box)
      appEndDragInBox();
    else if ((i = findAppWidget(w)) != -1)
      appEndDrag(i);
/*
    else
      error("Internal error:","widget not found in fileBeginDrag");
*/
  }

  /* Otherwise, if we didn't go anywhere, don't do anything */
  else if (fw == move_info.fw &&
      (i == -1 || fw->files[i]->selected ||
       (!S_ISDIR(fw->files[i]->stats.st_mode) &&
	!(fw->files[i]->stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) &&
	!(fw->files[i]->type && *fw->files[i]->type->drop_action))))
    ;

  /* Otherwise, if dragged onto a window or a directory, do the move */
  else if (i == -1 || S_ISDIR(fw->files[i]->stats.st_mode))
    if (move)
      fileEndMove(fw,i);
    else
      fileEndCopy(fw,i);

  /* Otherwise, if dragged onto an executable, run the program */
  else if (fw->files[i]->stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
    fileEndExec(fw,i);

  /* Otherwise, if the file has a drop action, invoke the action */
  else if (fw->files[i]->type && *fw->files[i]->type->drop_action)
    fileEndAction(fw,i);

  /* Otherwise, it must be a normal file so just do the move */
  else if (move)
    fileEndMove(fw,i);
  else
    fileEndCopy(fw,i);
}
  
/*---------------------------------------------------------------------------*/

void fileExecFile(Widget w, XEvent *event, String *params, 
		  Cardinal *num_params)
{
  int i, l;
  char *path;
  char **argv;
  FileWindowRec *fw;

  i = findWidget(w, &fw, event);
  if (i == -1 || !fw) {
/*
    error("Internal error:","widget not found in fileExecFile");
*/
    return;
  }

  l = strlen(fw->directory);
  path = (char *)alloca(l+strlen(fw->files[i]->name)+2);
  strcpy(path, fw->directory);
  if (path[l-1] != '/')
    path[l++] = '/';
  strcpy(path+l, fw->files[i]->name);
  argv = (char **) XtMalloc(2 * sizeof(char *));
  argv[0] = XtNewString(fw->files[i]->name);
  argv[1] = NULL;
  executeApplication(path, fw->directory, argv);

  freeArgv(argv);
}

/*---------------------------------------------------------------------------*/
void fileExecAction(Widget w, XEvent *event, String *params, 
		    Cardinal *num_params)
{
  int i;
  FileWindowRec *fw;
  char **argv;

  i = findWidget(w, &fw, event);
  if (i == -1 || !fw) {
/*
    error("Internal error:","widget not found in fileExecAction");
*/
    return;
  }

  if (fw->files[i]->type) {
    if (*fw->files[i]->type->push_action)
      if (!strcmp(fw->files[i]->type->push_action, "EDIT"))
	doEdit(fw->directory, fw->files[i]->name);
      else if (!strcmp(fw->files[i]->type->push_action, "VIEW"))
	doView(fw->directory, fw->files[i]->name);
      else if (!strcmp(fw->files[i]->type->push_action, "LOAD")) {
	int j, l;
	if (!resources.appmgr) return;
	pushApplicationsFile();
	l = strlen(fw->directory);
	strcpy(resources.app_file, fw->directory);
	if (resources.app_file[l-1] != '/')
	  resources.app_file[l++] = '/';
	strcpy(resources.app_file+l, fw->files[i]->name);
	for(j=0; j<aw.n_apps; j++)
	  freeApplicationResources(&aw.apps[j]);
	XTFREE(aw.apps);
	readApplicationData(resources.app_file);
	updateApplicationDisplay();
      } else {
        int k = 0;
	char *action = varPopup(fw->files[i]->type->icon_bm,
				fw->files[i]->type->push_action);

	if (!action) return;

	argv = (char **) XtMalloc( (user.arg0flag ? 6 : 5) * sizeof(char *));
	argv[k++] = user.shell;
	argv[k++] = "-c";
	argv[k++] = action;
        if (user.arg0flag)
          argv[k++] = user.shell;
	argv[k++] = fw->files[i]->name;
	argv[k] = NULL;

	executeApplication(user.shell, fw->directory, argv);

	XTFREE(argv);
      }
  } else
    doEdit(fw->directory, fw->files[i]->name);
}

/*---------------------------------------------------------------------------*/
void doEdit(char *directory, char *fname)
{
  char path[MAXPATHLEN];
  char **argv;

  if (resources.default_editor) {

    strcpy(path, resources.default_editor);
    strcat(path, " ");
    strcat(path, fname);

    argv = (char **) XtMalloc(4 * sizeof(char *));
    argv[0] = user.shell;
    argv[1] = "-c";
    argv[2] = path;
    argv[3] = NULL;

    executeApplication(user.shell, directory, argv);
  
    XTFREE(argv);
  }

  else
    error("No default editor", "");
}
/*---------------------------------------------------------------------------*/
void doView(char *directory, char *fname)
{
  char path[MAXPATHLEN];
  char **argv;

  if (resources.default_viewer) {

    strcpy(path, resources.default_viewer);
    strcat(path, " ");
    strcat(path, fname);

    argv = (char **) XtMalloc(4 * sizeof(char *));
    argv[0] = user.shell;
    argv[1] = "-c";
    argv[2] = path;
    argv[3] = NULL;

    executeApplication(user.shell, directory, argv);
  
    XTFREE(argv);
  }

  else
    error("No default viewer", "");
}
/*---------------------------------------------------------------------------*/
void doXterm(char *directory)
{
  char path[MAXPATHLEN];
  char **argv;

  if (resources.default_xterm) {

    strcpy(path, resources.default_xterm);

    argv = (char **) XtMalloc(4 * sizeof(char *));
    argv[0] = user.shell;
    argv[1] = "-c";
    argv[2] = path;
    argv[3] = NULL;

    executeApplication(user.shell, directory, argv);
  
    XTFREE(argv);
  }

  else
    error("No default xterm", "");
}
