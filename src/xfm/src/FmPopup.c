/*---------------------------------------------------------------------------
  Module FmPopup

  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994

  Routines for creating and managing popup forms and dialog boxes

  modified 1-29-95 by rodgers@lvs-emh.lvs.loral.com (Kevin M. Rodgers)
  to add filtering of icon/text directory displays by a filename filter.

  modified 7-1997 by strauman@sun6hft.ee.tu-berlin.de to add
  different enhancements (see README-NEW).

---------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Toggle.h>

#include "FileList.h"

#include "Fm.h"

#ifdef ENHANCE_HISTORY
#include "Am.h" 	/* need to know aw.shell */
#include "FmHistory.h"
#endif

#ifdef ENHANCE_SELECTION
#include "FmSelection.h"
#endif

/*---------------------------------------------------------------------------
  STATIC DATA
---------------------------------------------------------------------------*/

typedef struct {
  FileWindowRec *fw;
  Widget mkdir, createFile, goTo, rename, move, copy, link, select, filter;
  char s[MAXPATHLEN], t[MAXPATHLEN];
  char select_s[MAXPATHLEN];
  char filter_s[MAXPATHLEN];
  char mkdir_s[MAXPATHLEN];
  char createFile_s[MAXPATHLEN];
  char goTo_s[MAXPATHLEN];
  char rename_s[MAXPATHLEN];
  char move_s[MAXPATHLEN];
  char copy_s[MAXPATHLEN];
  char link_s[MAXPATHLEN];
} PopupsRec;

static PopupsRec popups;

/*---------------------------------------------------------------------------
  PRIVATE FUNCTIONS
---------------------------------------------------------------------------*/

char *dir_prefix(char *dir, char *path)
{
  char *p;
  if ((p = strrchr(path, '/'))) {
    strcpy(dir, path);
    if (p == path)
      dir[1] = '\0';
    else
      dir[p-path] = '\0';
  } else
    dir[0] = '\0';
  return dir;
}

static FmCallbackProc 
  mkdirOkCb, mkdirCancelCb, createFileOkCb, createFileCancelCb, goToOkCb,
  goToCancelCb, moveOkCb, moveCancelCb, copyOkCb, copyCancelCb, linkOkCb,
  linkCancelCb, selectAddCb, selectRemoveCb, selectCancelCb, selectReplaceCb,
  filterOkCb, filterClearCb, filterCancelCb;

static void mkdirOkCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  XtPopdown(popups.mkdir);
  fnexpand(popups.mkdir_s);
  if (chdir(popups.fw->directory))
    sysError("System error:");
  else if (mkdir(popups.mkdir_s, user.umask & 0777)) {
    char s[0xff];
    sprintf(s, "Error creating folder %s:", popups.mkdir_s);
    sysError(s);
  } else
    intUpdate();
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void mkdirCancelCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  XtPopdown(popups.mkdir);
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void createFileOkCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  XtPopdown(popups.createFile);
  fnexpand(popups.createFile_s);
  if (chdir(popups.fw->directory))
    sysError("System error:");
  else if (create(popups.createFile_s, user.umask & 0666)) {
    char s[0xff];
    sprintf(s, "Error creating file %s:", popups.createFile_s);
    sysError(s);
  } else
    intUpdate();
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void createFileCancelCb(Widget w, FileWindowRec *fw, 
			   XtPointer call_data)
{
  XtPopdown(popups.createFile);
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void goToOkCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  char path[MAXPATHLEN];
#ifdef ENHANCE_SCROLL
  Boolean keep_position=True;
#endif

  XtPopdown(popups.goTo);
  fnexpand(popups.goTo_s);
#ifdef ENHANCE_BUGFIX
  /* if it's impossible to chdir(popups.fw->directory)
   * it still might be possible to chdir(popups.goTo_s) !
   * eg. if the actual directory was deleted, an we want to
   * change to another one.
   */
  if (chdir(popups.goTo_s)) {
    char s[0xff];
    sprintf(s, "Can't open folder %s:", popups.goTo_s);
    sysError(s);
	if (chdir(popups.fw->directory)) {
    	  sprintf(s, "Can't open folder %s:", popups.fw->directory);
    	  sysError(s);
	}
#else
  if (chdir(popups.fw->directory) || chdir(popups.goTo_s)) {
    char s[0xff];
    sprintf(s, "Can't open folder %s:", popups.goTo_s);
    sysError(s);
#endif
  } else if (!getwd(path))
    sysError("System error:");
  else {
#ifdef ENHANCE_SCROLL
    if (!(keep_position= (!strcmp(popups.fw->directory,path))))
#endif
    strcpy(popups.fw->directory, path);
#ifdef ENHANCE_SCROLL
    updateFileDisplay(popups.fw,keep_position);
#else
    updateFileDisplay(popups.fw);
#endif
  }
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void goToCancelCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  XtPopdown(popups.goTo);
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void renameOkCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  struct stat stats;
  int i;
  char *from = NULL, to[MAXPATHLEN], todir[MAXPATHLEN];

  XtPopdown(popups.rename);
  strcpy(to, popups.rename_s);
  fnexpand(to);

  if (chdir(popups.fw->directory))

    sysError("System error:");

  else {

    struct stat stats1;

    for (i = 0; i < popups.fw->n_files; i++)
      if (popups.fw->files[i]->selected) {
	from = popups.fw->files[i]->name;
	break;
      }

    if (!strcmp(from, ".") || !strcmp(from, "..")) {
      error("Cannot rename . or ..", "");
      goto out;
    } else if (!lstat(to, &stats) && !lstat(from, &stats1) &&
	       stats.st_ino == stats1.st_ino) {
      error("Rename:", "Source and destination are identical");
      goto out;
    }

    if (exists(to) && resources.confirm_overwrite) {
      char s[0xff];
      sprintf(s, "Rename: file %s already exists", to);
      if (!confirm(s, "Overwrite?", ""))
	goto out;
    }

    if (rmove(from, to)) {
      char s[0xff];
      sprintf(s, "Error renaming %s:", from);
      sysError(s);
    } else {
      dir_prefix(todir, to);
      if ((*todir?chdir(todir):0) || !getwd(todir))
	sysError("System error:");
      markForUpdate(popups.fw->directory); markForUpdate(todir);
      intUpdate();
    }

  }

 out:
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void renameCancelCb(Widget w, FileWindowRec *fw, 
			   XtPointer call_data)
{
  XtPopdown(popups.rename);
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void moveOkCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  struct stat stats;
  int i, toi, n_moved = 0;
  char *from = NULL, to[MAXPATHLEN], todir[MAXPATHLEN];

  XtPopdown(popups.move);
  strcpy(to, popups.move_s);
  fnexpand(to);

  if (chdir(popups.fw->directory))

    sysError("System error:");

  else {

    /* if target exists and is a directory, move the source into that
       directory */

    if (!stat(to, &stats) && S_ISDIR(stats.st_mode)) {

      if (chdir(to) || !getwd(to) || chdir(popups.fw->directory)) {
	sysError("System error:");
	goto out;
      } else if (!strcmp(popups.fw->directory, to)) {
	error("Move:", "Source and destination are identical");
	goto out;
      }

      strcpy(todir, to);

      toi = strlen(to);
      if (to[toi-1] != '/') {
	to[toi++] = '/';
	to[toi] = '\0';
      }

      for (i=0; i < popups.fw->n_files; i++)
	if (popups.fw->files[i]->selected) {
	  if (!strcmp(popups.fw->files[i]->name, ".") ||
	      !strcmp(popups.fw->files[i]->name, "..")) {
	    error("Cannot move . or ..", "");
	    continue;
	  }
	  from = popups.fw->files[i]->name;
	  strcpy(to+toi, from);
	  if (exists(to) && resources.confirm_overwrite) {
	    char s[0xff];
	    sprintf(s, "Move: file %s already exists at destination", from);
	    if (!confirm(s, "Overwrite?", ""))
	      if (aborted)
		break;
	      else
		continue;
	  }
	  if (rmove(from,to)) {
	    char s[0xff];
	    sprintf(s, "Error moving %s:", from);
	    sysError(s);
	  } else
	    n_moved++;
	}
    }

    /* otherwise only a single file may be selected; move it to the target
       file */

    else if (popups.fw->n_selections > 1) {

      error("Move: target for multiple files", "must be a folder");
      goto out;

    } else {

      struct stat stats1;

      for (i = 0; i < popups.fw->n_files; i++)
	if (popups.fw->files[i]->selected) {
	  from = popups.fw->files[i]->name;
	  break;
	}

      if (!strcmp(from, ".") || !strcmp(from, "..")) {
	error("Cannot move . or ..", "");
	goto out;
      } else if (!lstat(to, &stats) && !lstat(from, &stats1) &&
		 stats.st_ino == stats1.st_ino) {
	error("Move:", "Source and destination are identical");
	goto out;
      }

      if (exists(to) && resources.confirm_overwrite) {
	char s[0xff];
	sprintf(s, "Move: file %s already exists", to);
	if (!confirm(s, "Overwrite?", ""))
	  goto out;
      }

      if (rmove(from, to)) {
	char s[0xff];
	sprintf(s, "Error moving %s:", from);
	sysError(s);
      } else {
	n_moved = 1;
	dir_prefix(todir, to);
	if ((*todir?chdir(todir):0) || !getwd(todir))
	  sysError("System error:");
      }
    }

    if (n_moved) {
      markForUpdate(popups.fw->directory); markForUpdate(todir);
      intUpdate();
    }

  }

 out:
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void moveCancelCb(Widget w, FileWindowRec *fw, 
			   XtPointer call_data)
{
  XtPopdown(popups.move);
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void copyOkCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  struct stat stats;
  int i, toi, n_copied = 0;
  char *from = NULL, to[MAXPATHLEN], todir[MAXPATHLEN];

  XtPopdown(popups.copy);
  strcpy(to, popups.copy_s);
  fnexpand(to);

  if (chdir(popups.fw->directory))

    sysError("System error:");

  else {

    /* if target exists and is a directory, copy the source into that
       directory */

    if (!stat(to, &stats) && S_ISDIR(stats.st_mode)) {

      if (chdir(to) || !getwd(to) || chdir(popups.fw->directory)) {
	sysError("System error:");
	goto out;
      } else if (!strcmp(popups.fw->directory, to)) {
	error("Copy:", "Source and destination are identical");
	goto out;
      }

      strcpy(todir, to);

      toi = strlen(to);
      if (to[toi-1] != '/') {
	to[toi++] = '/';
	to[toi] = '\0';
      }

      for (i=0; i < popups.fw->n_files; i++)
	if (popups.fw->files[i]->selected) {
	  if (!strcmp(popups.fw->files[i]->name, ".") ||
	      !strcmp(popups.fw->files[i]->name, "..")) {
	    error("Cannot copy . or ..", "");
	    continue;
	  }
	  from = popups.fw->files[i]->name;
	  strcpy(to+toi, from);
	  if (exists(to) && resources.confirm_overwrite) {
	    char s[0xff];
	    sprintf(s, "Copy: file %s already exists at destination", from);
	    if (!confirm(s, "Overwrite?", ""))
	      if (aborted)
		break;
	      else
		continue;
	  }
	  if (rcopy(from,to)) {
	    char s[0xff];
	    sprintf(s, "Error copying %s:", from);
	    sysError(s);
	  } else
	    n_copied++;
	}
    }

    /* otherwise only a single file may be selected; copy it to the target
       file */

    else if (popups.fw->n_selections > 1) {

      error("Copy: target for multiple files", "must be a folder");
      goto out;

    } else {

      struct stat stats1;

      for (i = 0; i < popups.fw->n_files; i++)
	if (popups.fw->files[i]->selected) {
	  from = popups.fw->files[i]->name;
	  break;
	}

      if (!strcmp(from, ".") || !strcmp(from, "..")) {
	error("Cannot copy . or ..", "");
	goto out;
      } else if (!lstat(to, &stats) && !lstat(from, &stats1) &&
		 stats.st_ino == stats1.st_ino) {
	error("Copy:", "Source and destination are identical");
	goto out;
      }

      if (exists(to) && resources.confirm_overwrite) {
	char s[0xff];
	sprintf(s, "Copy: file %s already exists", to);
	if (!confirm(s, "Overwrite?", ""))
	  goto out;
      }

      if (rcopy(from, to)) {
	char s[0xff];
	sprintf(s, "Error copying %s:", from);
	sysError(s);
      } else {
	n_copied = 1;
	dir_prefix(todir, to);
	if ((*todir?chdir(todir):0) || !getwd(todir))
	  sysError("System error:");
      }
    }

    if (n_copied) {
      markForUpdate(todir);
      intUpdate();
    }

  }

 out:
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void copyCancelCb(Widget w, FileWindowRec *fw, 
			   XtPointer call_data)
{
  XtPopdown(popups.copy);
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void linkOkCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  struct stat stats;
  int i, namei, toi, n_linked = 0;
  char *from = NULL, name[MAXPATHLEN], to[MAXPATHLEN], todir[MAXPATHLEN];

  XtPopdown(popups.link);
  strcpy(to, popups.link_s);
  fnexpand(to);

  strcpy(name, popups.fw->directory);

  namei = strlen(name);
  if (name[namei-1] != '/') {
    name[namei++] = '/';
    name[namei] = '\0';
  }

  if (chdir(popups.fw->directory))

    sysError("System error:");

  else {

    /* if target exists and is a directory, link the source into that
       directory */

    if (!stat(to, &stats) && S_ISDIR(stats.st_mode)) {

      if (chdir(to) || !getwd(to) || chdir(popups.fw->directory)) {
	sysError("System error:");
	goto out;
      } else if (!strcmp(popups.fw->directory, to)) {
	error("Link:", "Source and destination are identical");
	goto out;
      }

      strcpy(todir, to);

      toi = strlen(to);
      if (to[toi-1] != '/') {
	to[toi++] = '/';
	to[toi] = '\0';
      }

      for (i=0; i < popups.fw->n_files; i++)
	if (popups.fw->files[i]->selected) {
	  from = popups.fw->files[i]->name;
	  strcpy(name+namei, from);
	  strcpy(to+toi, from);
	  if (exists(to) && resources.confirm_overwrite) {
	    char s[0xff];
	    sprintf(s, "Link: file %s already exists at destination", from);
	    if (!confirm(s, "Overwrite?", ""))
	      if (aborted)
		break;
	      else
		continue;
	  }
	  if (symlink(name,to)) {
	    char s[0xff];
	    sprintf(s, "Error linking %s:", from);
	    sysError(s);
	  } else
	    n_linked++;
	}
    }

    /* otherwise only a single file may be selected; link it to the target
       file */

    else if (popups.fw->n_selections > 1) {

      error("Link: target for multiple files", "must be a folder");
      goto out;

    } else {

      struct stat stats1;

      for (i = 0; i < popups.fw->n_files; i++)
	if (popups.fw->files[i]->selected) {
	  from = popups.fw->files[i]->name;
	  break;
	}

      strcpy(name+namei, from);

      if (!lstat(to, &stats) && !lstat(from, &stats1) &&
		 stats.st_ino == stats1.st_ino) {
	error("Link:", "Source and destination are identical");
	goto out;
      }

      if (exists(to) && resources.confirm_overwrite) {
	char s[0xff];
	sprintf(s, "Link: file %s already exists", to);
	if (!confirm(s, "Overwrite?", ""))
	  goto out;
      }

      if (symlink(name, to)) {
	char s[0xff];
	sprintf(s, "Error linking %s:", from);
	sysError(s);
      } else {
	n_linked = 1;
	dir_prefix(todir, to);
	if ((*todir?chdir(todir):0) || !getwd(todir))
	  sysError("System error:");
      }
    }

    if (n_linked) {
      markForUpdate(todir);
      intUpdate();
    }

  }

 out:
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void linkCancelCb(Widget w, FileWindowRec *fw, 
			   XtPointer call_data)
{
  XtPopdown(popups.link);
  freeze = False;
}

/*---------------------------------------------------------------------------*/

/* The following variant of fnmatch matches the . and .. dirs only if
   specified explicitly. */

#define fnmatchnodot(pattern,fn) (strcmp(fn,".")&&strcmp(fn,"..")? \
				  fnmatch(pattern,fn):!strcmp(pattern,fn))

static void selectReplaceCb(Widget w, FileWindowRec *fw, 
			    XtPointer call_data)
{
  int i;
  Pixel pix;
  Boolean new_val;
  int onewid=XtIsSubclass(popups.fw->icon_box,fileListWidgetClass);

  XtPopdown(popups.select);
  popups.fw->n_selections = 0;
  popups.fw->n_bytes_selected = 0;
  for (i=0; i<popups.fw->n_files; i++) {
    if (onewid || popups.fw->files[i]->icon.toggle) {
      if (fnmatchnodot(popups.select_s, popups.fw->files[i]->name)) {
	new_val = True;
	popups.fw->n_selections++;
	popups.fw->n_bytes_selected += popups.fw->files[i]->stats.st_size;
      }
      else
	new_val = False;
      if (new_val!= popups.fw->files[i]->selected) {
	popups.fw->files[i]->selected = new_val;
        if (!onewid) {
          XtVaGetValues(popups.fw->files[i]->icon.toggle,
	  	      popups.fw->files[i]->selected?XtNforeground:XtNbackground,
		      &pix, NULL);
          XtVaSetValues(popups.fw->files[i]->icon.toggle, XtNborder,
		      (XtArgVal) pix, NULL);
        } else 
	  FileListRefreshItem((FileListWidget)popups.fw->icon_box,i);
     }
    }
  }
  updateStatus(popups.fw);
  freeze = False;
#ifdef ENHANCE_SELECTION
  FmOwnSelection(popups.fw,CurrentTime);
#endif
}

/*---------------------------------------------------------------------------*/

static void selectAddCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;
  Pixel pix;
  int onewid=XtIsSubclass(popups.fw->icon_box,fileListWidgetClass);
  
  XtPopdown(popups.select);
  for(i=0; i<popups.fw->n_files; i++)
    if (onewid || popups.fw->files[i]->icon.toggle) {
      if (!popups.fw->files[i]->selected && 
	  (fnmatchnodot(popups.select_s, popups.fw->files[i]->name))) {
	popups.fw->files[i]->selected = True;
	popups.fw->n_selections++;
	popups.fw->n_bytes_selected += popups.fw->files[i]->stats.st_size;
	if (!onewid) {
	  XtVaGetValues(popups.fw->files[i]->icon.toggle, XtNforeground, &pix,
		      NULL);
	  XtVaSetValues(popups.fw->files[i]->icon.toggle, XtNborder,
		      (XtArgVal) pix, NULL);
	} else FileListRefreshItem((FileListWidget)popups.fw->icon_box,i);
      }
    }
  updateStatus(popups.fw);
  freeze = False;
#ifdef ENHANCE_SELECTION
  FmOwnSelection(popups.fw,CurrentTime);
#endif
}

/*---------------------------------------------------------------------------*/

static void selectRemoveCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;
  Pixel pix;
  int onewid=XtIsSubclass(popups.fw->icon_box,fileListWidgetClass);
  
  XtPopdown(popups.select);
  for(i=0; i<popups.fw->n_files; i++)
    if (onewid || popups.fw->files[i]->icon.toggle) {
      if (popups.fw->files[i]->selected && 
	  (fnmatch(popups.select_s, popups.fw->files[i]->name))) {
	popups.fw->files[i]->selected = False;
	popups.fw->n_selections--;
	popups.fw->n_bytes_selected -= popups.fw->files[i]->stats.st_size;
	if (!onewid) {
	  XtVaGetValues(popups.fw->files[i]->icon.toggle, XtNbackground, &pix,
		      NULL);
	  XtVaSetValues(popups.fw->files[i]->icon.toggle, XtNborder,
		      (XtArgVal) pix, NULL);
	} else FileListRefreshItem((FileListWidget)popups.fw->icon_box,i);
      }
    }
  updateStatus(popups.fw);
  freeze = False;
#ifdef ENHANCE_SELECTION
  FmOwnSelection(popups.fw,CurrentTime);
#endif
}

/*---------------------------------------------------------------------------*/

static void selectCancelCb(Widget w, FileWindowRec *fw, 
			   XtPointer call_data)
{
  XtPopdown(popups.select);
  freeze = False;
}

/*---------------------------------------------------------------------------*/

/* KMR */
static void filterCancelCb(Widget w, FileWindowRec *fw, 
			   XtPointer call_data)
{
  XtPopdown(popups.filter);
  freeze = False;
}

/*---------------------------------------------------------------------------*/

/* KMR */
static void filterOkCb(Widget w, FileWindowRec *fw, 
			   XtPointer call_data)
{
  XtPopdown(popups.filter);
  popups.fw->do_filter = True;
  strcpy(popups.fw->dirFilter,popups.filter_s);
#ifdef ENHANCE_SCROLL
  updateFileDisplay(popups.fw,True);
#else
  updateFileDisplay(popups.fw);
#endif
  freeze = False;
}

/*---------------------------------------------------------------------------*/

/* KMR */
static void filterClearCb(Widget w, FileWindowRec *fw, 
			   XtPointer call_data)
{
  XtPopdown(popups.filter);
  popups.fw->do_filter = False;
  popups.fw->dirFilter[0] = '\0';
#ifdef ENHANCE_SCROLL
  updateFileDisplay(popups.fw,True);
#else
  updateFileDisplay(popups.fw);
#endif
  freeze = False;
}

/*---------------------------------------------------------------------------
  Button information for popups
---------------------------------------------------------------------------*/

static ButtonRec mkdir_buttons[] = {
  { "ok", "Ok", mkdirOkCb },
  { "cancel", "Cancel", mkdirCancelCb }
};

static ButtonRec createFile_buttons[] = {
  { "ok", "Ok", createFileOkCb },
  { "cancel", "Cancel", createFileCancelCb }
};

static ButtonRec goTo_buttons[] = {
  { "ok", "Ok", goToOkCb },
  { "cancel", "Cancel", goToCancelCb }
};

static ButtonRec rename_buttons[] = {
  { "ok", "Ok", renameOkCb },
  { "cancel", "Cancel", renameCancelCb }
};

static ButtonRec move_buttons[] = {
  { "ok", "Ok", moveOkCb },
  { "cancel", "Cancel", moveCancelCb }
};

static ButtonRec copy_buttons[] = {
  { "ok", "Ok", copyOkCb },
  { "cancel", "Cancel", copyCancelCb }
};

static ButtonRec link_buttons[] = {
  { "ok", "Ok", linkOkCb },
  { "cancel", "Cancel", linkCancelCb }
};

static ButtonRec select_buttons[] = {
  { "replace", "Replace", selectReplaceCb },
  { "add", "Add", selectAddCb },
  { "remove", "Remove", selectRemoveCb },
  { "cancel", "Cancel", selectCancelCb }
};

static ButtonRec filter_buttons[] = {       /* KMR */
  { "ok", "Ok", filterOkCb },
  { "clear", "Clear", filterClearCb },
  { "cancel", "Cancel", filterCancelCb }
}; 

/*---------------------------------------------------------------------------
  Question information for popups
---------------------------------------------------------------------------*/

static QuestionRec mkdir_questions[] = {
  { "Create folder:", popups.mkdir_s, MAXPATHLEN, NULL }
};

static QuestionRec createFile_questions[] = {
  { "Create file:", popups.createFile_s, MAXPATHLEN, NULL }
};

static QuestionRec goTo_questions[] = {
  { "Go to folder:", popups.goTo_s, MAXPATHLEN, NULL }
};

static QuestionRec rename_questions[] = {
  { "Rename to:", popups.rename_s, MAXPATHLEN, NULL }
};

static QuestionRec move_questions[] = {
  { "Move to:", popups.move_s, MAXPATHLEN, NULL }
};

static QuestionRec copy_questions[] = {
  { "Copy to:", popups.copy_s, MAXPATHLEN, NULL }
};

static QuestionRec link_questions[] = {
  { "Link to:", popups.link_s, MAXPATHLEN, NULL }
};

static QuestionRec select_questions[] = {
  { "Filename pattern:", popups.select_s, MAXPATHLEN, NULL }
};

static QuestionRec filter_questions[] = {             /* KMR */
  { "Filename pattern:", popups.filter_s, MAXPATHLEN, NULL }
};

#ifdef ENHANCE_HISTORY
/*--------------------------------------------------------------------------
  PUBLIC VARIABLES
--------------------------------------------------------------------------*/

HistoryList path_history;

#endif

/*---------------------------------------------------------------------------
  PUBLIC FUNCTIONS
---------------------------------------------------------------------------*/

void createMainPopups()
{
#ifdef ENHANCE_HISTORY
  char *fixed[5];
  int	nfixed;

  /* Path History menu is initialized
   * (need to do this first because the history feature
   * provides an additional action that must be added to the action table
   * so the different popups can use it :-)
   */

  nfixed=0;
  fixed[nfixed++]=user.home;
#ifdef ENHANCE_USERINFO
  fixed[nfixed++]=user.cwd;
#endif
  fixed[nfixed++]=0;
  path_history=FmCreateHistoryList(
	0, /* let the menu's name be "fm_history" */
	aw.shell,
	fixed);

  /* Note: important resources of the history menu are _not_ configured
   *       statically but by the resource files.
   */
#endif

  /* New Folder */
  popups.mkdir = createPopupQuestions("mkdir", "New Folder", bm[DIR_BM], 
			       mkdir_questions, XtNumber(mkdir_questions),
			       mkdir_buttons, XtNumber(mkdir_buttons),
			       XtNumber(mkdir_buttons)-1);

  /* New File */
  popups.createFile = createPopupQuestions("create", "New File", bm[FILE_BM],
			       createFile_questions,
			       XtNumber(createFile_questions),
			       createFile_buttons,
			       XtNumber(createFile_buttons),
			       XtNumber(createFile_buttons)-1);

  /* Change current folder */
  popups.goTo = createPopupQuestions("goto", "Go To", bm[DIR_BM],
			       goTo_questions, XtNumber(goTo_questions),
			       goTo_buttons, XtNumber(goTo_buttons),
			       XtNumber(goTo_buttons)-1);

  /* Rename file */
  popups.rename = createPopupQuestions("rename", "Rename", bm[FILE_BM], 
			       rename_questions, XtNumber(rename_questions),
			       rename_buttons, XtNumber(rename_buttons),
			       XtNumber(rename_buttons)-1);

  /* Move file */
  popups.move = createPopupQuestions("move", "Move", bm[FILES_BM], 
			       move_questions, XtNumber(move_questions),
			       move_buttons, XtNumber(move_buttons),
			       XtNumber(move_buttons)-1);

  /* Copy file */
  popups.copy = createPopupQuestions("copy", "Copy", bm[FILES_BM],
			       copy_questions, XtNumber(copy_questions),
			       copy_buttons, XtNumber(copy_buttons),
			       XtNumber(copy_buttons)-1);

  /* Create link */
  popups.link = createPopupQuestions("link", "Link", bm[SYMLNK_BM],
			       link_questions, XtNumber(link_questions),
			       link_buttons, XtNumber(link_buttons),
			       XtNumber(link_buttons)-1);

  /* Select */
  popups.select = createPopupQuestions("select", "Select", bm[FILES_BM],
			       select_questions, XtNumber(select_questions),
			       select_buttons, XtNumber(select_buttons),
			       XtNumber(select_buttons)-1);

  /* Filter */  /* KMR */
  popups.filter = createPopupQuestions("filter", "Filter", bm[FILES_BM],
			       filter_questions, XtNumber(filter_questions),
			       filter_buttons, XtNumber(filter_buttons),
			       XtNumber(filter_buttons)-1);

  /* Change Access Mode */
  createChmodPopup();

  /* Info */
  createInfoPopup();

  /* Errors */
  createErrorPopup();

  /* Deletions */
  createConfirmPopup();

}

/*---------------------------------------------------------------------------*/ 

void selectPopup(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  popups.fw = fw;
  XtVaSetValues(select_questions[0].widget, XtNstring, 
		(XtArgVal) popups.select_s, NULL);
  freeze = True;
  popupByCursor(popups.select, XtGrabExclusive);
}

/*---------------------------------------------------------------------------*/ 

/* KMR */
void filterPopup(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  popups.fw = fw;
  strcpy(popups.filter_s,fw->dirFilter);
  XtVaSetValues(filter_questions[0].widget, XtNstring, 
		(XtArgVal) popups.filter_s, NULL);
  freeze = True;
  popupByCursor(popups.filter, XtGrabExclusive);
}

/*---------------------------------------------------------------------------*/

void mkdirPopup(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  popups.fw = fw;
  XtVaSetValues(mkdir_questions[0].widget, XtNstring, 
		(XtArgVal) popups.mkdir_s, NULL);
  freeze = True;
  popupByCursor(popups.mkdir, XtGrabExclusive);
}

/*---------------------------------------------------------------------------*/

void createFilePopup(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  popups.fw = fw;
  XtVaSetValues(createFile_questions[0].widget, XtNstring, 
		(XtArgVal) popups.createFile_s, NULL);
  freeze = True;
  popupByCursor(popups.createFile, XtGrabExclusive);
}

/*---------------------------------------------------------------------------*/

void goToPopup(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  popups.fw = fw;
  XtVaSetValues(goTo_questions[0].widget, XtNstring, 
		(XtArgVal) popups.goTo_s, NULL);
  freeze = True;
  popupByCursor(popups.goTo, XtGrabExclusive);
}

/*---------------------------------------------------------------------------*/

void renamePopup(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;

  if (fw == NULL) fw = popup_fw;

  if (!fw->n_selections) return;

  popups.fw = fw;
  for (i = 0; i < popups.fw->n_files; i++)
    if (popups.fw->files[i]->selected) {
      strcpy(popups.rename_s, popups.fw->files[i]->name);
      break;
    }
  XtVaSetValues(rename_questions[0].widget, XtNstring, 
		(XtArgVal) popups.rename_s, NULL);
  freeze = True;
  popupByCursor(popups.rename, XtGrabExclusive);
}

/*---------------------------------------------------------------------------*/

void movePopup(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  if (fw == NULL) fw = popup_fw;

  if (!fw->n_selections) return;

  popups.fw = fw;
  XtVaSetValues(move_questions[0].widget, XtNstring, 
		(XtArgVal) popups.move_s, NULL);
  freeze = True;
  popupByCursor(popups.move, XtGrabExclusive);
}

/*---------------------------------------------------------------------------*/

void copyPopup(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  if (fw == NULL) fw = popup_fw;

  if (!fw->n_selections) return;

  popups.fw = fw;
  XtVaSetValues(copy_questions[0].widget, XtNstring, 
		(XtArgVal) popups.copy_s, NULL);
  freeze = True;
  popupByCursor(popups.copy, XtGrabExclusive);
}

/*---------------------------------------------------------------------------*/

void linkPopup(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  if (fw == NULL) fw = popup_fw;

  if (!fw->n_selections) return;

  popups.fw = fw;
  XtVaSetValues(link_questions[0].widget, XtNstring, 
		(XtArgVal) popups.link_s, NULL);
  freeze = True;
  popupByCursor(popups.link, XtGrabExclusive);
}
