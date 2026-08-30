/*---------------------------------------------------------------------------
  Module FmChmod

  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994

  modified 7-1997 by strauman@sun6hft.ee.tu-berlin.de to add
  different enhancements (see README-NEW).

  Functions & data for handling the chmod feature
---------------------------------------------------------------------------*/

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Box.h>

#include "Am.h"
#include "Fm.h"

#define FORM_WIDTH 96

#define OWNER 0
#define GROUP 1
#define OTHERS 2

#define READ 0
#define WRITE 1
#define EXECUTE 2

/*---------------------------------------------------------------------------
  STATIC DATA
---------------------------------------------------------------------------*/

typedef struct {
  Widget w;
  int value;
} ChmodItem;

typedef struct {
  Widget shell;
  Widget label;
  FileWindowRec *fw;
  int file;
  ChmodItem items[3][3];
} ChmodData;

static ChmodData chmode;

/*---------------------------------------------------------------------------
  Widget Argument lists
---------------------------------------------------------------------------*/

static Arg shell_args[] = {
  { XtNtitle, (XtArgVal) "Change Permissions" }
};

static Arg label_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNlabel, (XtArgVal) NULL },
  { XtNwidth, (XtArgVal) 0 },
  { XtNfont, (XtArgVal) NULL },
  { XtNjustify, XtJustifyLeft },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainRight }
};

static Arg tickbox_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNbitmap, (XtArgVal) None },
  { XtNwidth, (XtArgVal) 0 },
  { XtNresize, (XtArgVal) False },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainRight }
};

static Arg *form_args = NULL;

static Arg form2_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNwidth, (XtArgVal) FORM_WIDTH },
  { XtNdefaultDistance, (XtArgVal) 0 },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainLeft }
};

static Arg button_box_args[] = {
  { XtNfromHoriz, (XtArgVal) NULL },
  { XtNfromVert, (XtArgVal) NULL },
  { XtNtop, XtChainTop },
  { XtNbottom, XtChainTop },
  { XtNleft, XtChainLeft },
  { XtNright, XtChainLeft }
};

/*---------------------------------------------------------------------------
  Strings to display in labels
---------------------------------------------------------------------------*/

static String big_labels[] = { "Owner", "Group", "Others" };

static String small_labels[] = { "r", "w", "x" };


/*--------------------------------------------------------------------------
  PRIVATE FUNCTIONS
---------------------------------------------------------------------------*/

static FmCallbackProc chmodRestoreCb, chmodOkCb, chmodCancelCb;

static void setupTicks()
{
  register int i,j;
  struct stat *stats;
  Pixmap picts=0;

  stats = &chmode.fw->files[chmode.file]->stats;

  chmode.items[OWNER][READ].value     = (stats->st_mode) & S_IRUSR;
  chmode.items[OWNER][WRITE].value    = (stats->st_mode) & S_IWUSR;
  chmode.items[OWNER][EXECUTE].value  = (stats->st_mode) & S_IXUSR;
#ifdef ENHANCE_PERMS
  chmode.items[OWNER][EXECUTE].value  |= (stats->st_mode) & S_ISUID;
#endif

  chmode.items[GROUP][READ].value     = (stats->st_mode) & S_IRGRP;
  chmode.items[GROUP][WRITE].value    = (stats->st_mode) & S_IWGRP;
  chmode.items[GROUP][EXECUTE].value  = (stats->st_mode) & S_IXGRP;
#ifdef ENHANCE_PERMS
  chmode.items[GROUP][EXECUTE].value  |= (stats->st_mode) & S_ISGID;
#endif

  chmode.items[OTHERS][READ].value    = (stats->st_mode) & S_IROTH;
  chmode.items[OTHERS][WRITE].value   = (stats->st_mode) & S_IWOTH;
  chmode.items[OTHERS][EXECUTE].value = (stats->st_mode) & S_IXOTH;
#ifdef ENHANCE_PERMS
  chmode.items[OTHERS][EXECUTE].value  |= (stats->st_mode) & S_ISVTX;
  for (i=0; i<3; i++) {
    for (j=0; j<2; j++) {
      XtVaSetValues(chmode.items[i][j].w, XtNbitmap, 
		    chmode.items[i][j].value ? bm[TICK_BM] : bm[NOTICK_BM],
		    NULL);
    }
  }
  j=(chmode.items[0][2].value & S_IXUSR ? 1:0);
  j+=(chmode.items[0][2].value & S_ISUID ? 2:0);
  switch(j) { case 0: picts=bm[NOTICK_BM]; break; case 1: picts=bm[TICK_BM]; break;
	      case 2: picts=bm[SUID_BM]; break; case 3: picts=bm[sUID_BM]; break;
  }
  chmode.items[0][2].value=j;
  XtVaSetValues(chmode.items[0][2].w, XtNbitmap, picts, 0);

  j=(chmode.items[1][2].value & S_IXGRP ? 1:0);
  j+=(chmode.items[1][2].value & S_ISGID ? 2:0);
  switch(j) { case 0: picts=bm[NOTICK_BM]; break; case 1: picts=bm[TICK_BM]; break;
	      case 2: picts=bm[SUID_BM]; break; case 3: picts=bm[sUID_BM]; break;
  }
  chmode.items[1][2].value=j;
  XtVaSetValues(chmode.items[1][2].w, XtNbitmap, picts, 0);

  j=(chmode.items[2][2].value & S_IXOTH ? 1:0);
  j+=(chmode.items[2][2].value & S_ISVTX ? 2:0);
  switch(j) { case 0: picts=bm[NOTICK_BM]; break; case 1: picts=bm[TICK_BM]; break;
	      case 2: picts=bm[STICKY_BM]; break; case 3: picts=bm[StICKY_BM]; break;
  }
  chmode.items[2][2].value=j;
  XtVaSetValues(chmode.items[2][2].w, XtNbitmap, picts, 0);
#else
  for (i=0; i<3; i++) {
    for (j=0; j<3; j++) {
      XtVaSetValues(chmode.items[i][j].w, XtNbitmap, 
		    chmode.items[i][j].value ? bm[TICK_BM] : bm[NOTICK_BM],
		    NULL);
    }
  }
#endif
}

/*---------------------------------------------------------------------------*/
  
static void chmodRestoreCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  setupTicks();
}

/*---------------------------------------------------------------------------*/

static void tickBoxCb(Widget w, XtPointer client_data, XtPointer call_data)
{
  register int i,j;
#ifdef ENHANCE_PERMS
  register int	 k;
  Pixmap picts=0;
#endif

  i = (int) client_data;

  for (j=0; j<3; j++)
    if (w == chmode.items[i][j].w)
      break;
#ifdef ENHANCE_PERMS
  if (j<2) {
#endif
  if (chmode.items[i][j].value)
    chmode.items[i][j].value = False;
  else
    chmode.items[i][j].value = True;

  XtVaSetValues(chmode.items[i][j].w, XtNbitmap, 
	       chmode.items[i][j].value ? bm[TICK_BM] : bm[NOTICK_BM], NULL);
#ifdef ENHANCE_PERMS
  } else {
  k=chmode.items[i][j].value=(chmode.items[i][j].value+1)%4;
  if (i<2) 
  switch(k) { case 0: picts=bm[NOTICK_BM]; break; case 1: picts=bm[TICK_BM]; break;
	      case 2: picts=bm[SUID_BM]; break; case 3: picts=bm[sUID_BM]; break;
  }
  else
  switch(k) { case 0: picts=bm[NOTICK_BM]; break; case 1: picts=bm[TICK_BM]; break;
	      case 2: picts=bm[STICKY_BM]; break; case 3: picts=bm[StICKY_BM]; break;
  }
  XtVaSetValues(chmode.items[i][j].w, XtNbitmap, picts, 0);
  }
#endif
}

/*---------------------------------------------------------------------------*/

static void chmodOkCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  mode_t mode;

  XtPopdown(chmode.shell);
  mode = chmode.fw->files[chmode.file]->stats.st_mode;
  mode &= ~(S_IRUSR | S_IWUSR | S_IXUSR |
            S_IRGRP | S_IWGRP | S_IXGRP |
#ifdef ENHANCE_PERMS
	    S_ISUID  | S_ISGID  | S_ISVTX  |
#endif
            S_IROTH | S_IWOTH | S_IXOTH);

  mode |= chmode.items[OWNER][READ].value     ? S_IRUSR : 0;
  mode |= chmode.items[OWNER][WRITE].value    ? S_IWUSR : 0;
#ifdef ENHANCE_PERMS
  mode |= chmode.items[OWNER][EXECUTE].value & 1 ? S_IXUSR : 0;
  mode |= chmode.items[OWNER][EXECUTE].value & 2 ? S_ISUID : 0;
#else
  mode |= chmode.items[OWNER][EXECUTE].value  ? S_IXUSR : 0;
#endif

  mode |= chmode.items[GROUP][READ].value     ? S_IRGRP : 0;
  mode |= chmode.items[GROUP][WRITE].value    ? S_IWGRP : 0;
#ifdef ENHANCE_PERMS
  mode |= chmode.items[GROUP][EXECUTE].value & 1 ? S_IXGRP : 0;
  mode |= chmode.items[GROUP][EXECUTE].value & 2 ? S_ISGID : 0;
#else
  mode |= chmode.items[GROUP][EXECUTE].value  ? S_IXGRP : 0;
#endif

  mode |= chmode.items[OTHERS][READ].value    ? S_IROTH : 0;
  mode |= chmode.items[OTHERS][WRITE].value   ? S_IWOTH : 0;
#ifdef ENHANCE_PERMS
  mode |= chmode.items[OTHERS][EXECUTE].value & 1 ? S_IXOTH : 0;
  mode |= chmode.items[OTHERS][EXECUTE].value & 2 ? S_ISVTX : 0;
#else
  mode |= chmode.items[OTHERS][EXECUTE].value ? S_IXOTH : 0;
#endif

  if (chdir(chmode.fw->directory)) {
    sysError("System error:");
    goto out;
  }

  if (chmod(chmode.fw->files[chmode.file]->name, mode)) {
    char s[0xff];
    sprintf(s, "Can't change modes for %s:", 
	    chmode.fw->files[chmode.file]->name);
    sysError(s);
  }
  else {
    markForUpdate(chmode.fw->directory);
    intUpdate();
  }

 out:
  freeze = False;
}

/*---------------------------------------------------------------------------*/

static void chmodCancelCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  XtPopdown(chmode.shell);
  freeze = False;
}		   

/*---------------------------------------------------------------------------
  Button Information
---------------------------------------------------------------------------*/

static ButtonRec chmod_buttons[] = {
  { "ok", "Ok", chmodOkCb },
  { "restore", "Restore", chmodRestoreCb },
  { "cancel", "Cancel", chmodCancelCb }
};

/*---------------------------------------------------------------------------
  PUBLIC FUNCTIONS
---------------------------------------------------------------------------*/

void createChmodPopup()
{
  Widget form, form2, blabel, w;
  register int i,j;

  /* create shell */
  chmode.shell = XtCreatePopupShell("chmod", transientShellWidgetClass,
				   aw.shell, shell_args, XtNumber(shell_args));
  /* create outer form */
  form = XtCreateManagedWidget("form", formWidgetClass, chmode.shell,
			       form_args, XtNumber(form_args) );

  /* create two labels for message */
  label_args[0].value = (XtArgVal) NULL;
  label_args[1].value = (XtArgVal) NULL;
  label_args[3].value = (XtArgVal) FORM_WIDTH*3 + 30;
  label_args[4].value = (XtArgVal) resources.label_font;
  chmode.label = XtCreateManagedWidget("label1", labelWidgetClass, form, 
				       label_args, XtNumber(label_args) );

  form2_args[1].value = (XtArgVal) chmode.label;
  label_args[5].value = (XtArgVal) XtJustifyCenter;

  form2 = NULL;
  /* create smaller forms */
  for (i=0; i<3; i++) {
    form2_args[0].value = (XtArgVal) form2;
    form2 = XtCreateManagedWidget(big_labels[i], formWidgetClass, form,
				  form2_args, XtNumber(form2_args) );

    label_args[0].value = label_args[1].value = (XtArgVal) NULL;
    label_args[2].value = (XtArgVal) big_labels[i];
    label_args[3].value = (XtArgVal) FORM_WIDTH;
    blabel = XtCreateManagedWidget("label", labelWidgetClass, form2,
				   label_args, XtNumber(label_args) );


    w = NULL;
    for (j=0; j<3; j++) {
      label_args[0].value = tickbox_args[0].value = (XtArgVal) w;
      label_args[1].value = (XtArgVal) blabel;
      label_args[2].value = (XtArgVal) small_labels[j];
      label_args[3].value = (XtArgVal) FORM_WIDTH/3;
      w = XtCreateManagedWidget(small_labels[j], labelWidgetClass, form2,
				     label_args, XtNumber(label_args) );

      tickbox_args[1].value = (XtArgVal) w;
      tickbox_args[2].value = (XtArgVal) NULL;
      tickbox_args[3].value = (XtArgVal) FORM_WIDTH/3;
      w = XtCreateManagedWidget(small_labels[j], commandWidgetClass,
				form2, tickbox_args, XtNumber(tickbox_args) );
      XtAddCallback(w, XtNcallback, (XtCallbackProc) tickBoxCb, (XtPointer) i);
      chmode.items[i][j].w = w;
    }
  }
  
  /* create button box & buttons */
  button_box_args[1].value = (XtArgVal) form2;
  w = XtCreateManagedWidget("button box", boxWidgetClass, form, 
			    button_box_args, XtNumber(button_box_args) );
  createButtons(chmod_buttons, XtNumber(chmod_buttons), w, NULL);

#ifdef ENHANCE_POP_ACCEL
  XtInstallAllAccelerators(form,form);
  XtInstallAllAccelerators(chmode.shell,form);
#endif

  XtRealizeWidget(chmode.shell);
  setWMProps(chmode.shell);
}

/*---------------------------------------------------------------------------*/

void chmodPopup(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  char message[MAXPATHLEN];
  register int i;

  if (fw == NULL) fw = popup_fw;

  if (!fw->n_selections) return;

  chmode.fw = fw;

  for (i=0;; i++)
    if (fw->files[i]->selected) {
      chmode.file = i;
      break;
    }
  
  strcpy(message, "Changing access permissions for ");
  strcat(message, chmode.fw->files[chmode.file]->name);
  XtVaSetValues(chmode.label, XtNlabel, (XtArgVal) message, NULL);

  setupTicks();

  freeze = True;
  popupByCursor(chmode.shell, XtGrabExclusive);
}







