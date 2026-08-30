/*
 *  Enhancements to the X-File Manager XFM-1.3.2 (Path History)
 *  -----------------------------------------------------------
 
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
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "FmHistory.h"

#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>

#ifdef ENHANCE_TXT_FIELD
#include "TextField.h"
#endif

#include "Fm.h"

struct HistoryList_s {
	Widget 		menu,emanate;
	FileWindowRec 	*fw;
	HistoryList	next;
};

static Cardinal InsertAfterLine(Widget w);
static void	HistoryCB(Widget w,XtPointer cld,XtPointer cad);
static void	UpdateHistory(Widget w, XEvent *ev, String *args, Cardinal *nargs);

static XtActionsRec actions[]={
	{"FmUpdateHistory",UpdateHistory},
};

/* linked list of all historiy menus */
static HistoryList histories=0;

HistoryList FmCreateHistoryList(char* name, Widget parent, char *fixed_paths[])
{
 Widget menu;
 int	i,j,found;
 Arg	args[10];
 int	nargs;
 HistoryList hl;
 String str;

 hl=(HistoryList) XtMalloc(sizeof(struct HistoryList_s));
 hl->emanate=0;
 hl->fw=0;

 menu=XtVaCreatePopupShell(
	name?name:"fm_history",
	simpleMenuWidgetClass,
	parent,
	0);

 /* does the menu have a label? */
 XtSetArg(args[0],XtNlabel,&str);

 str=0;
 XtGetValues(menu,args,1);

 if (str) XtVaSetValues(
	XtNameToWidget(menu,"menuLabel"),
	XtNfont,resources.bold_font,
	0);

 nargs=0;
 XtSetArg(args[nargs],XtNlineWidth,0); nargs++;

 if (fixed_paths) for (i=0; fixed_paths[i]; i++) {
 /* eliminate redundant paths */
	found=0;
	for (j=0; j<i; j++) {
		if (found = (!strcmp(fixed_paths[i],fixed_paths[j])))
		break	;
	}
	if (!found) {
		XtAddCallback(
			XtVaCreateManagedWidget(
				fixed_paths[i],
				smeBSBObjectClass,
				menu,
				XtNfont,resources.menu_font,
				0),
			XtNcallback,
			HistoryCB,
			(XtPointer)hl);
	  nargs=0;
	}
 }

/* use a line to separate fixed paths and the history,
 * set line width to 0 in case there are no fixed paths
 * (the line widget is also used as a tag indicating the
 * beginning of the history, so it is necessary). In case
 * of it getting displayed, the resource still is user 
 * configurable :-)
 */

 XtCreateManagedWidget(
	"fm_history_sep",
	smeLineObjectClass,
	menu,
	args,
	nargs);

 /* from now on insert entries immediately after the line */
 XtVaSetValues(menu,
	XtNinsertPosition,InsertAfterLine,
	0);

 hl->menu=menu;

 if (!histories) { /* invoked for the first time */
	XtAppAddActions(
	  XtWidgetToApplicationContext(parent),
	  actions,
	  XtNumber(actions));
 }
 hl->next=histories;
 histories=hl;
 return hl;
}

void FmInsertHistoryPath(HistoryList hl, char *path)
{
 Widget   *chlds;
 Widget	  history_menu=hl->menu;
 Cardinal num_children;
 int	  i;
 String	  str;
 Arg	  arg;

 /* retrieve the list */
 XtVaGetValues(
	history_menu,
	XtNchildren,&chlds,
	XtNnumChildren,&num_children,
	0);


 XtSetArg(arg,XtNlabel,&str);

 /* omit the menu label */
 str=0;
 XtGetValues(hl->menu,&arg,1);
 i=(str?1:0);

 /* is it an existing fixed entry ? */
 while ( !XtIsSubclass(chlds[i],smeLineObjectClass) ) {
   XtGetValues(chlds[i],&arg,1);
   if (!strcmp(str,path)) return;
   i++;
 }

 i++; /* skip line */
	
 /* remove old list entry */

 while (i<(int)num_children) {
   XtGetValues(chlds[i],&arg,1);
   if (!strcmp(str,path)) { 
	/* found it, delete */
	XtDestroyWidget(chlds[i]);
	chlds=0; /* has changed! */
	break;
   }
   i++;
 }

  /* insert new entry */
  XtAddCallback(
  	XtVaCreateManagedWidget(
		path,
		smeBSBObjectClass,
		history_menu,
		XtNfont,resources.menu_font,
		0),
	XtNcallback,
	HistoryCB,
	(XtPointer)hl);

}
	

void FmDeleteHistoryPath(HistoryList hl, char *path)
{
 Arg arg;
 Widget   *chlds,found;
 Widget	  history_menu=hl->menu;
 Cardinal num_children;
 int	  i,haslabel;
 String	  str;

 XtVaGetValues(
	history_menu,
	XtNchildren,&chlds,
	XtNnumChildren,&num_children,
	0);

 XtSetArg(arg,XtNlabel,&str);

 /* look in the fixed entries omitting opt. label*/
 str=0;
 XtGetValues(history_menu,&arg,1);
 haslabel=(str?1:0);
 i=haslabel;
 found=0;
 while ( !XtIsSubclass(chlds[i],smeLineObjectClass) ) {
   XtGetValues(chlds[i],&arg,1);
   /* keep on looking for the line */
   if (!strcmp(str,path)) found=chlds[i]; 
   i++;
 }

 if (found) {
   /* if this is the last fixed path, set the line width to zero */
	if (i-haslabel==1) XtVaSetValues(
		chlds[i],
		XtNlineWidth,0,
		0);
	XtDestroyWidget(found);
	return;
 }

 i++; /* skip line */

 while (i<(int)num_children) {
   XtGetValues(chlds[i],&arg,1);
   if (!strcmp(str,path)) { 
	/* found it, delete */
	XtDestroyWidget(chlds[i]);
	return;
   }
   i++;
  }
}
 
	
 
/* chop the list to n entries */
void FmChopHistoryList(HistoryList hl, int n)
{
 Widget   *chlds,*copy;
 Widget	  history_menu=hl->menu;
 Cardinal num_children,i;

 if (n<0) return;

 XtVaGetValues(
	history_menu,
	XtNchildren,&chlds,
	XtNnumChildren,&num_children,
	0);

 i=0;
 while (!XtIsSubclass(chlds[i],smeLineObjectClass)) i++;
 i++;

 num_children-=i;
 chlds+=i;

 if (n>=(int)num_children) return;

 num_children-=(Cardinal)n;
 chlds+=n;
 /* while destroying the children the list *chlds may change,
  * we better get a private copy.
  */

 copy=(Widget*)(XtMalloc(sizeof(Widget)*(num_children)));
 
 for (i=0; i<num_children; i++) copy[i]=chlds[i];

 for (i=0; i<num_children; i++) XtDestroyWidget(copy[i]);

 XtFree((char*)copy);

}

void FmDestroyHistoryList(HistoryList hl)
{
 HistoryList hlp=histories,*last;


 last=&histories;
 while(hlp && hlp!=hl) {last=&(hlp->next); hlp=hlp->next;}

 if(hlp==0) XtError("FmHistory: tried to destroy invalid path history");

 *last=hl->next;
 
 XtDestroyWidget(hl->menu);
 
 XtFree((char*) hl);
}

static Cardinal InsertAfterLine(Widget w)
{
 Widget   cw=XtParent(w), *chlds;
 Cardinal pos;

 XtVaGetValues(cw,XtNchildren,&chlds,0);

 pos=0;
 while(!XtIsSubclass(chlds[pos],smeLineObjectClass)) pos++;

 return pos+1;
}

static void cb_error(void)
{
XtWarning(
"HistoryCB: something messed up, probably a translation\
 table error (was the 'UpdateHistory'\n\
action called before popping up the menu?");
}

static void HistoryCB(Widget w,XtPointer cld,XtPointer cad)
{
  char 		path[MAXPATHLEN];
  char 		*str;
  HistoryList	hl=(HistoryList)cld;
  char 		s[0xff];
  Widget	p;
#ifdef ENHANCE_SCROLL
  Boolean	keep_position=True;
#endif

  XtVaGetValues(w, XtNlabel, &str, 0);
  strcpy(path,str);

  fnexpand(path);

  /* was the history list set up correctly ? */
  if (hl==0 || hl->menu != XtParent(w)) {
	cb_error(); return;
  }

#ifdef ENHANCE_TXT_FIELD
  /* if the widget that popped up the menu is a text field,
   * then paste the selected path there.
   */
  if (XtIsSubclass(hl->emanate,textFieldWidgetClass)) {
	char *args[2]; 
	args[0]="All";
	XtCallActionProc(hl->emanate,"Delete",0,args,1);
	/* Suppress parsing of the string for special character
	 * sequences (\t etc), take it 'as is'
	 */
	args[0]=path;
	args[1]="F"; 
	XtCallActionProc(hl->emanate,"InsertChar",0,args,2);
	return;
  }
#endif

  /* is there a File Manager? */
  p=hl->emanate;
  if (!hl->fw) p=0;
  while(p && p != hl->fw->shell) p=XtParent(p);
  if (!p) {
	cb_error();
	return;
  }
  

  if (chdir(str=path)) {
    sprintf(s, "Can't open folder %s:", path);
    sysError(s);
    FmDeleteHistoryPath(hl,path);
    if (chdir(str=hl->fw->directory)) {
    	sprintf(s, "Can't open folder %s:", str);
        sysError(s);
        FmDeleteHistoryPath(hl,str);
    }
  } else if (!getwd(path))
    sysError("System error:");
  else {
#ifdef ENHANCE_SCROLL
    if (!(keep_position=(!strcmp(hl->fw->directory,path))))
#endif
    strcpy(hl->fw->directory, path);
#ifdef ENHANCE_SCROLL
    updateFileDisplay(hl->fw,keep_position);
#else
    updateFileDisplay(hl->fw);
#endif
  }
}


static void	UpdateHistory(Widget w, XEvent *ev, String *args, Cardinal *nargs)
{
Widget 		parent;
FileWindowRec	*fw;
HistoryList	hl;
String		str;
char 		error_buf[BUFSIZ];
WidgetList	chlds,destroy;
int		i,line_i;
Cardinal	num_children;
int		n_dest, fixed, haslabel;
Arg		arg;
struct stat 	buf; 

if (*nargs != 1) {
    (void) sprintf(error_buf, "%s %s",
	    "UpdateHistory:  expects only one",
	    "parameter which is the name of the menu.");
    XtAppWarning(XtWidgetToApplicationContext(w), error_buf);
    return;
}

/* get the history list, for which this action was invoked */
hl=histories;
while(hl && strcmp(XtName(hl->menu),args[0])) hl=hl->next;

if (!hl) {
    (void) sprintf(error_buf, "%s '%s' %s",
	    "UpdateHistory:  menu",args[0],
	    "not found in history list");
    XtAppWarning(XtWidgetToApplicationContext(w), error_buf);
    return;
}

/* update the history, eliminate invalid entries */
XtVaGetValues(
	hl->menu,
	XtNchildren,&chlds,
	XtNnumChildren,&num_children,
	0);

destroy=(WidgetList)XtMalloc(sizeof(Widget) *num_children);
n_dest=0;

XtSetArg(arg,XtNlabel,&str);

/* omit menu label */
str=0;
XtGetValues(hl->menu,&arg,1);
haslabel=(str?1:0);
line_i=-1; fixed=0;
for (i=(int)num_children-1; i>=haslabel ; i--) {
	str=0;
	XtGetValues(chlds[i],&arg,1);
	if ( XtIsSubclass(chlds[i],smeLineObjectClass)) {
	     line_i=i;
	     fixed=i-haslabel;
	}

	/* look for invalid path */
	if (str && ( stat(str,&buf) || !S_ISDIR(buf.st_mode) )) {
		/* The list of children should not be tampered,
		 * so we copy the death candidates and destroy
		 * them later.
		 */
		destroy[n_dest++]=chlds[i];
		if (i<line_i) fixed--;
	}
  }

  if (fixed<1) XtVaSetValues(chlds[line_i],XtNlineWidth,0,0);
  chlds=0; /* for safety */
  for (i=0; i<n_dest; i++) XtDestroyWidget(destroy[i]);
  XtFree((char*)destroy); destroy=0;


/* If the widget that popped up the menu is a child of
 * a file window, then get a pointer to its record.
 */

for(fw=file_windows; fw; fw=fw->next) {
  parent=w;
  while (parent!=NULL && parent!=fw->shell)
	parent=XtParent(parent);
  if (parent) break;
}

hl->fw=fw;
hl->emanate=w;

XtCallActionProc(w,"XawPositionSimpleMenu",ev,args,*nargs);
}
