/*
 *  $Id: FocusForm.c,v 1.1.1.1.4.1 2000/02/23 16:21:52 till Exp $
 *
 *  The FocusForm Widget
 *  --------------------
 
 *  Copyright (C) 1997  by Till Straumann   <strauman@sun6hft.ee.tu-berlin.de>

 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Library Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Library Public License for more details.

 *  You should have received a copy of the GNU General Library Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  $Log: FocusForm.c,v $
 *  Revision 1.1.1.1.4.1  2000/02/23 16:21:52  till
 *   - merged albert graef's changes from p3 to p4
 *
 *  Revision 1.6  1999/10/18 22:37:56  cvs
 *   - Imakefile supports building a (static) library now.
 *   - all (resource etc.) strings go to strDefs.c if SEPARATE_STRINGS is defined.
 *
 *  Revision 1.5  1999/10/18 03:51:46  cvs
 *   - fixed TextField bug: when restoring to a string_in_place buffer,
 *     XtVaSetValues(tfw,XtNstring..) must be called with the buffer pointer
 *     (and not the backup).
 *   - fixed/updated doc in TextField.h
 *   - FocusForm paranoia: SetValues checks for being FocusForm subclass.
 *
 *  Revision 1.4  1998/10/30 22:18:57  till
 *  -fixed some bugs in TextField
 *  -C++ compliance for TextField.h
 *  -replace selected part of string
 *
 *  Revision 1.3  1998/05/10 22:59:52  till
 *  dummy action prints a message
 *
 *  Revision 1.2  1998/04/19 11:00:24  till
 *  started cvs logging
 *

 */

/* This is a subclass of the athena 'Form' widget */
/* Copyright (c) 1987, 1988, 1994  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
*/
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xos.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <X11/Xaw/XawInit.h>

#include "FocusFormP.h"

/* Macros */

#define TAKES_FOCUS(widget) \
 (((FocusFormConstraints)((Widget)(widget)->core.constraints))->focusForm.focus_interest &&\
 XtIsSensitive(widget) && \
 XtIsManaged(widget))

/* Methods */
static void	Initialize();
static Boolean	ConstraintSetVals();
static void	ConstraintInitialize();
static void	ChangeManaged();
static void	Realize();
static void	Destroy();

/* Actions */
static void	FocusTraverse();
static void	FocusSet();
static void	Detail_NotifyAncestor();

/* Event Handlers */
static void	RevertFocus();

/* private	Procedures */
static int	FindCompositeChild();
#ifdef notused
static int	FindCompositeNamedChild();
#endif
static Widget	FindFocusFormAncestor();
static Boolean  CanSetFocus();

#ifdef DEBUG
static void dummy(Widget w, XEvent *ev, String *a, Cardinal *na)
{
fprintf(stderr,"'%s' incoked dummy action\n",XtName(w));
}
#endif

static XtActionsRec  actions[]={
 {"FocusTraverse",FocusTraverse},
 {"FocusSet",FocusSet},
 {"Detail_NotifyAncestor",Detail_NotifyAncestor},
#ifdef DEBUG
 {"dummy",dummy},
#endif
};

static char default_translations[]=
#ifdef DEBUG
"Shift	<Key>Tab:FocusTraverse(b)\n\
	<Key>Tab:FocusTraverse()\n\
	<FocusIn>:Detail_NotifyAncestor(dammi)\n\
	<FocusOut>:Detail_NotifyAncestor(dammi)\n\
	<BtnUp> :FocusSet()\n\
	<Enter> :FocusSet()";
#else
"Shift	<Key>Tab:FocusTraverse(b)\n\
	<Key>Tab:FocusTraverse()\n\
	<BtnUp> :FocusSet()\n\
	<Enter> :FocusSet()";
#endif

#ifndef SEPARATE_STRINGS
/* include the automatically generated strings of resource names */
#ifdef __VMS
#include "FocusForm_res_c."
#include "FocusForm_class_c."
#include "FocusForm_type_c."
#else
#include "FocusForm_res_c"
#include "FocusForm_class_c"
#include "FocusForm_type_c"
#endif /* __VMS */
#endif


static XtResource focusFormConstraintResources[]={
	{XtNfocusInterest, XtCFocusInterest, XtRBoolean, sizeof(Boolean),
	XtOffsetOf(FocusFormConstraintsRec, focusForm.focus_interest), XtRImmediate, (XtPointer)False},
	{XtNsensitive, XtCSensitive, XtRBoolean, sizeof(Boolean),
	XtOffsetOf(FocusFormConstraintsRec, focusForm.sensitive), XtRImmediate, (XtPointer)True},
};

static XtResource focusFormResources[]={
	{XtNfocusInitChild, XtCFocusInitChild, XtRString, sizeof(String),
	 XtOffsetOf(FocusFormRec, focusForm.focus_init_child),XtRImmediate,(XtPointer)NULL},
};

FocusFormClassRec focusFormClassRec = {
  { /* core_class fields */
    /* superclass         */    (WidgetClass) &formClassRec,
    /* class_name         */    "FocusForm",
    /* widget_size        */    sizeof(FocusFormRec),
    /* class_initialize   */    XawInitializeWidgetSet,
    /* class_part_init    */    NULL,
    /* class_inited       */    FALSE,
    /* initialize         */    Initialize,
    /* initialize_hook    */    NULL,
    /* realize            */    Realize,
    /* actions            */    actions,
    /* num_actions        */    XtNumber(actions),
    /* resources          */    focusFormResources,
    /* num_resources      */    XtNumber(focusFormResources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion    */    TRUE,
    /* compress_exposure  */    TRUE,
    /* compress_enterleave*/    TRUE,
    /* visible_interest   */    FALSE,
    /* destroy            */    Destroy,
    /* resize             */    XtInheritResize,
    /* expose             */    XtInheritExpose,
    /* set_values         */    NULL,
    /* set_values_hook    */    NULL,
    /* set_values_almost  */    XtInheritSetValuesAlmost,
    /* get_values_hook    */    NULL,
    /* accept_focus       */    NULL,
    /* version            */    XtVersion,
    /* callback_private   */    NULL,
    /* tm_table           */    default_translations,
    /* query_geometry     */	XtInheritQueryGeometry,
    /* display_accelerator*/	XtInheritDisplayAccelerator,
    /* extension          */	NULL
  },
  { /* composite_class fields */
    /* geometry_manager   */   XtInheritGeometryManager,
    /* change_managed     */   ChangeManaged,
    /* insert_child       */   XtInheritInsertChild,
    /* delete_child       */   XtInheritDeleteChild,
    /* extension          */   NULL
  },
  { /* constraint_class fields */
    /* subresourses       */   focusFormConstraintResources,
    /* subresource_count  */   XtNumber(focusFormConstraintResources),
    /* constraint_size    */   sizeof(FocusFormConstraintsRec),
    /* initialize         */   ConstraintInitialize,
    /* destroy            */   NULL,
    /* set_values         */   ConstraintSetVals,
    /* extension          */   NULL
  },
  { /* form_class fields */
    /* layout             */   XtInheritLayout,
  }
};

WidgetClass focusFormWidgetClass = (WidgetClass)&focusFormClassRec;


static void Initialize(treq,tnew,args,narg)
Widget treq,tnew;
ArgList args;
Cardinal *narg;
{
 FocusFormWidget ffw=(FocusFormWidget)tnew;
 ffw->focusForm.focus_child=(Widget)0;
 ffw->focusForm.initialized=False;
 if (!XtIsSubclass(XtParent(tnew),focusFormWidgetClass))
   XtAddEventHandler(tnew,LeaveWindowMask,False,RevertFocus,0);
}

static void ConstraintInitialize(treq,tnew)
Widget treq,tnew;
{
 tnew->core.sensitive=((FocusFormConstraints)(tnew->core.constraints))->focusForm.sensitive;
}


static Boolean ConstraintSetVals(current,request,new,args,narg)
Widget current,request,new;
ArgList args;
Cardinal *narg;
{
Boolean				loose_focus;
Cardinal			dummy=0;
FocusFormConstraintsPart	*new_data;
new_data=&(((FocusFormConstraints)(new->core.constraints))->focusForm);
new->core.sensitive=new_data->sensitive;
loose_focus= !new->core.sensitive || ! new_data->focus_interest;
if (XtIsSubclass(XtParent(current),focusFormWidgetClass)) {
	if ( ((FocusFormWidget)XtParent(current))->focusForm.focus_child==new &&
     	loose_focus ) FocusTraverse(XtParent(current),0,0,&dummy);
} else {
  char *mess=XtMalloc(256);
  sprintf(mess,"FocusForm Action: %s is not managed by a FocusForm",XtName(current));
  XtAppWarning(XtWidgetToApplicationContext(current),mess);
  XtFree(mess);
}
return True;
}

static void Destroy(w)
 Widget w;
{
 XtRemoveEventHandler(w,LeaveWindowMask,False,RevertFocus,0);
}

static void Realize(w,vm,wa)
Widget w;
XtValueMask vm;
XSetWindowAttributes *wa;
{
register Widget wtmp;
/* Do what must be done */
focusFormWidgetClass->core_class.superclass->core_class.realize(w,(XtPointer)vm,wa);
/* The topmost FocusForm sets the Focus finally */
if (!FindFocusFormAncestor(w)) {
  wtmp=w;
  while((wtmp=((FocusFormWidget)wtmp)->focusForm.focus_child) && 
	XtIsSubclass(wtmp,focusFormWidgetClass));
  XtSetKeyboardFocus(w,wtmp);
}
}


static void ChangeManaged(w)
 Widget w;
{
FocusFormWidget ffw=(FocusFormWidget)w;
int		focus_num;
Cardinal	narg;
register Widget top,wtmp;
Widget		winit;
char		*name;

/* Do what must be done */
((CompositeWidgetClass)(focusFormWidgetClass->core_class.superclass))->composite_class.change_managed(w);

if (! ffw->focusForm.initialized) {

  name=ffw->focusForm.focus_init_child;
  top=wtmp=w;
  while (!XtIsShell(top=XtParent(top)));
  winit=0;
  if (! (name && (winit=XtNameToWidget(top,name)) && 
         w==XtParent(winit) && TAKES_FOCUS(winit)) ) {
   /* no initial widget?, take first */
   for (focus_num=0;focus_num<ffw->composite.num_children;focus_num++) {
 	if (TAKES_FOCUS(ffw->composite.children[focus_num])) {
	  winit=ffw->composite.children[focus_num];
	  break;
	}
   }
  }
  ffw->focusForm.focus_child=winit;
  ffw->focusForm.initialized=True;
}


/* will the focus widget become unmanaged ? */
focus_num=FindCompositeChild(w,ffw->focusForm.focus_child);
if (focus_num>=0 && !XtIsManaged(ffw->focusForm.focus_child)) {
	narg=0;
	FocusTraverse(w,0,0,&narg);
}

}

/* This action is needed because the translation manager
 * does not allow to specify the detail of focusIn/Out events.
 * 
 * If children of FocusForm need to perform some action as 
 * a response to Focus in/out (eg. TextField cursor state
 * must be set) they should do so only if they really get
 * the focus from us.
 *
 * If the pointer enters a child of FFW from outside the
 * toplevel window (overlapping windows), the child
 * receives a FocusIn event with detail 'NotifyPointer'.
 *
 * This action simply calls the action proc. with the name
 * given in args[0] and arguments args[1]...args[nargs].
 * This 'forwarding' is suppressed however, if the detail
 * of a FocusIn/Out event is not NotifyAncestor.
 */

static void Detail_NotifyAncestor(w,ev,args,nargs)
Widget		w;
XEvent		*ev;
String		*args;
Cardinal	*nargs;
{
if (*nargs<1) {
 XtAppWarning(XtWidgetToApplicationContext(w),
	"Action 'DetailAncestor()' needs min. one argument");
 return;
}
if ( (ev->type==FocusIn || ev->type==FocusOut) ){
#ifdef DEBUG
   fprintf(stderr,"%s: %c - Mode: %i Detail: %i\n",
	XtName(w),
	(ev->type==FocusIn?(char)'i':(char)'o'),
	(int)(ev->xfocus.mode),
	(int)(ev->xfocus.detail));
#endif
   if (ev->xfocus.detail!=NotifyAncestor) return;
}

XtCallActionProc(w,args[0],ev,args+1,*nargs+1);
}

static void FocusSet(w,ev,args,nargs)
Widget		w;
XEvent		*ev;
String		*args;
Cardinal	*nargs;
{
Widget top,tmpw,new_focus_widget,tmpw1;
FocusFormWidget ffw;
 

top=tmpw=w;
while(!XtIsShell(top=XtParent(top)));

if (ev->type==ButtonPress || ev->type==ButtonRelease){
	tmpw1=top;
}

new_focus_widget=0;
if (*nargs>0) new_focus_widget=XtNameToWidget(top,args[0]);
if (new_focus_widget==0) new_focus_widget=w;

if (!CanSetFocus(new_focus_widget)) return;

tmpw=new_focus_widget;

/* if the focus is set to a focusForm Widget, look recursively for a child,
 * to accept the focus
 */
while (XtIsSubclass(new_focus_widget,focusFormWidgetClass) &&
       (new_focus_widget=(ffw=(FocusFormWidget)new_focus_widget)->focusForm.focus_child));

if (new_focus_widget && TAKES_FOCUS(new_focus_widget)) {

  /* adjust the parents recursively */
  while (0!=(tmpw1=FindFocusFormAncestor(tmpw) )) {
  	((FocusFormWidget)tmpw1)->focusForm.focus_child=tmpw;
  	tmpw=tmpw1;
  }
  XtSetKeyboardFocus(top,new_focus_widget);
#ifdef DEBUG
  fprintf(stderr,"FocusSet for tree: %s, widget %s\n",
	XtName(top),XtName(new_focus_widget));
#endif
}

}

static void FocusTraverse(w,ev,args,nargs)
Widget		w;
XEvent		*ev;
String		*args;
Cardinal	*nargs;
{
Widget			new_focus_widget,old_focus_widget;
register Widget		top,wtmp;
FocusFormWidget		comp;
Widget			*children;
int			num_children,new_focus;
Boolean			forward=True;

if (!CanSetFocus(w)) return;

if (*nargs>0) switch (toupper((int)*args[0])) {
	case 'F':
	case 'V': break;
	
	case 'B':
	case 'R': forward=False; break;

	default: XtWarning("FocusTraverse(): invalid Parameter\n"); break;
}

/* If w is not a FocusForm, look for the closest ancestor which is */
top=wtmp=w;
while( !XtIsShell(top=XtParent(top)) );
wtmp=w;
while (wtmp && !XtIsSubclass(wtmp,focusFormWidgetClass)) wtmp=XtParent(wtmp);
if (!(comp=(FocusFormWidget)wtmp)) goto errorret;

children=comp->composite.children;
num_children=comp->composite.num_children;

if (num_children==0) {
 XtWarning("FocusTraverse (FocusTraverse()): FocusForm has no children");
 return;
}

old_focus_widget=comp->focusForm.focus_child;
new_focus=FindCompositeChild(comp,old_focus_widget);
if (new_focus<0) {
	/* lost track */
	comp->focusForm.focus_child=0;
	XtSetKeyboardFocus(top,top);
	goto errorret;
}

do {
 if (forward) new_focus++; else new_focus--;
 if (new_focus>=num_children) new_focus=0;
 if (new_focus<0) new_focus+=num_children;
 new_focus_widget=children[new_focus];
 if (!TAKES_FOCUS(new_focus_widget)) continue;

 XtSetKeyboardFocus(top,new_focus_widget);
 comp->focusForm.focus_child=new_focus_widget;
 XSync(XtDisplay(comp),False);
 return;
} while (new_focus_widget!=old_focus_widget);
/* no one wants the focus */
comp->focusForm.focus_child=0;
XtSetKeyboardFocus(top,top);
return;

errorret:
XtWarning("FocusForm (FocusTraverse()): Focus could not be changed");

}

/* finds widget in composite's children-list.
 * returns -1 if nothing found
 */
static int FindCompositeChild(parent,wid)
Widget parent,wid;
{
CompositeWidget comp=(CompositeWidget)parent;
int	n;

n=comp->composite.num_children;
do {
 n--;
} while (n>=0 && comp->composite.children[n]!=wid);
return n;
}

#ifdef notused
static int FindCompositeNamedChild(parent,name)
Widget parent;
String name;
{
CompositeWidget comp=(CompositeWidget)parent;
int	n;

if (name==NULL) return -1;

n=comp->composite.num_children;
do {
 n--;
} while (n>=0 && !strcmp(XtName(comp->composite.children[n]),name));
return n;
}
#endif

static Widget FindFocusFormAncestor(widget)
 Widget widget;
{
 do {
  if ( XtIsShell(widget=XtParent(widget)) ) return 0;
 } while (widget && !XtIsSubclass(widget,focusFormWidgetClass));
 return widget;
}

static Boolean CanSetFocus(w)
Widget w;
{
char *mess;
if (! (XtIsSubclass(w,focusFormWidgetClass) || 
       XtIsSubclass(XtParent(w),focusFormWidgetClass)) ) {
  mess=XtMalloc(128);
  sprintf(mess,"FocusForm Action: %s is not managed by a FocusForm",XtName(w));
  XtAppWarning(XtWidgetToApplicationContext(w),mess);
  XtFree(mess);
  return False;
} else
  return True;
}

/* if we leave the topmost focusForm, we are outside the
 * scope of what ffw can do. Therefore we set the focus
 * to the parent.
 */
static void RevertFocus(wid,cld,ev,ctd)
 Widget wid;
 XtPointer *cld;
 XEvent *ev;
 Boolean *ctd;
{
if (ev->xcrossing.detail!=NotifyInferior)
  XtSetKeyboardFocus(XtParent(wid),XtParent(wid));
}
