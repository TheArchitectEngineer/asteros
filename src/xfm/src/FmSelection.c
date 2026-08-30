#include <stdio.h>
#include <string.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>

#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/StdSel.h>

#define FMSEL  XA_PRIMARY

#include "Fm.h"
#include "FmSelection.h"

static void actOwnSelection(Widget w, XEvent *ev, String *args, Cardinal *nargs)
{
FileWindowRec *fw;

 while (w && !XtIsShell(w)) w=XtParent(w);

 if (w==0) return;

 for (fw=file_windows; fw; fw=fw->next)
   if (fw->shell==w) break;

 if (fw) FmOwnSelection(fw,CurrentTime);
}

static XtActionsRec actions[]={
  { "OwnSelection", actOwnSelection },
};


static FileWindowRec *selection_owner=0;

/* since there can be only one selection owner
 * we can save its colors in global vars
 */
Pixel		     saved_fg;
Pixel		     saved_bg;

static void unhighlight(FileWindowRec *fw)
{
 XtVaSetValues(fw->status,
	XtNbackground,saved_bg,
	XtNforeground,saved_fg,
	0);
}

static Boolean ConvertSelection( 
		Widget w,
		Atom *selection, Atom *target, Atom *type,
		XtPointer *valp,
		unsigned long *length,
		int *format)
{
    char **value=(char**)valp;
    char	*dir;
    Display* d = XtDisplay(w);
    XSelectionRequestEvent* req =
	XtGetSelectionRequest(w, *selection, (XtRequestId)NULL);
    FileWindowRec *fw;
    FileRec	  *file;
    int		  i,len;

    if (*target == XA_TARGETS(d)) {
	Atom* targetP;
	Atom* std_targets;
	unsigned long std_length;
	XmuConvertStandardSelection(w, req->time, selection, target, type,
				  (XPointer*)&std_targets, &std_length, format);
	*value = (XtPointer)XtMalloc(sizeof(Atom)*((unsigned)std_length + 5));
	targetP = *(Atom**)value;
	*targetP++ = XA_STRING;
	*targetP++ = XA_TEXT(d);
	*length = std_length + (targetP - (*(Atom **) value));
	(void)memcpy( (void*)targetP, (void*)std_targets, 
		      (size_t)(sizeof(Atom)*std_length));
	XtFree((char*)std_targets);
	*type = XA_ATOM;
	*format = 32;
	return True;
    }

    if (*target == XA_STRING ||
        *target == XA_TEXT(d))
    {
	*type = XA_STRING;

	/* find our file window pointer */
        fw=file_windows; while(fw && fw->shell!=w) fw=fw->next;

        if (!fw) return False;

	dir=fw->directory;
        len=1;
	if (strcmp(dir,"/")) len+=strlen(dir);
	else dir="";
	len+=strlen(resources.selection_paths_separator);

        len*=fw->n_selections;
	for (i=0; i<fw->n_files; i++) {
	  if ((file=fw->files[i])->selected) len+=strlen(file->name);
        }
	*length=  len;
	{ register char *dest,*src;

    	  *value =  (XtPointer)(dest=XtMalloc(len));
	  for (i=0; i<fw->n_files; i++) {
	    if ((file=fw->files[i])->selected) {
		for (src=dir; *src; src++)
		  *dest++=*src;
		*dest++=(char)'/';
		for (src=file->name; *src; src++)
		  *dest++=*src;
	        for(src=resources.selection_paths_separator; *src; src++)
		  *dest++=*src;
          }
         }
	}
    	*format = 8;
    	return True;
    }
    
    if (XmuConvertStandardSelection(w, req->time, selection, target, type,
				    (XPointer *)value, length, format))
	return True;

    return False;
}

static void LoseSelection(Widget w, Atom* selection)

{
 FileWindowRec *fw;

 if (*selection != FMSEL) return;

 fw=file_windows; while(fw && fw->shell!=w) fw=fw->next;

 if (fw) {
   unhighlight (fw);
   selection_owner=0;
 }
}


Boolean FmOwnSelection(FileWindowRec *fw,Time time)
{
 Boolean rval=False;

 if (!fw->shell) return False;

 if (fw->n_selections == 0) {
   FmDisownSelection(fw);
   return False;
 } else {
   if (fw == selection_owner) return True; /* are already owner */

   if (rval = XtOwnSelection(
		fw->shell,
		FMSEL,
		time,
		ConvertSelection,
		LoseSelection,
		(XtSelectionDoneProc)0)) {
     XtVaGetValues(fw->status,
	XtNbackground,&saved_bg,
	XtNforeground,&saved_fg,
	0);
     XtVaSetValues(fw->status,
	XtNbackground,resources.highlight_pixel,
	XtNforeground,saved_bg,
	0);
     selection_owner=fw;
   } else {
     unhighlight(fw);
     selection_owner=0;
   }
 }
 return rval;
}

void FmDisownSelection(FileWindowRec *fw)
{
 if (fw->shell==0) return;
 if (fw==selection_owner) {
   unhighlight(fw);
   XtDisownSelection(fw->shell,FMSEL,CurrentTime);
   selection_owner=0;
 }
}

void FmSelectionInit(Display *di)
{
 (void)XmuInternAtom(di,XmuMakeAtom("NULL"));
 XtAppAddActions(XtDisplayToApplicationContext(di),actions,XtNumber(actions));
}

Boolean FmIsSelectionOwner(FileWindowRec *fw)
{
 return fw && (fw==selection_owner);
}
