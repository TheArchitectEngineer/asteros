/*---------------------------------------------------------------------------
  Module FmFwCb

  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994

  modified 7-1997 by strauman@sun6hft.ee.tu-berlin.de to add
  different enhancements (see README-NEW).

  Callback routines for widgets in a file window
---------------------------------------------------------------------------*/

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Toggle.h>

#include "FileList.h"
#include "TextFileList.h"

#include <string.h>
#include <stdio.h>

#include "Fm.h"

#ifdef ENHANCE_SELECTION
#include "FmSelection.h"
#endif
/*-----------------------------------------------------------------------------
  This function is also used in FmFwActions when a directory is pulled onto
  the root window. In this case, w will be zero and we use this to popup
  the new window by the cursor.
-----------------------------------------------------------------------------*/
void fileOpenCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;
  char pathname[MAXPATHLEN];

  if (fw == NULL)
    fw = popup_fw;

  for (i=0; i<fw->n_files; i++) {
    if (fw->files[i]->selected && S_ISDIR(fw->files[i]->stats.st_mode)) {
      strcpy(pathname, fw->directory);
      if (pathname[strlen(pathname)-1] != '/')
	strcat(pathname, "/");
      strcat(pathname, fw->files[i]->name);      
      newFileWindow(pathname,resources.default_display_type,
		    w ? False : True);
    }
  }
}

#ifdef ENHANCE_MENU
/*---------------------------------------------------------------------------*/


void fileCloneCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{

 newFileWindow(fw->directory,fw->display_type,True);
 /* new file window was created at the beginning of the list */
 file_windows[0].showInode=fw->showInode;
 file_windows[0].showType=fw->showType;
 file_windows[0].showOwner=fw->showOwner;
 file_windows[0].showPermissions=fw->showPermissions;
 file_windows[0].showGroup=fw->showGroup;
 file_windows[0].showLinks=fw->showLinks;
 file_windows[0].showLength=fw->showLength;
 file_windows[0].showDate=fw->showDate;
 
 if (fw->display_type==Text) updateTxtOpts(&file_windows[0]);
}
#endif

/*---------------------------------------------------------------------------*/

void fileEditCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;

  if (fw == NULL)
    fw = popup_fw;

  for (i=0; i<fw->n_files; i++)
    if (fw->files[i]->selected && !S_ISDIR(fw->files[i]->stats.st_mode))
	doEdit(fw->directory,fw->files[i]->name);
}

/*---------------------------------------------------------------------------*/

void fileViewCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;

  if (fw == NULL)
    fw = popup_fw;

  for (i=0; i<fw->n_files; i++)
    if (fw->files[i]->selected && !S_ISDIR(fw->files[i]->stats.st_mode))
	doView(fw->directory,fw->files[i]->name);
}

/*---------------------------------------------------------------------------*/

void xtermCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  if (fw == NULL)
    fw = popup_fw;
  doXterm(fw->directory);
}

/*---------------------------------------------------------------------------*/

void fileTreeCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
#ifdef ENHANCE_SCROLL
  Boolean keep_position=(fw->display_type==Tree);
#endif
  fw->display_type = Tree;
#ifdef ENHANCE_SCROLL
  updateFileDisplay(fw,keep_position);
#else
  updateFileDisplay(fw);
#endif
}

/*---------------------------------------------------------------------------*/

void fileIconsCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  DisplayType t = fw->display_type;

#ifdef ENHANCE_SCROLL
  Boolean keep_position= (t==Icons);
#endif

  fw->display_type = Icons;
  if (t == Text)
    reDisplayFileWindow(fw);
  else
#ifdef ENHANCE_SCROLL
    updateFileDisplay(fw,keep_position);
#else
    updateFileDisplay(fw);
#endif
}

/*---------------------------------------------------------------------------*/

void fileTextCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  DisplayType t = fw->display_type;

#ifdef ENHANCE_SCROLL
  Boolean keep_position= (t==Text);
#endif

  fw->display_type = Text;
  if (t == Icons)
    reDisplayFileWindow(fw);
  else
#ifdef ENHANCE_SCROLL
    updateFileDisplay(fw,keep_position);
#else
    updateFileDisplay(fw);
#endif
}

/*---------------------------------------------------------------------------*/

void fileSelectAllCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;
  Pixel pix;
  int onewid=(fw->icon_box && XtIsSubclass(fw->icon_box,fileListWidgetClass));
  
  fw->n_selections = 0;
  fw->n_bytes_selected = 0;
  for (i=0; i < fw->n_files; i++) {
    if ( (onewid || fw->files[i]->icon.toggle) &&
	strcmp(fw->files[i]->name, ".") &&
	strcmp(fw->files[i]->name, "..")) {
      fw->files[i]->selected = True;
      fw->n_selections++;
      fw->n_bytes_selected += fw->files[i]->stats.st_size;
    }
    else
      fw->files[i]->selected = False;
    if ( !onewid && fw->files[i]->icon.toggle) {
      XtVaGetValues(fw->files[i]->icon.toggle,
		    fw->files[i]->selected?XtNforeground:XtNbackground, &pix,
		    NULL);
      XtVaSetValues(fw->files[i]->icon.toggle, XtNborder, (XtArgVal) pix,
		    NULL);
    }
  }
  updateStatus(fw);
  /* normally the mayority of files is not selected, so we just force
   * a redraw of everything.
   */

  if (onewid)
    XClearArea(XtDisplay(fw->icon_box),XtWindow(fw->icon_box),0,0,0,0,True);
#ifdef ENHANCE_SELECTION
  FmOwnSelection(fw,CurrentTime);
#endif
}

/*---------------------------------------------------------------------------*/

void fileDeselectCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;
  Pixel pix;
  int onewid=(fw->icon_box && XtIsSubclass(fw->icon_box,fileListWidgetClass));
  
  for (i=0; i < fw->n_files; i++)
    if (fw->files[i]->selected && (onewid || fw->files[i]->icon.toggle)) {
      fw->files[i]->selected = False;
      if (!onewid) {
        XtVaGetValues(fw->files[i]->icon.toggle, XtNbackground, &pix, NULL);
        XtVaSetValues(fw->files[i]->icon.toggle, XtNborder, (XtArgVal) pix,
		    NULL);
      } else FileListRefreshItem((FileListWidget)fw->icon_box,i);
    }
  fw->n_selections = 0;
  fw->n_bytes_selected = 0;
  updateStatus(fw);
#ifdef ENHANCE_SELECTION
  FmDisownSelection(fw);
#endif
}

#ifdef ENHANCE_SELECTION
/*---------------------------------------------------------------------------*/

void selectionOwnCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
 FmOwnSelection(fw,CurrentTime);
}
#endif

/*---------------------------------------------------------------------------*/

void fileSortNameCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  fw->sort_type = SortByName;
  reSortFileDisplay(fw);
}

/*---------------------------------------------------------------------------*/

void fileSortSizeCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  fw->sort_type = SortBySize;
  reSortFileDisplay(fw);
}

/*---------------------------------------------------------------------------*/

void fileSortMTimeCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  fw->sort_type = SortByMTime;
  reSortFileDisplay(fw);
}

/*---------------------------------------------------------------------------*/

void fileCloseCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  FileWindowRec *p;
  int d;

  if (fw == file_windows && fw->next == NULL && !resources.appmgr)
    if (!resources.confirm_quit || confirm("", "Exit file manager?", ""))
      quit();
    else
      return;

  if ((d = findDev(fw->directory)) != -1) umountDev(d);

#ifdef ENHANCE_BUGFIX
  /* The registered event handler should be removed */
  XtRemoveEventHandler(fw->shell,XtAllEvents,True,
	(XtEventHandler)clientMessageHandler,(XtPointer)0);
#endif
#ifdef ENHANCE_SELECTION
  /* give the selection up now (might be called
   * after destroying fw->shell!
   */
  FmDisownSelection(fw);
#endif

  XtDestroyWidget(fw->shell);
  
  if (fw == file_windows)
    file_windows = fw->next;
  else {
    for (p = file_windows; p->next != fw; p = p->next);
    p->next = fw->next;
  }

  freeFileList(fw);
  XTFREE(fw->file_items);
  XTFREE(fw->folder_items);
  XTFREE(fw->view_items);
  XTFREE(fw->option_items);
  XTFREE(fw);

  chdir(user.home);
}

/*---------------------------------------------------------------------------*/

void fileHomeCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
#ifdef ENHANCE_SCROLL
  Boolean keep_position=True;
#endif
  freeze = True;
  if (chdir(user.home))
    sysError("Can't open folder:");
  else if (!getwd(fw->directory))
    sysError("System error:");
#ifdef ENHANCE_SCROLL
  else keep_position=(!strcmp(fw->directory,user.home));

  updateFileDisplay(fw,keep_position);
#else
  updateFileDisplay(fw);
#endif
  freeze = False;
}

/*---------------------------------------------------------------------------*/

void fileUpCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
#ifdef ENHANCE_SCROLL
  Boolean keep_position=True;
#endif
  freeze = True;
  if (chdir(fw->directory) || chdir(".."))
    sysError("Can't open folder:");
  else if (!getwd(fw->directory))
    sysError("System error:");
#ifdef ENHANCE_SCROLL
  else keep_position=False;

  updateFileDisplay(fw,keep_position);
#else
  updateFileDisplay(fw);
#endif
  freeze = False;
}

/*---------------------------------------------------------------------------*/

void mainArrowCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;

  freeze = True;
  for (i=0; i<fw->n_files; i++)
    if (fw->files[i]->icon.arrow == w) {
      if (chdir(fw->directory) || chdir(fw->files[i]->name))
	sysError("Can't open folder:");
      else if (!getwd(fw->directory))
	sysError("System error:");
      break;
    }
#ifdef ENHANCE_SCROLL
  updateFileDisplay(fw,False);
#else
  updateFileDisplay(fw);
#endif
  freeze = False;
}


/*---------------------------------------------------------------------------*/

void fileShowDirsCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  fw->show_dirs = !fw->show_dirs;
#ifdef ENHANCE_SCROLL
  updateFileDisplay(fw,True);
#else
  updateFileDisplay(fw);
#endif
}

/*---------------------------------------------------------------------------*/

void fileDirsFirstCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  fw->dirs_first = !fw->dirs_first;
  reSortFileDisplay(fw);
}

/*---------------------------------------------------------------------------*/

void fileShowHiddenCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  fw->show_hidden = !fw->show_hidden;
#ifdef ENHANCE_SCROLL
  updateFileDisplay(fw,True);
#else
  updateFileDisplay(fw);
#endif
}

/*---------------------------------------------------------------------------*/

void timeoutCb(XtPointer data, XtIntervalId *id)
{
  if (!freeze) intUpdate();
  XtAppAddTimeOut(app_context, resources.update_interval, timeoutCb, NULL);
}

/*---------------------------------------------------------------------------*/

void showTxtOptsCb(Widget w, FileWindowRec *fw, XtPointer cad)
{
 int i;
 Boolean state,*fw_state=0;
 Widget tw=fw->icon_box;
 String resource=0;

 i=0;
 while(i<8 && w!=fw->option_items[i])i++;
 if (i>=8) return; /* not found */

 switch(i) {
	case 0: resource=XtNshowInode; fw_state=&fw->showInode; break;
	case 1: resource=XtNshowType;  fw_state=&fw->showType; break;
	case 2: resource=XtNshowPermissions; fw_state=&fw->showPermissions; break;
	case 3: resource=XtNshowLinks; fw_state=&fw->showLinks; break;
	case 4: resource=XtNshowOwner; fw_state=&fw->showOwner; break;
	case 5: resource=XtNshowGroup; fw_state=&fw->showGroup; break;
	case 6: resource=XtNshowLength;fw_state=&fw->showLength; break;
	case 7: resource=XtNshowDate;  fw_state=&fw->showDate; break;
 }

 XtVaGetValues(tw,resource,&state,NULL);
 state=!state;
 if (state) tick(w);
 else	    noTick(w);
 XtVaSetValues(tw,resource,state,NULL);

 /* update the state in the FileWindowRec as well */
 *fw_state=state;
}
