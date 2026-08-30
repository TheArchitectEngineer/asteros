#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include <X11/Xaw/Form.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/AsciiText.h>

#include "Fm.h"
#include "FmLog.h"

typedef struct LogData_ {
	Widget shell,text,auto_flag;
  	XtInputId inp_id;
} LogData;
	

Widget		fmLogPopupShell=0;
FILE	*orig_stderr;
static FILE	*orig_stdout;
static int	peipd[2];

static XawTextPosition pos=(XawTextPosition)0;

static void get_pipe_input(XtPointer cld,int *fid, XtInputId *id)
{
 XawTextBlock           block;
 int  			nbytes;
 char 			buf[BUFSIZ];
 Boolean		auto_popup;
 LogData		*ldp=(LogData*)cld;

 if ((nbytes=read(*fid,buf,BUFSIZ))==-1)
	fprintf(orig_stderr,"\nread error %i in 'get_pipe_input()'\n",errno);

 if (nbytes>0) {
   XtVaSetValues(ldp->text,XtNeditType,XawtextEdit,0);
   block.format=FMT8BIT;
   block.length=nbytes;
   block.ptr=buf;
   block.firstPos=0;
   buf[nbytes]=0;

   XawTextReplace(ldp->text,pos,pos,&block);
   pos+=block.length;

   /* Setting the insertPosition scrolls the text to the bottom */
   XtVaSetValues(ldp->text,
		XtNeditType,XawtextRead,
		XtNinsertPosition,pos,
		0);

   XtVaGetValues(ldp->auto_flag,XtNstate,&auto_popup,0);
   if (auto_popup)
     popupByCursor(ldp->shell,XtGrabNone);
 }

}

static void update_tick_cb(Widget w, XtPointer cld, XtPointer cad)
{
Boolean flag;
Pixel   bg=(Pixel)cld,fg;
XtVaGetValues(w,XtNstate,&flag,XtNforeground,&fg,0);
XtVaSetValues(w,XtNbackground,(flag?bg:fg),0);
}

static void hide_log_cb(Widget w, XtPointer cld, XtPointer cad)
{
 Widget shell=(Widget)cld;
 XtPopdown(shell);
}
 
static void clear_log_cb(Widget w, XtPointer cld, XtPointer cad)
{
 Widget text=(Widget)cld;
 XawTextBlock           block;

 XtVaSetValues(text,XtNeditType,XawtextEdit,0);
 block.firstPos=0; block.length=0; block.ptr=0;
 block.format=FMT8BIT;
 XawTextReplace(text,0,pos,&block);
 XtVaSetValues(text,XtNeditType,XawtextRead,0);
 pos=0;
}

static void destroy_log_cb(Widget w, XtPointer cld, XtPointer cad)
{
LogData *ldp=(LogData*)cld;

XtRemoveInput(ldp->inp_id);
/* try to restore stdout/stderr, hope the buffering doesn't
 * get mixed up :-0
 */
fflush(stdout); fflush(stderr);
close(STDOUT_FILENO); dup(fileno(orig_stdout)); fclose(orig_stdout);
close(STDERR_FILENO); dup(fileno(orig_stderr)); fclose(orig_stderr);
close(peipd[0]);

fmLogPopupShell=0;
XtFree((char*)ldp);
}

#define FROM_HORIZ	0
#define FONT		1
#define BITMAP		6
#define N_BUTTON_ARGS   BITMAP

static Arg button_args[]={
	{XtNfromHoriz,0},
	{XtNfont,0},
	{XtNleft,XtChainLeft},
	{XtNright,XtChainLeft},
	{XtNtop,XtChainTop},
	{XtNbottom,XtChainTop},
	{XtNbitmap,0},
	{XtNinternalHeight,0},
	{XtNinternalWidth,0},
};

Widget FmCreateLog(Widget parent,XFontStruct *font)
{
  Widget form,cmd;
  XtAppContext app=XtWidgetToApplicationContext(parent);
  LogData	*ldp=(LogData*)XtMalloc(sizeof(LogData));
  Pixel		fg,bg;
  Window	win;
  Boolean	flag;
  Dimension	vdist,h1,h2,bw;

  if (fmLogPopupShell) {
	XtAppError(app,
	  "FmCreateLog: only one log allowed");
  }

  ldp->shell=XtVaCreatePopupShell(
	  "fmLog", transientShellWidgetClass, parent,
	  NULL);

  form=XtVaCreateManagedWidget(
	  "form", formWidgetClass, ldp->shell, 
	  NULL);

  XtVaGetValues(form,XtNdefaultDistance,&vdist,0);


  button_args[FONT].value=(XtArgVal)font;
  button_args[BITMAP].value=(XtArgVal)bm[TICK_BM];

  cmd =XtCreateManagedWidget(
	  "Hide", commandWidgetClass, form,
	  button_args,N_BUTTON_ARGS);
  XtAddCallback(cmd,XtNcallback,hide_log_cb,(XtPointer)ldp->shell);

  ldp->text=XtVaCreateManagedWidget(
	  "log_text", asciiTextWidgetClass, form,
	  XtNscrollVertical,  XawtextScrollWhenNeeded,
	  XtNdisplayCaret,False,
	  XtNfromVert,cmd,
	  XtNleft,XtChainLeft, XtNright, XtChainRight,
	  XtNtop, XtChainTop,  XtNbottom,XtChainBottom,
	  NULL);

/* this is another hack: realize now, so the width
 * of the text is calculated better. We need the
 * shell's window anyway just down a few lines...
 */

  XtRealizeWidget(ldp->shell);

  win=XtWindow(ldp->shell);

  button_args[FROM_HORIZ].value=(XtArgVal)cmd;
  cmd =XtCreateManagedWidget(
	  "Clear Log", commandWidgetClass, form,
	  button_args,N_BUTTON_ARGS);
  XtAddCallback(cmd,XtNcallback,clear_log_cb,(XtPointer)ldp->text);

  XtVaGetValues(cmd,XtNheight,&h1, XtNborderWidth,&bw, 0);

  button_args[FROM_HORIZ].value=(XtArgVal)cmd;
  cmd =XtVaCreateManagedWidget("Auto Popup", labelWidgetClass, form,
	  XtNborderWidth,0,
	  XtNresize,False,
	  XtNfromHoriz,cmd,
	  XtNleft,XtChainLeft,
	  XtNright,XtChainLeft,
	  XtNtop,XtChainTop,
	  XtNbottom,XtChainTop,
	  XtNfont,font,
	  XtNheight,h1+2*bw,
	  0);

  button_args[FROM_HORIZ].value=(XtArgVal)cmd;
  ldp->auto_flag =XtCreateManagedWidget(
	  "Auto Flag", toggleWidgetClass, form,
	  button_args,XtNumber(button_args));

  /* get what the user thinks are the colours and flip them */
  XtVaGetValues(ldp->auto_flag,
	XtNforeground,&fg,XtNbackground,&bg,XtNstate,&flag,
	XtNheight,&h2,
	0);

  vdist+=(h1-h2)/2;

  XtVaSetValues(ldp->auto_flag,
	XtNvertDistance,vdist,
	XtNforeground,bg,
	XtNbackground,(flag?fg:bg),
	0);

  XtAddCallback(ldp->auto_flag,XtNcallback,update_tick_cb,(XtPointer)fg);
	
  pipe(peipd);
  orig_stderr=fdopen(dup(STDERR_FILENO),"w");
  orig_stdout=fdopen(dup(STDOUT_FILENO),"w");
  /* close old stderr, stdout */
  close(STDOUT_FILENO); close(STDERR_FILENO);
  /* redirect stdout, stderr to the pipe */
  dup(peipd[1]); dup(peipd[1]);
  close(peipd[1]);
  ldp->inp_id=XtAppAddInput(
	app,
	peipd[0],
	(XtPointer)XtInputReadMask,
	get_pipe_input,(XtPointer)ldp);

  XtAddCallback(ldp->shell,XtNdestroyCallback,destroy_log_cb,(XtPointer)ldp);

  fmLogPopupShell=ldp->shell;

  setWMProps(ldp->shell);
#ifdef ENHANCE_POP_ACCEL
  XtInstallAllAccelerators(ldp->shell,form);
  XtInstallAllAccelerators(form,form);
  XtInstallAllAccelerators(ldp->text,form);
#endif

  return ldp->shell;
}

void logPopup(Widget w, FileWindowRec *fw, XtPointer cad)
{
if (fmLogPopupShell) 
 popupByCursor(fmLogPopupShell,XtGrabNone);
}
