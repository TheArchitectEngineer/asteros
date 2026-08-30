/*
 *  Enhancements to the X-File Manager XFM-1.3.2 (FileList Widget)
 *  --------------------------------------------------------------
 
 *  Copyright (C) 1997  by Till Straumann   <strauman@sun6hft.ee.tu-berlin.de>

 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 */
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include "FileListP.h"
#include <X11/Xaw/XawInit.h>

#define FL (flw->fileList)

static XtResource fl_resources[] = {
#define offset(field) XtOffsetOf(FileListRec, fileList.field)
    /* {name, class, type, size, offset, default_type, default_addr}, */
    { XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct*), 
	offset(font), XtRString, (XtPointer) XtDefaultFont },
    { XtNfiles, XtCFiles, XtRFileList, sizeof(FileList),
	offset(files), XtRImmediate, (XtPointer) 0 },
    { XtNnFiles, XtCNFiles, XtRInt, sizeof(int),
	offset(n_files), XtRImmediate, (XtPointer) -1 },
    { XtNhighlightColor, XtCHighlightColor, XtRPixel, sizeof(Pixel),
	offset(highlight_pixel), XtRString, (XtPointer) XtDefaultForeground },
    { XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(foreground), XtRString, (XtPointer) XtDefaultForeground },
    { XtNuserData, XtCUserData, XtRPointer, sizeof(XtPointer),
	offset(user_data), XtRPointer, (XtPointer) NULL },
    { XtNcallback, XtCCallback, XtRCallback, sizeof(XtPointer),
	offset(notify_cbl), XtRCallback, (XtPointer)NULL},
    { XtNdragTimeout, XtCDragTimeout, XtRInt, sizeof(int),
	offset(drag_timeout), XtRImmediate, (XtPointer)200},
/* modified core resources */
    { XtNborderWidth, XtCBorderWidth, XtRDimension, sizeof(Dimension),
	offset(border_width), XtRImmediate, (XtPointer) 2 },
#undef offset
};

/* Methods */
static void flDestroy(Widget);
static void ClassInitialize(void);
static void flInitialize(Widget,Widget,ArgList,Cardinal*);
static Boolean flSetValues(Widget,Widget,Widget,ArgList,Cardinal*);

/* private procedures */
static void ReleaseGCs(FileListWidget);
static void GetGCs(FileListWidget);
static void GetDefaultFileType(FileRec*);

/* Event Handlers */
static void FakeEntryLeave(Widget,XtPointer,XEvent*,Boolean *ctd);
static void CatchEntryLeave(Widget,XtPointer,XEvent*,Boolean *ctd);
static void StartTimer(Widget,XtPointer,XEvent*,Boolean *ctd);
static void DiscardMotion(Widget,XtPointer,XEvent*,Boolean *ctd);

/* Timer Callbacks */
static void EnableMotion(XtPointer,XtIntervalId *);

/* Actions */
static void DispatchDirExeFile(Widget, XEvent*, String*, Cardinal* );
static void DispatchDirExeFilePopup(Widget, XEvent*, String*, Cardinal* );
static void Notify(Widget, XEvent*, String*, Cardinal*);


static XtActionsRec actions[] =
{
  /* {name, procedure}, */
    {"dispatchDirExeFile",	DispatchDirExeFile},
    {"dispatchDirExeFilePopup",	DispatchDirExeFilePopup},
    {"notify",			Notify},

};

static char translations[] ="";
/*
"<Key>:		fileList()	\n\
";
*/

FileListClassRec fileListClassRec = {
  { /* core fields */
    /* superclass		*/	(WidgetClass) &widgetClassRec,
    /* class_name		*/	"FileList",
    /* widget_size		*/	sizeof(FileListRec),
    /* class_initialize		*/	ClassInitialize,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	flInitialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	XtInheritRealize,
    /* actions			*/	actions,
    /* num_actions		*/	XtNumber(actions),
    /* resources		*/	fl_resources,
    /* num_resources		*/	XtNumber(fl_resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	flDestroy,
    /* resize			*/	NULL,
    /* expose			*/	NULL,
    /* set_values		*/	flSetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	translations,
    /* query_geometry		*/	XtInheritQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
  { /* fileList fields */
    /* findEntry		*/	NULL,
    /* entryPosition		*/	NULL,
  }
};

WidgetClass fileListWidgetClass = (WidgetClass)&fileListClassRec;

static void flDestroy(Widget w)
{
 FileListWidget flw=(FileListWidget)w;
 ReleaseGCs(flw);
 XtRemoveEventHandler(w,XtAllEvents,True,FakeEntryLeave,(XtPointer)0);
 XtRemoveEventHandler(w,XtAllEvents,True,CatchEntryLeave,(XtPointer)0);
 XtRemoveEventHandler(w,XtAllEvents,True,DiscardMotion,(XtPointer)0);
 XtRemoveEventHandler(w,XtAllEvents,True,StartTimer,(XtPointer)0);
 if (FL.timer_running) XtRemoveTimeOut(FL.timeout_id);
 FL.timer_running=False;
}

static void ClassInitialize(void)
{
 XawInitializeWidgetSet();

 XtRegisterGrabAction(DispatchDirExeFilePopup, 
			True, ButtonPressMask | ButtonReleaseMask,
			GrabModeAsync, GrabModeAsync);
}

static void flInitialize(treq,tnew,args,n_args)
 Widget	  treq,tnew;
 ArgList  args;
 Cardinal *n_args;
{
 FileRec *file;
 int  i,tmp,width;
 char *chpt;
 FileListWidget flw=(FileListWidget)tnew;

 if (FL.files==0 || FL.n_files<0) {
	XtAppError(XtWidgetToApplicationContext(tnew),
	 	"FileList must be set upon creation");
 }
 width=0;
 for (i=0; i<FL.n_files; i++) {
 	file=FL.files[i];
	chpt=file->name;
	if ((tmp=XTextWidth(FL.font,chpt,strlen(chpt)))>width) width=tmp;
	file->icon.toggle=tnew;
	file->icon.form=(Widget)0;
	file->icon.arrow=(Widget)0;
	file->icon.label=(Widget)0;
#ifdef MAGIC_HEADERS
	if ((file->type=fileType(file->name,file->magic_type))==0) 
	  GetDefaultFileType(file);
#else
	GetDefaultFileType(file);
#endif
 }
 width+=XTextWidth(FL.font,"[]",2);
 width+=2*FL.border_width;
 FL.name_w=width;
 flw->core.border_width=0;

 FL.timer_running=False;
 if (FL.drag_timeout > 10000 || FL.drag_timeout<=0) {
   chpt=XtMalloc(128);
   sprintf(chpt,"FileListWidget: %ims seems a very unusual drag timeout to me",FL.drag_timeout);
   XtAppWarning(XtWidgetToApplicationContext(tnew),chpt);
   XtFree(chpt);
 }

 XtInsertEventHandler(tnew,
			ButtonMotionMask,False,DiscardMotion,(XtPointer)0,XtListHead);

 /* This one must go on top of the list, so entry/leave still are
  * generated if ButtonMotions are discarded
  */
 XtInsertEventHandler(tnew,
			PointerMotionMask,
			False,
			(XtEventHandler)FakeEntryLeave,
			(XtPointer)0,
			XtListHead);
 XtInsertEventHandler(tnew,
			EnterWindowMask|LeaveWindowMask,
			False,
			(XtEventHandler)CatchEntryLeave,
			(XtPointer)0,
			XtListHead);
 XtInsertEventHandler(tnew,
			ButtonPressMask,False,StartTimer,(XtPointer)0,XtListHead);
 FL.pointer_entry=-1;
 GetGCs(flw);
}

#define NF (new->fileList)
#define CF (current->fileList)
static Boolean flSetValues(Widget c, Widget r, Widget super,
	ArgList args, Cardinal *nargs)
{
Boolean rval=False,warn=False;
FileListWidget current=(FileListWidget)c;
FileListWidget new=(FileListWidget)super;
char	*buff;

if (NF.highlight_pixel   != CF.highlight_pixel ||
    NF.foreground        != CF.foreground      ||
    new->core.background_pixel != current->core.background_pixel) {
  ReleaseGCs(new);
  GetGCs(new);
  rval=True;
}

if (NF.drag_timeout!=CF.drag_timeout && (NF.drag_timeout > 10000 || NF.drag_timeout<=0)) {
   buff=XtMalloc(128);
   sprintf(buff,"FileListWidget: %ims seems a very unusual drag timeout to me",NF.drag_timeout);
   XtAppWarning(XtWidgetToApplicationContext(super),buff);
   XtFree(buff);
 }

if (NF.font!=CF.font) {NF.font=CF.font; warn=True; }
if (NF.border_width!=CF.border_width) {
 NF.border_width=CF.border_width; warn=True;
}
if (NF.files!=CF.files) {NF.files=CF.files; warn=True;}
if (NF.n_files!=CF.n_files) {NF.n_files=CF.n_files; warn=True;}

if (warn) XtAppWarning(XtWidgetToApplicationContext(super),
	    "most resources of FileList Widget cannot be changed");
return rval;
}

#undef NF
#undef CF

/* Actions */

/*ARGSUSED*/
static void Notify( Widget w, XEvent *ev, String *args, Cardinal *nargs)
{
FileListWidget flw=(FileListWidget)w;
String *alist=(String*) XtMalloc(*nargs+1);
int	i;

alist[*nargs]=0;
for (i=0; i<*nargs; i++) alist[i]=args[i];

XtCallCallbackList(w,FL.notify_cbl,(XtPointer)alist);

XtFree((char*)alist);
}

/*ARGSUSED*/
static void DispatchDirExeFilePopup( Widget w, XEvent *ev, String *args, Cardinal *nargs)
{
 DispatchDirExeFile(w,ev,args,nargs);
}

/*ARGSUSED*/
static void DispatchDirExeFile( Widget w, XEvent *ev, String *args, Cardinal *nargs)
{
FileListWidget flw=(FileListWidget)w;
int i,idx;
FileRec *file;

if (*nargs<4) {
	XtAppWarning(XtWidgetToApplicationContext(w),
		"DispatchDirExeFile() needs min. 4 arguments");
	return;
}
i=FileListFindEntry(flw,ev);
if (i==-1) idx=3;
else {
  file=FL.files[i];
  if (S_ISDIR(file->stats.st_mode)) idx=0;
  else if (file->stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) idx=1;
  else idx=2;
}
if (*args[idx]!=(char)0)
  XtCallActionProc(w,args[idx],ev,args+4,idx-4);
}


/* public procedures */
void FileListEventPosition(XEvent *ev, int *x, int *y)
{
 switch (ev->type) {
	case KeyPress: case KeyRelease:
		*x=ev->xkey.x;		*y=ev->xkey.y;    return;
	case ButtonPress: case ButtonRelease:
		*x=ev->xbutton.x;	*y=ev->xbutton.y; return;
	case MotionNotify:
		*x=ev->xmotion.x;	*y=ev->xmotion.y; return;
	case EnterNotify : case LeaveNotify:
		*x=ev->xcrossing.x;	*y=ev->xcrossing.y; return;
	default: *x=*y=-1;		return;
 }
}


int FileListFindEntry(FileListWidget flw, XEvent *ev)
{
 int x,y;
 FileListWidgetClass classptr=(FileListWidgetClass)XtClass((Widget)flw);

 if (! classptr->fileList_class.findEntry) return -1;

 FileListEventPosition(ev,&x,&y);

 if (x<0) return -1;

 return 
   classptr->fileList_class.findEntry(flw,x,y,ev);
}

void FileListEntryPosition(FileListWidget flw, 
	int entry, 
	Position *x, Position *y,
	Dimension *w, Dimension *h)
{
 FileListWidgetClass classptr=(FileListWidgetClass)XtClass((Widget)flw);

 if (! classptr->fileList_class.entryPosition || entry<0 || entry>FL.n_files) {
	*x=*y=-1; 
	return;
 }
 classptr->fileList_class.entryPosition(flw,entry,x,y,w,h);
}

void FileListRefreshItem(FileListWidget flw, int item)
{
 Position x,y;
 Dimension w,h;
 if (item<0 || item>=FL.n_files) {
   XtAppWarning(
	XtWidgetToApplicationContext((Widget)flw),
	"FileListRefreshItem(): item nr. out of range");
   return;
 }

 FileListEntryPosition(flw,item,&x,&y,&w,&h);
 XClearArea(XtDisplay((Widget)flw),XtWindow((Widget)flw),
	x,y,
	(unsigned int) w,
	(unsigned int) h,
	True);
}
 
static void CatchEntryLeave(Widget w, XtPointer cld, XEvent* ev, Boolean *ctd)
{
int x,y,entry;
FileListWidget flw=(FileListWidget)w;
FileListWidgetClass classptr=(FileListWidgetClass)XtClass((Widget)flw);

if (ev->xcrossing.send_event) return;

entry=-1;
*ctd=False; /* eat up the original event */
if (ev->type==EnterNotify) {
    x=ev->xcrossing.x;
    y=ev->xcrossing.y;
    entry=classptr->fileList_class.findEntry(flw,x,y,ev);
    if (entry!=-1) {
      FL.enter_x=x; FL.enter_y=y;
	  /* `kwm' inserts Entry/Leave events before a buttonPress;
	   * which disturbs the whole translation stuff. We just
	   * throw away enter/leave events that don't origin from
	   * our SendEvents (The semantics of Entry/Leave for FileLists
	   * is changed anyway: these events refer to entry/leaves to
	   * the items and not for the whole window anymore).
	   */
      /* *ctd=True; */
      /* don't do SendEvent, because it would go back to the end of the queue;
       * in the queue might be other events that must be handled _after_ the
       * faked leave of the file item.
       */
    }
  } else {
    if (FL.pointer_entry!=-1) {
      ev->xcrossing.x=FL.enter_x;
      ev->xcrossing.y=FL.enter_y;
	  /* for kwm behavior see above */
      /* *ctd=True; */ /* see above */
    }
  }
  FL.pointer_entry=entry;
}

/* A button was pressed, suppress propagation of motion events for 
 * some time; that way drags can be separated from clicks if the
 * user just moved the pointer a bit while clicking
 */
static void StartTimer(Widget w, XtPointer cld, XEvent* ev, Boolean *ctd)
{
 FileListWidget flw=(FileListWidget)w;
 if (FL.timer_running) XtRemoveTimeOut(FL.timeout_id);
 FL.timeout_id=XtAppAddTimeOut(
		XtWidgetToApplicationContext(w),
		(unsigned long) FL.drag_timeout,
		EnableMotion,(XtPointer)w);
 FL.timer_running=True;
}

/* If a timer is still running, don't continue to dispatch
 * ButtonMotion events.
 */
static void DiscardMotion(Widget w, XtPointer cld, XEvent* ev, Boolean *ctd)
{
FileListWidget flw=(FileListWidget)w;
if (FL.timer_running) *ctd=False;
}

static void FakeEntryLeave(Widget w, XtPointer cld, XEvent* ev, Boolean *ctd)
{
int x,y,entry;
FileListWidget flw=(FileListWidget)w;
FileListWidgetClass classptr=(FileListWidgetClass)XtClass((Widget)flw);
static XCrossingEvent elev={
	(int) 0,				/* type */
	(unsigned long)0,			/* req # */
	True,					/* send event */
	(Display*)0,				/* Display */
	(Window)0,				/* Window */
	(Window)0,				/* root window */
	None,					/* child window */
	CurrentTime,				/* time */
	(int)-1,				/* x */
	(int)-1,				/* y */
	(int)-1,				/* root x */
	(int)-1,				/* root y */
	NotifyNormal,				/* mode (grab) */
	NotifyAncestor,				/* detail (?) */
	(Boolean)-1,				/* same screen */
	False,					/* focus */
	(unsigned int) 0			/* Modifier / button state */
};

x=ev->xmotion.x;
y=ev->xmotion.y;

entry=classptr->fileList_class.findEntry(flw,x,y,ev);

if (entry==FL.pointer_entry) return;

elev.display=	ev->xmotion.display;
elev.window= 	ev->xmotion.window;
elev.root=   	ev->xmotion.root;
elev.time=	ev->xmotion.time;
elev.x_root=	ev->xmotion.x_root;
elev.y_root=	ev->xmotion.y_root;
elev.same_screen=ev->xmotion.same_screen;
elev.state=	ev->xmotion.state;

if (FL.pointer_entry!=-1) {
  elev.x=		FL.enter_x;
  elev.y=		FL.enter_y;
  elev.type=LeaveNotify;
  XSendEvent(XtDisplay(w),XtWindow(w),False,LeaveWindowMask,(XEvent*)&elev);
}

if ((FL.pointer_entry=entry)!=-1) {
  elev.x=FL.enter_x=x;		/* save position of entry, so when the correspoinding
				 * leave event will be faked, we remember a position
				 * inside the list item.
				 */
  elev.y=FL.enter_y=y;
  elev.type=EnterNotify;
  XSendEvent(XtDisplay(w),XtWindow(w),False,EnterWindowMask,(XEvent*)&elev);
}

}

/* Timer callback */

/* Some time has expired since the last Button was pressed;
 * we guess that the user really intends to drag something
 * if he/she moves the pointer now.
 */ 
static void EnableMotion(XtPointer cld, XtIntervalId *id)
{
FileListWidget flw=(FileListWidget)cld;
FL.timer_running=False; /*Timer just expired*/
}


/* private procedures */
static void ReleaseGCs(FileListWidget flw)
{
 Widget w=(Widget)flw;
 XtReleaseGC(w,FL.gc_highlight);
 XtReleaseGC(w,FL.gc_norm);
 XtReleaseGC(w,FL.gc_invert);
}

static void GetGCs(FileListWidget flw)
{
 XGCValues vals;
 XtGCMask  mask=GCForeground|GCBackground|GCFont|GCGraphicsExposures|GCLineWidth;

 vals.foreground=FL.highlight_pixel;
 vals.background=flw->core.background_pixel;
 vals.font=FL.font->fid;
 vals.line_width=FL.border_width;
 vals.graphics_exposures=False;

 FL.gc_highlight=XtGetGC((Widget)flw,mask,&vals);

 vals.foreground=FL.foreground;
 FL.gc_norm=XtGetGC((Widget)flw,mask,&vals);

 vals.foreground=vals.background;
 vals.background=FL.highlight_pixel;
 FL.gc_invert=XtGetGC((Widget)flw,mask,&vals);
}

static void GetDefaultFileType(FileRec *file)
{
   if (S_ISLNK(file->stats.st_mode)) {
	file->type=&builtin_types[BLACKHOLE_T];
	return;
   } else if (S_ISDIR(file->stats.st_mode)) {
	if (file->sym_link) {
	    file->type=&builtin_types[DIRLNK_T];
	    return;
	} else if (!strcmp(file->name, "..")) {
	    file->type=&builtin_types[UPDIR_T];
	    return;
	} else {
	    file->type=&builtin_types[DIR_T];
	    return;
	}
   } else if (file->stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
	if (file->sym_link) {
	  file->type=&builtin_types[EXECLNK_T];
	  return;
	} else {
	  file->type=&builtin_types[EXEC_T];
	  return;
	}
    } else {
#ifndef MAGIC_HEADERS
	file->type=fileType(file->name);
	if (file->type) return;
#endif
	if (file->sym_link) {
	  file->type=&builtin_types[SYMLNK_T];
	  return;
	} else {
	  file->type=&builtin_types[FILE_T];
	  return;
	}
    }
/* should never get here */
}
