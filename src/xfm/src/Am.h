/*-----------------------------------------------------------------------------
  Am.h
  
  (c) Simon Marlow 1990-1993
  (c) Albert Graef 1994
-----------------------------------------------------------------------------*/

#ifndef AM_H
#define AM_H

#include "Fm.h"
#include <sys/param.h>

/*--FmAw---------------------------------------------------------------------*/

#define MAXAPPSTRINGLEN MAXPATHLEN

typedef struct {
	char *name;
	char *directory;
	char *fname;
	char *icon;
	char *push_action;
	char *drop_action;
	Pixmap icon_bm;
	Boolean loaded;
	Widget form, toggle, label;
	Boolean selected;
} AppRec, *AppList;

typedef struct {
	Widget shell, form, viewport, button_box, icon_box;
	AppList apps;
	int n_apps;
	int n_selections;
} AppWindowRec;
 
extern AppWindowRec aw;
extern Widget app_popup_widget, *app_popup_items, app_popup_widget1;

extern int n_appst;
extern char **appst;

Pixmap defaultIcon(char *name, char *directory, char *fname);
int parseApp(FILE *fp, char **name, char **directory, char **fname,
	     char **icon, char **push_action, char **drop_action);
void createApplicationWindow();
void createApplicationDisplay();
void updateApplicationDisplay();
void setApplicationWindowName();
void readApplicationData(String path);
int writeApplicationData(String path);
void installApplication(char *name, char *directory, char *fname, char *icon,
			char *push_action, char *drop_action);
void replaceApplication(AppRec *app, char *name, char *directory, char *fname,
			char *icon, char *push_action, char *drop_action);
void freeApplicationResources(AppRec *app);
void pushApplicationsFile();
void popApplicationsFile();
void clearApplicationsStack();

/*--FmAwCb-------------------------------------------------------------------*/

FmCallbackProc 
  appInstallAppCb, appInstallGroupCb, appEditCb, appCutCb, appCopyCb,
  appPasteCb, appRemoveCb, appSelectAllCb, appDeselectCb, appLoadCb,
  appMainCb, appBackCb, appOpenCb, appCloseCb, aboutCb;

/*--FmAwActions--------------------------------------------------------------*/

int  findAppWidget(Widget w);

FmActionProc appPopup, appMaybeHighlight, runApp, appSelect, appToggle;

void appBeginDrag(Widget w, XEvent *event, String *params, 
		  Cardinal *num_params);

void appEndMove(int i);
void appEndMoveInBox(void);
void appEndCopy(int i);
void appEndCopyInBox(void);

void appEndDrag(int i);
void appEndDragInBox(void);

/*--FmAwPopup----------------------------------------------------------------*/

void installNewAppPopup();
void installExistingAppPopup();
void installGroupPopup();
void createInstallPopups();

#endif
