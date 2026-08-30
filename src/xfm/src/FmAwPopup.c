/*---------------------------------------------------------------------------
 Module FmAwPopup

 (c) Simon Marlow 1990-92
 (c) Albert Graef 1994

 Functions & data for creating the popup 'install application' window
---------------------------------------------------------------------------*/

#include <string.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xmu/Drawing.h>

#include "Am.h"

/*---------------------------------------------------------------------------
  STATIC DATA
---------------------------------------------------------------------------*/

static Widget install_app_popup, install_group_popup;

static int app_number;

static char app_name[MAXAPPSTRINGLEN], app_directory[MAXAPPSTRINGLEN],
  app_fname[MAXAPPSTRINGLEN], app_icon[MAXAPPSTRINGLEN],
  app_push_action[MAXAPPSTRINGLEN], app_drop_action[MAXAPPSTRINGLEN];

/*---------------------------------------------------------------------------
  PRIVATE FUNCTIONS
---------------------------------------------------------------------------*/

static FmCallbackProc installAppOkCb, installAppCancelCb,
installGroupOkCb, installGroupCancelCb;

static void installAppOkCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  XtPopdown(install_app_popup);

  if (app_number != -1)
    replaceApplication(aw.apps+app_number, app_name, app_directory, app_fname,
		       app_icon, app_push_action, app_drop_action);
  else
    installApplication(app_name, app_directory, app_fname, app_icon,
		       app_push_action, app_drop_action);

  updateApplicationDisplay();
  writeApplicationData(resources.app_file);
}

/*---------------------------------------------------------------------------*/

static void installAppCancelCb(Widget w, FileWindowRec *fw, 
			       XtPointer call_data)
{
  XtPopdown(install_app_popup);
}

/*---------------------------------------------------------------------------*/

static void installGroupOkCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  XtPopdown(install_group_popup);

  installApplication(app_name, resources.app_dir, app_fname, app_icon,
		     "LOAD", "");

  updateApplicationDisplay();
  writeApplicationData(resources.app_file);
}

/*---------------------------------------------------------------------------*/

static void installGroupCancelCb(Widget w, FileWindowRec *fw, 
			       XtPointer call_data)
{
  XtPopdown(install_group_popup);
}

/*---------------------------------------------------------------------------
  Question and button data
---------------------------------------------------------------------------*/

static QuestionRec install_app_questions[] = {
  { "Name:", app_name, MAXAPPSTRINGLEN, NULL },
  { "Directory:", app_directory, MAXAPPSTRINGLEN, NULL },
  { "File name:", app_fname, MAXAPPSTRINGLEN, NULL },
  { "Icon:", app_icon, MAXAPPSTRINGLEN, NULL },
  { "Push action:", app_push_action, MAXAPPSTRINGLEN, NULL },
  { "Drop action:", app_drop_action, MAXAPPSTRINGLEN, NULL }
};

static ButtonRec install_app_buttons[] = {
  { "install", "Install", installAppOkCb },
  { "cancel", "Cancel", installAppCancelCb }
};

static QuestionRec install_group_questions[] = {
  { "Name:", app_name, MAXAPPSTRINGLEN, NULL },
  { "File name:", app_fname, MAXAPPSTRINGLEN, NULL },
  { "Icon:", app_icon, MAXAPPSTRINGLEN, NULL }
};

static ButtonRec install_group_buttons[] = {
  { "install", "Install", installGroupOkCb },
  { "cancel", "Cancel", installGroupCancelCb }
};

/*---------------------------------------------------------------------------
  PUBLIC FUNCTIONS
---------------------------------------------------------------------------*/

void createInstallPopups()
{
  install_app_popup =
    createPopupQuestions("install app",
			 "Install Application",
			 None,
			 install_app_questions,
			 XtNumber(install_app_questions),
			 install_app_buttons,
			 XtNumber(install_app_buttons),
			 XtNumber(install_app_buttons)-1);
  install_group_popup =
    createPopupQuestions("install group",
			 "Install Group",
			 None,
			 install_group_questions,
			 XtNumber(install_group_questions),
			 install_group_buttons,
			 XtNumber(install_group_buttons),
			 XtNumber(install_group_buttons)-1);
}

/*----------------------------------------------------------------------------*/

void installNewAppPopup()
{
  register int i;

  for (i=0; i < XtNumber(install_app_questions); i++) {
    install_app_questions[i].value[0] = '\0';
#ifdef ENHANCE_TXT_FIELD
    XtVaSetValues(install_app_questions[i].widget, 
		  XtNinsertPosition,0,XtNstring, 
#else
    XtVaSetValues(install_app_questions[i].widget, XtNstring, 
#endif
		  install_app_questions[i].value, NULL);
  }

  app_number = -1;

  popupByCursor(install_app_popup, XtGrabExclusive);
}

/*----------------------------------------------------------------------------*/

void installExistingAppPopup()
{
  register int i;

  for (i=0; i<aw.n_apps; i++)
    if (aw.apps[i].selected) {
      app_number = i;
      strcpy(install_app_questions[0].value, aw.apps[i].name);
      strcpy(install_app_questions[1].value, aw.apps[i].directory);
      strcpy(install_app_questions[2].value, aw.apps[i].fname);
      strcpy(install_app_questions[3].value, aw.apps[i].icon);
      strcpy(install_app_questions[4].value, aw.apps[i].push_action);
      strcpy(install_app_questions[5].value, aw.apps[i].drop_action);
      break;
    }

  for (i=0; i < XtNumber(install_app_questions); i++) {
#ifdef ENHANCE_TXT_FIELD
    int len,txtpos;
    XtVaGetValues(install_app_questions[i].widget,
		  XtNinsertPosition,&txtpos,NULL);
    if (txtpos>(len=strlen(install_app_questions[i].value))) txtpos=len;
    XtVaSetValues(install_app_questions[i].widget,
		  XtNstring,install_app_questions[i].value,
		  XtNinsertPosition,txtpos,
		  NULL);
#else
    XtVaSetValues(install_app_questions[i].widget, XtNstring, 
		  install_app_questions[i].value, NULL);
#endif
  }

  popupByCursor(install_app_popup, XtGrabExclusive);
}

/*----------------------------------------------------------------------------*/

void installGroupPopup()
{
  register int i;

  for (i=0; i < XtNumber(install_group_questions); i++) {
    install_group_questions[i].value[0] = '\0';
#ifdef ENHANCE_TXT_FIELD
    XtVaSetValues(install_group_questions[i].widget, 
		  XtNinsertPosition,0,XtNstring, 
#else
    XtVaSetValues(install_group_questions[i].widget, XtNstring, 
#endif
		  install_group_questions[i].value, NULL);
  }

  popupByCursor(install_group_popup, XtGrabExclusive);
}
