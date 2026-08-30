/*---------------------------------------------------------------------------
  Module FmAwCb

  (c) S.Marlow 1990-92
  (c) A.Graef 1994

  Callback routines for widgets in the application window
---------------------------------------------------------------------------*/

#include <stdio.h>
#include <memory.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include "Am.h"

/*---------------------------------------------------------------------------
  PUBLIC FUNCTIONS
---------------------------------------------------------------------------*/

void appInstallAppCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  installNewAppPopup();
}

/*---------------------------------------------------------------------------*/

void appInstallGroupCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  installGroupPopup();
}

/*---------------------------------------------------------------------------*/

void appEditCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  installExistingAppPopup();
}

/*---------------------------------------------------------------------------*/

void appCutCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  char s[0xff];
  FILE *fp;
  int i, j;
  char name[2*MAXAPPSTRINGLEN], directory[2*MAXAPPSTRINGLEN],
    fname[2*MAXAPPSTRINGLEN], icon[2*MAXAPPSTRINGLEN],
    push_action[2*MAXAPPSTRINGLEN], drop_action[2*MAXAPPSTRINGLEN];

  if (resources.confirm_moves) {
    sprintf(s, "Cutting %d item%s from", aw.n_selections,
	    aw.n_selections > 1 ? "s" : "" );
    if (!confirm(s, "the application window", ""))
      return;
  }

  if (! (fp = fopen(resources.app_clip, "w") )) {
    sysError("Error writing clip file:");
    return;
  }

  fprintf(fp, "#XFM\n");

  for (i=j=0; j<aw.n_apps; j++)
    if (!aw.apps[j].selected) {
      if (i != j)
	memcpy(&aw.apps[i], &aw.apps[j], sizeof(AppRec));
      i++;
    } else {
      expand(name, aw.apps[j].name, "\\:");
      expand(directory, aw.apps[j].directory, "\\:");
      expand(fname, aw.apps[j].fname, "\\:");
      expand(icon, aw.apps[j].icon, "\\:");
      expand(push_action, aw.apps[j].push_action, "\\:");
      expand(drop_action, aw.apps[j].drop_action, "\\:");
      fprintf(fp, "%s:%s:%s:%s:%s:%s\n", name, directory, fname, icon,
	      push_action, drop_action);
      freeApplicationResources(&aw.apps[j]);
    }
  aw.n_apps =i;
  updateApplicationDisplay();
  writeApplicationData(resources.app_file);

  if (fclose(fp))
    sysError("Error writing clip file:");
}

/*---------------------------------------------------------------------------*/

void appCopyCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  char s[0xff];
  FILE *fp;
  int i;
  char name[2*MAXAPPSTRINGLEN], directory[2*MAXAPPSTRINGLEN],
    fname[2*MAXAPPSTRINGLEN], icon[2*MAXAPPSTRINGLEN],
    push_action[2*MAXAPPSTRINGLEN], drop_action[2*MAXAPPSTRINGLEN];

  if (resources.confirm_copies) {
    sprintf(s, "Copying %d item%s from", aw.n_selections,
	    aw.n_selections > 1 ? "s" : "" );
    if (!confirm(s, "the application window", ""))
      return;
  }

  if (! (fp = fopen(resources.app_clip, "w") )) {
    sysError("Error writing clip file:");
    return;
  }

  fprintf(fp, "#XFM\n");

  for (i=0; i < aw.n_apps; i++)
    if (aw.apps[i].selected) {
      expand(name, aw.apps[i].name, "\\:");
      expand(directory, aw.apps[i].directory, "\\:");
      expand(fname, aw.apps[i].fname, "\\:");
      expand(icon, aw.apps[i].icon, "\\:");
      expand(push_action, aw.apps[i].push_action, "\\:");
      expand(drop_action, aw.apps[i].drop_action, "\\:");
      fprintf(fp, "%s:%s:%s:%s:%s:%s\n", name, directory, fname, icon,
	      push_action, drop_action);
    }
  
  if (fclose(fp))
    sysError("Error writing clip file:");
}

/*---------------------------------------------------------------------------*/

void appPasteCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  FILE *fp;
  char *name, *directory, *fname, *icon, *push_action, *drop_action;
  char s[MAXAPPSTRINGLEN];
  int p;
  
  if (!(fp = fopen(resources.app_clip, "r"))) return;

  for (; (p = parseApp(fp, &name, &directory, &fname, &icon, &push_action,
		       &drop_action)) > 0; aw.n_apps++) {
    aw.apps = (AppList) XTREALLOC(aw.apps, (aw.n_apps+1)*sizeof(AppRec) );
    aw.apps[aw.n_apps].name = XtNewString(strparse(s, name, "\\:"));
    aw.apps[aw.n_apps].directory = XtNewString(strparse(s, directory, "\\:"));
    aw.apps[aw.n_apps].fname = XtNewString(strparse(s, fname, "\\:"));
    aw.apps[aw.n_apps].icon = XtNewString(strparse(s, icon, "\\:"));
    aw.apps[aw.n_apps].push_action = XtNewString(strparse(s, push_action,
							  "\\:"));
    aw.apps[aw.n_apps].drop_action = XtNewString(strparse(s, drop_action,
							  "\\:"));
    aw.apps[aw.n_apps].loaded = False;
    if (!aw.apps[aw.n_apps].icon[0])
      aw.apps[aw.n_apps].icon_bm = defaultIcon(aw.apps[aw.n_apps].name,
					       aw.apps[aw.n_apps].directory,
					       aw.apps[aw.n_apps].fname);
    else if ((aw.apps[aw.n_apps].icon_bm = readIcon(aw.apps[aw.n_apps].icon,0,0))
	     == None) {
      fprintf(stderr, "%s: can't read icon for application %s\n",
	      progname, aw.apps[aw.n_apps].name);
      aw.apps[aw.n_apps].icon_bm = defaultIcon(aw.apps[aw.n_apps].name,
					       aw.apps[aw.n_apps].directory,
					       aw.apps[aw.n_apps].fname);
    } else
      aw.apps[aw.n_apps].loaded = True;
  }

  if (p == -1)
    error("Error in clip file", "");

  if (fclose(fp))
    sysError("Error reading clip file:");

  updateApplicationDisplay();
  writeApplicationData(resources.app_file);
}

/*---------------------------------------------------------------------------*/

void appRemoveCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  char s[0xff];
  int i, j;

  if (resources.confirm_deletes) {
    sprintf(s, "Deleting %d item%s from", aw.n_selections,
	    aw.n_selections > 1 ? "s" : "" );
    if (!confirm(s, "the application window", ""))
      return;
  }

  for (i=j=0; j<aw.n_apps; j++)
    if (!aw.apps[j].selected) {
      if (i != j)
	memcpy(&aw.apps[i], &aw.apps[j], sizeof(AppRec));
      i++;
    } else
      freeApplicationResources(&aw.apps[j]);
  aw.n_apps =i;
  updateApplicationDisplay();
  writeApplicationData(resources.app_file);
}

/*---------------------------------------------------------------------------*/

void appSelectAllCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;
  Pixel pix;
  
  for (i=0; i < aw.n_apps; i++)
    if (!aw.apps[i].selected) {
      XtVaGetValues(aw.apps[i].toggle, XtNforeground, &pix, NULL);
      XtVaSetValues(aw.apps[i].toggle, XtNborder, (XtArgVal) pix, NULL);
      aw.apps[i].selected = True;
    }
  aw.n_selections = aw.n_apps;
}

/*---------------------------------------------------------------------------*/

void appDeselectCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;
  Pixel pix;
  
  for (i=0; i < aw.n_apps; i++)
    if (aw.apps[i].selected) {
      XtVaGetValues(aw.apps[i].toggle, XtNbackground, &pix, NULL);
      XtVaSetValues(aw.apps[i].toggle, XtNborder, (XtArgVal) pix, NULL);
      aw.apps[i].selected = False;
    }
  aw.n_selections = 0;
}

/*---------------------------------------------------------------------------*/

void appLoadCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;

  for(i=0; i<aw.n_apps; i++)
    freeApplicationResources(&aw.apps[i]);
  XTFREE(aw.apps);

  readApplicationData(resources.app_file);
  updateApplicationDisplay();
}

/*---------------------------------------------------------------------------*/

void appMainCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  strcpy(resources.app_file, resources.main_app_file);
  clearApplicationsStack();
  appLoadCb(w, fw, call_data);
}

/*---------------------------------------------------------------------------*/

void appBackCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  int i;

  if (n_appst > 0) {
    for(i=0; i<aw.n_apps; i++)
      freeApplicationResources(&aw.apps[i]);
    XTFREE(aw.apps);

    popApplicationsFile();
    readApplicationData(resources.app_file);
    updateApplicationDisplay();
  }
}

/*---------------------------------------------------------------------------*/

void appOpenCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  newFileWindow(user.home,resources.initial_display_type,False);
}

/*---------------------------------------------------------------------------*/

void appCloseCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  if (resources.confirm_quit && !confirm("", "Exit file manager?", ""))
    return;

  quit();
}

/*---------------------------------------------------------------------------*/

void aboutCb(Widget w, FileWindowRec *fw, XtPointer call_data)
{
  aboutPopup();
}
