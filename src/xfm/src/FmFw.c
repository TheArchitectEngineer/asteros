/*-----------------------------------------------------------------------------
  Module FmFw.c                                                             

  (c) Simon Marlow 1991
  (c) Albert Graef 1994
                                                                           
  functions & data for creating a file window, and various functions        
  related to file windows                                                   

  modified 1-29-95 by rodgers@lvs-emh.lvs.loral.com (Kevin M. Rodgers)
  to add filtering of icon/text directory displays by a filename filter.

  modified 7-1997 by strauman@sun6hft.ee.tu-berlin.de to add
  different enhancements (see README-NEW).

-----------------------------------------------------------------------------*/
#include <stdlib.h>

#include <pwd.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#ifdef _AIX
#include <sys/resource.h>
#endif

#include <sys/wait.h>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Label.h>

#include "XtHelper.h"

#include "TextFileList.h"
#include "IconFileList.h"

#include "Am.h"
#include "Fm.h"

#ifdef ENHANCE_HISTORY
#include "FmHistory.h"
#endif

#ifdef ENHANCE_SELECTION
#include "FmSelection.h"
#endif

#ifdef ENHANCE_TRANSLATIONS
/* This is a hack in order not to change the resource tables
 * because I don't want to re-index all references to these tables
 * (would have been nicer, if some symbols were defined)
 */
#undef  XtNtranslations
#define XtNtranslations ""
#endif

#ifdef ENHANCE_LOG
#include "FmLog.h"
#endif

#define FW_WIDTH 400
#define FW_HEIGHT 300
#define TEXT_PADDING 10
#define MAXCFGLINELEN 1024

/*-----------------------------------------------------------------------------
  PUBLIC DATA                                       
-----------------------------------------------------------------------------*/

#ifdef ENHANCE_HISTORY
extern HistoryList path_history;
#endif

FileWindowList file_windows = NULL;
FileWindowRec *popup_fw;
Widget file_popup_widget, *file_popup_items;
Widget dir_popup_widget, *dir_popup_items;

int n_types;
TypeList types;

int n_devices;
DevList devs;

/*-----------------------------------------------------------------------------
  STATIC DATA                                       
-----------------------------------------------------------------------------*/

static MenuItemRec file_popup_menu[] = {
  { "edit", "Edit", fileEditCb },
  { "view", "View", fileViewCb },
  { "line1", NULL, NULL },
  { "rename", "Rename...", renamePopup },
  { "line2", NULL, NULL },
  { "move", "Move...", movePopup },
  { "copy", "Copy...", copyPopup },
  { "link", "Link...", linkPopup },
  { "line3", NULL, NULL },
  { "delete", "Delete", deleteItems },
  { "line4", NULL, NULL },
  { "info", "Information...", infoPopup },
  { "chmod", "Permissions...", chmodPopup }
};

static MenuItemRec dir_popup_menu[] = {
  { "open", "Open", fileOpenCb },
  { "line1", NULL, NULL },
  { "rename", "Rename...", renamePopup },
  { "line2", NULL, NULL },
  { "move", "Move...", movePopup },
  { "copy", "Copy...", copyPopup },
  { "link", "Link...", linkPopup },
  { "line3", NULL, NULL },
  { "delete", "Delete", deleteItems },
  { "line4", NULL, NULL },
  { "info", "Information...", infoPopup },
  { "chmod", "Permissions...", chmodPopup }
};

static MenuItemRec file_menu[] = {
  { "new", "New...", createFilePopup },
  { "line1", NULL, NULL },
  { "move", "Move...", movePopup },
  { "copy", "Copy...", copyPopup },
  { "link", "Link...", linkPopup },
  { "line2", NULL, NULL },
  { "delete", "Delete",  deleteItems },
  { "line3", NULL, NULL },
  { "select", "Select...", selectPopup },
  { "select all", "Select all", fileSelectAllCb },
  { "deselect all", "Deselect all", fileDeselectCb },
#ifdef ENHANCE_SELECTION
  { "own Selection", "Own Selection", selectionOwnCb },
#endif
  { "line4", NULL, NULL },
  { "xterm", "Xterm", xtermCb },
  { "line5", NULL, NULL },
  { "about", "About xfm...", aboutCb },
  { "line6", NULL, NULL },
  { "quit", "Quit", appCloseCb },
};

static MenuItemRec folder_menu[] = {
  { "new", "New...", mkdirPopup },
  { "line1", NULL, NULL },
  { "goto", "Go to...", goToPopup },
  { "home", "Home", fileHomeCb },
  { "up", "Up", fileUpCb },
  { "line2", NULL, NULL },
  { "empty", "Empty", emptyDir },
  { "line3", NULL, NULL },
#ifdef ENHANCE_MENU
  { "clone", "Clone", fileCloneCb },
#endif
  { "close", "Close", fileCloseCb },
};

static MenuItemRec view_menu[] = {
  { "tree", "Tree",  fileTreeCb },
  { "icons", "Icons",  fileIconsCb },
  { "text", "Text",  fileTextCb },
  { "line1", NULL,  NULL },
  { "sort by name", "Sort by name",  fileSortNameCb },
  { "sort by size", "Sort by size",  fileSortSizeCb },
  { "sort by mtime", "Sort by date",  fileSortMTimeCb },
  { "line2", NULL,  NULL },
  { "filter", "Filter...", filterPopup },          /* KMR */
  { "line3", NULL,  NULL },
  { "hide folders", "Hide folders",  fileShowDirsCb },
  { "mix folders/files", "Mix folders/files",
       fileDirsFirstCb },
  { "show hidden files", "Show hidden files", fileShowHiddenCb },
#ifdef ENHANCE_LOG
  { "line4", NULL,  NULL },
  { "show log", "Show log", logPopup },
#endif
};

/* Don't change the order of menu entries
 * (showTxtOptsCb() and updateTxtOpts() depend on it)
 */
static MenuItemRec text_opts_menu[]={
  { "show Inode", "Show Inode", showTxtOptsCb},
  { "show Type", "Show Type", showTxtOptsCb},
  { "show Permission", "Show Permission", showTxtOptsCb},
  { "show Link Count", "Show Link Count", showTxtOptsCb},
  { "show User", "Show User", showTxtOptsCb},
  { "show Group", "Show Group", showTxtOptsCb},
  { "show Size", "Show Size", showTxtOptsCb},
  { "show MTime", "Show MTime", showTxtOptsCb},
};

/*-----------------------------------------------------------------------------
  Widget Argument Lists
-----------------------------------------------------------------------------*/

static Arg shell_args[] = {
  { XtNtitle, (XtArgVal) NULL },
  { XtNiconPixmap, (XtArgVal) NULL },
  { XtNiconMask, (XtArgVal) NULL }
};

static Arg form_args[] = {
  { XtNdefaultDistance, (XtArgVal) 0 }
};

static Arg button_box_args[] = {
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainLeft },
};

static Arg label_args[] = {
  { XtNfromVert, (XtArgVal) NULL },
  { XtNlabel, (XtArgVal) NULL },
  { XtNwidth, (XtArgVal) FW_WIDTH },
  { XtNfont, (XtArgVal) NULL },
  { XtNresize, (XtArgVal) False },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainRight },
  { XtNtranslations, (XtArgVal) NULL },
};

static Arg viewport_args[] = {
  { XtNfromVert, (XtArgVal) NULL },
  { XtNwidth, (XtArgVal) FW_WIDTH },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainBottom },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainRight },
  { XtNallowVert, (XtArgVal) True }
};

static Arg status_args[] = {
  { XtNfromVert, (XtArgVal) NULL },
  { XtNlabel, (XtArgVal) NULL },
  { XtNwidth, (XtArgVal) FW_WIDTH },
  { XtNfont, (XtArgVal) NULL },
  { XtNresize, (XtArgVal) False },
  { XtNtop, XtChainBottom },
  { XtNbottom, XtChainBottom },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainRight },
  { XtNjustify, XtJustifyLeft }
};

static Arg text_list_args[] = {
  { XtNwidth,  (XtArgVal) 0 },
  { XtNfont, (XtArgVal)0 },
  { XtNfiles, (XtArgVal) 0},
  { XtNnFiles, (XtArgVal) 0},
  { XtNtranslations, (XtArgVal) NULL},
  { XtNhighlightColor, (XtArgVal) 0 },
};

static Arg icon_list_args[] = {
  { XtNwidth,  (XtArgVal) 0 },
  { XtNfont, (XtArgVal)0 },
  { XtNfiles, (XtArgVal) 0},
  { XtNnFiles, (XtArgVal) 0},
  { XtNminIconWidth, (XtArgVal) 0},
  { XtNminIconHeight, (XtArgVal) 0},
  { XtNtranslations, (XtArgVal) NULL},
  { XtNhighlightColor, (XtArgVal) 0 },
};

static Arg tree_box_args[] = {
  { XtNwidth, (XtArgVal) 1 },
  { XtNdefaultDistance, (XtArgVal) 0 },
  { XtNheight, (XtArgVal) 1 }
};

static Arg tree_form_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNdefaultDistance, (XtArgVal) 0 },
  { XtNwidth, (XtArgVal) 0 },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainLeft },
#ifdef VIEWPORT_HACK
  { XtNresizable, (XtArgVal) True },
#endif
};

static Arg icon_toggle_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNbitmap, (XtArgVal) NULL },
  { XtNtranslations, (XtArgVal) NULL },
  { XtNwidth, (XtArgVal) 0 },
  { XtNheight, (XtArgVal) 0 },
  { XtNforeground, (XtArgVal) 0 },
};

static Arg icon_label_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNlabel, (XtArgVal) NULL },
  { XtNfont, (XtArgVal) NULL },
  { XtNwidth, (XtArgVal) 0 },
  { XtNtranslations, (XtArgVal) NULL },
  { XtNinternalWidth, (XtArgVal) 0 },
  { XtNinternalHeight, (XtArgVal) 0 },
};

static Arg arrow_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNbitmap, (XtArgVal) NULL },
  { XtNsensitive, (XtArgVal) True },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainLeft },
  { XtNinternalWidth, (XtArgVal) 0 },
  { XtNinternalHeight, (XtArgVal) 0 },
  { XtNhighlightThickness, (XtArgVal) 0 }
};

static Arg line_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNbitmap, (XtArgVal) NULL },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainLeft },
  { XtNinternalWidth, (XtArgVal) 0 },
  { XtNinternalHeight, (XtArgVal) 0 }
};

/*-----------------------------------------------------------------------------
  Translation tables
-----------------------------------------------------------------------------*/

#ifndef ENHANCE_TRANSLATIONS
#ifdef ENHANCE_HISTORY
static char label_translations_s[] = "\
  <Btn3Down>	      : FmUpdateHistory(fm_history) MenuPopup(fm_history)\n\
  <Btn1Up>(2)         : fileRefresh()\n";
#else
static char label_translations_s[] = "\
  <Btn1Up>(2)         : fileRefresh()\n";
#endif

static char tree_translations_s[] = "\
  <Enter>             : fileHighlight()\n\
  <Leave>             : resetCursor()\n\
  <Btn1Down>,<Btn1Up> : fileSelect()\n\
  <Btn1Down>,<Leave>  : fileBeginDrag(1,move)\n\
  <Btn2Down>,<Btn2Up> : fileToggle()\n\
  <Btn2Down>,<Leave>  : fileBeginDrag(2,copy)\n\
  <Btn3Down>          : dirPopup()\n";

static char file_list_translations_s[] = "\
  <Enter>	      : dispatchDirExeFile(fileHighlight,fileHighlight,fileMaybeHighlight,\"\")\n\
  <Leave>	      : dispatchDirExeFile(resetCursor,resetCursor,unhighlight,\"\")\n\
  <Btn1Up>(2)         : dispatchDirExeFile(fileOpenDir,fileExecFile,fileExecAction,\"\")\n\
  <Btn1Down>,<Btn1Up> : fileSelect()\n\
  <Btn1Down>,<Motion> : fileBeginDrag(1,move)\n\
  <Btn2Down>,<Btn2Up> : fileToggle()\n\
  <Btn2Down>,<Motion> : fileBeginDrag(2,copy)\n\
  <Btn3Down>          : dispatchDirExeFilePopup(dirPopup,filePopup,filePopup,\"\")\n\
	<Key>Delete   : notify(*file*delete)\n\
	<Key>BackSpace: notify(*file*delete)\n\
	<Key>m	      : notify(*file*move)\n\
	<Key>n	      : notify(*file*new)\n\
	<Key>x	      : notify(*file*xterm)\n\
	<Key>q	      : notify(*file*quit)\n\
	<Key>c	      : notify(*file*copy)\n\
	<Key>s	      : notify(*file*select)\n\
	<Key>o	      : notify(\"*file*own selection\")\n\
	<Key>a	      : notify(\"*file*select all\")\n\
	<Key>u	      : notify(\"*file*deselect all\")\n\
	<Key>l	      : notify(*file*link)\n";

#endif
  
static void dummy(Widget w, XEvent *event, String *params, 
		       Cardinal *num_params) {}


static XtActionsRec file_actions[] = {
  { "fileRefresh", fileRefresh },
  { "fileToggle", fileToggle },
  { "fileSelect", fileSelect },
  { "fileHighlight", fileHighlight },
  { "fileOpenDir", fileOpenDir },
  { "fileBeginDrag", fileBeginDrag },
  { "fileExecFile", fileExecFile },
  { "fileExecAction", fileExecAction },
  { "resetCursor", resetCursor },
  { "trackCursor", trackCursor },
  { "fileMaybeHighlight", fileMaybeHighlight },
  { "filePopup", filePopup },
  { "dirPopup", dirPopup },
  { "dummy", dummy },
};

#ifndef ENHANCE_TRANSLATIONS
static XtTranslations label_translations, file_list_translations, tree_translations;
#endif

/*-----------------------------------------------------------------------------
  PRIVATE FUNCTIONS
-----------------------------------------------------------------------------*/

static int longestName(FileWindowRec *fw)
{
  int i,l;
  int longest = 0;

  for (i=0; i<fw->n_files; i++)
    if ((l = XTextWidth(resources.icon_font, fw->files[i]->name, 
			strlen(fw->files[i]->name))) > longest)
      longest = l;
  return longest;
}

static void
unmapTxtOptsCb(Widget w, XtPointer cld, XtPointer cad)
{
 FileWindowRec	*fw=(FileWindowRec *)cld;
 Widget			options_button;

 /* don't do nothing if our parent is already being
  * destroyed.
  * (fw may be a dangling pointer in this case!)
  */
 if (XtIsBeingDestroyed(XtParent(w))) return;

 /* if they continue to use the options button, return */
 if (--fw->option_in_use >0) return;

 options_button=XtParent(XtParent(fw->option_items[0]));
 if (XtIsRealized(options_button))
   XtUnmapWidget(options_button);
 else
   XtSetMappedWhenManaged(options_button,False);
}

/* This callback is registered on the callback list of a fileList widget.
 * It is called by the 'notify()' action.
 * A pointer to the (null terminated) argument list is passed in the
 * 'call data' parameter. We use it to determine what to do
 *
 * Notify(menu entry path)
 *
 * calls the callback registered with an arbitrary menu entry.
 * the name of the menu entry is resolved by means of
 * 'XtNameToWidget', starting with the FileWindow's shell.
 */
static void fileListCb(Widget w, XtPointer cld, XtPointer cad)
{
 FileWindowRec  *fw=(FileWindowRec*)cld;
 String	        *args=(String*)cad;
 String	        entry_path,mess;
 Widget 	entry;

 if ((entry_path=args[0])==0) {
	XtAppWarning(XtWidgetToApplicationContext(w),
		"fileListCb (Notify): need 1 arg");
	return;
 }

 if ((entry=XtNameToWidget(fw->shell,entry_path))==0) {
	mess=XtMalloc(256);
        sprintf(mess,"fileListCb (Notify): menu entry '%s' not found",entry_path);
	XtAppWarning(XtWidgetToApplicationContext(w),mess);
	XtFree(mess);
 	return;
 }

 XtCallCallbacks(entry,XtNcallback,0);

}

/*---------------------------------------------------------------------------*/

static int parseType(FILE *fp, char **pattern,
#ifdef MAGIC_HEADERS
                     char **magic_type,
#endif
                     char **icon, char **push_action, char **drop_action)
{
  static char s[MAXCFGLINELEN];
  int l;

 start:
  if (feof(fp)||!fgets(s, MAXCFGLINELEN, fp))
    return 0;
  l = strlen(s);
  if (s[l-1] == '\n')
    s[--l] = '\0';
  if (!l || *s == '#')
    goto start;
  if (!(*pattern = split(s, ':')))
    return -1;
#ifdef MAGIC_HEADERS
  if (**pattern == '<') {
    char *ptr;
    ptr = *pattern + 1;
    while(*ptr && (*ptr != '>' || ptr[-1] == '\\'))
	ptr++;
    if(*ptr != '>')
	return -1;
    *ptr = '\0';
    *magic_type = *pattern + 1;
    *pattern = ptr + 1;
  }
  else
    *magic_type = NULL;
#endif
  if (!(*icon = split(NULL, ':')))
    return -1;
  if (!(*push_action = split(NULL, ':')))
    return -1;
  if (!(*drop_action = split(NULL, ':')))
    return -1;
  return l;
}

/*---------------------------------------------------------------------------*/

static void readFileBitmaps(void)
{
  int i;

  for (i=0; i<n_types; i++)
    if (!types[i].icon[0]) {
      types[i].icon_bm = bm[FILE_BM];
      types[i].bm_width=builtin_types[FILE_T].bm_width;
      types[i].bm_height=builtin_types[FILE_T].bm_height;
    }
    else if ((types[i].icon_bm = readIcon(types[i].icon,
					&types[i].bm_width,
					&types[i].bm_height)) == None) {
#ifdef MAGIC_HEADERS
      fprintf(stderr, "%s: can't read icon for type %s%s%s%s%s%s\n", progname,
	      types[i].magic_type?"<":"",
	      types[i].magic_type?types[i].magic_type:"",
	      types[i].magic_type?">":"",
	      types[i].dir<0?"*":"", types[i].pattern,
	      types[i].dir>0?"*":"");
#else
      fprintf(stderr, "%s: can't read icon for type %s%s%s\n", progname,
	      types[i].dir<0?"*":"", types[i].pattern,
	      types[i].dir>0?"*":"");
#ifndef ENHANCE_BUGFIX
      types[i].icon_bm = bm[FILE_BM];
#endif
#endif
#ifdef ENHANCE_BUGFIX  
     /* we need a default icon in any case, whether we use MAGIC_HEADERS or not */
      types[i].icon_bm = bm[FILE_BM];
#endif
      types[i].bm_width=builtin_types[FILE_T].bm_width;
      types[i].bm_height=builtin_types[FILE_T].bm_height;
    }
}

/*---------------------------------------------------------------------------*/

static void readFileTypes(String path)
{
  FILE *fp;
  char *pattern, *icon, *push_action, *drop_action;
#ifdef MAGIC_HEADERS
  char *magic_type;
#endif
  char s[MAXCFGSTRINGLEN];
  int i, l, p;
  
  n_types = 0;
  types = NULL;
  
  if (!(fp = fopen(path, "r"))) return;

  for (i=0; (p = parseType(fp, &pattern,
#ifdef MAGIC_HEADERS
                           &magic_type,
#endif
                           &icon, &push_action,
			   &drop_action)) > 0; i++) {
    types = (TypeList) XTREALLOC(types, (i+1)*sizeof(TypeRec) );
    l = strlen(pattern);
    if (pattern[0] == '*') {
      types[i].dir = -1;
#ifdef MAGIC_HEADERS
      strparse(s, pattern+1, "\\:<>");
#else
      strparse(s, pattern+1, "\\:");
#endif
    } else if (pattern[l-1] == '*') {
      types[i].dir = 1;
      pattern[l-1] = '\0';
#ifdef MAGIC_HEADERS
      strparse(s, pattern, "\\:<>");
#else
      strparse(s, pattern, "\\:");
#endif
    } else {
      types[i].dir = 0;
#ifdef MAGIC_HEADERS
      strparse(s, pattern, "\\:<>");
#else
      strparse(s, pattern, "\\:");
#endif
    }
    types[i].len = strlen(s);
    types[i].pattern = XtNewString(s);
#ifdef MAGIC_HEADERS
    if(magic_type)
      types[i].magic_type = XtNewString(strparse(s, magic_type, "\\:"));
    else
      types[i].magic_type = NULL;
#endif
    types[i].icon = XtNewString(strparse(s, icon, "\\:"));
    types[i].push_action = XtNewString(strparse(s, push_action, "\\:"));
    types[i].drop_action = XtNewString(strparse(s, drop_action, "\\:"));
  }

  if (p == -1)
    error("Error in configuration file", "");

  n_types = i;
  
  if (fclose(fp))
    sysError("Error reading configuration file:");

  readFileBitmaps();
}

/*---------------------------------------------------------------------------*/

#ifdef MAGIC_HEADERS
TypeRec *fileType(char *name, char *magic_type)
#else
TypeRec *fileType(char *name)
#endif
{
  int i, l = strlen(name);

  for (i = 0; i < n_types; i++) {
#ifdef MAGIC_HEADERS
    if (types[i].magic_type) {
      if(strcmp(types[i].magic_type, magic_type))
        continue;
      else if (!strcmp(types[i].pattern, "")) /* Empty pattern. */
        return types+i;
    }
#endif
    switch (types[i].dir) {
    case 0:
      if (!strcmp(name, types[i].pattern))
	return types+i;
      break;
    case 1:
      if (!strncmp(types[i].pattern, name, types[i].len))
	return types+i;
      break;
    case -1:
      if (l >= types[i].len && !strncmp(types[i].pattern, name+l-types[i].len,
					types[i].len))
	return types+i;
      break;
    }
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/

static int parseDev(FILE *fp, char **name, char **mount_action,
		    char **umount_action)
{
  static char s[MAXCFGLINELEN];
  int l;

 start:
  if (feof(fp)||!fgets(s, MAXCFGLINELEN, fp))
    return 0;
  l = strlen(s);
  if (s[l-1] == '\n')
    s[--l] = '\0';
  if (!l || *s == '#')
    goto start;
  if (!(*name = split(s, ':')))
    return -1;
  if (!(*mount_action = split(NULL, ':')))
    return -1;
  if (!(*umount_action = split(NULL, ':')))
    return -1;
  return l;
}

/*---------------------------------------------------------------------------*/

static void readDevices(String path)
{
  FILE *fp;
  char *name, *mount_action, *umount_action;
  char s[MAXCFGSTRINGLEN];
  int i, p;
  
  n_devices = 0;
  devs = NULL;
  
  if (!(fp = fopen(path, "r"))) return;

  for (i=0; (p = parseDev(fp, &name, &mount_action, &umount_action)) > 0;
       i++) {
    devs = (DevList) XTREALLOC(devs, (i+1)*sizeof(DevRec) );
    devs[i].name = XtNewString(strparse(s, name, "\\:"));
    devs[i].mount_action = XtNewString(strparse(s, mount_action, "\\:"));
    devs[i].umount_action = XtNewString(strparse(s, umount_action, "\\:"));
    devs[i].mounted = 0;
  }

  if (p == -1)
    error("Error in devices file", "");

  n_devices = i;
  
  if (fclose(fp))
    sysError("Error reading devices file:");
}

/*---------------------------------------------------------------------------*/

static int devAction(int d, char *action)
{
  int pid, status;

  if ((pid = fork()) == -1) {
    sysError("Can't fork:");
    return 0;
  } else if (chdir(user.home)) {
    sysError("Can't chdir:");
    return 0;
  } else if (!pid) {
    if (resources.echo_actions)
      fprintf(stderr, "%s\n", action);
    freopen("/dev/null", "r", stdin);
    if (user.arg0flag)
      execlp(user.shell, user.shell, "-c", action, user.shell, NULL);
    else
      execlp(user.shell, user.shell, "-c", action, NULL);
    perror("Exec failed");
    exit(1);
  } else if (waitpid(pid, &status, 0) == -1 || !WIFEXITED(status) ||
	     WEXITSTATUS(status))
    return 0;
  else
    return 1;
}

/*----------------------------------------------------------------------------*/

/* This is new */
static void createFileIcons(FileWindowRec *fw)
{
 Dimension width;
 int	  nhoriz;



 XtVaGetValues(fw->viewport, XtNwidth, &width, NULL);
 icon_list_args[0].value = (XtArgVal) width;
 icon_list_args[1].value = (XtArgVal) resources.icon_font;
 icon_list_args[2].value = (XtArgVal) fw->files;
 icon_list_args[3].value = (XtArgVal) fw->n_files;
 icon_list_args[4].value = (XtArgVal) resources.file_icon_width;
 icon_list_args[5].value = (XtArgVal) resources.file_icon_height;
#ifndef ENHANCE_TRANSLATIONS
 icon_list_args[6].value = (XtArgVal) file_list_translations;
#endif
#ifdef ENHANCE_SELECTION
 icon_list_args[7].value = (XtArgVal) resources.highlight_pixel;
#endif
 
 fw->icon_box = XtCreateWidget("icon list",
				iconFileListWidgetClass,
				fw->viewport, icon_list_args,
				XtNumber(icon_list_args) );

 XtAddCallback(fw->icon_box,XtNcallback,fileListCb,(XtPointer)fw);

 XtVaGetValues(fw->icon_box,XtNnHoriz,&nhoriz,0);
 XtVaSetValues(fw->viewport, XtNallowHoriz, nhoriz!=0 , NULL);
}

/*----------------------------------------------------------------------------*/

/* This is new */
static void createTextDisplay(FileWindowRec *fw)
{
 Dimension width;
 Widget	   options_button;

 XtVaSetValues(fw->viewport, XtNallowHoriz, True, NULL);

 XtVaGetValues(fw->viewport, XtNwidth, &width, NULL);
 text_list_args[0].value = (XtArgVal) width;
 text_list_args[1].value = (XtArgVal) resources.icon_font;
 text_list_args[2].value = (XtArgVal) fw->files;
 text_list_args[3].value = (XtArgVal) fw->n_files;
#ifndef ENHANCE_TRANSLATIONS
 text_list_args[4].value = (XtArgVal) file_list_translations;
#endif
#ifdef ENHANCE_SELECTION
 text_list_args[5].value = (XtArgVal) resources.highlight_pixel;
#endif

 fw->icon_box = XtCreateWidget("text list",
				textFileListWidgetClass,
				fw->viewport, text_list_args,
				XtNumber(text_list_args) );

 XtAddCallback(fw->icon_box,XtNcallback,fileListCb,(XtPointer)fw);

 /* increment in_use count for the options button; the unmapTxtOptsCb
  * will only unmap the button if the count drops to zero.
  */
 fw->option_in_use++;
 XtAddCallback(fw->icon_box,XtNdestroyCallback, unmapTxtOptsCb,(XtPointer)fw);
 
 updateTxtOpts(fw);

 options_button=XtParent(XtParent(fw->option_items[0]));

 /* show the text options menu */
 if (XtIsRealized(fw->shell))
   XtMapWidget(options_button);
 else
   XtSetMappedWhenManaged(options_button,True);
}

/*----------------------------------------------------------------------------*/

/* create a directory icon in position specified by horiz & vert */
static Widget createDirIcon(FileWindowRec *fw, int i, Widget horiz,Widget vert)
{
  FileRec *file = fw->files[i];
  char *dirlabel;
  Pixel back;
  Pixmap icon = None;

#ifdef MAGIC_HEADERS
  file->type = fileType(file->name, file->magic_type);
  if (file->type)
    icon = (XtArgVal) file->type->icon_bm;
#endif
  if (icon == None)
    icon = bm[DIR_BM];

  /* create form */
  tree_form_args[0].value = (XtArgVal) horiz;
  tree_form_args[1].value = (XtArgVal) vert;
#ifdef VIEWPORT_HACK
  file->icon.form = XtCreateWidget(file->name,
#else
  file->icon.form = XtCreateManagedWidget(file->name,
#endif
    formWidgetClass, fw->icon_box, tree_form_args, XtNumber(tree_form_args) );

  /* create icon */
  icon_toggle_args[0].value = (XtArgVal) NULL;
  icon_toggle_args[1].value = (XtArgVal) NULL;
  icon_toggle_args[2].value = (XtArgVal) icon;
#ifdef ENHANCE_SELECTION
  icon_toggle_args[6].value = (XtArgVal) resources.highlight_pixel;
#endif
#ifndef ENHANCE_TRANSLATIONS
  icon_toggle_args[3].value = (XtArgVal) tree_translations;
  file->icon.toggle = XtCreateManagedWidget("icon",
#else
  file->icon.toggle = XtCreateManagedWidget("tree_icon",
#endif
    toggleWidgetClass, file->icon.form, icon_toggle_args,
    XtNumber(icon_toggle_args) );

  XtVaGetValues(file->icon.toggle, XtNbackground, &back, NULL);
  XtVaSetValues(file->icon.toggle, XtNborder, (XtArgVal) back, NULL);


  /* create label */
  icon_label_args[0].value = (XtArgVal) NULL;
  icon_label_args[1].value = (XtArgVal) file->icon.toggle;
  if (i == 0)
    dirlabel = fw->directory[1]?strrchr(fw->directory, '/')+1:fw->directory;
  else
    dirlabel = file->name;
  icon_label_args[2].value = (XtArgVal)dirlabel;
  file->icon.label = XtCreateManagedWidget("label",
    labelWidgetClass, file->icon.form, icon_label_args,
    XtNumber(icon_label_args) );

  return file->icon.form;
}

/*----------------------------------------------------------------------------*/

/* create the icons for the directory display */
static void createTreeDisplay(FileWindowRec *fw)
{
  int i, l;
  char *s = fw->directory[1]?strrchr(fw->directory, '/')+1:fw->directory;
  Widget vert, horiz;
  Pixmap line_bm;
  Dimension width;
  FileList files = fw->files;
#ifdef VIEWPORT_HACK
  Boolean force_bars;
  WidgetList children;
  Cardinal   num_children;
#endif

  /* find width of icons */
  width = longestName(fw);
  if (width < (l = XTextWidth(resources.icon_font, s, strlen(s))))
    width = l;
  if (width < resources.tree_icon_width)
    width = resources.tree_icon_width;
  tree_form_args[3].value = (XtArgVal) width;
  icon_toggle_args[4].value = (XtArgVal) width;
  icon_toggle_args[5].value = (XtArgVal) resources.tree_icon_height;
  icon_label_args[4].value = (XtArgVal) width;

  /* create icon box in viewport */
#ifdef VIEWPORT_HACK
  XtVaGetValues(fw->viewport, XtNwidth, &width, XtNforceBars, &force_bars, NULL);
  /* force creation of scrollbars */
  XtVaSetValues(fw->viewport, XtNallowHoriz, True, XtNforceBars, True, NULL);
#else		
  XtVaSetValues(fw->viewport, XtNallowHoriz, True, NULL);
  XtVaGetValues(fw->viewport, XtNwidth, &width, NULL);
#endif
  tree_box_args[0].value = (XtArgVal) width;
#ifdef VIEWPORT_HACK
  /* widget must be managed by viewport 
   */
  fw->icon_box = XtCreateManagedWidget("icon box", formWidgetClass,
#else
  fw->icon_box = XtCreateWidget("icon box", formWidgetClass,
#endif
    fw->viewport, tree_box_args, XtNumber(tree_box_args) );

#ifdef VIEWPORT_HACK
  XawFormDoLayout(fw->icon_box,False);
#endif

  /* The '..' directory is not displayed, and no arrow for '.'  */
  files[1]->icon.form = files[1]->icon.toggle = 
    files[1]->icon.label = NULL;
  files[0]->icon.arrow = NULL;
    
  /* create left arrow */
  arrow_args[0].value = (XtArgVal) NULL;
  arrow_args[1].value = (XtArgVal) NULL;
  if (!permission(&files[1]->stats, P_EXECUTE)) {
    arrow_args[2].value = bm[NOENTRY_CBM];
    arrow_args[3].value = False;
  }
  else {
    arrow_args[2].value = bm[LARROW_BM];
    arrow_args[3].value = True;
  }
  horiz = files[1]->icon.arrow = XtCreateManagedWidget("left arrow",
	commandWidgetClass, fw->icon_box, arrow_args, XtNumber(arrow_args) );
  XtAddCallback(horiz, XtNcallback, (XtCallbackProc) mainArrowCb, fw);

  /* create current directory icon */
  horiz = createDirIcon(fw, 0,  horiz, NULL);

  vert = NULL;
 
  for(i = 2; i < fw->n_files; i++, horiz = files[0]->icon.form) {
    
    /* create line */
    if (i == 2)
      if (fw->n_files == 3)
	line_bm = bm[LLINE_BM];
      else
	line_bm = bm[TLINE_BM];
    else
      if (i == fw->n_files - 1)
	line_bm = bm[CLINE_BM];
      else
	line_bm = bm[FLINE_BM];
    line_args[0].value = (XtArgVal) horiz;
    line_args[1].value = (XtArgVal) vert;
    line_args[2].value = (XtArgVal) line_bm;
    horiz  = XtCreateManagedWidget("line", labelWidgetClass, 
      fw->icon_box, line_args, XtNumber(line_args) );
    
    /* create icon */
    horiz = createDirIcon(fw, i, horiz, vert);
    
    /* create right arrow */
    arrow_args[0].value = (XtArgVal) horiz;
    arrow_args[1].value = (XtArgVal) vert;
    if (!permission(&files[i]->stats, P_EXECUTE)) {
      arrow_args[2].value = bm[NOENTRY_CBM];
      arrow_args[3].value = False;
    }
    else if (files[i]->sym_link) {
      arrow_args[2].value = bm[WAVY_BM];
      arrow_args[3].value = True;
    }
    else {
      arrow_args[2].value = bm[RARROW_BM];
      arrow_args[3].value = True;
    }
    vert = files[i]->icon.arrow 
      = XtCreateManagedWidget("right arrow", commandWidgetClass, fw->icon_box, 
			      arrow_args, XtNumber(arrow_args) );
    XtAddCallback(vert, XtNcallback, (XtCallbackProc) mainArrowCb, fw);
  }
#ifdef VIEWPORT_HACK
  XawFormDoLayout(fw->icon_box,False);
  XtVaGetValues(fw->icon_box,XtNchildren,&children,XtNnumChildren,&num_children,0);
  XtManageChildren(children,num_children);
  if (!force_bars)
    XtVaSetValues(fw->viewport,XtNforceBars,False,0);
#endif
}

/*-----------------------------------------------------------------------------
  PUBLIC FUNCTIONS
-----------------------------------------------------------------------------*/

/* find the device for a directory */
int findDev(char *path)
{
  int d;

  for (d = 0; d < n_devices; d++)
    if (prefix(devs[d].name, path))
      return d;
  return -1;
}

/*---------------------------------------------------------------------------*/
/* mount a device */
void mountDev(int d)
{
  if (d == -1)
    ;
  else if (devs[d].mounted)
    devs[d].mounted++;
  else
    devs[d].mounted += devAction(d, devs[d].mount_action);
}

/*---------------------------------------------------------------------------*/
/* unmount a device */
void umountDev(int d)
{
  if (d == -1 || !devs[d].mounted)
    ;
  else if (devs[d].mounted > 1)
    devs[d].mounted--;
  else
    devs[d].mounted -= devAction(d, devs[d].umount_action);
}

/*---------------------------------------------------------------------------*/
/* initialise the file Windows module */
void initFileWindows()
{
  XtAppAddActions(app_context, file_actions, XtNumber(file_actions));
#ifndef ENHANCE_TRANSLATIONS
  label_translations = XtParseTranslationTable(label_translations_s);
  tree_translations = XtParseTranslationTable(tree_translations_s);
  file_list_translations = XtParseTranslationTable(file_list_translations_s);

  label_args[9].value = (XtArgVal) label_translations;
#endif

  label_args[3].value = (XtArgVal) resources.label_font;
  status_args[3].value = (XtArgVal) resources.status_font;
  icon_label_args[3].value = (XtArgVal) resources.icon_font;
  shell_args[1].value = (XtArgVal) bm[ICON_BM];
  shell_args[2].value = (XtArgVal) bm[ICONMSK_BM];

  file_popup_items = createFloatingMenu("file popup", file_popup_menu,
					XtNumber(file_popup_menu), 4, aw.shell,
					NULL, &file_popup_widget);
  XtRegisterGrabAction(filePopup, True, ButtonPressMask | ButtonReleaseMask,
		       GrabModeAsync, GrabModeAsync);
  dir_popup_items = createFloatingMenu("dir popup", dir_popup_menu,
					XtNumber(dir_popup_menu), 4, aw.shell,
					NULL, &dir_popup_widget);
  XtRegisterGrabAction(dirPopup, True, ButtonPressMask | ButtonReleaseMask,
		       GrabModeAsync, GrabModeAsync);
  readFileTypes(resources.cfg_file);
  readDevices(resources.dev_file);
#ifdef MAGIC_HEADERS
  magic_parse_file(resources.magic_file);
#endif
}

/*---------------------------------------------------------------------------*/
/* Create a file Window at the specified path, in the specified format */

static FileWindowRec *createFileWindow(String path, String title, 
				       DisplayType format)
{
  FileWindowRec *fw;
  char *shell_name;

#ifdef DEBUG_MALLOC
  fprintf(stderr, "entering createFileWindow: %lu\n", malloc_inuse(NULL));
#endif

  if (chdir(path)) {
    sysError("Can't open folder:");
    return NULL;
  }

  /* put at front of linked list */
  fw = (FileWindowRec *) XtMalloc(sizeof(FileWindowRec));
  fw->next = file_windows;
  file_windows = fw;
  
  if (!getwd(fw->directory)) {
    sysError("Can't open folder:");
    return NULL;
  }

  /* set up defaults */
  fw->dev = -1;
  fw->display_type = format;
  fw->sort_type = resources.default_sort_type;
  fw->show_dirs = True;
  fw->show_hidden = False;
  fw->dirs_first = True;
  fw->n_selections = 0;
  fw->n_bytes_selected = 0;
  fw->unreadable = NULL;
  fw->files = NULL;
  fw->n_files = 0;
  fw->n_bytes = 0;
  fw->update = False;
  /* KMR */ /* AG removed inherited do_filter attribute */
  fw->do_filter = False;
  fw->dirFilter[0] = '\0';

  fw->showInode=resources.show_inode;
  fw->showType=resources.show_type;
  fw->showLinks=resources.show_links;
  fw->showPermissions=resources.show_perms;
  fw->showOwner=resources.show_owner;
  fw->showGroup=resources.show_group;
  fw->showLength=resources.show_length;
  fw->showDate=resources.show_date;

  shell_name = "file window";
  shell_args[0].value = (XtArgVal) title;
  fw->shell = XtCreatePopupShell(shell_name, topLevelShellWidgetClass,
				 aw.shell, shell_args, XtNumber(shell_args) );
  if (resources.init_geometry)
    XtVaSetValues(fw->shell, XtNgeometry, resources.init_geometry, NULL);
  
  /* create form */
  fw->form = XtCreateManagedWidget("form", formWidgetClass, fw->shell,
				   form_args, XtNumber(form_args) );
  
  /* create button box */
  fw->button_box = XtCreateManagedWidget("button box", boxWidgetClass,
					 fw->form, button_box_args, 
					 XtNumber(button_box_args) );
  
  /* create the menus */
  fw->file_items = createMenu("file", "File", file_menu, XtNumber(file_menu),
			      4, fw->button_box, (XtPointer) fw);
  fw->folder_items = createMenu("folder", "Folder", folder_menu, 
				XtNumber(folder_menu), 4, fw->button_box,
				(XtPointer) fw);
  fw->view_items = createMenu("view", "View", view_menu, XtNumber(view_menu),
			      16, fw->button_box, (XtPointer) fw);
  fw->option_items=createMenu("options","Options",text_opts_menu,XtNumber(text_opts_menu),
			      16, fw->button_box, (XtPointer) fw);
  XtSetMappedWhenManaged(XtParent(XtParent(fw->option_items[0])),False);
  fw->option_in_use=0;
  /* count the number of textFileLists currently
   * using the options menu (maybe 2; one, old, being destroyed + a
   * a new one.
   */

  /* create folder label */
  label_args[0].value = (XtArgVal) fw->button_box;
  label_args[1].value = (XtArgVal) fw->directory;
#ifndef ENHANCE_TRANSLATIONS
  fw->label = XtCreateManagedWidget("label", labelWidgetClass, fw->form,
#else
  fw->label = XtCreateManagedWidget("folderlabel", labelWidgetClass, fw->form,
#endif
				    label_args, XtNumber(label_args) );
  
  /* create viewport */
  viewport_args[0].value = (XtArgVal) fw->label;
  fw->viewport = XtCreateManagedWidget("viewport", viewportWidgetClass,
				       fw->form, viewport_args, 
				       XtNumber(viewport_args) );

  /* create status line */
  status_args[0].value = (XtArgVal) fw->viewport;
  status_args[1].value = (XtArgVal) "";
  fw->status = XtCreateManagedWidget("status", labelWidgetClass, fw->form,
				     status_args, XtNumber(status_args) );
  
#ifdef DEBUG_MALLOC
  fprintf(stderr, "exiting createFileWindow: %lu\n", malloc_inuse(NULL));
#endif

  return fw;
}

/*----------------------------------------------------------------------------*/

void newFileWindow(String path, DisplayType d, Boolean by_cursor)
{
  FileWindowRec *fw;

#ifdef DEBUG_MALLOC
  fprintf(stderr, "entering newFileWindow: %lu\n", malloc_inuse(NULL));
#endif

  if (!(fw = createFileWindow(path, "File Manager", d)))
    return;
  createFileDisplay(fw);
  XtRealizeWidget(fw->shell);
  XSetIconName(XtDisplay(fw->shell), XtWindow(fw->shell), fw->directory);
  XStoreName(XtDisplay(fw->shell), XtWindow(fw->shell), fw->directory);
  setWMProps(fw->shell);
  XtAddEventHandler(fw->shell, (EventMask)0L, True,
		    (XtEventHandler)clientMessageHandler, (XtPointer)NULL);
  if (by_cursor)
    popupByCursor(fw->shell, XtGrabNone);
  else
    XtPopup(fw->shell, XtGrabNone);

#ifdef DEBUG_MALLOC
  fprintf(stderr, "exiting newFileWindow: %lu\n", malloc_inuse(NULL));
#endif
}

/*---------------------------------------------------------------------------*/

/* Main procedure to create the display in the viewport */
void createFileDisplay(FileWindowRec *fw)
{
  int i;

#ifdef DEBUG_MALLOC
  fprintf(stderr, "entering createFileDisplay: %lu\n", malloc_inuse(NULL));
#endif

  XtVaSetValues(fw->label, XtNlabel, (XtArgVal) fw->directory, NULL);

  fw->icon_box = NULL;

  if (fw->unreadable) {
    XtDestroyWidget(fw->unreadable);
    fw->unreadable = NULL;
  }

  if (!readDirectory(fw)) {
    fw->unreadable = 
      XtVaCreateManagedWidget("label", labelWidgetClass, fw->viewport,
			      XtNlabel, "Directory is unreadable",
			      XtNfont, resources.label_font, NULL);
    return;
  }

  for (i=0; i<fw->n_files; i++)
    fw->files[i]->selected = False;
  fw->n_selections = 0;
  fw->n_bytes_selected = 0;

  switch (fw->display_type) {
  case Tree:
    filterDirectory(fw, Directories);
    sortDirectory(fw->files+2, fw->n_files-2, fw->sort_type, False);
    createTreeDisplay(fw);
    break;
  case Icons:
    filterDirectory(fw, fw->show_dirs ? All : Files);
    sortDirectory(fw->files, fw->n_files, fw->sort_type, fw->dirs_first);
    createFileIcons(fw);
    break;
  case Text:
    filterDirectory(fw, fw->show_dirs ? All : Files);
    sortDirectory(fw->files, fw->n_files, fw->sort_type, fw->dirs_first);
    createTextDisplay(fw);
    break;
  }
  updateStatus(fw);

#ifdef ENHANCE_HISTORY
  FmInsertHistoryPath(path_history,fw->directory);
  if (resources.history_max_n>0)
    FmChopHistoryList(path_history,resources.history_max_n);
#endif

  XtManageChild(fw->icon_box);

#ifdef DEBUG_MALLOC
  fprintf(stderr, "exiting createFileDisplay: %lu\n", malloc_inuse(NULL));
#endif
}

/*---------------------------------------------------------------------------*/

/* Update the display in the viewport */
#ifdef ENHANCE_SCROLL
void updateFileDisplay(FileWindowRec *fw, Boolean keep_position)
#else
void updateFileDisplay(FileWindowRec *fw)
#endif
{
  int d;
#ifdef ENHANCE_SCROLL
  Position x,y;
#endif

#ifdef DEBUG_MALLOC
  fprintf(stderr, "entering updateFileDisplay: %lu\n", malloc_inuse(NULL));
#endif

  zzz();

  d = fw->dev;

#ifdef ENHANCE_SCROLL
  x=y=0;
  if (fw->icon_box) {
    if (keep_position)
      XtVaGetValues(fw->icon_box,XtNx,&x,XtNy,&y,0);
    XtUnrealizeWidget(fw->icon_box);
    XtDestroyWidget(fw->icon_box);
  }
#else
  if (fw->icon_box)
    XtDestroyWidget(fw->icon_box);
#endif

  freeFileList(fw);
  createFileDisplay(fw);

#ifdef ENHANCE_SCROLL
  if (keep_position)
    XawViewportSetCoordinates(fw->viewport,-x,-y);
#endif

  if (d != -1) umountDev(d);

  XSetIconName(XtDisplay(fw->shell), XtWindow(fw->shell), fw->directory);
  XStoreName(XtDisplay(fw->shell), XtWindow(fw->shell), fw->directory);

  wakeUp();

#ifdef DEBUG_MALLOC
  fprintf(stderr, "exiting updateFileDisplay: %lu\n", malloc_inuse(NULL));
#endif
}

/*---------------------------------------------------------------------------*/

/* resort the icons in the display */
void reSortFileDisplay(FileWindowRec *fw)
{
#ifdef ENHANCE_BUGFIX
  int i;
#endif
#ifdef DEBUG_MALLOC
  fprintf(stderr, "entering resortFileDisplay: %lu\n", malloc_inuse(NULL));
#endif

  if (fw->unreadable)
    return;

  zzz();


  switch (fw->display_type) {
  case Tree:
#ifdef ENHANCE_SELECTION
    FmDisownSelection(fw);
#endif
    XtDestroyWidget(fw->icon_box);
#ifdef ENHANCE_BUGFIX
    /* when we set n_selections=0 we have to
     * deselect all the files too! (createTreeDisplay
     * does not check for the status of file->selected)
     */
    for (i=0; i<fw->n_files; i++) {
	fw->files[i]->selected=False;
    }
#endif
    fw->n_selections = 0;
    fw->n_bytes_selected = 0;
    sortDirectory(fw->files+2, fw->n_files-2, fw->sort_type, False);
    createTreeDisplay(fw);
    break;
  case Icons:
    sortDirectory(fw->files, fw->n_files, fw->sort_type, fw->dirs_first);
    /* force a redisplay (file list, widget and selections are preserved) */
    XClearArea(XtDisplay(fw->icon_box),XtWindow(fw->icon_box),0,0,0,0,True);
    break;
  case Text:
    sortDirectory(fw->files, fw->n_files, fw->sort_type, fw->dirs_first);
    /* force a redisplay */
    XClearArea(XtDisplay(fw->icon_box),XtWindow(fw->icon_box),0,0,0,0,True);
    break;
  }

  updateStatus(fw);
  XtManageChild(fw->icon_box);

  wakeUp();

#ifdef DEBUG_MALLOC
  fprintf(stderr, "exiting resortFileDisplay: %lu\n", malloc_inuse(NULL));
#endif
}

/*---------------------------------------------------------------------------*/

void reDisplayFileWindow(FileWindowRec *fw)
{
#ifdef DEBUG_MALLOC
  fprintf(stderr, "entering redisplayFileWindow: %lu\n", malloc_inuse(NULL));
#endif

  if (fw->unreadable)
    return;

  zzz();

  XtDestroyWidget(fw->icon_box);

  switch (fw->display_type) {
  case Tree:
    createTreeDisplay(fw);
    break;
  case Icons:
    createFileIcons(fw);
    break;
  case Text:
    createTextDisplay(fw);
    break;
  }

  updateStatus(fw);
  XtManageChild(fw->icon_box);

  wakeUp();

#ifdef DEBUG_MALLOC
  fprintf(stderr, "exiting redisplayFileWindow: %lu\n", malloc_inuse(NULL));
#endif
}

/*----------------------------------------------------------------------------
  Intelligent update - only update the windows needed.
  Use markForUpdate() to explicitly mark a directory for update.
  Call intUpdate to execute all the actions.
-----------------------------------------------------------------------------*/
void markForUpdate(String path)
{
  FileWindowRec *fw;

  for (fw = file_windows; fw; fw = fw->next)
    if (!strcmp(path, fw->directory))
      fw->update = True;
}

#ifdef ENHANCE_PERMS
/* change of user permission or removing files which
 * are pointed to by symlinks in our directory are
 * only detected the hard way :-(, so we must stat
 * everything to be sure.
 */
static Boolean files_changed(FileWindowRec *fw)
{
  FileList	fl=fw->files;
  int		i,statfailed;
  struct stat	cur;
  
  if (chdir(fw->directory)) return True;
  for (i=0; i<fw->n_files; i++) {
    statfailed=stat(fl[i]->name,&cur);
    if (fl[i]->sym_link) {
	if ( (statfailed && !S_ISLNK(fl[i]->stats.st_mode)) ||
	     (!statfailed && S_ISLNK(fl[i]->stats.st_mode)) ) {
	  return True;
	}
	if (!statfailed && (cur.st_ctime > fl[i]->stats.st_ctime)) {
	  return True;
	}
    } else {
    	if ( statfailed || (cur.st_ctime > fl[i]->stats.st_ctime) ) {
	  return True;
	}
    }
  }
  return False;
}
#endif

void intUpdate()
{
  FileWindowRec *fw;
  struct stat cur;
#ifdef ENHANCE_PERMS
  static int ticks=0;

  ticks++;
#endif

  for (fw = file_windows; fw; fw = fw->next) {
    if (fw->update ||
	stat(fw->directory, &cur) ||
#ifdef ENHANCE_PERMS
	cur.st_ctime > fw->stats.st_ctime || 
	(ticks==resources.hard_update_ticks && files_changed(fw)))
#else
	cur.st_ctime > fw->stats.st_ctime)
#endif
#ifdef ENHANCE_SCROLL
      updateFileDisplay(fw,True);
#else
      updateFileDisplay(fw);
#endif
	
  }

  for (fw = file_windows; fw; fw = fw->next)
    fw->update = False;

#ifdef ENHANCE_PERMS
  if (ticks>=resources.hard_update_ticks) ticks=0;
#endif
}

/*-----------------------------------------------------------------------------
  Keep menus and status line consistent with the number of selections in each
  window. Currently this must be called manually, which is bad news.
-----------------------------------------------------------------------------*/
void updateStatus(FileWindowRec *fw)
{
  char s[1024], t[1024];
  int n_files, n_selections;
  long n_bytes, n_bytes_selected;

  if (fw->n_selections >= 1) {
    fillIn(fw->file_items[2]);
    fillIn(fw->file_items[3]);
    fillIn(fw->file_items[4]);
    fillIn(fw->file_items[6]);
    fillIn(fw->file_items[10]);
#ifdef ENHANCE_SELECTION
    fillIn(fw->file_items[11]);
#endif
  }else {
    grayOut(fw->file_items[2]);
    grayOut(fw->file_items[3]);
    grayOut(fw->file_items[4]);
    grayOut(fw->file_items[6]);
    grayOut(fw->file_items[10]);
#ifdef ENHANCE_SELECTION
    grayOut(fw->file_items[11]);
#endif
  }

  if (fw->display_type == Tree) {         /* incremented the view_item */
    grayOut(fw->view_items[10]);          /* numbers by 1 since I added */
    grayOut(fw->view_items[11]);          /* a new menu pick in slot 7 */
    noTick(fw->view_items[10]);           /* only affects items 8 and above */
    noTick(fw->view_items[11]);           /* KMR */
  }
  else {
    fillIn(fw->view_items[10]);
    if (fw->show_dirs) {
      fillIn(fw->view_items[11]);
      noTick(fw->view_items[10]);
      if (fw->dirs_first)
	noTick(fw->view_items[11]);
      else
	tick(fw->view_items[11]);
    }
    else {
      grayOut(fw->view_items[11]);
      tick(fw->view_items[10]);
      noTick(fw->view_items[11]);
    }
  }

  if (fw->show_hidden)
    tick(fw->view_items[12]);
  else
    noTick(fw->view_items[12]);

  noTick(fw->view_items[0]);
  noTick(fw->view_items[1]);
  noTick(fw->view_items[2]);
  noTick(fw->view_items[4]);
  noTick(fw->view_items[5]);
  noTick(fw->view_items[6]);

  switch (fw->display_type) {
  case Tree:
    tick(fw->view_items[0]);
    break;
  case Icons:
    tick(fw->view_items[1]);
    break;
  case Text:
    tick(fw->view_items[2]);
    break;
  }

  switch (fw->sort_type) {
  case SortByName:
    tick(fw->view_items[4]);
    break;
  case SortBySize:
    tick(fw->view_items[5]);
    break;
  case SortByMTime:
    tick(fw->view_items[6]);
    break;
  }

  /* update the status line */

  n_bytes = fw->n_bytes;
  n_files = fw->n_files;
  n_bytes_selected = fw->n_bytes_selected;
  n_selections = fw->n_selections;

  if (fw->display_type == Tree) {
    n_bytes -= fw->files[1]->stats.st_size;
    n_files--;
  }

  if (fw->do_filter)
    sprintf(t, " [%s]", fw->dirFilter);
  else
    *t = '\0';

  if (n_selections > 0)
    sprintf(s,
	"%ld byte%s in %d item%s, %ld byte%s in %d selected item%s%s",
	n_bytes, n_bytes==1?"":"s",
	n_files, n_files==1?"":"s",
	n_bytes_selected, n_bytes_selected==1?"":"s",
	n_selections, n_selections==1?"":"s", t);
  else
    sprintf(s, "%ld byte%s in %d item%s%s", n_bytes, n_bytes==1?"":"s",
	n_files, n_files==1?"":"s", t);

  XtVaSetValues(fw->status, XtNlabel, (XtArgVal) s, NULL);
}

/*----------------------------------------------------------------------------
  Update a the tick marks of a text options menu and the resources
  of a text file List to the settings of a FileWindowRec
-----------------------------------------------------------------------------*/

void updateTxtOpts(FileWindowRec *fw)
{
 int i;
 WidgetList menu_items;
 static Arg args[]={
  { XtNshowInode,	(XtArgVal) 0},
  { XtNshowType,	(XtArgVal) 0},
  { XtNshowPermissions,	(XtArgVal) 0},
  { XtNshowLinks,	(XtArgVal) 0},
  { XtNshowOwner,	(XtArgVal) 0},
  { XtNshowGroup,	(XtArgVal) 0},
  { XtNshowLength,	(XtArgVal) 0},
  { XtNshowDate,	(XtArgVal) 0},
 };

 if (!XtIsSubclass(fw->icon_box,textFileListWidgetClass)) {
   XtAppWarning(app_context,"updateTxtOpts(): not a textFileList widget");
   return;
 }
 /* retrieve the menu items */
 menu_items=fw->option_items;

 i=0;
 args[i].value=(XtArgVal)fw->showInode; i++;
 args[i].value=(XtArgVal)fw->showType; i++;
 args[i].value=(XtArgVal)fw->showPermissions; i++;
 args[i].value=(XtArgVal)fw->showLinks; i++;
 args[i].value=(XtArgVal)fw->showOwner; i++;
 args[i].value=(XtArgVal)fw->showGroup; i++;
 args[i].value=(XtArgVal)fw->showLength; i++;
 args[i].value=(XtArgVal)fw->showDate; i++;

 for (i=0; i<XtNumber(args); i++) 
   if ((Boolean)args[i].value) tick(menu_items[i]); else noTick(menu_items[i]);

 XtSetValues(fw->icon_box,args,XtNumber(args));
}
