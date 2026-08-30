/*---------------------------------------------------------------------------
 Module FmMain

 (c) S.Marlow 1990-92
 (c) A.Graef 1994
 (c) R.Vogelgesang 1994 (`Xfm.BourneShells' stuff)

  modified 7-1997 by strauman@sun6hft.ee.tu-berlin.de to add
  different enhancements (see README-NEW).

 main module for file manager    
---------------------------------------------------------------------------*/

#include "FmVersion.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#ifdef ENHANCE_USERINFO
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef ENHANCE_TRANSLATIONS
#include <string.h>
#endif

#ifdef _AIX
#include <sys/resource.h>
#endif

#include <sys/wait.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Shell.h>

#include "Am.h"
#include "Fm.h"

#ifdef ENHANCE_SELECTION
#include "FmSelection.h"
#endif

#ifdef ENHANCE_LOG
#include "FmLog.h"
#endif

#define XtRDisplayType "DisplayType"
#define XtRSortType "SortType"

/*---------------------------------------------------------------------------
  Public variables
---------------------------------------------------------------------------*/

/* program name */
char *progname;

/* information about the user */
UserInfo user;

/* application resource values */
Resources resources;

/* application context */
XtAppContext app_context;

/* Update semaphor */
int freeze = False;

/*---------------------------------------------------------------------------
  Command line options
---------------------------------------------------------------------------*/

static XrmOptionDescRec options[] = {
  { "-appmgr", ".appmgr", XrmoptionNoArg, "True" },
  { "-filemgr", ".filemgr", XrmoptionNoArg, "True" },
  { "-version", ".version", XrmoptionNoArg, "True" }
};

/*---------------------------------------------------------------------------
  Application Resources
---------------------------------------------------------------------------*/

static XtResource resource_list[] = {
  { "appmgr", "Appmgr", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, appmgr), XtRImmediate, (XtPointer) False },
  { "filemgr", "Filemgr", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, filemgr), XtRImmediate, (XtPointer) False },
  { "version", "Version", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, version), XtRImmediate, (XtPointer) False },
  { "initGeometry", "InitGeometry", XtRString, sizeof(String),
      XtOffsetOf(Resources, init_geometry), XtRString, NULL },
  { "iconFont", XtCFont, XtRFontStruct, sizeof(XFontStruct *), 
      XtOffsetOf(Resources, icon_font), XtRString, XtDefaultFont },
  { "buttonFont", XtCFont, XtRFontStruct, sizeof(XFontStruct *), 
      XtOffsetOf(Resources, button_font), XtRString, XtDefaultFont },
  { "menuFont", XtCFont, XtRFontStruct, sizeof(XFontStruct *), 
      XtOffsetOf(Resources, menu_font), XtRString, XtDefaultFont },
  { "labelFont", XtCFont, XtRFontStruct, sizeof(XFontStruct *), 
      XtOffsetOf(Resources, label_font), XtRString, XtDefaultFont },
  { "statusFont", XtCFont, XtRFontStruct, sizeof(XFontStruct *), 
      XtOffsetOf(Resources, status_font), XtRString, XtDefaultFont },
  { "boldFont", XtCFont, XtRFontStruct, sizeof(XFontStruct *), 
      XtOffsetOf(Resources, bold_font), XtRString, XtDefaultFont },
  { "cellFont", XtCFont, XtRFontStruct, sizeof(XFontStruct *), 
      XtOffsetOf(Resources, cell_font), XtRString, XtDefaultFont },
  { "appIconWidth", "Width", XtRInt, sizeof(int),
      XtOffsetOf(Resources, app_icon_width), XtRImmediate, (XtPointer) 48 },
  { "appIconHeight", "Height", XtRInt, sizeof(int),
      XtOffsetOf(Resources, app_icon_height), XtRImmediate, (XtPointer) 40 },
  { "fileIconWidth", "Width", XtRInt, sizeof(int),
      XtOffsetOf(Resources, file_icon_width), XtRImmediate, (XtPointer) 48 },
  { "fileIconHeight", "Height", XtRInt, sizeof(int),
      XtOffsetOf(Resources, file_icon_height), XtRImmediate, (XtPointer) 40 },
  { "treeIconWidth", "Width", XtRInt, sizeof(int),
      XtOffsetOf(Resources, tree_icon_width), XtRImmediate, (XtPointer) 48 },
  { "treeIconHeight", "Height", XtRInt, sizeof(int),
      XtOffsetOf(Resources, tree_icon_height), XtRImmediate, (XtPointer) 32 },
  { "bitmapPath", "Path", XtRString, sizeof(String),
      XtOffsetOf(Resources, bitmap_path), XtRString, NULL },
  { "pixmapPath", "Path", XtRString, sizeof(String),
      XtOffsetOf(Resources, pixmap_path), XtRString, NULL },
  { "applicationDataFile", "ConfigFile",  XtRString, sizeof(String),
      XtOffsetOf(Resources, app_file_r), XtRString, "~/.xfm/Apps" },
  { "applicationDataDir", "ConfigDir",  XtRString, sizeof(String),
      XtOffsetOf(Resources, app_dir_r), XtRString, "~/.xfm" },
  { "applicationDataClip", "File",  XtRString, sizeof(String),
      XtOffsetOf(Resources, app_clip_r), XtRString, "~/.xfm/.XfmClip" },
  { "configFile", "ConfigFile",  XtRString, sizeof(String),
      XtOffsetOf(Resources, cfg_file_r), XtRString, "~/.xfm/xfmrc" },
  { "devFile", "ConfigFile",  XtRString, sizeof(String),
      XtOffsetOf(Resources, dev_file_r), XtRString, "~/.xfm/xfmdev" },
#ifdef MAGIC_HEADERS
  { "magicFile", "ConfigFile",  XtRString, sizeof(String),
      XtOffsetOf(Resources, magic_file_r), XtRString, "~/.xfm/magic" },
#endif
  { "confirmDeletes", "Confirm", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_deletes), XtRImmediate, (XtPointer) True },
  { "confirmDeleteFolder", "Confirm", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_delete_folder), XtRImmediate,
      (XtPointer) True },
  { "confirmMoves", "Confirm", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_moves), XtRImmediate, (XtPointer) True },
  { "confirmCopies", "Confirm", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_copies), XtRImmediate, (XtPointer) True },
  { "confirmOverwrite", "Confirm", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_overwrite), XtRImmediate,
      (XtPointer) True },
  { "confirmQuit", "Confirm", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, confirm_quit), XtRImmediate, (XtPointer) True },
  { "echoActions", "Echo", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, echo_actions), XtRImmediate, (XtPointer) False },
  { "showOwner", "ShowOwner", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_owner), XtRImmediate, (XtPointer) True },
  { "showDate", "ShowDate", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_date), XtRImmediate, (XtPointer) True },
  { "showPermissions", "ShowPermissions", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_perms), XtRImmediate, (XtPointer) True },
  { "showLength", "ShowLength", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_length), XtRImmediate, (XtPointer) True },
  { "showInode", "ShowInode", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_inode), XtRImmediate, (XtPointer) True },
  { "showType", "ShowType", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_type), XtRImmediate, (XtPointer) True },
  { "showGroup", "ShowGroup", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_group), XtRImmediate, (XtPointer) True },
  { "showLinks", "ShowLinks", XtRBoolean, sizeof(Boolean),
      XtOffsetOf(Resources, show_links), XtRImmediate, (XtPointer) True },
  { "defaultDisplayType", "DefaultDisplayType", XtRDisplayType, 
      sizeof(DisplayType), XtOffsetOf(Resources, default_display_type),
      XtRImmediate, (XtPointer) Icons },
  { "initialDisplayType", "InitialDisplayType", XtRDisplayType, 
      sizeof(DisplayType), XtOffsetOf(Resources, initial_display_type),
      XtRImmediate, (XtPointer) Tree },
  { "defaultSortType", "DefaultSortType", XtRSortType, 
      sizeof(SortType), XtOffsetOf(Resources, default_sort_type),
      XtRImmediate, (XtPointer) SortByName },
  { "doubleClickTime", "DoubleClickTime", XtRInt, sizeof(int),
      XtOffsetOf(Resources, double_click_time), XtRImmediate,
      (XtPointer) 300 },
  { "updateInterval", "UpdateInterval", XtRInt, sizeof(int),
      XtOffsetOf(Resources, update_interval), XtRImmediate,
      (XtPointer) 10000 },
#ifdef ENHANCE_PERMS
  { "hardUpdateTicks", "HardUpdateTicks", XtRInt, sizeof(int),
      XtOffsetOf(Resources, hard_update_ticks), XtRImmediate, (XtPointer)0 },
#endif
  { "defaultEditor", "DefaultEditor", XtRString, sizeof(String),
      XtOffsetOf(Resources, default_editor), XtRString, NULL },
  { "defaultViewer", "DefaultViewer", XtRString, sizeof(String),
      XtOffsetOf(Resources, default_viewer), XtRString, NULL },
  { "defaultXterm", "DefaultXterm", XtRString, sizeof(String),
      XtOffsetOf(Resources, default_xterm), XtRString, NULL },
  { "BourneShells", "ShellList", XtRString, sizeof(String),
      XtOffsetOf(Resources, sh_list), XtRString, NULL },
#ifdef ENHANCE_USERINFO
  { "shellCommand", "ShellCommand", XtRString, sizeof(String),
      XtOffsetOf(Resources, sh_cmd), XtRString, NULL },
#endif
#ifdef ENHANCE_HISTORY
  { "historyMaxN", "HistoryMaxN", XtRInt, sizeof(int),
      XtOffsetOf(Resources, history_max_n), XtRImmediate, (XtPointer)30 },
#endif
#ifdef ENHANCE_TRANSLATIONS
  { "appDefsVersion","AppDefsVersion",XtRString,sizeof(String),
      XtOffsetOf(Resources, app_defs_version), XtRString, (XtPointer)"NOT SET"},
#endif
#ifdef XPM
#ifdef ENHANCE_CMAP
  { "colorCloseness","ColorCloseness",XtRInt,sizeof(int),
      XtOffsetOf(Resources, color_closeness), XtRImmediate, (XtPointer)40000},
#endif
#endif
#ifdef ENHANCE_SELECTION
  { "highlightColor","HighlightColor",XtRPixel,sizeof(Pixel),
      XtOffsetOf(Resources, highlight_pixel), XtRString, (XtPointer)XtDefaultForeground},
  { "selectionPathsSeparator","SelectionPathsSeparator",XtRString,sizeof(String),
      XtOffsetOf(Resources, selection_paths_separator), XtRString, (XtPointer)" "},
#endif
};

/*---------------------------------------------------------------------------
 Fallback resources
---------------------------------------------------------------------------*/

static String fallback_resources[] = {
#ifdef ENHANCE_TRANSLATIONS
  "Xfm.appDefsVersion: \"\"",
#endif
  "Xfm*Command.cursor : hand2",
  "Xfm*MenuButton.cursor : hand2",
  "Xfm*popup form*bitmap.borderWidth : 0",
  "Xfm*popup form*label.borderWidth : 0",
  "Xfm*button box.orientation : horizontal",
  "Xfm*button box.borderWidth: 0",
  "Xfm*file window*viewport.borderWidth: 0",
  "Xfm*viewport.icon box*Label.borderWidth : 0",
  "Xfm*viewport.icon box.Command.borderWidth : 0",
  "Xfm*viewport.icon box.Form.borderWidth : 0",
  "Xfm*viewport.icon box*Toggle.borderWidth : 1",
  "Xfm*chmod*Label.borderWidth : 0",
  "Xfm*info*Label.borderWidth : 0",
  "Xfm*error*Label.borderWidth : 0",
  "Xfm*confirm*Label.borderWidth : 0",
#ifndef ENHANCE_TXT_FIELD
  "Xfm*Text*translations : #override \\n\
    <Key>Return: no-op() \\n\
    <Key>Linefeed : no-op() \\n\
    Ctrl<Key>J : no-op() \\n",
  "Xfm*Text*baseTranslations : #override \\n\
    <Key>Return: no-op() \\n\
    <Key>Linefeed : no-op() \\n\
    Ctrl<Key>J : no-op() \\n",
#endif
NULL,
};

/*---------------------------------------------------------------------------
  Widget argument lists
---------------------------------------------------------------------------*/

static Arg shell_args[] = {
  { XtNtitle, (XtArgVal) "Applications" }
};

/*-----------------------------------------------------------------------------
  Signal handler - clears up Zombie processes
  I'll probably extend this in the future to do something useful.
-----------------------------------------------------------------------------*/
static void sigcldHandler(int i)
{
  waitpid(-1,NULL,WNOHANG);
}

static struct sigaction sigcld, sigterm;

/*---------------------------------------------------------------------------
  Resource converter functions
---------------------------------------------------------------------------*/

static void CvtStringToDisplayType(XrmValue *args, Cardinal *n_args,
				   XrmValue *fromVal, XrmValue *toVal)
{
  static DisplayType d;

  if (!strcmp(fromVal->addr, "Tree"))
    d = Tree;
  else if (!strcmp(fromVal->addr, "Icons"))
    d = Icons;
  else if (!strcmp(fromVal->addr, "Text"))
    d = Text;
  else {
    XtStringConversionWarning(fromVal->addr, XtRDisplayType);
    return;
  }
  
  toVal->addr = (caddr_t) &d;
  toVal->size = sizeof(DisplayType);
}

/*---------------------------------------------------------------------------*/
 
static void CvtStringToSortType(XrmValue *args, Cardinal *n_args,
				XrmValue *fromVal, XrmValue *toVal)
{
  static SortType d;

  if (!strcmp(fromVal->addr, "SortByName"))
    d = SortByName;
  else if (!strcmp(fromVal->addr, "SortBySize"))
    d = SortBySize;
  else if (!strcmp(fromVal->addr, "SortByDate"))
    d = SortByMTime;
  else {
    XtStringConversionWarning(fromVal->addr, XtRSortType);
    return;
  }
  
  toVal->addr = (caddr_t) &d;
  toVal->size = sizeof(SortType);
}

/*---------------------------------------------------------------------------
  `Xfm.BourneShells' related functions  
---------------------------------------------------------------------------*/  

int shell_test(UserInfo *ui)
{
  int pipe_fd[2];
  int p;
  char val[3];

  if (pipe(pipe_fd) < 0) {
    perror("Can't create pipe");
    exit(1);
  }

  p = fork();
  if (p < 0) {
    perror("Can't fork");
    exit(1);
  }

  if (!p) {       /* child; exec the shell w/ test args */
    dup2(pipe_fd[1], fileno(stdout));
    if (close(pipe_fd[0]) == -1) {
      perror("(child) Can't close pipe");
      exit(1);
    }
    execlp(ui->shell, ui->shell, "-c", "echo $*", "1", NULL);
    perror("Exec failed");
    exit(1);
  } else {        /* parent; read and check the child's output */
    if (close(pipe_fd[1]) == -1) {
      perror("(parent) Can't close pipe");
      exit(1);
    }
    val[0] = '\0';
    while ((p = read(pipe_fd[0], val, 3)) < 0) {
      if (errno != EINTR) {
	perror("Reading child's output failed");
	exit(1);
      }
    }
    if (p == 3)
      return -1;
    ui->arg0flag = (val[0] != '1');
    return 0;
  }
}

char *get_first(char *s)
{
  char *p;

  p = strtok(s, ",");
  if (p != NULL)
    while (isspace(*p))
      p++;
  return p;
}

char *get_next()
{
  char *p;
  
  p = strtok((char *) NULL, ",");
  if (p != NULL)
    while (isspace(*p))
      p++;
  return p;
}

void init_arg0flag()
{
  if (resources.sh_list == NULL || !strcmp(resources.sh_list, "AUTO")) {
    if (shell_test(&user) == -1) {
      fprintf(stderr, "Xfm.BourneShells: AUTO test failed.\n");
      exit(1);
    }
  } else {
    char *p;

    for (p = get_first(resources.sh_list); p != NULL; p = get_next()) {
      if ((user.arg0flag = !strcmp(p, user.shell)))
        return;
    }
  }
}

#ifdef ENHANCE_USERINFO

/* 
 * test if the file named '*name' is executable
 * *name ist copied to ui->shell in this case.
 *
 * return values:
 *   0	  ok (copy *name to ui->shell)
 *   1    path not a regular file
 *   2	  regular file, but no permission to execute
 */

static int shellpath_not_valid(char *name,UserInfo *ui)
{
  struct stat	buf;
  int		rval;

  if (stat(name,&buf) || ! S_ISREG(buf.st_mode)) rval=1;
  else if (ui->uid==buf.st_uid) rval=(buf.st_mode & S_IXUSR ? 0:2); 
  else if (ui->gid==buf.st_gid) rval=(buf.st_mode & S_IXGRP ? 0:2); 
  else rval = (buf.st_mode & S_IXOTH ? 0:2);

  if (rval==2) fprintf(stderr,"%s exists, no permission to execute\n",name);
  if (rval==0) strcpy(ui->shell,name);
  return rval;
}

/* 
 * retrieve the name of shell form the
 * application resources and put it into ui->shell.
 * if this fails, try to read the environment var SHELL
 * and finally use "/bin/sh"
 *
 * returns 0 if no executable shell was found.
 */
static int get_sh_from_resources(UserInfo *ui)
{
  char 	*p;

  if (0!=(p=resources.sh_cmd)) {
    if (!shellpath_not_valid(p,ui)) return 1;
  }

  if (!(p = getenv("SHELL"))) p="/bin/sh";

  return ! shellpath_not_valid(p,ui);
}
#endif

/*---------------------------------------------------------------------------
  Main function
---------------------------------------------------------------------------*/

#ifdef ENHANCE_TXT_FIELD
void noWarnings(String msg)
{
}
#endif

#ifdef ENHANCE_TRANSLATIONS
static char app_ver0[100], *app_ver;
#endif

int main(int argc, char *argv[])
{
  char *s;
#ifdef ENHANCE_TRANSLATIONS
  char *p, *xfmversion = XFMVERSION;
#endif

  progname = argv[0];

  /* get some information about the user */
  user.uid = getuid();
  user.gid = getgid();

#ifndef ENHANCE_USERINFO /* read app-resources first */
  if ((s = getenv("HOME")))
    strcpy(user.home,s);
  else
    getwd(user.home);

  if ((s = getenv("SHELL")))
    strcpy(user.shell,s);
  else
    strcpy(user.shell,"/bin/sh");
#endif

  user.umask = umask(0);
  umask(user.umask);
  user.umask = 0777777 ^ user.umask;

  /* initialise the application and create the application shell */

  aw.shell = XtAppInitialize(&app_context, "Xfm", options, XtNumber(options),
			     &argc, argv, fallback_resources, shell_args,
			     XtNumber(shell_args) );

  /* First, initialize the atoms */
  /* initialise the communications module */
  initComms();

  /* make sure we can close-on-exec the display connection */
  if (fcntl(ConnectionNumber(XtDisplay(aw.shell)), F_SETFD, 1) == -1)
    abortXfm("Couldn't mark display connection as close-on-exec");

  /* register resource converters */
  XtAppAddConverter(app_context, XtRString, XtRDisplayType, 
		    CvtStringToDisplayType, NULL, ZERO);
  XtAppAddConverter(app_context, XtRString, XtRSortType, 
		    CvtStringToSortType, NULL, ZERO);

  /* get the application resources */
  XtGetApplicationResources(aw.shell, &resources, resource_list,
			    XtNumber(resource_list), NULL, ZERO);

  /* -version: print version number and exit: */
  if (resources.version) {
    printf("xfm version %s.%s\n", xfmversion,XFMMINORVERSION);
    exit(0);
  }

#ifdef ENHANCE_LOG
  /* we need the 'tick' bitmap already here */
  readBitmaps();
  (void)FmCreateLog(aw.shell,resources.button_font);
#endif


#ifdef ENHANCE_USERINFO
  getwd(user.cwd);
  if (!(s = getenv("HOME"))) s=user.cwd;
  strcpy(user.home,s);

  if (!get_sh_from_resources(&user)) {
	fprintf(stderr, "%s%s\n",
			"no executable shell found in ",
			"(resources, environment, /bin/sh)");
	exit(1);
  }
#endif

  /* default is to launch both application and file manager */
  if (!resources.appmgr && !resources.filemgr)
    resources.appmgr = resources.filemgr = True;

  /* set the multi-click time */
  XtSetMultiClickTime(XtDisplay(aw.shell), resources.double_click_time);

  /* initialise the utilities module */
  initUtils();

  /* set up signal handlers */
  sigcld.sa_handler = sigcldHandler;
  sigemptyset(&sigcld.sa_mask);
  sigcld.sa_flags = 0;
  sigaction(SIGCHLD,&sigcld,NULL);
  sigterm.sa_handler = quit;
  sigemptyset(&sigterm.sa_mask);
  sigterm.sa_flags = 0;
  sigaction(SIGTERM,&sigterm,NULL);

  /* check the user's shell; needs signal handlers (to avoid a zombie) */
  init_arg0flag();

#ifndef ENHANCE_LOG
  /* create all the bitmaps & cursors needed */
  readBitmaps();
#endif
  
  /* Set the icon for the application manager */
  XtVaSetValues(aw.shell, XtNiconPixmap, bm[APPMGR_BM], NULL);
  XtVaSetValues(aw.shell, XtNiconMask, bm[APPMGRMSK_BM], NULL);

  /* create the main popup shells */
  createMainPopups();

#ifdef ENHANCE_TRANSLATIONS
  /* we don't want to define all the translations as fallback_resources
   * rather we give up if the application defaults are not properly
   * installed. (appDefsVersion should be set by the app. defs. file to
   * the version nr. matching XFMVERSION)
   */
  /*strip eventual " */
  if (resources.app_defs_version)
    strcpy(app_ver0, resources.app_defs_version);
  else
    app_ver0[0] = '\0';
  app_ver = app_ver0;
  if (*app_ver==(char)'"') app_ver++;
  if (p=strrchr(app_ver,'"')) *p=0;
  if (strcmp(app_ver,xfmversion)) {
    if (*app_ver ==(char)0)
	error("Sorry: Appl. Defaults Not Found",
	      "check XFILESEARCHPATH and friends");
    else {
	p=XtMalloc(128);
	sprintf(p,"found: %s, needed: %s",
	        app_ver,xfmversion);
	error("Appl. Defaults: wrong version number",p);
        XTFREE(p);
    }
    exit(1);
  }
#endif

  /* construct the config file names */
  strcpy(resources.app_file, resources.app_file_r);
  fnexpand(resources.app_file);
  strcpy(resources.main_app_file, resources.app_file);
  strcpy(resources.app_dir, resources.app_dir_r);
  fnexpand(resources.app_dir);
  strcpy(resources.app_clip, resources.app_clip_r);
  fnexpand(resources.app_clip);
  strcpy(resources.cfg_file, resources.cfg_file_r);
  fnexpand(resources.cfg_file);
  strcpy(resources.dev_file, resources.dev_file_r);
  fnexpand(resources.dev_file);
#ifdef MAGIC_HEADERS
  strcpy(resources.magic_file, resources.magic_file_r);
  fnexpand(resources.magic_file);
#endif

  /* check for config dir and run xfm.install if appropriate */
  if (!exists(resources.app_dir))
    if (confirm("It appears that you are running xfm for the first time.",
		"Would you like me to run the xfm.install script now",
		"to create your personal setup?"))
      system("xfm.install -n");
    else if (aborted)
      exit(1);

  /* initialise the applications module & create the window */
  if (resources.appmgr) {
    readApplicationData(resources.app_file);
    createApplicationWindow();
    createApplicationDisplay();
  } else {
	/* create a dummy window to store the WM_COMMAND property */
	XtVaSetValues(aw.shell,
		XtNmappedWhenManaged,False,
		0);
  }
  /* still realize and set the WM properties. the WM needs
   * a window around and kwm is even able to restart an app
   * without a mapped toplevel window.
   */
  XtRealizeWidget(aw.shell);
  setApplicationWindowName();
  setWMProps(aw.shell);
  XtAddEventHandler(aw.shell, (EventMask)0L, True,
	    (XtEventHandler)clientMessageHandler, (XtPointer)NULL);

#ifdef ENHANCE_SELECTION
  /* initialize the selection ICCM */
  FmSelectionInit(XtDisplay(aw.shell));
#endif

  /* initialise the file windows module & create a file window */
  initFileWindows();
  if (resources.filemgr)
#ifdef ENHANCE_USERINFO
    newFileWindow(user.cwd,resources.initial_display_type,False);
#else
    newFileWindow(user.home,resources.initial_display_type,False);
#endif
  resources.init_geometry = NULL;

  /* start up window refresh timer */
  if (resources.update_interval > 0)
    XtAppAddTimeOut(app_context, resources.update_interval, timeoutCb, NULL);

#ifdef ENHANCE_TXT_FIELD
  /* those 'text cursor out of range' messages from the TextField widget kept
     annoying me, so I switch Xt warnings off here --AG */
  XtAppSetWarningHandler(app_context, noWarnings);
#endif

  /* collect & process events */
  XtAppMainLoop(app_context);
  return 0;
}

/*---------------------------------------------------------------------------*/

void quit()
{
  int d;

  for (d = 0; d < n_devices; d++)
    if (devs[d].mounted) {
      devs[d].mounted = 1;
      umountDev(d);
    }

  XtDestroyApplicationContext(app_context);
  exit(0);
}
