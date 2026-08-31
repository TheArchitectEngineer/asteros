/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)            | */
/* | Copyright 1995, 1996 Torsten Martinsen (bullestock@dk-online.dk)  | */
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

/* $Id: main.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#define DEFAULT_GEOMETRY "640x480"

#ifdef __VMS
#define XtDisplay XTDISPLAY
#define XtScreen XTSCREEN
#define XtWindow XTWINDOW
#endif

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#ifndef NOSTDHDRS
#include <stdlib.h>
#include <unistd.h>
#endif

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/xpm.h>
#include <X11/Xft/Xft.h>

#define DEFINE_GLOBAL
#include "xpaint.h"
#include "Paint.h"
#include "misc.h"
#include "operation.h"
#include "graphic.h"
#include "messages.h"
#include "protocol.h"
#include "image.h"
#include "rw/rwTable.h"

#include "XPaintIcon.xpm"

String *helpText;
int helpNum;
String *msgText;
int msgNum;

char lang[10];
char *routine;

extern int magnifier_closing_down;
extern int file_specified_zoom;
extern char *fontNames[];

extern void BrushInit(Widget toplevel);
extern int TestScriptC(char *file);
extern Image * ReadScriptC(char *file);
#ifdef XAW3DXFT
extern void SetMenuSpacing(int value);
extern void SetXftEncoding(int value);
extern void SetXftDefaultFontName(char *name);
extern void SetXftInsensitiveTwist(char *value);
extern void SetXawHilitColor(char *value);
#endif

static char *appDefaults[] =
{
#include "XPaint.ad.h"
    NULL
};

typedef struct {
    String visualType;
    String lang;
    String encoding;
    String menufont;
    String textfont;
    String spacing;
    String twistcolor;
    String hilitcolor;
    String size;
    String winsize;
    String help;
    String shareDir;   
    String rcFile;
    String helpFile;
    String msgFile;
    String dpi;
    String filter;
    String proc;
    String zoom;
    int undosize;
    int operation;
    Boolean fullmenu;
    Boolean menubar;
    Boolean canvas;
    Boolean screenshot;
    Boolean magnifier;
    Boolean horizontal;
    Boolean nowarn;
    Boolean astext;
} AppInfo;

static AppInfo appInfo;

static XtResource resources[] =
{
    {"visualType", "VisualType", XtRString, sizeof(String),
     XtOffset(AppInfo *, visualType), XtRImmediate, (XtPointer) "default"},
    {"lang", "Lang", XtRString, sizeof(String),
     XtOffset(AppInfo *, lang), XtRImmediate, (XtPointer) NULL},
    {"encoding", "Encoding", XtRString, sizeof(String),
     XtOffset(AppInfo *, encoding), XtRImmediate, (XtPointer) NULL},
    {"menufont", "MenuFont", XtRString, sizeof(String),
     XtOffset(AppInfo *, menufont), XtRImmediate, (XtPointer) NULL},
    {"textfont", "TextFont", XtRString, sizeof(String),
     XtOffset(AppInfo *, textfont), XtRImmediate, (XtPointer) NULL},
    {"spacing", "Spacing", XtRString, sizeof(String),
     XtOffset(AppInfo *, spacing), XtRImmediate, (XtPointer) NULL},
    {"twistcolor", "TwistColor", XtRString, sizeof(String),
     XtOffset(AppInfo *, twistcolor), XtRImmediate, (XtPointer) NULL},
    {"hilitcolor", "HilitColor", XtRString, sizeof(String),
     XtOffset(AppInfo *, hilitcolor), XtRImmediate, (XtPointer) NULL},
    {"size", "Size", XtRString, sizeof(String),
     XtOffset(AppInfo *, size), XtRImmediate, (XtPointer) DEFAULT_GEOMETRY},
    {"winsize", "WinSize", XtRString, sizeof(String),
     XtOffset(AppInfo *, winsize), XtRImmediate, (XtPointer) "0x0"},
    {"shareDir", "ShareDir", XtRString, sizeof(String),
     XtOffset(AppInfo *, shareDir), XtRImmediate, (XtPointer) NULL},   
    {"rcFile", "RcFile", XtRString, sizeof(String),
     XtOffset(AppInfo *, rcFile), XtRImmediate, (XtPointer) NULL},
    {"help", "Help", XtRString, sizeof(String),
     XtOffset(AppInfo *, help), XtRImmediate, (XtPointer) NULL},
    {"helpFile", "HelpFile", XtRString, sizeof(String),
     XtOffset(AppInfo *, helpFile), XtRImmediate, (XtPointer) NULL},
    {"msgFile", "MsgFile", XtRString, sizeof(String),
     XtOffset(AppInfo *, msgFile), XtRImmediate, (XtPointer) NULL},
    {"dpi", "Dpi", XtRString, sizeof(String),
     XtOffset(AppInfo *, dpi), XtRImmediate, (XtPointer) NULL},
    {"filter", "Filter", XtRString, sizeof(String),
     XtOffset(AppInfo *, filter), XtRImmediate, (XtPointer) NULL},
    {"proc", "Proc", XtRString, sizeof(String),
     XtOffset(AppInfo *, proc), XtRImmediate, (XtPointer) NULL},
    {"zoom", "Zoom", XtRString, sizeof(String),
     XtOffset(AppInfo *, zoom), XtRImmediate, (XtPointer) NULL},
    {"undosize", "UndoSize", XtRInt, sizeof(int),
     XtOffset(AppInfo *, undosize), XtRImmediate, (XtPointer) 4},
    {"operation", "Operation", XtRInt, sizeof(int),
     XtOffset(AppInfo *, operation), XtRImmediate, (XtPointer) 1},
    {"fullmenu", "FullMenu", XtRBoolean, sizeof(Boolean),
     XtOffset(AppInfo *, fullmenu), XtRImmediate, (XtPointer) False},
    {"menubar", "Menubar", XtRBoolean, sizeof(Boolean),
     XtOffset(AppInfo *, menubar), XtRImmediate, (XtPointer) True},
    {"canvas", "Canvas", XtRBoolean, sizeof(Boolean),
     XtOffset(AppInfo *, canvas), XtRImmediate, (XtPointer) False},
    {"screenshot", "Screenshot", XtRBoolean, sizeof(Boolean),
     XtOffset(AppInfo *, screenshot), XtRImmediate, (XtPointer) False},
    {"magnifier", "Magnifier", XtRBoolean, sizeof(Boolean),
     XtOffset(AppInfo *, magnifier), XtRImmediate, (XtPointer) False},
    {"horizontal", "Horizontal", XtRBoolean, sizeof(Boolean),
     XtOffset(AppInfo *, horizontal), XtRImmediate, (XtPointer) False},
    {"nowarn", "NoWarn", XtRBoolean, sizeof(Boolean),
     XtOffset(AppInfo *, nowarn), XtRImmediate, (XtPointer) False},
    {"astext", "AsText", XtRBoolean, sizeof(Boolean),
     XtOffset(AppInfo *, astext), XtRImmediate, (XtPointer) False},
};

static XrmOptionDescRec options[] =
{
    {"-24", ".visualType", XrmoptionNoArg, (XtPointer) "true24"},
    {"-12", ".visualType", XrmoptionNoArg, (XtPointer) "cmap12"},
    {"-language", ".lang", XrmoptionSepArg, (XtPointer) NULL},
    {"-encoding", ".encoding", XrmoptionSepArg, (XtPointer) NULL},
    {"-menufont", ".menufont", XrmoptionSepArg, (XtPointer) NULL},
    {"-textfont", ".textfont", XrmoptionSepArg, (XtPointer) NULL},
    {"-spacing", ".spacing", XrmoptionSepArg, (XtPointer) NULL},
    {"-twistcolor", ".twistcolor", XrmoptionSepArg, (XtPointer) NULL},
    {"-hilitcolor", ".hilitcolor", XrmoptionSepArg, (XtPointer) NULL},
    {"-size", ".size", XrmoptionSepArg, (XtPointer) NULL},
    {"-winsize", ".winsize", XrmoptionSepArg, (XtPointer) "0x0"},
    {"-sharedir", ".shareDir", XrmoptionSepArg, (XtPointer) SHAREDIR},   
    {"-rcfile", ".rcFile", XrmoptionSepArg, (XtPointer) NULL},
    {"-helpfile", ".helpFile", XrmoptionSepArg, (XtPointer) NULL},
    {"-msgfile", ".msgFile", XrmoptionSepArg, (XtPointer) NULL},
    {"-filter", ".filter", XrmoptionSepArg, (XtPointer) NULL},
    {"-proc", ".proc", XrmoptionSepArg, (XtPointer) NULL},
    {"-zoom", ".zoom", XrmoptionSepArg, (XtPointer) "1"},
    {"-undosize", ".undosize", XrmoptionSepArg, (XtPointer) "2"},
    {"-operation", ".operation", XrmoptionSepArg, (XtPointer) "1"},
    {"-fullmenu", ".fullmenu", XrmoptionNoArg, (XtPointer) "True"},
    {"-simplemenu", ".fullmenu", XrmoptionNoArg, (XtPointer) "False"},
    {"-menubar", ".menubar", XrmoptionNoArg, (XtPointer) "True"},
    {"-nomenubar", ".menubar", XrmoptionNoArg, (XtPointer) "False"},
    {"-canvas", ".canvas", XrmoptionNoArg, (XtPointer) "True"},
    {"-screenshot", ".screenshot", XrmoptionNoArg, (XtPointer) "True"},
    {"-magnifier", ".magnifier", XrmoptionNoArg, (XtPointer) "True"},
    {"-horizontal", ".horizontal", XrmoptionNoArg, (XtPointer) "True"},
    {"-nowarn", ".nowarn", XrmoptionNoArg, (XtPointer) "True"},
    {"-astext", ".astext", XrmoptionNoArg, (XtPointer) "True"},
    {"-help", ".help", XrmoptionNoArg, (XtPointer) "command"},
    {"+help", ".help", XrmoptionSepArg, (XtPointer) NULL},
    {"-8", ".visualType", XrmoptionNoArg, (XtPointer) "cmap8"},
    {"-visual", ".visualType", XrmoptionSepArg, (XtPointer) NULL},
    {"-dpi", ".dpi", XrmoptionSepArg, (XtPointer) NULL}
};


/*
**  Public query functions for application defaults
 */

int 
GetDefaultUndosize()
{
    if (appInfo.undosize<0) appInfo.undosize = 0;
    return appInfo.undosize;
}

void 
SetDefaultWHZ(int w, int h, int zoom)
{
    if (w<=0) w = 1;
    if (h<=0) h = 1;
    if (zoom==-1) zoom = 1;
    if (zoom<-16) zoom = -16;
    if (zoom>32) zoom = 32;
    Global.default_width = w;
    Global.default_height = h;
    Global.default_zoom = zoom;
}

void 
GetPaintWH(int *w, int *h)
{
    int x, y;
    unsigned int width, height;

    XParseGeometry(appInfo.winsize, &x, &y, &width, &height);
    *w = width;
    *h = height;
}

int
GetInitZoom()
{
    char *ptr;
    int z;

    if (!appInfo.zoom)
        return 1; 

    ptr = strchr(appInfo.zoom,':');
    if (!ptr) 
        ptr=strchr(appInfo.zoom,'/');
    if (ptr)
        z = -atoi(ptr+1);
    else
        z = atoi(appInfo.zoom);
    if (z<-8) z = -8;
    if (z==-1) z = 0;
    if (z>32) z = 32;
    return z;
}

void 
SetMenuBarVisibility(Widget w, Boolean flag)
{
    XtVaSetValues(w, XtNmenubar, flag, NULL);
}

Boolean
IsMenuBarVisible(Widget w)
{
    Boolean v;
    XtVaGetValues(w, XtNmenubar, &v, NULL);
    return v;
}

Boolean
IsMenuBarGlobal()
{
    return appInfo.menubar;
}

void 
SetFullMenu(Widget w, Boolean flag)
{
    XtVaSetValues(w, XtNfullmenu, flag, NULL);
}

Boolean
IsFullMenuSet(Widget w)
{
    Boolean v;
    XtVaGetValues(w, XtNfullmenu, &v, NULL);
    return v;
}

Boolean
IsFullMenuGlobal()
{
    return appInfo.fullmenu;
}

Boolean
ToolsAreHorizontal()
{
    return appInfo.horizontal;
}

char *
GetDefaultRC(void)
{
    return appInfo.rcFile;
}

char *
GetShareDir(void)
{
    return appInfo.shareDir;
}

double
GetDpi(void)
{
    return Global.dpi;
}

/*
**  Create a nice icon image for XPaint...
 */
void 
SetIconImage(Widget w)
{
    static Pixmap icon = None;
    static int iconW = 1, iconH = 1;
    Window iconWindow;
    Screen *screen = XtScreen(w);
    Display *dpy = XtDisplay(w);
    XpmAttributes myattributes;

    /*
    **  Build the XPaint icon
     */
    iconWindow = XCreateSimpleWindow(dpy, RootWindowOfScreen(screen),
				     0, 0,	/* x, y */
				     iconW, iconH, 0,
				     BlackPixelOfScreen(screen),
				     BlackPixelOfScreen(screen));
    if (icon == None) {
 	myattributes.valuemask = XpmCloseness;
 	myattributes.closeness = 65535;
 	if (XpmCreatePixmapFromData(dpy, iconWindow,
				    XPaintIcon_xpm, &icon, NULL, &myattributes)) {
	    fprintf(stderr, "%s",msgText[CANNOT_INSTALL_XPM_ICON]);
	    exit(1);
 	}
	GetPixmapWHD(dpy, icon, &iconW, &iconH, NULL);
	XResizeWindow(dpy, iconWindow, iconW, iconH);
    }
    XSetWindowBackgroundPixmap(dpy, iconWindow, icon);

    XtVaSetValues(w, XtNiconWindow, iconWindow, NULL);
}


/*
**  The rest of main
 */
static void 
usage(char *prog, char *msg)
{
    if (msg)
	fprintf(stderr, "%s\n", msg);
    fprintf(stderr, "%s %s (XPaint %s):\n", 
            msgText[USAGE_FOR], prog, XPAINT_VERSION);
    HelpTextOutput(stderr, appInfo.help == NULL ? "command" : appInfo.help);
    exit(1);
}

typedef struct {
    XtWorkProcId id;
    Widget toplevel;
    int nfiles;
    int pos;
    char **files;
} LocalInfo;

static void
processFile(char *file)
{
    void *v;
    int zoom = Global.default_zoom;

    StateSetBusy(True);

    if ((v = ReadMagic(file)) != NULL) {
        if (file_specified_zoom) zoom = file_specified_zoom;
        GraphicOpenFileZoom(Global.toplevel, file, v, zoom);
    } else
	Notice(Global.toplevel, msgText[UNABLE_TO_OPEN_INPUT_FILE],
	       file, RWGetMsg());

    StateSetBusy(False);
}

/*
 * You can specify:
 * the visual
 * the visual and the depth
 * the visualID
 * Any other combinations may not generate the results that you expect
 * Also note that the visualID is strongly dependent on the platform
 * being used.  We will let the user specify any visualID that they want.
 */
XVisualInfo
GetFutureVisual(Widget *toplevel, int desiredDepth, int desiredVisual,
		int desiredVisualID, Boolean *valid)
{
    Display *display = XtDisplay(*toplevel);
    int i, visnum = 0;
	
    XVisualInfo rtnval;
	
    int visualsMatched;    
     
    XVisualInfo vTemplate;   
    XVisualInfo visual[50]; /* If a machine has more than 50 visuals then this will fail. Higher end SGI's have 20 so this is probably OK */
    XVisualInfo *visualList = visual;
	

    *valid=FALSE;
      
    vTemplate.screen = DefaultScreen(display);

    if (desiredVisualID >= 0)
        visualList = XGetVisualInfo (display, VisualScreenMask, &vTemplate, &visualsMatched);
    else {
        vTemplate.class = desiredVisual;
	if (desiredDepth > 0) {
	    vTemplate.depth = desiredDepth;
	    visualList = XGetVisualInfo(display,
					VisualClassMask | VisualScreenMask |
					VisualDepthMask, &vTemplate,
					&visualsMatched);
	}
	else {
  	    visualList = XGetVisualInfo(display,
					VisualClassMask | VisualScreenMask,
					&vTemplate, &visualsMatched);
	}
    }

    if (visualsMatched && desiredVisualID >= 0) {
        visnum = -1;
	/* look through the entire list for our visual ID */
	for (i = 0; i < visualsMatched; i++) {
  	    if (XVisualIDFromVisual(visualList[i].visual) == desiredVisualID)
	        visnum = i;
	}
	if (visnum < 0)
	    visualsMatched=0;
    }
	
    if (visualsMatched == 0) {
        fprintf(stderr, "%s", msgText[NO_MATCH_FOUND_FOR_VISUAL]);
	exit(1);
    }
	
    rtnval = visualList[visnum];
    *valid = TRUE;
	  
    XFree(visualList);
	
    return rtnval;
}

void
LoadMessages()
{
    FILE *fd;
    int i, j;
    char namebuf[256];
    char buf[256];
    static char alert[] = "!! No messages found !!";
    static const char failure[] = "Memory allocation failure\n";
    static const char msgs_ver_str[] = "# XPaint Messages version ";
    static const int msgs_ver_str_len = sizeof(msgs_ver_str)/sizeof(char) - 1;

    helpNum = 0; 

    if (appInfo.helpFile) {
       if (*appInfo.helpFile!='/' && *appInfo.helpFile!='.')
          sprintf(namebuf, "%s/%s", appInfo.shareDir, appInfo.helpFile);
       else
	  strcpy(namebuf, appInfo.helpFile);
       fd = fopen(namebuf, "r");
    } else
       fd = NULL;
   
    if (!fd) {
        sprintf(namebuf, "%s/help/Help", appInfo.shareDir);
	fd = fopen(namebuf, "r");
    }
    if (fd) {
        while (!feof(fd)) {
            ++helpNum;
            helpText = (String *)realloc(helpText, helpNum*sizeof(String));
            fgets(buf, 250, fd);
            i = strlen(buf)-1;
            if (i>0 && buf[i] == '\n') buf[i] = '\0';
            helpText[helpNum-1] = strdup(buf);
        }
        fclose(fd);
    } else {
        fprintf(stderr, "No help file found !!\n");
    }

    msgNum = 0;
    if (appInfo.msgFile) {
       if (*appInfo.msgFile!='/' && *appInfo.msgFile!='.')
          sprintf(namebuf, "%s/%s", appInfo.shareDir, appInfo.msgFile);
       else
	  strcpy(namebuf, appInfo.msgFile);
       fd = fopen(namebuf, "r");
    } else
       fd = NULL;
    if (!fd) {
        sprintf(namebuf, "%s/messages/Messages", appInfo.shareDir);
	fd = fopen(namebuf, "r");
    }

    if (fd) {
        int version_checked = 0;
       
        while (!feof(fd)) {
            fgets(buf, 250, fd);
	    if (*buf == '#') {
	        /* GRR 20050806:  Avoid version incompatibilities by requiring
	         *  version number of runtime Messages file to match that with
	         *  which we were compiled.  Note that it need not necessarily
	         *  match XPAINT_VERSION, however. */
	        if (!strncmp(buf, msgs_ver_str, msgs_ver_str_len)) {
	            char *ptr = buf + msgs_ver_str_len;
	            int last = strlen(ptr) - 1;

	            if    (last >= 0 && ptr[last] == '\n')  ptr[last--] = '\0';
	            while (last >= 0 && isspace(ptr[last])) ptr[last--] = '\0';
	            if (strcmp(ptr, MESSAGES_VERSION)) {
	                fprintf(stderr, "Error: %s is version %s; expected "
	                  "version %s\n", namebuf, ptr, MESSAGES_VERSION);
	                exit(1);
	            }
	            version_checked = 1;
	        }
	        if (!strncmp(buf+1, "END_MESSAGES", 12)) break;
	        continue;
	    }
	    if (!version_checked) {
	        fprintf(stderr, "Error: %s is version 2.7.7 or earlier; "
	          "expected version %s\n", namebuf, MESSAGES_VERSION);
	        exit(1);
  	    }
            ++msgNum;
            msgText = (String *)realloc(msgText, msgNum*sizeof(String));
	    if (!msgText) {
	        fprintf(stderr, "%s", failure);
	        exit(1);
	    }
            i = 0;
            j = 0;
            while (buf[i]) {
	        if (buf[i] == '\n') break;
	        if (buf[i] == '\\' && buf[i+1] == 'n') {
                     ++i;
		     buf[j] = '\n';
		} else
		if (j<i)
		     buf[j] = buf[i];
                ++i;
                ++j;
	    }
	    buf[j] = '\0';
            msgText[msgNum-1] = strdup(buf);
	    if (!msgText[msgNum-1]) {
	        fprintf(stderr, "%s", failure);
	        exit(1);
	    }
        }
        fclose(fd);
    } else {
        msgNum = END_MESSAGES;
        msgText = (String *)realloc(msgText, msgNum*sizeof(String));
        for (i=0; i<msgNum; i++) msgText[i] = alert;
	fprintf(stderr, "%s\n\n", msgText[0]);
    }
}

String GetAppDefaultFile()
{
    char *lg;
    char name[256];
    int i;
    FILE *fd = NULL;

    if (!*lang && (lg = getenv("LANG"))) strncpy(lang, lg, 8);
    if (*lang) {
        for (i=0; i<=8 ; i++) lang[i] = tolower(lang[i]);
    }
    else
        strcpy(lang, "en");
    sprintf(name, "%s/XPaint_%s", XAPPLOADDIR, lang);
    fd = fopen(name, "r");
    if (*lang && !fd) {
        lang[2] = '\0';
        sprintf(name, "%s/XPaint_%s", XAPPLOADDIR, lang);
	fd = fopen(name, "r");
    }   
    if (fd) {
        fclose(fd);
	sprintf(name, "XPaint_%s", lang);
	return strdup(name);
    } else
        return strdup("XPaint");
}


static void
checkRCsize()
{
    FILE *fd;
    char buf[256];
    char *ptr;
    int w, h, i;

    if (!strcmp(appInfo.size, DEFAULT_GEOMETRY)) {
        if (appInfo.rcFile)
	    strcpy(buf, appInfo.rcFile);
	else
	    strcpy(buf, ".XPaintrc");
	if ((fd = fopen(buf, "r"))) {
	    i = 0;
            while (!feof(fd)) {
                fgets(buf, 250, fd);
		if (*buf && isspace(*buf)) ++i;
		if (!strncasecmp(buf, "defaultsize", 11)) {
		    ptr = buf + 12;
		    while (isspace(*ptr)) ++ptr;
	            if (sscanf(ptr, "%dx%d", &w, &h) == 2) {
	                 SetDefaultWHZ(w, h, GetInitZoom());
		    }
		    break;
		}
		if (i >= 4) break;
	    }
	    fclose(fd);
	}
    }
}

int
main(int argc, char *argv[])
{
    Display *dpy;
    Widget toplevel;
    char xpTitle[20];
    int i, j;
    XEvent event;
    Arg args[5];
    int nargs = 0;
    XrmDatabase rdb;
    Boolean isIcon, valid;

    Widget temp;
    int desiredVisual = -1;
    int opened = 0, mode;
    XVisualInfo vis;
    Image * image;
    int depth;
    int desiredDepth;
    int tempargc;
    char **tempargv;
    char phrase[128];
    String app_def_file;
    int desiredVisualID = -1;	 
    
    XftInitFtLibrary();
    InitTypeConverters();

    msgText = NULL;
    helpText = NULL;
    tempargc = argc;
    tempargv = xmalloc(argc * sizeof(char *));

    *lang = '\0';
    for (i = 0; i < argc; i++) {
        tempargv[i] = xmalloc(strlen(argv[i])+1);
	if (i < argc-1 && !strncasecmp(argv[i], "-language", 5))
	   strncpy(lang, argv[i+1], 8);
	strcpy(tempargv[i], argv[i]);
    }

    app_def_file = GetAppDefaultFile();
    if (!app_def_file) {
        fprintf(stderr, "%s : %s\n",
            "Can't find app_defaults file", app_def_file);
        exit(1);
    }

    /*
     * Create a temp widget that we can interrogate.
     * This widget will not be realized. 
     * There should be a better way to do this...
     */
    memset(&appInfo, 0, sizeof(AppInfo));
    temp = XtAppInitialize(&Global.appContext, app_def_file,
			   options, XtNumber(options), &tempargc, tempargv,
			   appDefaults, args, nargs);

    XtGetApplicationResources(temp, (XtPointer) & appInfo,
			      resources, XtNumber(resources), NULL, 0);

#ifdef XAW3DXFT
    /* set UTF8 (code 0) as default encoding */
    SetXftEncoding((appInfo.encoding)? atoi(appInfo.encoding) : 0);
    /* set default Xft police = Liberation-9 point if none defined */
    SetXftDefaultFontName((appInfo.menufont)? appInfo.menufont:"Liberation-9");
    SetMenuSpacing((appInfo.spacing)? atoi(appInfo.spacing):1);
    SetXftInsensitiveTwist((appInfo.twistcolor)? appInfo.twistcolor:"#a00000");
    SetXawHilitColor((appInfo.hilitcolor)? appInfo.hilitcolor:"#332211");
#endif
   /*
    **  Load help and messages files
     */
    LoadMessages();

    rdb = XtDatabase(XtDisplay(temp));
    for (i = 0; i < tempargc; i++)
        free(tempargv[i]);
	 
    if (appInfo.help != NULL)
	usage(argv[0], NULL);

    desiredDepth = 0;
    if (strcmp(appInfo.visualType, "default") != 0) {
        char *cp = appInfo.visualType;
	char *vp = phrase;

	/* this includes the terminating null character */
	for (i = 0; i <= strlen(cp); i++)
	    phrase[i] = tolower(cp[i]);
		 
	if (strncmp(vp, "cmap", 4) == 0) {
	    XrmPutStringResource(&rdb, "Canvas*visual", "PseudoColor");
	    desiredVisual = PseudoColor;
	    vp += 4;
	} else if (strncmp(vp, "directcolor", 11) == 0) {
	    XrmPutStringResource(&rdb, "Canvas*visual", "DirectColor");
	    desiredVisual = DirectColor;
	    desiredDepth = 24;
	    vp += 11;
	} else if (strncmp(vp, "truecolor", 9) == 0) {
	    XrmPutStringResource(&rdb, "Canvas*visual", "TrueColor");
	    desiredVisual = TrueColor;
	    desiredDepth = 24;
	    vp += 9;
	} else if (strncmp(vp, "grayscale", 9) == 0) {
	    XrmPutStringResource(&rdb, "Canvas*visual", "GrayScale");
	    desiredVisual = GrayScale;
	    desiredDepth = 8;
	    vp += 9;
	} else if (strncmp(vp, "pseudocolor", 11) == 0) {
	    XrmPutStringResource(&rdb, "Canvas*visual", "PseudoColor");
	    desiredVisual = PseudoColor;
	    vp += 11;
	} else if (strncmp(vp, "staticgray", 10) == 0) {
	    XrmPutStringResource(&rdb, "Canvas*visual", "StaticGray");
	    desiredVisual = StaticGray;
	    desiredDepth = 8;
	    vp += 10;
	} else if (strncmp(vp, "staticcolor", 11) == 0) {
	    XrmPutStringResource(&rdb, "Canvas*visual", "StaticColor");
	    desiredVisual = StaticColor;
	    desiredDepth = 8;
	    vp += 11;
	} else if (strncmp(vp, "true", 4) == 0) {
	    XrmPutStringResource(&rdb, "Canvas*visual", "TrueColor");
	    desiredVisual = TrueColor;
	    vp += 4;
	} else if (strncmp(vp, "gray", 4) == 0) {
	    XrmPutStringResource(&rdb, "Canvas*visual", "StaticGray");
	    desiredVisual = StaticGray;
	    vp += 4;
	} else if (atoi(vp)>0 || (vp[0]=='0' && vp[1]=='\0')) {
 	    desiredVisualID = atoi(vp);
	    *vp = '\0';
	} else {
	    fprintf(stderr, msgText[BAD_VISUAL_TYPE_SPECIFICATION],
		    argv[0], vp);
	    exit(1);
	}
	if (*vp != '\0') {
	    desiredDepth=atoi(vp);
	    if (desiredDepth > 0) 
	        XrmPutStringResource(&rdb, "Canvas*depth", vp);
	    else
	        desiredDepth = 0;
	}
    }
    else {
        /* go find out what the default visual is and get its parameters */
        Global.vis.depth = DefaultDepth(XtDisplay(temp),
					DefaultScreen(XtDisplay(temp)));
	Global.vis.visual = DefaultVisual(XtDisplay(temp),
					  DefaultScreen(XtDisplay(temp)));
    }

    if (desiredVisual >= 0 || desiredVisualID >= 0) {
        vis = GetFutureVisual(&temp, desiredDepth,
			      desiredVisual, desiredVisualID, &valid);
		 
	if (valid) {
	    depth = vis.depth;
	    Global.vis = vis;
	}
	else {
	    fprintf(stderr, "%s", msgText[NO_MATCH_FOUND_FOR_VISUAL]);
	    exit(1);
	}
    }
    else {
        Global.vis.depth = DefaultDepth(XtDisplay(temp),
					DefaultScreen(XtDisplay(temp)));
	Global.vis.visual = DefaultVisual(XtDisplay(temp),
					  DefaultScreen(XtDisplay(temp)));
    }
	 
    sprintf(phrase, "%d", Global.vis.depth);
    XrmPutStringResource(&rdb, "Canvas*depth", phrase);

    /* destroy our temporary widget */
    XtDestroyWidget(temp);

    /*
    **  Create the application context
     */

    toplevel = XtAppInitialize(&Global.appContext, app_def_file,
			       options, XtNumber(options), &argc, argv,
			       appDefaults, args, nargs);

    dpy = Global.display = XtDisplay(toplevel);
    Global.visual = DefaultVisual(dpy, DefaultScreen(dpy));
    Global.depth = DefaultDepth(dpy, DefaultScreen(dpy));
    if (Global.depth == 16 && Global.visual->green_mask == 992) 
        Global.depth = 15;

    memset(&appInfo, 0, sizeof(AppInfo));
    XtGetApplicationResources(toplevel, (XtPointer) &appInfo,
			      resources, XtNumber(resources), NULL, 0);

    if (argc != 1 && argv[1][0] == '-')
	usage(argv[0], "Invalid option");

    checkRCsize();

    /*
    **  A little initialization
     */
    Global.toplevel = toplevel;
    Global.canvas = None;
    Global.back = None;
    Global.patternshell = None;
    Global.patterninfo = NULL;
    Global.clipboard.image = NULL;
    Global.clipboard.cmap = None;
    Global.clipboard.width = 0;
    Global.clipboard.height = 0;
    Global.clipboard.pix = None;
    Global.clipboard.mask = None;
    Global.clipboard.alpha = NULL;
    Global.brushpix = None;
    Global.nbrushes = 0;
    Global.brushes = NULL;
    Global.cap = 0;
    Global.join = 0;
    Global.escape = False;
    Global.transparent = True;
    Global.astext = False;
    Global.dashlist[0] = '\0';
    Global.dashnumber = 0;
    Global.dashoffset = 0;
    Global.dpi = 300.0;
    Global.numregions = 0;
    Global.regiondata = NULL;
    Global.curpaint = None;
    Global.popped_up = None;
    Global.popped_parent = None;
    Global.numfiles = 0;
    Global.loadedfiles = NULL;
    Global.numpage = 1;
    Global.operation = 0;
    Global.alpha_bg = 0;
    Global.alpha_fg = 144;
    Global.alpha_threshold = 128;
    memset(Global.bg, 0xff, 3);
    if (Global.depth <= 8) {
        Global.bg[4] = BlackPixelOfScreen(XtScreen(toplevel));
        Global.bg[6] = WhitePixelOfScreen(XtScreen(toplevel));
    }
    /* select by default Times Regular at 18 pt */
    Global.xft_text = NULL;
    Global.xft_name = NULL;
    Global.xft_font = NULL;

    /*
    **
     */
    XtVaGetValues(toplevel, XtNiconic, &isIcon, NULL);
    if (isIcon)
	XrmPutStringResource(&rdb, "Canvas.iconic", "on");

	 
    /*
    **  GRR 960525:  check depth and warn user (use AlertBox() instead?)
     */
    if (!appInfo.nowarn && !appInfo.screenshot) {
	int depth = Global.vis.depth;

	/* XtVaGetValues(toplevel, XtNdepth, &depth, NULL); */
	HelpTextOutput(stderr, "data_loss");
	fprintf(stderr, msgText[YOUR_CANVAS_DEPTH_IS_THAT_MANY_BITS], depth);
	switch (depth) {
	    case 8:
	    case 12:
		fprintf(stderr, "%s", msgText[SEPARATOR_STRIP_ONE]);
		fprintf(stderr, "%s", msgText[WARNING_ONE]);
		fprintf(stderr, "%s", msgText[SEPARATOR_STRIP_ONE]);
		fprintf(stderr, "\n");
		break;
	    case 1:
	    case 2:
	    case 4:
	    case 6:
	    case 15:
	    case 16:
		fprintf(stderr, "%s", msgText[SEPARATOR_STRIP_TWO]);
		fprintf(stderr, "%s", msgText[WARNING_TWO_A]);
		fprintf(stderr, "%s", msgText[WARNING_TWO_B]);
		fprintf(stderr, "%s", msgText[SEPARATOR_STRIP_TWO]);
		fprintf(stderr, "\n");
		break;
	    case 24:
	    case 32:
		fprintf(stderr, "%s", msgText[WARNING_THREE_A]);
		fprintf(stderr, "%s", msgText[WARNING_THREE_B]);
		fprintf(stderr, "%s", msgText[WARNING_THREE_C]);
		break;
	    default:
		break;
	}
    }

    /*
    **  Now build and construct the widgets
     */

    Global.timeToDie = False;
    Global.explore = False;

    Global.appContext = XtWidgetToApplicationContext(toplevel);

    /*
    **  Call the initializers
     */

    OperationInit(toplevel);
    if (appInfo.operation>0) OperationSet(NULL, -appInfo.operation);

    /*
    **  A few rogue initializers
     */
    BrushInit(toplevel);
    HelpInit(toplevel);
    setDefaultGlobalFont(dpy, 
        (appInfo.textfont)?appInfo.textfont:fontNames[1]);
    SRANDOM(time(0));
    SetIconImage(toplevel);

    /*
    **  GRR 960526:  put version number in title string (and icon name?)
     */
    sprintf(xpTitle, "XPaint %s", XPAINT_VERSION);
    XtVaSetValues(toplevel, XtNtitle, xpTitle, NULL);
    XtVaSetValues(toplevel, XtNiconName, xpTitle, XtNtitle, xpTitle, NULL);

    if (appInfo.dpi) {
        Global.dpi = atof(appInfo.dpi);
        if (Global.dpi<0) Global.dpi = 0.0;
    }
    Global.astext = appInfo.astext;

    XParseGeometry(appInfo.size, &i, &j, 
                   &Global.default_width, &Global.default_height);
    Global.default_zoom = GetInitZoom();

    if (appInfo.screenshot) {
        ScreenshotImage(toplevel, NULL, 0);
    }

    if (appInfo.magnifier) {
        StartMagnifier(toplevel);
    }

    /*
    **  Realize it (doesn't hurt)
     */
    if (appInfo.magnifier && argc==1)
        XtVaSetValues(toplevel, XtNmappedWhenManaged, False, NULL);
    XtRealizeWidget(toplevel);
    XtSetMinSizeHints(toplevel, 120, 82);

    /*
    **  Now open any file on the command line
     */
    mode = 0;

    if (argc > 1)
    for (i = 1; i < argc; i++) {
        /* loading a file */
        if (!strcmp(argv[i], "/l")) mode = 0;
        else
        /* loading clipboard */
        if (!strcmp(argv[i], "/c")) mode = 1;
        else
        /* opening a file */
        if (!strcmp(argv[i], "/o")) mode = 2;
        else
	switch(mode) {
	    case 0:
	        AddFileToGlobalList(argv[i]);
	        break;
	    case 1:
	        AddFileToGlobalList(argv[i]);
  	        image = ImageFromFile(argv[i]);
                if (image)
                    ImageToMemory(image);
                break;
	    case 2:
	        ++opened;
                processFile(argv[i]);
                break;
	}
    } else if (appInfo.canvas) {
	/*
	**  If nothing is coming up, bring up a blank canvas
	 */
        ++opened;
	GraphicCreate(toplevel, 0);
    }

    XtUnmanageChild(Global.bar);
    XMapWindow(dpy, XtWindow(Global.bar));
    XtUnmanageChild(Global.back);
    XMapWindow(dpy, XtWindow(Global.back));

    /* Check if there's a filter being loaded */
    if (appInfo.filter) {
        if (TestScriptC(appInfo.filter) == 2)
           ReadScriptC(appInfo.filter);
        else
           Notice(toplevel, msgText[UNABLE_TO_LOAD_FILTER]);
    }

    /* Check if there's a procedure being loaded */
    if (appInfo.proc) {
        if (TestScriptC(appInfo.proc) == 3)
           ReadScriptC(appInfo.proc);
        else
           Notice(toplevel, msgText[UNABLE_TO_LOAD_PROCEDURE]);
    }      

    if (Global.numfiles>0 && opened==0) {
        ++opened;
        processFile(Global.loadedfiles[0]);
    }

    if (Global.numfiles>opened)
        GetFileName(Global.toplevel, BROWSER_LOADED, NULL, 
                    GraphicOpenFile, NULL);

    /*
    **  MainLoop
     */
    do {
	XtAppNextEvent(Global.appContext, &event);
	if (event.type == ButtonPress || event.type == ButtonRelease)
	    Global.currentTime = event.xbutton.time;
	XtDispatchEvent(&event);
        if (appInfo.magnifier && argc==1 && magnifier_closing_down) {
	    XtVaSetValues(toplevel, XtNmappedWhenManaged, True, NULL);
            RaiseWindow(dpy, XtWindow(toplevel));
            appInfo.magnifier = 0;
	}
    }
    while (!Global.timeToDie);
    return 0;
}

