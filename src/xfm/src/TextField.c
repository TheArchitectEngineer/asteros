/*
 *  $Id: TextField.c,v 1.1.1.2.2.1 2000/02/23 16:21:52 till Exp $
 *
 *  The TextField Widget
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
 *  $Log: TextField.c,v $
 *  Revision 1.1.1.2.2.1  2000/02/23 16:21:52  till
 *   - merged albert graef's changes from p3 to p4
 *
 *  Revision 1.29  2000/02/10 00:42:06  cvs
 *   - more paranoid castings.
 *   - added depend for target3 to Imakefile
 *
 *  Revision 1.28  1999/10/28 18:09:10  cvs
 *   - userData needs #ifndef, not #ifdef!!!
 *
 *  Revision 1.27  1999/10/28 18:06:49  cvs
 *   - added userData resource #ifndef XAW3D
 *   - fixed setValues: any change of `sensitive' needs an expose event.
 *
 *  Revision 1.26  1999/10/22 23:32:42  cvs
 *   - added the showNChars resource.
 *
 *  Revision 1.25  1999/10/21 15:28:16  cvs
 *   - added support for a label to TextField.
 *
 *  Revision 1.24  1999/10/18 22:37:57  cvs
 *   - Imakefile supports building a (static) library now.
 *   - all (resource etc.) strings go to strDefs.c if SEPARATE_STRINGS is defined.
 *
 *  Revision 1.23  1999/10/18 03:51:46  cvs
 *   - fixed TextField bug: when restoring to a string_in_place buffer,
 *     XtVaSetValues(tfw,XtNstring..) must be called with the buffer pointer
 *     (and not the backup).
 *   - fixed/updated doc in TextField.h
 *   - FocusForm paranoia: SetValues checks for being FocusForm subclass.
 *
 *  Revision 1.22  1999/10/11 22:40:58  cvs
 *   - really minor / unsignificant changes
 *
 *  Revision 1.21  1999/10/05 03:49:19  cvs
 *   - if use_string_in_place, setValues cannot know whether the string has
 *     changed.
 *     For now, it searches the arglist for XtNstring as a criteria
 *
 *  Revision 1.20  1999/07/23 02:04:42  till
 *   - set_values: start blinker only if not running already
 *
 *  Revision 1.19  1999/06/09 16:35:30  till
 *  *** empty log message ***
 *
 *  Revision 1.18  1999/03/11 11:32:31  cvs
 *  	- hadn't understood the Xmu Atom cache
 *  	  calling XmuMakeAtom("NULL") in Initialize caused memory leak
 *
 *  Revision 1.17  1999/02/05 17:55:36  till
 *   - still working on 'modifiedCallback' reason.
 *
 *  Revision 1.16  1999/02/05 17:41:50  till
 *   - added modifiedCallback reason (callData parameter).
 *
 *  Revision 1.15  1999/02/05 14:07:26  cvs
 *   - must reset 'modifiedByUser' in setValues if the app changed the
 *     string.
 *
 *  Revision 1.14  1999/02/05 12:54:02  till
 *   - TextFieldValue2String() / TextFieldString2Value() take a Widget typed
 *     argument again (TextFieldWidget makes no sense in the outside world).
 *
 *  Revision 1.13  1999/02/05 12:37:01  till
 *   - cplusplus / function prototypes support
 *   - added blinker, autoBlink, Commit (to includes also)
 *   - added Backup / Restore (to includes also)
 *   - updated the doc (in TextField.h)
 *   - minor changes to scan_strings.l
 *
 *  Revision 1.12  1999/02/04 23:01:05  cvs
 *   -	Blinker support, modifiedByUser resource.
 *  	Added the 'autoBlink' / 'Commit()' action feature.
 *
 *  Revision 1.11  1998/10/30 22:18:58  till
 *  -fixed some bugs in TextField
 *  -C++ compliance for TextField.h
 *  -replace selected part of string
 *
 *  Revision 1.10  1998/07/27 16:51:11  cvs
 *  bugfix in Pointer2Cursor
 *
 *  Revision 1.9  1998/05/20 19:06:33  till
 *  PointerToCursor yielded an invalid result if strlen(buffer)==0
 *
 *  Revision 1.8  1998/05/02 19:56:04  till
 *  InsertChar() action: '\0' handling changed
 *  inserted default translation for <Ctrl>U
 *
 *  Revision 1.7  1998/04/23 20:15:48  till
 *  (hopefully) fixed 'text cursor out of range' warnings.
 *  (Added extra check of text_cursor, hil_start/end,
 *  visible_start/end in SetValues, if use_stringinplace.)
 *
 *  Revision 1.6  1998/04/19 11:00:25  till
 *  started cvs logging
 *

 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>

#ifdef XAW3D
#include <X11/Xaw3d/XawInit.h>
#define SHADOW_WIDTH(tfw) ((tfw)->threeD.shadow_width)
#else
#include <X11/Xaw/XawInit.h>
#define SHADOW_WIDTH(tfw) 0
#endif

/* convert to int to allow for negative labelSeps */
#define LABEL_WIDTH(tfw) ((tfw)->textField.label ? (Dimension)(\
				(int)(tfw)->textField.internalWidth + \
				(int)(tfw)->textField.labelWidth + \
				(tfw)->textField.labelSep) : 0)

#define MAX_ASCENT(tfw) (((tfw)->textField.label && \
	(tfw)->textField.lblFont->max_bounds.ascent > (tfw)->textField.font->max_bounds.ascent) ? \
	(tfw)->textField.lblFont->max_bounds.ascent : (tfw)->textField.font->max_bounds.ascent)

#define MAX_FONTH(tfw) (((tfw)->textField.label && \
	((tfw)->textField.lblFont->max_bounds.ascent + (tfw)->textField.lblFont->max_bounds.descent) > \
	((tfw)->textField.font->max_bounds.ascent + (tfw)->textField.font->max_bounds.descent) ) ? \
	((tfw)->textField.lblFont->max_bounds.ascent + (tfw)->textField.lblFont->max_bounds.descent) : \
	((tfw)->textField.font->max_bounds.ascent + (tfw)->textField.font->max_bounds.descent) )

#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/Converters.h>
#include <X11/Xmu/Drawing.h>
#include <X11/Xmu/StdSel.h>

#ifdef TEXT_COMPAT
#include <X11/Xmu/CharSet.h> /* XmuCopyISOLatin1Lowered() */
#endif

#include "TextFieldP.h"

/* Macro definitions */

#define SetPoint(arg,a1,a2) ((void)((arg).x=(short)(a1), (arg).y=(short)(a2)))
#define TF (tfw->textField)

#ifdef TEXT_COMPAT
#define RESIZE(w) (((w)->textField).resize==XawtextResizeWidth || \
		   ((w)->textField).resize==XawtextResizeBoth)
#else
#define RESIZE(w) ((w)->textField.resize.b)
#endif

/* is this Dimension probably negative ? */
#define NEGDIMENSION(x) 	(x & (1<< (8*sizeof(Dimension)-1)))

#define WIDTH				4
#define CURSOR_WIDTH_DIV2	3
#define CURSOR_ASCENT		3
#define CURSOR_HEIGHT		8

#define PRIMARY_BIT			1
#define SECONDARY_BIT		2
#define CLIPBOARD_BIT		4

#define DEF_BUFFER_SIZE		15

/* extern declarations */
extern long strtol();

/* global variables */

#ifndef SEPARATE_STRINGS
/* include the automatically generated strings of resource names */
#ifdef __VMS
#include "TextField_res_c."
#include "TextField_class_c."
#include "TextField_type_c."
#else
#include "TextField_res_c"
#include "TextField_class_c"
#include "TextField_type_c"
#endif /* __VMS */
#endif
/* The memory for '*value' allocated in 'ConvertSelection()'
 * must be freed in 'ClipboardXferDone()', for we own it 
 * if a 'done_proc' is registered
 */

static
String clipboardValue;

/* resources */

static XtResource resources[] = {
#define offset(field) XtOffsetOf(TextFieldRec, textField.field)
 /* {name, class, type, size, offset, default_type, default_addr}, */
    { XtNfont, XtCFont, XtRFontStruct,
	  sizeof(XFontStruct*), offset(font), XtRString, (XtPointer) XtDefaultFont},
    { XtNforeground, XtCForeground, XtRPixel,
	  sizeof(Pixel), offset(foreground), XtRString, (XtPointer) XtDefaultForeground},
    { XtNinsensitiveForeground, XtCInsensitiveForeground, XtRPixel,
	  sizeof(Pixel), offset(insensitive_foreground), XtRString,(XtPointer)  XtDefaultForeground},
    { XtNinternalHeight, XtCHeight, XtRDimension,
	  sizeof(Dimension), offset(internalHeight), XtRImmediate, (XtPointer)2},
    { XtNinternalWidth, XtCWidth, XtRDimension,
	  sizeof(Dimension), offset(internalWidth), XtRImmediate, (XtPointer)2},
    { XtNinsertPosition, XtCTextPosition, XtRInt,
	  sizeof(TfTextPosition), offset(text_cursor), XtRImmediate, (XtPointer)0},
#ifdef TEXT_COMPAT
    { XtNresize, XtCResize, XtRResizeMode,
	  sizeof(XawTextResizeMode), offset(resize), XtRImmediate, (XtPointer)XawtextResizeNever},
#else
    { XtNresize, XtCResize, XtRBoolean,
	  sizeof(Boolean), offset(resize), XtRImmediate, (XtPointer)False},
#endif
    { XtNstring, XtCLabel, XtRString,
	  sizeof(String), offset(string), XtRString, ""},
    { XtNuseStringInPlace, XtCUseStringInPlace, XtRBoolean,
	  sizeof(Boolean), offset(use_string_in_place), XtRImmediate, (XtPointer)False},
    { XtNlength, XtCLength, XtRInt,
	  sizeof(int), offset(maxlen), XtRImmediate, (XtPointer) 0},
    { XtNjustify, XtCJustify, XtRJustify,
	  sizeof(XtJustify), offset(justify), XtRImmediate, (XtPointer)XtJustifyLeft},
    { XtNreadOnly, XtCReadOnly, XtRBoolean,
	  sizeof(Boolean), offset(readonly), XtRImmediate, (XtPointer)False},
    { XtNscrollChars, XtCScrollChars, XtRShort,
	  sizeof(short), offset(scroll_chars), XtRImmediate, (XtPointer)4},
    { XtNcallback, XtCCallback, XtRCallback,
	  sizeof(XtPointer), offset(notify_cbl), XtRCallback, (XtPointer)NULL},
    { XtNmodifiedByUser, XtCModifiedByUser, XtRBoolean,
	  sizeof(Boolean), offset(modified_by_user), XtRImmediate, (XtPointer)False},
    { XtNmodifiedCallback, XtCCallback, XtRCallback,
	  sizeof(XtPointer), offset(modified_cbl), XtRCallback, (XtPointer)NULL},
    { XtNvalueAddress, XtCValue, XtRPointer,
	  sizeof(caddr_t), offset(value.addr), XtRImmediate, (XtPointer)NULL},
    { XtNvalueSize, XtCValue, XtRInt,
	  sizeof(unsigned int), offset(value.size), XtRImmediate, (XtPointer)NULL},
    { XtNvalueType, XtCValue, XtRString,
	  sizeof(String), offset(value_type), XtRString, (XtPointer)XtRCFormat},
    { XtNinputFormat, XtCValueFormat, XtRString,
	  sizeof(String), offset(input_format), XtRImmediate, (XtPointer)NULL},
    { XtNoutputFormat, XtCValueFormat, XtRString,
	  sizeof(String), offset(output_format), XtRImmediate, (XtPointer)NULL},
    { XtNdisplayCaret, XtCOutput, XtRBoolean,
	  sizeof(Boolean), offset(display_caret), XtRImmediate, (XtPointer)True},
/* modified Simple defaults */
    { XtNcursor, XtCCursor, XtRCursor,
	  sizeof(Cursor), XtOffsetOf(TextFieldRec, simple.cursor), XtRString, "xterm"},
    { XtNblink, XtCBlink, XtRBoolean,
	  sizeof(Boolean), offset(blink), XtRImmediate, (XtPointer)False},
    { XtNautoBlink, XtCAutoBlink, XtRBoolean,
	  sizeof(Boolean), offset(auto_blink), XtRImmediate, (XtPointer)False},
	{ XtNautoBlinkWaitTime, XtCBlinkTime, XtRInt,
	  sizeof(int), offset(auto_blink_wait_time), XtRImmediate, (XtPointer) 2500},
	{ XtNblinkOnTime, XtCBlinkTime, XtRInt,
	  sizeof(int), offset(blink_on_time), XtRImmediate, (XtPointer) 700},
	{ XtNblinkOffTime, XtCBlinkTime, XtRInt,
	  sizeof(int), offset(blink_off_time), XtRImmediate, (XtPointer) 700},
    { XtNleftLabel, XtCLeftLabel, XtRBoolean,
	  sizeof(Boolean), offset(leftLabel), XtRImmediate, (XtPointer)True},
    { XtNlabelFont, XtCFont, XtRFontStruct,
	  sizeof(XFontStruct*), offset(lblFont), XtRString, (XtPointer) XtDefaultFont},
    { XtNlabel, XtCLabel, XtRString,
	  sizeof(String), offset(label), XtRString, (XtPointer) 0},
    { XtNlabelWidth, XtCLabelWidth, XtRDimension,
	  sizeof(Dimension), offset(labelWidth), XtRDimension, (XtPointer) 0},
    { XtNlabelSep, XtCLabelSep, XtRInt,
	  sizeof(int), offset(labelSep), XtRImmediate, (XtPointer) 2 },
    { XtNlabelJustify, XtCLabelJustify, XtRJustify,
	  sizeof(XtJustify), offset(lblJustify), XtRImmediate, (XtPointer)XtJustifyRight},
    { XtNshowNChars, XtCShowNChars, XtRInt,
	  sizeof(int), offset(showNChars), XtRImmediate, (XtPointer)0},
#ifndef XAW3D
    { XtNuserData, XtCUserData, XtRPointer,
	  sizeof(XtPointer), offset(userData), XtRImmediate, (XtPointer)0},
#endif
#undef offset
};

/* Private Procedures */
static int str2chr(
#if NeedFunctionPrototypes
	unsigned char **chpt
#endif
);

static int PointerToCursor(
#if NeedFunctionPrototypes
	TextFieldWidget tfw,
	int pos 
#endif
);

static Boolean GetEventTime(
#if NeedFunctionPrototypes
	XEvent		*ev,
	Time		*time
#endif
);

static Boolean GetSelectionFromArgs(
#if NeedFunctionPrototypes
	Widget		w,
	String		*args,
	Cardinal	*nargs,
	Atom		*selection
#endif
);

static Boolean ConvertSelection(
#if NeedFunctionPrototypes
	Widget w, Atom *selection, Atom *target, Atom *type,
	XtPointer *value, unsigned long *length, int *format
#endif
);

static void LoseSelection(
#if NeedFunctionPrototypes
	Widget w,
	Atom *selection 
#endif
);

static void DoPastingCallback(
#if NeedFunctionPrototypes
	Widget		w, XtPointer	cld,
	Atom		*selection, Atom *type,
	XtPointer	value, unsigned long	*length,
	int		*format
#endif
);

static void ClipboardXferDone(
#if NeedFunctionPrototypes
	Widget w, Atom *selection, Atom *target 
#endif
);

static int GetTextStart(
#if NeedFunctionPrototypes
	TextFieldWidget tfw
#endif
);

static Boolean make_cursor_visible(
#if NeedFunctionPrototypes
	TextFieldWidget tfw
#endif
);

static void DrawCursor(
#if NeedFunctionPrototypes
	TextFieldWidget tfw, int txt_x, int liney
#endif
);

static void DrawLabel(
#if NeedFunctionPrototypes
TextFieldWidget tfw, Boolean do_clear
#endif
);

static void ReDisplay(
#if NeedFunctionPrototypes
	TextFieldWidget tfw, Boolean do_clear
#endif
);

static void getGCs(
#if NeedFunctionPrototypes
	TextFieldWidget tfw
#endif
);

static Boolean adj_left(
#if NeedFunctionPrototypes
	TextFieldWidget tfw
#endif
);

static Boolean adj_right(
#if NeedFunctionPrototypes
	TextFieldWidget tfw
#endif
);

static void curs_visible_left(
#if NeedFunctionPrototypes
	TextFieldWidget tfw
#endif
);

static void curs_visible_right(
#if NeedFunctionPrototypes
	TextFieldWidget tfw
#endif
);

static Boolean resize_width(
#if NeedFunctionPrototypes
	TextFieldWidget tfw
#endif
);

static void Unhighlight(
#if NeedFunctionPrototypes
	TextFieldWidget tfw,
	unsigned long selectionBits
#endif
);

static unsigned long AtomToBit(
#if NeedFunctionPrototypes
	Display *d, Atom	sel 
#endif
);

static void Unselect(
#if NeedFunctionPrototypes
	TextFieldWidget tfw, Atom *selection
#endif
);

static void DoDelete(
#if NeedFunctionPrototypes
	TextFieldWidget tfw,
	int				len,
	int				shift,
	int				curs
#endif
);

/* blinker Support */
static void blinkerToggle(
#if NeedFunctionPrototypes
TextFieldWidget
#endif
);
static void blinkerOff(
#if NeedFunctionPrototypes
TextFieldWidget
#endif
);
static void autoBlinkerOn(
#if NeedFunctionPrototypes
TextFieldWidget
#endif
);

/* the blinker timer callback */
static void blinkerCallback(
#if NeedFunctionPrototypes
 XtPointer, XtIntervalId* 
#endif
);


/* Actions */

static void MoveCursor(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void InsertChar(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void Delete(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void HighlightStart(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void HighlightExtend(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void Highlight(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void MakeSelection(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void DeleteToClipboard(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void InsertSelection(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void Notify(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void CursorState(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void Backup(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void Restore(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);
static void Commit(
#if NeedFunctionPrototypes
 Widget, XEvent*, String*, Cardinal* 
#endif
);

/* Event handlers */
static void handleVisibilityEvents(
#if NeedFunctionPrototypes
Widget, XtPointer, XEvent *, Boolean * 
#endif
);

static XtActionsRec actions[] =
{
 /* {name, procedure}, */
    {"MoveCursor",			MoveCursor},
    {"InsertChar",			InsertChar},
    {"Delete",				Delete},
    {"HighlightStart",		HighlightStart},
    {"HighlightExtend",		HighlightExtend},
    {"Highlight",			Highlight},
    {"MakeSelection",		MakeSelection},
    {"DeleteToClipboard",	DeleteToClipboard},
    {"InsertSelection", 	InsertSelection},
    {"Notify", 				Notify},
    {"Commit", 				Commit},
    {"CursorState",			CursorState},
    {"Backup",				Backup},
    {"Restore",				Restore},
};

/* default translations */

static char translations[] ="\
			<Enter>:		CursorState(Active)\n\
			<Leave>:		CursorState(Inactive)\n\
	None	<Btn1Down>:		MoveCursor()\
							HighlightStart()\n\
			<Btn1Motion>:	MoveCursor()\
							HighlightExtend()\n\
			<Btn1Up>:		HighlightExtend()\
							MakeSelection(PRIMARY)\n\
			<Btn2Up>:		MoveCursor()\
							InsertSelection(PRIMARY)\n\
	Shift	<Key>Right:		MoveCursor()\
							HighlightExtend()\
							MakeSelection(PRIMARY)\n\
 	Shift	<Key>Home:		MoveCursor(Home)\
							HighlightExtend()\
							MakeSelection(PRIMARY)\n\
 	Shift	<Key>End:		MoveCursor(End)\
							HighlightExtend()\
							MakeSelection(PRIMARY)\n\
 	Shift	<Key>Left:		MoveCursor(-1)\
							HighlightExtend()\
							MakeSelection(PRIMARY)\n\
 	Shift	<Key>Delete:	Delete(Selection,End)    \n\
 	Shift	<Key>BackSpace:	Delete(Selection,Home)  \n\
 	Alt		<Key>Delete:	Delete(Selection,All)    \n\
 	Alt		<Key>BackSpace:	Delete(Selection,All)  \n\
	Ctrl	<Key>U:			Delete(All)	\n\
			<Key>Right:		MoveCursor()	\
							HighlightStart()\n\
 			<Key>Home:		MoveCursor(Home)\
							HighlightStart()\n\
 			<Key>End:		MoveCursor(End) \
							HighlightStart()\n\
 			<Key>Left:		MoveCursor(-1)  \
							HighlightStart()\n\
 			<Key>Delete:	Delete(Selection,1)    \n\
 			<Key>BackSpace:	Delete(Selection,-1)  \n\
			<Key>SunCopy:	MakeSelection(CLIPBOARD)\n\
			<Key>SunPaste:	InsertSelection(CLIPBOARD)\n\
			<Key>SunCut:	DeleteToClipboard()\n\
			<Key>Escape:	Restore()\n\
			<Key>:			InsertChar()\
";

/* Methods */

static void ClassInitialize();
static void Initialize();
static void Destroy();
static void TfwExpose();
static void Resize();
static Boolean SetValues();
static XtGeometryResult QueryGeometry();
static Boolean AcceptFocus();
#ifdef XAW3D
static void TfwDrawShadows(
#if NeedFunctionPrototypes
Widget gw, XEvent *event, Region region, Boolean out
#endif
);
#endif

/* Converters */
static Boolean CvtByFormat(
#if NeedFunctionPrototypes
Display *, XrmValuePtr , Cardinal *,
XrmValuePtr , XrmValuePtr, XtPointer *
#endif
);

#ifdef TEXT_COMPAT
static Boolean CvtStringToResizeMode (
#if NeedFunctionPrototypes
Display *, XrmValuePtr , Cardinal *,
XrmValuePtr , XrmValuePtr , XtPointer *
#endif
);
#endif

TextFieldClassRec textFieldClassRec = {
  { /* core fields */
#ifdef XAW3D
    /* superclass		*/	(WidgetClass) &threeDClassRec,
#else
    /* superclass		*/	(WidgetClass) &simpleClassRec,
#endif
    /* class_name		*/	"TextField",
    /* widget_size		*/	sizeof(TextFieldRec),
    /* class_initialize		*/	ClassInitialize,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	XtInheritRealize,
    /* actions			*/	actions,
    /* num_actions		*/	XtNumber(actions),
    /* resources		*/	resources,
    /* num_resources		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	Destroy,
    /* resize			*/	Resize,
    /* expose			*/	TfwExpose,
    /* set_values		*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	AcceptFocus,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	translations,
    /* query_geometry		*/	QueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
  { /* simple    fields */
    /* change_sensitive */		XtInheritChangeSensitive
  },
#ifdef XAW3D
  { /* ThreeD class fields */
    /* shadowdraw	*/		(void(*)())TfwDrawShadows
  },
#endif
  { /* textField fields */
    /* empty			*/	0
  }
};

WidgetClass textFieldWidgetClass = (WidgetClass)&textFieldClassRec;

/* private Procedures */
static int str2chr
#if NeedFunctionPrototypes
(unsigned char **chpt)
#else
(chpt)
unsigned char **chpt;
#endif
{
unsigned char ch=(*(*chpt)++);
int  i,rval;
if (ch != '\\' ) return((int)ch);
ch=(*(*chpt)++);
switch (ch) {
/*  \0 is handled by the default branch
	case '0': return((int)'\0');
 */
	case 'n': return((int)'\n');
	case 't': return((int)'\t');
	case 'b': return((int)'\b');
	case 'f': return((int)'\f');
	case 'r': return((int)'\r');
	case 'v': return((int)'\v');
	case '\'': return((int)'\'');
	case '\\': return((int)'\\');
	default: 
	  rval=0;
	  i=0;
	  do {
	    if ( ch >= '0' && ch <='7' ) {
	      rval=8*rval+(int)(ch-'0');
	      ch=(*(*chpt)++);
	    } else {
	      break;
	    }
	    i++;
          } while (i<=2);
	  return( (i>0 && rval<256) ? rval: -1);
}
}

static unsigned long AtomToBit
#if NeedFunctionPrototypes
(Display *d, Atom    sel)
#else
(d,sel)
 Display *d;
 Atom    sel;
#endif
{
 if      (sel==XA_PRIMARY)    		return((unsigned long)PRIMARY_BIT);
 else if (sel==XA_SECONDARY)  		return((unsigned long)SECONDARY_BIT);
 else if (sel==XA_CLIPBOARD(d))		return((unsigned long)CLIPBOARD_BIT);
 else					return((unsigned long)0);
}

static void Unhighlight
#if NeedFunctionPrototypes
(TextFieldWidget tfw, unsigned long   selectionBits)
#else
(tfw,selectionBits)
TextFieldWidget tfw;
unsigned long   selectionBits;
#endif
{
TF.selections &= ~selectionBits;
if (TF.selections==(unsigned long)0) {
  TF.hil_start=TF.hil_end=-1;
}
}

static void Unselect
#if NeedFunctionPrototypes
( TextFieldWidget tfw, Atom		*selection)
#else
(tfw,selection)
TextFieldWidget tfw;
Atom		*selection;
#endif
{
Display *di=XtDisplay((Widget)tfw);
unsigned long bits;

if (selection==NULL) {
  XtDisownSelection((Widget)tfw,XA_CLIPBOARD(di),CurrentTime);
  XtDisownSelection((Widget)tfw,XA_PRIMARY,CurrentTime);
  XtDisownSelection((Widget)tfw,XA_SECONDARY,CurrentTime);
  bits=0;
} else {
  XtDisownSelection((Widget)tfw,*selection,CurrentTime);
  bits=AtomToBit(di,*selection);
}

Unhighlight(tfw,bits);
}

/* adjust the right margin of the visible window */
/* RETURNS: true if any changes were made */

static Boolean adj_right
#if NeedFunctionPrototypes
(TextFieldWidget tfw)
#else
(tfw)
TextFieldWidget tfw;
#endif
{
 int len    =strlen(&TF.buffer[TF.visible_start]);
 int toolong=XTextWidth(TF.font,TF.buffer+TF.visible_start,len)
	     - (int)(tfw->core.width - LABEL_WIDTH(tfw)
                     -2*(TF.internalWidth+WIDTH+SHADOW_WIDTH(tfw))
		    );

 int old = TF.visible_end;

 TF.visible_end=TF.visible_start+len;

 while(toolong>0 && TF.visible_end>TF.text_cursor) {
	toolong-=XTextWidth(TF.font,&TF.buffer[TF.visible_end-- -1],1);
 }

 return TF.visible_end != old;
}

/* adjust the left margin of the visible window */
/* RETURNS: true if any changes were made */

static Boolean adj_left
#if NeedFunctionPrototypes
(TextFieldWidget tfw)
#else
(tfw)
TextFieldWidget tfw;
#endif
{
 int old=TF.visible_start;
 int toolong=XTextWidth(TF.font,TF.buffer,(int)TF.visible_end)
	     - (int)(((Widget)tfw)->core.width - LABEL_WIDTH(tfw)
                     -2*(TF.internalWidth+WIDTH+SHADOW_WIDTH(tfw))
		    );

 TF.visible_start=0;

 while( toolong>0 && TF.visible_start<TF.text_cursor) {
	toolong-=XTextWidth(TF.font,&TF.buffer[TF.visible_start++],1);
 }
 return TF.visible_start != old;
}

/* adjust the visible area so that the cursor is seen
 * at the right margin (preserving 'scroll_chars')
 */

static void curs_visible_right
#if NeedFunctionPrototypes
(TextFieldWidget tfw)
#else
(tfw)
TextFieldWidget tfw;
#endif
{
 register int len=strlen(TF.buffer);
 if ( (TF.visible_end=TF.text_cursor+TF.scroll_chars) >len) {
  TF.visible_end=len;
 }
 adj_left(tfw);
}

/* adjust the visible area so that the cursor is seen
 * at the left margin (preserving 'scroll_chars')
 */
static void curs_visible_left
#if NeedFunctionPrototypes
(TextFieldWidget tfw)
#else
(tfw)
TextFieldWidget tfw;
#endif
{
 if ( (TF.visible_start=TF.text_cursor-TF.scroll_chars) < 0) {
  TF.visible_start=0;
 }
 adj_right(tfw);
}

/* request the parent to change our width
 * returns false, if this could not be done
 * (might be due to 'resize' resource beeing set to False)
 */

static Boolean resize_width
#if NeedFunctionPrototypes
(TextFieldWidget tfw)
#else
(tfw)
TextFieldWidget tfw;
#endif
{
 XtWidgetGeometry request;
 Boolean rval;
 int     len=strlen(TF.buffer);
 if (!RESIZE(tfw)) return(False);
 request.request_mode=CWWidth;
 request.width= ( TF.showNChars > 0 ?
			TF.showNChars * TF.font->max_bounds.width :
			XTextWidth(TF.font,TF.buffer,len) )
		+ LABEL_WIDTH(tfw)
		+ 2*( SHADOW_WIDTH(tfw)+TF.internalWidth+WIDTH);
 rval=(XtMakeGeometryRequest((Widget)tfw,&request,NULL)==XtGeometryYes);
 if (rval) {
  TF.visible_start=0; TF.visible_end=len;
 }
 return(rval);
}

/* release any old GCs before calling this function */

static void getGCs
#if NeedFunctionPrototypes
(TextFieldWidget tfw)
#else
(tfw)
 TextFieldWidget tfw;
#endif
{
XGCValues	values;
XtGCMask	mask	=GCForeground|GCBackground|GCFont|GCGraphicsExposures;
XtGCMask	inv_mask=GCFunction|GCPlaneMask|GCGraphicsExposures;

	values.foreground=TF.foreground;
	values.background=tfw->core.background_pixel;
	values.function=GXinvert;
	values.graphics_exposures=False;
	values.plane_mask=values.background^values.foreground;
	values.font=TF.lblFont->fid;

	TF.lbl=XtGetGC((Widget)tfw,mask,&values);
	values.font=TF.font->fid;
	TF.norm=XtGetGC((Widget)tfw,mask,&values);
	TF.norm_hil=XtGetGC((Widget)tfw,inv_mask,&values);


	if (TF.foreground==TF.insensitive_foreground) {
		mask		|=GCFillStyle|GCTile;
		values.fill_style=FillTiled;
		values.tile      =XmuCreateStippledPixmap(XtScreen((Widget)tfw),
				TF.foreground,
				tfw->core.background_pixel,
				tfw->core.depth);
	} else {
		values.foreground=TF.insensitive_foreground;
		values.plane_mask=values.background^values.foreground;
	}

	TF.insens    =XtGetGC((Widget)tfw,mask,&values);
	TF.insens_hil=XtGetGC((Widget)tfw,inv_mask,&values);
}

/* Draw the text cursor at txt_x, liney */

static void DrawCursor
#if NeedFunctionPrototypes
(TextFieldWidget tfw, int txt_x, int liney)
#else
(tfw,txt_x,liney)
TextFieldWidget tfw;
int		txt_x;
int		liney;
#endif
{
 XPoint		cur_shape[5];
 int		cur_x,cwd2,cheight,h,peak;
 GC		gc;
 Display	*di=XtDisplay((Widget)tfw);
 Window		wi =XtWindow((Widget)tfw);
 int		i;

 cur_x=txt_x + XTextWidth(TF.font,
			 &TF.buffer[TF.visible_start],
		    (int)(TF.text_cursor-TF.visible_start));

 cheight=(TF.font->max_bounds.ascent+TF.font->max_bounds.descent)/3;
 if (cheight<4) cheight=4;

 gc=(!tfw->core.sensitive || 
      TF.readonly       ||
     !TF.active_cursor )?TF.insens:TF.norm;

 peak=liney-CURSOR_ASCENT;
 h=tfw->core.height-SHADOW_WIDTH(tfw) - peak;
 if (h>cheight) h=cheight;
 cwd2=h/2;
 i=0;
 /* !! KEEP i < 6 !! */
 SetPoint(cur_shape[i],cur_x,liney-CURSOR_ASCENT);i++;
 SetPoint(cur_shape[i],cwd2,h);i++;
 SetPoint(cur_shape[i],-(2*cwd2+1),0); i++;

 XFillPolygon(di,wi,gc,cur_shape,i,Convex,CoordModePrevious);
}

static void
#if NeedFunctionPrototypes
DrawLabel(TextFieldWidget tfw, Boolean do_clear)
#else
DrawLabel(tfw, do_clear)
TextFieldWidget tfw;
Boolean do_clear;
#endif
{
Display *di;
Window	wi;

	if (0==LABEL_WIDTH(tfw) ||
		!XtIsRealized((Widget)tfw) ||
		!tfw->core.visible) return;

	di=XtDisplay(tfw);
	wi=XtWindow((Widget)tfw);

	if (do_clear) {
		XClearArea(di,wi,
			TF.leftLabel ? 0 : tfw->core.width-LABEL_WIDTH(tfw),
			0,
			LABEL_WIDTH(tfw),
			tfw->core.height,
			False);
	}

	if (TF.label && *TF.label) {
		int txt_x, liney;

 		liney = TF.internalHeight+SHADOW_WIDTH(tfw) + MAX_ASCENT(tfw);
		/* internalHeight is around the bigger font */

		txt_x = TF.leftLabel ?
				TF.internalWidth : tfw->core.width - LABEL_WIDTH(tfw) + TF.labelSep;

		switch (TF.lblJustify) {
			case XtJustifyLeft:
				break;

			case XtJustifyRight:
				txt_x += TF.labelWidth -
						 XTextWidth(TF.lblFont,TF.label,strlen(TF.label));
				break;

			case XtJustifyCenter:
				txt_x += ((int)(TF.labelWidth -
						  XTextWidth(TF.lblFont,TF.label,strlen(TF.label))))/2;
				break;
		}

		XDrawString(di,
				wi,
				TF.lbl ? TF.lbl : TF.norm,
				txt_x,
				liney,
				TF.label,
				strlen(TF.label));
	}
}

/* redisplay everything but 3D borders and the label; erase if 'do_clear' */

static void ReDisplay
#if NeedFunctionPrototypes
(TextFieldWidget tfw, Boolean 	do_clear)
#else
(tfw,do_clear)
TextFieldWidget tfw;
Boolean 	do_clear;
#endif
{
 GC		 gc ;
 Display *di;
 Window  wi;
 int	 liney,txt_x,left,right,len;

 if (!XtIsRealized((Widget)tfw) || !((Widget)tfw)->core.visible) return;

 di	  = XtDisplay((Widget)tfw);
 wi	  = XtWindow((Widget)tfw);
 len  = strlen(TF.buffer);
 left = TF.internalWidth+WIDTH+SHADOW_WIDTH(tfw);
 right= tfw->core.width-left;
 liney = TF.internalHeight+SHADOW_WIDTH(tfw) + MAX_ASCENT(tfw);
		/* internalHeight is around the bigger font */

 if (TF.leftLabel)
	left+=LABEL_WIDTH(tfw);
 else
	right-=LABEL_WIDTH(tfw);

 if (do_clear) {
 /* clear the internal Width and Height area also */
   XClearArea(di,wi,
		(TF.leftLabel ? LABEL_WIDTH(tfw) : 0 ) + SHADOW_WIDTH(tfw),
		SHADOW_WIDTH(tfw),
		tfw->core.width-LABEL_WIDTH(tfw)-2*SHADOW_WIDTH(tfw),
		tfw->core.height-2*SHADOW_WIDTH(tfw),
		False);
 }

 txt_x=GetTextStart(tfw);


/* Draw String */

 if (TF.blink_state_on) {
	 Boolean doHighlight = ( TF.hil_start<TF.visible_end && 
			 				 TF.hil_end>TF.visible_start &&
			 				 TF.hil_end!=TF.hil_start);
	 /* init to 0 in - keep compiler quiet */
     int hil_x=0,hil_y=0,hil1,hil2;
     unsigned int hil_w=0,hil_h=0;

	 gc=(((Widget)tfw)->core.sensitive && !TF.readonly)?TF.norm:TF.insens;

	 if (doHighlight) {
		/* calculate highlighted area */
		hil1=(TF.hil_start>TF.visible_start)?TF.hil_start:TF.visible_start;
		hil2=(TF.hil_end  <TF.visible_end)  ?TF.hil_end  :TF.visible_end;
	 	hil_x = txt_x+XTextWidth(TF.font,
								TF.buffer+TF.visible_start,
						 		hil1-TF.visible_start),
		hil_y = liney-TF.font->max_bounds.ascent;
		hil_w = XTextWidth(TF.font, TF.buffer+hil1, hil2-hil1);
		hil_h = TF.font->max_bounds.ascent+TF.font->max_bounds.descent;
	 	if (!do_clear) /* clear highlighted area however */
			XClearArea(di,wi,hil_x,hil_y,hil_w,hil_h,False);
	 }

	 /* then draw the string */

	 XDrawString(di,
			 wi,
			 gc,
			 txt_x,
			 liney,
			 &TF.buffer[TF.visible_start],
			 (int)(TF.visible_end-TF.visible_start));

	 /* now invert highlighted area */

	 if ( doHighlight ) {
		 XFillRectangle(di,wi,
				 (((Widget)tfw)->core.sensitive && !TF.readonly)?TF.norm_hil:TF.insens_hil,
				 hil_x,hil_y,hil_w,hil_h);
	 }
 }
		
/* Draw Cursor */

 if (TF.display_caret) DrawCursor(tfw,txt_x,liney);


/* Draw scroll marks */

 gc=(((Widget)tfw)->core.sensitive && ! TF.readonly) ? TF.norm : TF.insens;
 if (TF.visible_start>0) {
  XDrawLine(di,wi,gc,left-1,liney+TF.font->max_bounds.descent,
		     left-1,liney-TF.font->max_bounds.ascent);

  XFillRectangle(di,wi,gc,
		 left-WIDTH,
		 liney-TF.font->max_bounds.ascent+TF.font->max_bounds.descent,
		 WIDTH-1,
		 TF.font->max_bounds.ascent-TF.font->max_bounds.descent);
 }
 if (TF.visible_end < len) {
  XDrawLine(di,wi,gc,right+1,liney+TF.font->max_bounds.descent,
		     right+1,liney-TF.font->max_bounds.ascent);
  XFillRectangle(di,wi,gc,
		 right+2,
		 liney-TF.font->max_bounds.ascent+TF.font->max_bounds.descent,
		 WIDTH-1,
		 TF.font->max_bounds.ascent-TF.font->max_bounds.descent);
 }

}

/* adjust the visible window, if cursor is outside */
/* RETURNS: true if changes were made (hint if redisplay 
 *			must be called)
 */

static Boolean make_cursor_visible
#if NeedFunctionPrototypes
(TextFieldWidget tfw)
#else
(tfw)
TextFieldWidget tfw;
#endif
{
Boolean rval=False;
if (TF.visible_end<TF.text_cursor) {
 rval=True;
 curs_visible_right(tfw);
}
if (TF.visible_start>TF.text_cursor) {
 rval=True;
 curs_visible_left(tfw);
}
return rval;
}

/* Get X-coordinate of the visible part of the text string
 * (depends on 'justify' etc. )
 */

static int GetTextStart
#if NeedFunctionPrototypes
(TextFieldWidget tfw)
#else
(tfw)
TextFieldWidget tfw;
#endif
{
 int left = TF.internalWidth+WIDTH+SHADOW_WIDTH(tfw);
 int right= tfw->core.width-left;
 int txt_x;
 int txt_width=XTextWidth(  TF.font,
		&TF.buffer[TF.visible_start],
		(int)(TF.visible_end-TF.visible_start));

 if (TF.leftLabel)
	left+=LABEL_WIDTH(tfw);
 else
	right-=LABEL_WIDTH(tfw);

 txt_x = left;

 switch (TF.justify) {
	case XtJustifyRight: txt_x= right -txt_width; break;

	case XtJustifyCenter:txt_x= (right+left-txt_width)/2;

        default:	     break;
 }

 return(txt_x);
}

/* The hightlighted area has been successfully transferred to
 * the CLIPBOARD-owner ('DeleteToClipboard()' action), so
 * it can be deleted now.
 */

/*ARGSUSED*/
static void ClipboardXferDone
#if NeedFunctionPrototypes
(Widget w, Atom  *selection, Atom *target)
#else
(w,selection,target)
Widget w;
Atom   *selection,*target;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
Cardinal	nargs=1;
static char	*args[]={"S"};

/* free the converted value, because we own it */
XtFree(clipboardValue);
/* Call with NULL event arg. to suppress giving up the clipboard */
Delete((Widget)tfw,(XEvent*)0,args,&nargs);
}


/* A selection has been successfully transferred from another client - it
 * can be inserted into the buffer.
 * This procedure is registered in the 'InsertSelection()' action
 */

/*ARGSUSED*/
static void DoPastingCallback
#if NeedFunctionPrototypes
( Widget w, XtPointer cld, Atom *selection, Atom *type,
  XtPointer vpar, unsigned long *length, int *format)
#else
(w,cld,selection,type,vpar,length,format)
	Widget w;
	XtPointer cld;
	Atom *selection, *type;
	XtPointer vpar;
	unsigned long *length;
	int *format;
#endif
{
 TextFieldWidget tfw=(TextFieldWidget)w;
 Display	 *di=XtDisplay((Widget)tfw);
 register int	 ncopychars;
 register char   *chpt,*cursp;
 int		 *value=(int*)vpar;

 if ( ! tfw->core.sensitive || TF.readonly ){
	XBell(di,100);
 } else {
   if ( (*type==XT_CONVERT_FAIL)||
        (value==NULL)		||
	(*value==0)		||
	(*length==0)){
	  XBell(di,100);
	  XtWarning("TextField: no selection or selection timed out");
   } else {

	if (!TF.modified_by_user) Backup(w,0,0,0);

	ncopychars=TF.maxlen-strlen(TF.buffer);
	if ((int)*length>ncopychars) {
	   XBell(di,100);
	   XtWarning("TextField: Buffer full");
	} else { 
	   ncopychars=(int)*length;
	}

	if (ncopychars>0) {
	  Unselect(tfw,(Atom*)0);

	  cursp=TF.buffer+TF.text_cursor;
	  for (chpt=TF.buffer+strlen(TF.buffer);
	       chpt>=cursp; chpt-- ) {
		  *(chpt+ncopychars)=*chpt;
	  }

	  (void)strncpy(cursp,(char*)value,ncopychars);
	  TF.text_cursor+=ncopychars;
  
          adj_right(tfw);
          if (TF.text_cursor>=TF.visible_end) {
	    curs_visible_right(tfw);
          }
          ReDisplay(tfw,True);
	  /* set resource that indicates that the user modified
	   * the string, call the callbacks and trigger the
	   * auto blinker.
	   */
 	  XtCallCallbackList((Widget)tfw,TF.modified_cbl,(XtPointer)tfwModified);
	  /* set this _after_ calling the callback, so they get a chance to
	   * detect changes to this resource.
	   */
	  TF.modified_by_user = True;
   	  if (TF.auto_blink) autoBlinkerOn(tfw);
	}
   }
 }
 if (value) XtFree((char *)value);
}

/* The ownership of *selection has been lost */

/*ARGSUSED*/
static void LoseSelection
#if NeedFunctionPrototypes
(Widget w, Atom *selection)
#else
(w, selection)
Widget w;
Atom   *selection;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
Unhighlight(tfw,AtomToBit(XtDisplay(w),*selection));
if (TF.selections==(unsigned long)0) ReDisplay(tfw,True);
}

static Boolean ConvertSelection
#if NeedFunctionPrototypes
(Widget w, Atom *selection, Atom *target, Atom *type,
 XtPointer *valp, unsigned long *length, int *format)
#else
(w, selection, target, type, valp, length, format)
    Widget w;
    Atom *selection, *target, *type;
    XtPointer *valp;
    unsigned long *length;
    int *format;
#endif
{
    char **value=(char**)valp;
    TextFieldWidget tfw=(TextFieldWidget)w;
    Display* d = XtDisplay(w);
    XSelectionRequestEvent* req =
	XtGetSelectionRequest(w, *selection, (XtRequestId)NULL);
    unsigned int tmp;

    if (*target == XA_TARGETS(d)) {
	Atom* targetP;
	Atom* std_targets;
	unsigned long std_length;
#ifdef __VMS
	XmuConvertStandardSelection(w, req->time, selection, target, type,
				  (void*)&std_targets, &std_length, format);
#else	
	XmuConvertStandardSelection(w, req->time, selection, target, type,
				  (XPointer*)&std_targets, &std_length, format);
#endif
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
      *target == XA_TEXT(d) ||
      *target == XA_COMPOUND_TEXT(d))
    {
    	if (*target == XA_COMPOUND_TEXT(d))
	    *type = *target;
    	else
	    *type = XA_STRING;
        tmp=      TF.hil_end-TF.hil_start;
	*length=  tmp;
    	*value =  (XtPointer)strncpy(XtMalloc(tmp+1),TF.buffer+TF.hil_start, (int) tmp);
        (*value)[tmp]=(char)0;
    	*format = 8;
        if (*selection==XA_CLIPBOARD(d)) {
	  clipboardValue=*value;

/* Disown the clipboard now; disowning it in
 * 'ClipboardXferDone()' behaves wrong!
 * (Communication with 'xclipboard')
 */
	  XtDisownSelection((Widget)tfw,*selection,CurrentTime);
	}
    	return True;
    }
    
#ifdef __VMS
    if (XmuConvertStandardSelection(w, req->time, selection, target, type,
				    (void *)value, length, format))
#else
    if (XmuConvertStandardSelection(w, req->time, selection, target, type,
				    (XPointer *)value, length, format))
#endif
	return True;

    return False;
}


/* Convert a Pointer position to a cursor position
 *
 * return value -1 indicates a Pointer pos. to the left of the string
 * return value -2 indicates a Pointer pos. to the right of the string
 */
static int PointerToCursor
#if NeedFunctionPrototypes
(TextFieldWidget tfw, int pos)
#else
(tfw,pos)
TextFieldWidget tfw;
int		pos;
#endif
{
register int c1pos,c2pos,curs,lst_ch;
int left = TF.internalWidth+WIDTH+SHADOW_WIDTH(tfw);
int right= tfw->core.width-left;

if (TF.leftLabel)
	left+=LABEL_WIDTH(tfw);
else
	right-=LABEL_WIDTH(tfw);

if (pos<left)
	return(-1);
if (pos>right)
	return(-2);

pos-=GetTextStart(tfw);
c1pos=c2pos=0;
curs=TF.visible_start;
lst_ch=strlen(TF.buffer)-1;
while (c1pos<pos && curs<=lst_ch) {
        c2pos=c1pos;
        c1pos=c2pos+XTextWidth(TF.font,&TF.buffer[curs],1);
        curs++;
}
if (abs(c2pos-pos) < abs(c1pos-pos)) curs--;
return(curs);
}

/* Get the Time field from an event structure
 * The function returns false, if ev is the pointer to
 * an inappropiate Event, the function prints a warning
 * message and returns 'False'
 */

static Boolean GetEventTime
#if NeedFunctionPrototypes
(XEvent *ev, Time	*time)
#else
( ev, time )
	XEvent		*ev;
	Time		*time;
#endif
{
switch (ev->type) {
	case KeyPress:
	case KeyRelease:	*time=ev->xkey.time; break;

	case ButtonPress:
	case ButtonRelease:	*time=ev->xbutton.time;break;

	case MotionNotify:	*time=ev->xmotion.time;break;

	case EnterNotify:
	case LeaveNotify:	*time=ev->xcrossing.time;break;

	default:
	  XtWarning("TextField: invalid event type (no time field)");
	  return(False);
}
return(True);
}

/* Retrieve a selection name from 
 * an action-procedure's arglist.
 * Prints a warning message and
 * returns 'False' if there are too
 * many arguments, or if the conversion 
 * yields no result.
 * for *nargs==0 *selection defaults to XA_PRIMARY
 *
 * if additional selections will be added to this
 * procedure, don't forget to care about them 
 * in 'Unselect()' 'AtomToBit()' and 'ClipboardXferDone()'
 */

static Boolean GetSelectionFromArgs
#if NeedFunctionPrototypes
( Widget w, String *args, Cardinal *nargs, Atom *selection)
#else
(w,args, nargs, selection)
	Widget		w;
	String		*args;
	Cardinal	*nargs;
	Atom		*selection;
#endif
{
char mess[128];

if (*nargs>1) {
	XtWarning("TextField: Too many arguments");
	return(False);
}

if (*nargs>0) {
	switch( *args[0]){
	  case 'p': case 'P': *selection=XA_PRIMARY; 	  	     break;
	  case 's': case 'S': *selection=XA_SECONDARY;	  	     break;
	  case 'c': case 'C': *selection=XA_CLIPBOARD(XtDisplay(w)); break;

	  default:  
		(void)sprintf(mess,"TextField: %s invalid selection name",args[0]);
		XtWarning(mess);
		return(False);
	}
} else { /* no arguments */
	*selection=XA_PRIMARY;
}
return(True);
}

/* Action Procedures */


/* Move the Cursor
 * a) (ButtonPress, ButtonRelease or MotionNotify event)
 *	to the Pointer position. If this is one of the scroll
 *	marks then scroll the visible window one character.
 * b) (other events)
 *	evaluate 1st arg: 
 *		'E': move to eol
 *		'H': move to beginning of line
 *		<n>: move n chars
 */

static void MoveCursor
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
int   shift; 
int   len=strlen(TF.buffer);
char *chpt;

switch (ev->type) {
  case ButtonPress:
  case ButtonRelease: shift=PointerToCursor(tfw,ev->xbutton.x); break;

  case MotionNotify:  shift=PointerToCursor(tfw,ev->xmotion.x); break;

 /* retrieve args later */
  default:	      shift=-3; break;
}

switch(shift) {

	default:    TF.text_cursor=shift;
	break;

	case -1:	if (TF.visible_start>0) TF.text_cursor--;
				else TF.text_cursor = 0;
	break;

	case -2:	if (TF.visible_end<len) TF.text_cursor++;
				else TF.text_cursor=len;
	break;

/* retrieve args */
	case -3: 	if (*nargs==0) {
					TF.text_cursor++;
				} else {
					switch (*args[0]) {
						case 'e':
						case 'E':   TF.text_cursor=len;
						break;

						case 'h':
						case 'H':   TF.text_cursor=0;
						break;

						default:	shift=(int) (strtol(args[0],&chpt,10));
									if (chpt==args[0]) {
										XtWarning("TextField: MoveCursor() wrong Parameter");
										shift=1;
									}
									TF.text_cursor+=shift;
									if (shift==0) return;
						break;
					}
  				}
	break;
}
if (TF.text_cursor<0)   TF.text_cursor=0;
if (TF.text_cursor>len) TF.text_cursor=len;

make_cursor_visible(tfw);
ReDisplay(tfw,True);
}

/* Insert one Character at cursor position (key event)
 * if any argument is present, insert 1st arg
 * a 2nd argument can be specified to suppress parsing of
 * special character sequences in the 1st arg (\t, \xxx expansion)
 */
#define STRBUFSIZE 4

static void InsertChar
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
 TextFieldWidget tfw=(TextFieldWidget)w;
 unsigned char	  ch,*valpt,chbuf[STRBUFSIZE];
 int		  val,end;
 Boolean	  	  redisp=False, modified=False, parse=True;

 if (*nargs>0) {
   valpt=(unsigned char *)args[0];
   if (*nargs>1) switch(*args[1]) {
	case 'n': case 'N': case 'F': case 'f': parse=False;
	default: break;
   }
 } else {
   if (ev->type!=KeyPress) return;
   if (0==XLookupString (&(ev->xkey), (char*)chbuf, STRBUFSIZE, NULL, NULL)) {
	/* nothing to insert; maybe a modifier key which we ignore */
	return;
   }
   parse=False;
   chbuf[1]=(unsigned char)0;
   valpt=chbuf;
 }

 /* check for readonly now (don't ring the bell if a modifier was pressed */
 if (TF.readonly){
	XBell(XtDisplay((Widget)tfw),100);
	return;
 }

 /* delete the selection if any */
 if (TF.hil_end>TF.hil_start) {
	int shift;

	TF.text_cursor=TF.hil_start;
	shift=TF.hil_end-TF.hil_start;
	make_cursor_visible(tfw);
	
	if (0 != shift) {
		Unselect(tfw,(Atom*)0);
		if ( ! TF.modified_by_user ) Backup(w,0,0,0);
		DoDelete(tfw,strlen(TF.buffer),shift,TF.text_cursor);
		modified = True;
	}
 }

 end=strlen(TF.buffer)+1;
 while( (val=(parse?str2chr(&valpt):(int)*valpt++)) > 0 ) {

   ch=(char)val;
   if (end>TF.maxlen) {
	XtWarning("TextField: Buffer full");
        XBell(XtDisplay(w),100);
	return;
   }

   if (!modified) {
		if ( ! TF.modified_by_user ) Backup(w,0,0,0);
		Unselect(tfw,(Atom*)0);
		modified = True;
   }

   {
	register char   *chpt=TF.buffer;
 	register int     idx=end;
 	register int     curs=TF.text_cursor;
	while (idx > curs) {chpt[idx]=chpt[idx-1]; idx--;}
	chpt[TF.text_cursor++]=ch;
	curs++; end++;
   }

   if (!resize_width(tfw)) {
	if (TF.justify!=XtJustifyRight) {
     adj_right(tfw);
     if (TF.text_cursor>=TF.visible_end) {
       curs_visible_right(tfw);
	 }
   	} else {
     TF.visible_end++;
     adj_left(tfw);
   	}
   	redisp=True;
   }
 }

 if (val==-1) {
	XtWarning("TextField: InsertChar(): invalid argument");
 }
 
 if (modified) {
   if (redisp) ReDisplay(tfw,True);
   /* set resource that indicates that the user modified
	* the string and call the callbacks.
	*/
   XtCallCallbackList((Widget)tfw,TF.modified_cbl,(XtPointer)tfwModified);
  /* set this _after_ calling the callback, so they get a chance to
   * detect changes to this resource.
   */
   TF.modified_by_user = True;
   /* fire up the auto blinker */
   if (TF.auto_blink) autoBlinkerOn(tfw);
 }
}

/* Called with an ev=NULL by ClipboardXferDone()
 */

static void Delete
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
 TextFieldWidget tfw=(TextFieldWidget)w;
 int			len =strlen(TF.buffer);
 register int   curs=TF.text_cursor;
 register int   shift=0, argindx;
 Boolean	moreargs;

 if (TF.readonly) return;
 if (*nargs==0) {
    shift=(len>curs)?1:0;
 } else {
    argindx=0;
    do {
	moreargs=False;
	switch (*args[argindx]) {

	  /* delete to end of line */
  	  case 'e': case 'E':   shift= len-curs; break;

	  /* delete to beginning of line */
	  case 'h': case 'H':   shift= -curs; break;

	  /* delete all */
	  case 'a': case 'A':   shift= len; break;

	  /* delete highlighted area */
	  case 's': case 'S':   if (TF.hil_end>TF.hil_start) {
			        curs=TF.text_cursor=TF.hil_start;
			        shift=TF.hil_end-TF.hil_start;
				make_cursor_visible(tfw);
			        break;
			      } else  if (*nargs <2 ) {
				/* no second parameter that indicates 
				 * what to delete if there's no selection
				 */
				return;
			      }
				/* if there is, get next parameter and 
				 * evaluate it
				 */
			      argindx++;
			      moreargs=True;
			      break;

        default:
           shift=atoi(args[argindx]);
	   if (shift==0) {
		XtWarning("TextField: Delete() wrong Parameter");
		return;
  	   }
	   if (shift>len-curs) shift=len-curs;
	   if (-shift>curs)    shift=-curs;
	   break;
   	}
   } while (moreargs);
  }

 if (shift==0) return;

 if (ev!=NULL) { 
  /* Dont give up the Clipboard now if called
   * by ClipboardXferDone()
   */
  Unselect(tfw,(Atom*)0);
 } else {
  XtDisownSelection((Widget)tfw,XA_PRIMARY,CurrentTime);
  XtDisownSelection((Widget)tfw,XA_SECONDARY,CurrentTime);
  Unhighlight(tfw,(unsigned long)0);
 }
 
 if ( ! TF.modified_by_user ) Backup(w,0,0,0);
 DoDelete(tfw,len,shift,curs);

 if (!resize_width(tfw)) ReDisplay(tfw,True);

 /* set resource that indicates that the user modified
  * the string and call the callbacks.
  */
 XtCallCallbackList((Widget)tfw,TF.modified_cbl,(XtPointer)tfwModified);
 /* set this _after_ calling the callback, so they get a chance to
  * detect changes to this resource.
  */
 TF.modified_by_user = True;
 /* fire up the auto blinker */
 if (TF.auto_blink) autoBlinkerOn(tfw);
}


/* do the work here */
static void DoDelete
#if NeedFunctionPrototypes
(TextFieldWidget tfw, int len, int shift, int curs)
#else
(tfw, len, shift, curs)
 TextFieldWidget tfw;
 int			 len;
 int			 shift;
 int			 curs;
#endif
{
 if (shift==len) {     /* delete everything */
  TF.text_cursor=0;
  TF.visible_start=0;
  TF.visible_end=0;
  TF.buffer[0]=(char)0;
 } else if (shift>0) { /* delete to the right */
  register char *chpt = TF.buffer;
  curs--; do {curs++;} while((chpt[curs]=chpt[curs+shift])!=0);
 } else { 		/* delete to the left */
  register char *chpt = TF.buffer;
  curs--; do {curs++;} while((chpt[curs+shift]=chpt[curs])!=0);
  TF.text_cursor+=shift;
 }

 if (!resize_width(tfw)) { /* resize not possible */
  if (shift>0) {	   /* deleted to the right */
    TF.visible_end-=shift;
    if (TF.justify==XtJustifyRight) {
	if (TF.text_cursor<TF.visible_end) adj_left(tfw);
    } else {
	adj_right(tfw);
    }
    if (TF.text_cursor>=TF.visible_end &&
        (TF.justify!=XtJustifyLeft || TF.text_cursor<strlen(TF.buffer)) ) {
	curs_visible_right(tfw);
    }
   } else { 		/* delete to the left */
    TF.visible_end+=shift;
    if (TF.justify==XtJustifyLeft) {
	if (TF.text_cursor>TF.visible_start) adj_right(tfw);
    } else {
	adj_left(tfw);
    }
    if (TF.text_cursor <= TF.visible_start &&
        (TF.justify!=XtJustifyRight || TF.text_cursor>0 )) {
	curs_visible_left(tfw);
    }
   }
 }
}

/*ARGSUSED*/
static void HighlightStart
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;

if (ev->type==ButtonPress || ev->type==ButtonRelease) {
  TF.hil_start=TF.hil_end=PointerToCursor(tfw,ev->xbutton.x);
} else {
  TF.hil_start=TF.hil_end=TF.text_cursor;
}
ReDisplay(tfw,True);
}

/*ARGSUSED*/
static void HighlightExtend
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
int		eventpos;
String		word="W";
String		all="A";
Cardinal	n=1;

eventpos=TF.text_cursor;

/* Translations allow no coexistence of
 * Motion Events and multiclicks, so
 * we have to implement them
 */

if (ev->type==ButtonRelease) {
   if (TF.multiclick >= 3)  { /* maximum already reached */
	TF.multiclick=1;
   } else if ((int)(ev->xbutton.time-TF.multiclick_time) 
	      < XtGetMultiClickTime(XtDisplay(w))) { /* multiclick */
	TF.multiclick++;
   } else {
	TF.multiclick=1; /* reset */
   }
   TF.multiclick_time=ev->xbutton.time;

   switch (TF.multiclick) {
	case 1: /* 1st click  */ break;
	case 2: /* 2   clicks */ Highlight(w,ev,&word,&n); return;
	case 3: /* 3   clicks */ Highlight(w,ev,&all,&n); return;
   }
}


if (TF.hil_start<0) {
   TF.hil_start=TF.hil_end=eventpos;
   return;
}

if (TF.hil_start==TF.hil_end) {
  if (eventpos>TF.hil_start)	TF.hil_end=eventpos;
  else				TF.hil_start=eventpos;
} else {
 if (abs(eventpos-TF.hil_start)<abs(eventpos-TF.hil_end))
  TF.hil_start=eventpos;
 else 
  TF.hil_end  =eventpos;
}

ReDisplay(tfw,True);
}

/*ARGSUSED*/
static void Highlight
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
int		eventpos;
char		ch,*chpt;

if (*nargs==0) {
  XtWarning("TextField: Highlight(): no arguments");
  return;
}

switch(ev->type) {
	case ButtonPress:
	case ButtonRelease: 
		eventpos=PointerToCursor(tfw,ev->xbutton.x);
		break;
	case MotionNotify:
		eventpos=PointerToCursor(tfw,ev->xmotion.x);
		break;
	default: 
		eventpos=TF.text_cursor;
		break;
}

switch(*args[0]) {
	case 'w':
	case 'W': /* select word */
		if (eventpos<0) return;
		chpt=strchr(TF.buffer+eventpos,(int)' ');
		TF.hil_end=(chpt)?chpt-TF.buffer:strlen(TF.buffer);

		ch=TF.buffer[eventpos]; TF.buffer[eventpos]=0;
		chpt=strrchr(TF.buffer,(int)' ');
		TF.buffer[eventpos]=ch;
		TF.hil_start=(chpt)?chpt+1-TF.buffer:0;

		break;

	case 'a':
	case 'A': /* select all */
		  TF.hil_start=0; TF.hil_end=strlen(TF.buffer);
		  break;

	case 'e':
	case 'E': /* select to eol */
		  TF.hil_start=eventpos; TF.hil_end=strlen(TF.buffer);
		  break;

	case 'h':
	case 'H': /* select to home */
		  TF.hil_start=0; TF.hil_end=eventpos;
		  break;
	default:
  		XtWarning("TextField: Highlight(): wrong arguments");
		return;
}
ReDisplay(tfw,True);
}

/*ARGSUSED*/
static void MakeSelection
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
Atom		selection;
Display		*di=XtDisplay((Widget)tfw);
Time		time;

if (TF.hil_start>=TF.hil_end) return; /* nothing highlighted */

if (! (GetEventTime(ev,&time) &&
       GetSelectionFromArgs(w,args,nargs,&selection))) {
	XtWarning("(from MakeSelection())");
	return;
}

if (  !	XtOwnSelection(w,
			selection,
			time,
			ConvertSelection,
			LoseSelection,
			(XtSelectionDoneProc) 0)) {
	XtWarning("TextField: Selection ownership not granted");
	XBell(di,100);
   } else {
    TF.selections|=AtomToBit(di,selection);
   }

}


/*ARGSUSED*/
static void DeleteToClipboard
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
Time		time;
Display		*di=XtDisplay((Widget)tfw);

if (TF.hil_start>=TF.hil_end) return; /* nothing highlighted */
if (TF.readonly)	      return;

if ( ! GetEventTime(ev,&time)) {
	  XtWarning("(from DeleteToClipboard())");
	  return;
}

if (  !	XtOwnSelection(w,
			XA_CLIPBOARD(di),
			time,
			ConvertSelection,
			LoseSelection,
			ClipboardXferDone)) {
	XtWarning("TextField: Selection ownership not granted");
	XBell(di,100);
   } else {
    TF.selections|=CLIPBOARD_BIT;
   }
}

/*ARGSUSED*/
static void InsertSelection
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
Time		time;
Atom		selection;

if ( TF.readonly ) {
	XBell(XtDisplay((Widget)tfw),100);
	return;
}

if (! (GetEventTime(ev,&time) &&
       GetSelectionFromArgs(w,args,nargs,&selection))) {
	XtWarning("(from MakeSelection())");
	return;
}

XtGetSelectionValue(
	(Widget)tfw,
	selection,
	XA_STRING,
	DoPastingCallback,
	(XtPointer)(unsigned long)TF.text_cursor,
	time);
}

/*ARGSUSED*/
static void Notify
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
XtCallCallbackList((Widget)tfw,TF.notify_cbl,(XtPointer)((*nargs>0) ? args[0] : 0));
}

/*ARGSUSED*/
static void Commit
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
XtCallCallbackList((Widget)tfw,TF.notify_cbl,(XtPointer)((*nargs>0) ? args[0] : 0));
/* switch off the auto-blinker */
if (TF.auto_blink) blinkerOff(tfw);
/* commit the changes (notify user) */
XtCallCallbackList((Widget)tfw,TF.modified_cbl,(XtPointer)tfwCommitted);
TF.modified_by_user = False;
/* throw away the backup */
if (TF.backup) {
	XtFree(TF.backup);
	TF.backup=0;
}
}

/*ARGSUSED*/
static void CursorState
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
( w, ev, args, nargs )
 Widget		w;
 XEvent		*ev;
 String		*args;
 Cardinal	*nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;

if ((*nargs) == 0) TF.active_cursor=!TF.active_cursor;
else {
 switch (*args[0]) {
	case 'a':
	case 'A': TF.active_cursor=True;  break;

	case 'i':
	case 'I': TF.active_cursor=False; break;

 	case 't':
	case 'T': TF.active_cursor=!TF.active_cursor; break;

	default:
	   XtWarning("TextField: CursorState(): wrong parameter");
	   return;
 }
}
		   
if (TF.display_caret) DrawCursor(tfw,
	   GetTextStart(tfw),
 	   TF.internalHeight+SHADOW_WIDTH(tfw)+MAX_ASCENT(tfw));
		/* internalHeight is around the bigger font */
}

/* Methods */

/*ARGSUSED*/
static void TfwExpose
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, Region reg)
#else
(w,ev,reg)
Widget w;
XEvent *ev;
Region reg;
#endif
{ 
TextFieldWidget tfw=(TextFieldWidget)w;

ReDisplay(tfw,False);
if (TF.label)
	DrawLabel(tfw,False);

#ifdef XAW3D
 (*((TextFieldWidgetClass)(XtClass((Widget)tfw)))->threeD_class.shadowdraw)(w,ev,reg,
		(Boolean)(! (tfw->core.sensitive && !TF.readonly)));
#endif

}

#ifdef XAW3D
#define TOP 0
/*ARGSUSED*/
static void TfwDrawShadows
#if NeedFunctionPrototypes
(Widget gw, XEvent *event, Region region, Boolean out)
#else
(gw, event, region, out)
    Widget gw;
    XEvent *event;
    Region region;
    Boolean out;
#endif
{
    XPoint  pt[6];
    TextFieldWidget tfw = (TextFieldWidget) gw;
    Dimension   	s = tfw->threeD.shadow_width;
    /*
     * draw the shadows using the core part width and height,
     * threeD part shadow_width and our LABEL_WIDTH.
     *
     *  no point to do anything if the shadow_width is 0 or the
     *  widget has not been realized.
     */
	if((s > 0) && XtIsRealized (gw)){

		Dimension   h = tfw->core.height;
		Dimension   w = tfw->core.width-LABEL_WIDTH(tfw);
		Dimension   wms = w - s;
		Dimension   hms = h - s;
		Display     *dpy = XtDisplay (gw);
		Window      win = XtWindow (gw);
		GC      	top, bot;
		Position	left=TF.leftLabel ? LABEL_WIDTH(tfw) : 0;

		if (out) {
			top = tfw->threeD.top_shadow_GC;
			bot = tfw->threeD.bot_shadow_GC;
		} else {
			top = tfw->threeD.bot_shadow_GC;
			bot = tfw->threeD.top_shadow_GC;
		}
		/* top-left shadow */
		if ((region == NULL) ||
				(XRectInRegion (region, left, TOP, w, s) != RectangleOut) ||
				(XRectInRegion (region, left, TOP, s, h) != RectangleOut)) {

			pt[0].x = left;    pt[0].y = TOP+h;
			pt[1].x = left;    pt[1].y = TOP;
			pt[2].x = left+w;  pt[2].y = TOP;
			pt[3].x = left+wms;pt[3].y = TOP+s;
			pt[4].x = left+s;  pt[4].y = TOP+s;
			pt[5].x = left+s;  pt[5].y = TOP+hms;
			XFillPolygon (dpy, win, top, pt, 6,Complex,CoordModeOrigin);
		}

		/* bottom-right shadow */
		if ((region == NULL) ||
				(XRectInRegion (region, left, TOP + hms, w, s) != RectangleOut) ||
				(XRectInRegion (region, left + wms, TOP, s, h) != RectangleOut)) {

			pt[0].x = left;      pt[0].y = TOP+h;
			pt[1].x = left+w;    pt[1].y = TOP+h;
			pt[2].x = left+w;    pt[2].y = TOP;
			pt[3].x = left+wms;  pt[3].y = TOP+s;
			pt[4].x = left+wms;  pt[4].y = TOP+hms;
			pt[5].x = left+s;    pt[5].y = TOP+hms;
			XFillPolygon (dpy, win, bot, pt,6, Complex,CoordModeOrigin);
		}
	}
}
#undef TOP
#endif /* defined XAW3D */



static void ClassInitialize ()
{
  static XtConvertArgRec formatCvtFromStringArgs[]={
	{ XtWidgetBaseOffset,
	  (XtPointer) XtOffsetOf(TextFieldRec,textField.input_format),
	  sizeof(String)
	},
	{ XtImmediate,
	  (XtPointer)True,
	  sizeof(XtPointer)
	}
  };
  static XtConvertArgRec formatCvtToStringArgs[]={
	{ XtWidgetBaseOffset,
	  (XtPointer) XtOffsetOf(TextFieldRec,textField.output_format),
	  sizeof(String)
	},
	{ XtImmediate,
	  (XtPointer)False,
	  sizeof(XtPointer)
	}
  };

  XawInitializeWidgetSet();
  XtAddConverter(XtRString, XtRJustify, XmuCvtStringToJustify,
		  (XtConvertArgList)NULL, 0);
#ifdef TEXT_COMPAT
  XtSetTypeConverter(XtRString, XtRResizeMode, CvtStringToResizeMode,
		  (XtConvertArgList)NULL, 0, XtCacheAll, NULL);
#endif
  XtSetTypeConverter(XtRString, XtRCFormat, CvtByFormat,
		  formatCvtFromStringArgs, XtNumber(formatCvtFromStringArgs), XtCacheAll, NULL);
  XtSetTypeConverter(XtRCFormat, XtRString, CvtByFormat,
		  formatCvtToStringArgs, XtNumber(formatCvtToStringArgs), XtCacheAll, NULL);
}

/*ARGSUSED*/
static void Initialize
#if NeedFunctionPrototypes
(Widget treq, Widget tnew, ArgList args, Cardinal *num_args)
#else
(treq, tnew, args, num_args)
 Widget	  treq, tnew;
 ArgList  args;
 Cardinal *num_args;
#endif
{
 TextFieldWidget tfw=(TextFieldWidget)tnew;
 Dimension       minh;
 char		 	 mess[256];
 int		 	 len;

 /* get Xt's multiclick time */

 TF.norm=TF.insens=0;
 TF.selections=0;
 TF.active_cursor=False;
 /* set multicklick so that the count will be
  * reset upon first call of HighlightExtend()
  * anyway
  */
 TF.multiclick=4;
 TF.multiclick_time=(Time)0;

 Unselect(tfw,(Atom*)0);

 if (TF.use_string_in_place) {
	TF.buffer=TF.string;
	if (TF.maxlen < strlen(TF.string)) {
	  if (TF.maxlen) /* maxlen==0 means: set maxlen to strlen */
	    XtWarning("TextField: buffer length < strlen() (set to strlen())");
	  TF.maxlen=strlen(TF.string)+1;
	}
 } else {
 	if (TF.maxlen<1) {
        	TF.maxlen=DEF_BUFFER_SIZE;
 	} else if (TF.maxlen>1023) {
		XtWarning("TextField: buffer length > 1023 (correct?)");
 	}
	
 	TF.buffer=(String)XtMalloc((unsigned) TF.maxlen+1);
 	TF.buffer[0]=TF.buffer[TF.maxlen]=(char)0;

 	if (strlen(TF.string)>TF.maxlen) {
		(void)sprintf(mess,"TextField: Buffer full (%i chars)",(int)TF.maxlen);
		XBell(XtDisplay((Widget)tfw),100);
		XtWarning(mess);
 	}

 	if (TF.string) (void)strncpy(TF.buffer,TF.string,TF.maxlen);

 	TF.string=TF.buffer;
 }

 TF.backup = (String)0;

 len=strlen(TF.buffer);
 
 if (TF.text_cursor <  0 || TF.text_cursor>len) {
	XtWarning("TextField: cursor position out of range (using 0)");
	TF.text_cursor=0;
 }

 if (NEGDIMENSION(TF.internalHeight)) {
	XtWarning("TextField: internalHeight < 0 ? (using 0)");
	TF.internalHeight=0;
 }

 if (NEGDIMENSION(TF.internalWidth)) {
	XtWarning("TextField: internalWidth < 0 ? (using 0)");
	TF.internalWidth=0;
 }

 if (NEGDIMENSION(TF.labelWidth)) {
	XtWarning("TextField: labelWidth < 0 ? (using 0)");
	TF.labelWidth=0;
 }

 TF.visible_start=(short)0; TF.visible_end=(short)len;
 
 /* if there is a label, make a copy */
 if (TF.label) {
	int lw;
	TF.label = XtNewString(TF.label);
	lw = XTextWidth(TF.lblFont,TF.label,strlen(TF.label));
	if (lw>(int)TF.labelWidth) TF.labelWidth=lw;
 }

 /* set Font */
 minh = MAX_FONTH(tfw) + 2*(  TF.internalHeight + SHADOW_WIDTH(tfw));

 if (tfw->core.height < minh) {
   if (tfw->core.height > 0) {
     (void)sprintf(mess,"TextField: Height too small (using %i)",(int)minh);
     XtWarning(mess);
   }
   tfw->core.height=minh;
 }
 getGCs(tfw);

 if (tfw->core.width<=0 || RESIZE(tfw)) {
   tfw->core.width = ((TF.showNChars > 0 ) ?
							TF.showNChars*TF.font->max_bounds.width :
							XTextWidth(TF.font,TF.buffer,len))
		   					+ LABEL_WIDTH(tfw)
		   					+ 2*( TF.internalWidth + WIDTH 
		        			+SHADOW_WIDTH(tfw));
 } else {
   switch (TF.justify) {
    case XtJustifyLeft: TF.visible_start=TF.text_cursor-TF.scroll_chars;
			if (TF.visible_start<0) TF.visible_start=0;
			adj_right(tfw);
			break;
    case XtJustifyRight:TF.visible_end=TF.text_cursor+TF.scroll_chars;
			if (TF.visible_end>len) TF.visible_end=len;
			adj_left(tfw);
			break;
    default:		adj_right (tfw);
   			adj_left (tfw);
   }
 }
 /* fix core bug (?), initially 'visible' is set to true although
  * the window is not even mapped !!!
  */
 tfw->core.visible=False;
 /*
  * another problem: core sets 'visible' handling VisiblityNotify events.
  *					 Such an event is _not_ sent after unmapping a widget
  *					 however. (if mapping a window the server sends
  *					 visiblitynotify though).
  *	YAP (yet another problem):
  *					 we get no callback if the visibility changes, so we better
  *					 handle visibility events ourselves.
  */
  XtAddEventHandler((Widget)tfw,VisibilityChangeMask,False,
					handleVisibilityEvents,(XtPointer)0);

 /* initialize the blinker */
 TF.blink_state_on = True;
 TF.blink_timer = (XtIntervalId) 0;
 if (TF.auto_blink_wait_time < 100) TF.auto_blink_wait_time = 100;
 if (TF.blink_on_time < 100) TF.blink_on_time = 100;
 if (TF.blink_off_time < 100) TF.blink_off_time = 100;
}

static void Destroy
#if NeedFunctionPrototypes
(Widget w)
#else
(w)
Widget w;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;

Unselect(tfw,(Atom*)0);
XtReleaseGC(w,TF.lbl);
XtReleaseGC(w,TF.norm);
XtReleaseGC(w,TF.norm_hil);
XtReleaseGC(w,TF.insens);
XtReleaseGC(w,TF.insens_hil);

/* remove event handlers */
XtRemoveEventHandler(w, XtAllEvents, True, handleVisibilityEvents, (XtPointer)0);

/* remove the blinker */
if (TF.blink_timer) {
	XtRemoveTimeOut(TF.blink_timer);	
	TF.blink_timer=(XtIntervalId)0;
}

if (! TF.use_string_in_place) XtFree(TF.buffer);
if (TF.backup) XtFree(TF.backup);
if (TF.label)  XtFree(TF.label);
}

static void Resize
#if NeedFunctionPrototypes
(Widget w)
#else
(w)
Widget w;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;

TF.internalHeight=(int)(tfw->core.height - MAX_FONTH(tfw))/2 - SHADOW_WIDTH(tfw);

if (NEGDIMENSION(TF.internalHeight)) TF.internalHeight=0;

if (TF.justify==XtJustifyRight) {
  adj_left (tfw);
  adj_right(tfw);
} else {
  adj_right(tfw);
  adj_left (tfw);
}
}

#define NTF (ntfw->textField)

/*ARGSUSED*/
static Boolean SetValues
#if NeedFunctionPrototypes
(Widget current, Widget req, Widget super, ArgList args, Cardinal *nargs)
#else
(current, req, super, args, nargs)
Widget   current,req,super;
ArgList  args;
Cardinal *nargs;
#endif
{
 TextFieldWidget tfw=(TextFieldWidget)  current;
 TextFieldWidget ntfw=(TextFieldWidget) super;

 Boolean adjust=False,new_string,new_buffer;
 Boolean needsExpose=False;

 TfwModificationType reason=tfwNone;
 int     nlen;
 int	 maxFntH;

 if (XtIsSensitive((Widget)ntfw)!=XtIsSensitive((Widget)tfw)) { /* Switch to insensitive */
	 needsExpose=True;
 }
 
 if (TF.font->fid !=NTF.font->fid ||
	 TF.lblFont->fid != NTF.lblFont->fid ||
     tfw->core.background_pixel != ntfw->core.background_pixel ||
     TF.foreground!=NTF.foreground ||
	 TF.insensitive_foreground!=NTF.insensitive_foreground) {
	 XtReleaseGC((Widget)tfw,TF.lbl);
	 XtReleaseGC((Widget)tfw,TF.norm);
	 XtReleaseGC((Widget)tfw,TF.norm_hil);
	 XtReleaseGC((Widget)tfw,TF.insens);
	 XtReleaseGC((Widget)tfw,TF.insens_hil);
	 getGCs(ntfw);
	 if (TF.font->fid !=NTF.font->fid ||
			 TF.lblFont->fid != NTF.lblFont->fid)
		 adjust=True;
	 needsExpose=True;
 }

 /* check for changes affecting the label */
 if ( TF.label != NTF.label ) {
	if (TF.label) {
		XtFree(TF.label); TF.label=0;
	}
	if (NTF.label){
		NTF.label=XtNewString(NTF.label);
	}
	adjust=True;
	needsExpose=True;
 }
 if (TF.labelSep!=NTF.labelSep) {
	adjust=True;
	needsExpose=True;
 }
 {
 Dimension lw = NTF.label ?
		 XTextWidth(NTF.lblFont,NTF.label,strlen(NTF.label)) : 0;
	 if (lw > NTF.labelWidth) {
		 /* set labelWidth to text width */
		 NTF.labelWidth=lw;
	 }
 }
 if (NTF.labelWidth!=TF.labelWidth) {
	adjust=True;
	needsExpose=True;
 }

 /* calculate the height of the bigger font */
 maxFntH=MAX_FONTH(ntfw);

 if (!RESIZE(ntfw)) {
	 if (adjust) {
		 NTF.internalHeight=(int)(ntfw->core.height - maxFntH)/2
			 					  -SHADOW_WIDTH(ntfw);
		 if (NEGDIMENSION(NTF.internalHeight)) {
			 ntfw->core.height-=NTF.internalHeight;
			 NTF.internalHeight=0;
		 }
	 } else {
		 NTF.internalHeight=TF.internalHeight;
	 }
	 if ((int)NTF.internalWidth*2 > (int)tfw->core.width-(int)LABEL_WIDTH(tfw)-30) {
		 NTF.internalWidth=TF.internalWidth;
	 } 
	 if (TF.internalWidth!=NTF.internalWidth) adjust=True;
 } else {
   if (adjust || (NTF.internalHeight!=TF.internalHeight)) {
    ntfw->core.height=maxFntH + 2*(SHADOW_WIDTH(ntfw)+NTF.internalHeight);
   }
 }

   if (NTF.use_string_in_place != TF.use_string_in_place) {
	XtWarning("TextField (SetValues): useStringInPlace cannot be changed");
	NTF.use_string_in_place = TF.use_string_in_place;
   }

   nlen=strlen(NTF.string);

   if (NTF.use_string_in_place) {
	if ( !  (new_buffer = (NTF.string != TF.string)) &&
   		(NTF.maxlen!=TF.maxlen)) {
	  XtWarning("TextField (SetValues): new length for old buffer req (use old)");
	  NTF.maxlen=TF.maxlen;
	}
	if (new_buffer) {
		NTF.buffer=NTF.string;
		if (NTF.maxlen == 0) NTF.maxlen=strlen(NTF.buffer)+1;
	}
   } else {
	if (0!=(new_buffer = (NTF.maxlen!=TF.maxlen))) {
		NTF.buffer=XtMalloc((unsigned)NTF.maxlen+1);
        	NTF.buffer[0]=NTF.buffer[NTF.maxlen]=0;
   	} else {
		NTF.buffer=TF.buffer;
   	}
   }

   if (NTF.use_string_in_place && !new_buffer) {
	int k;
		/* we have no means to know that the string changed at this point :-(
		 * just trust them and search for XtNstring in the argument list;
		 * that's the way they tell us they tampered with the string.
		 */
	new_string=False;
	for (k=0; k<*nargs; k++)
		if (!strcmp(args[k].name,XtNstring)) {
			new_string=True;
			break;
		}
   } else {
   	new_string = (Boolean)(strcmp( NTF.string, TF.string));
   }

   if (new_string) {
   		NTF.modified_by_user = False;
		if (NTF.backup && ! strcmp(NTF.backup,NTF.string)) {
			/* string was restored. */
			reason=tfwStringRestored;
		} else
			reason=tfwStringChanged;
   }

   if (new_string || new_buffer){
        if (new_string || NTF.hil_start>NTF.maxlen) {
			Unselect(tfw,(Atom*)0);
			needsExpose=True;
		}

		if (!NTF.use_string_in_place) {
			(void)strncpy(NTF.buffer,NTF.string,NTF.maxlen);
			if (nlen > NTF.maxlen) {
				nlen = NTF.maxlen;
				XtWarning("TextField: Buffer full");
				XBell(XtDisplay(ntfw),100);
			}
			if (new_buffer) XtFree(TF.buffer);
		}

		/* throw away the backup, we don't bother allocating a
		 * new buffer. (Do this only _after_ copying the source
		 * string to the destination, since Restore() sets the
		 * source to 'backup'
		 */
		if (NTF.backup) {
			XtFree(NTF.backup);
			NTF.backup=(String)0;
		}

		/*
		 * maybe the string was edited; verify that the
		 * cursor, the highlighted and visible areas are
		 * valid.
		 */
		if (new_string){
			NTF.visible_start=0;
			NTF.visible_end=nlen;
			if (NTF.hil_start>=nlen) NTF.hil_start=NTF.hil_end=-1;
			if (NTF.hil_end > nlen) NTF.hil_end = nlen;
			if (NTF.text_cursor > nlen) NTF.text_cursor=nlen;
			adjust=True;
		} else {
			if (NTF.visible_end>NTF.maxlen) {
				NTF.visible_end=NTF.maxlen;
				adjust=True;
			}
			if (NTF.hil_end>NTF.maxlen) {
				NTF.hil_end=NTF.maxlen;
				adjust=True;
			}
		}
   }

   NTF.string=NTF.buffer;  

   if (NTF.text_cursor<0 ||
       NTF.text_cursor>nlen) {
	XtWarning("TextField: text cursor out of range");
	NTF.text_cursor=0;
   }
   if (NTF.scroll_chars<0) {
	XtWarning("TextField: scrollChars out of range");
	NTF.scroll_chars=1;
   }

   if (NTF.text_cursor>NTF.visible_end) {
     NTF.visible_end=NTF.text_cursor; adjust=True;
   }
   if (NTF.text_cursor<NTF.visible_start){
     NTF.visible_start=NTF.text_cursor; adjust=True;
   }

   if ((int)NTF.value.size<0) {
     XtWarning("TextField: valueSize<0");
     NTF.value.size=TF.value.size;
   }

   if (RESIZE(ntfw) &&
		(adjust || NTF.showNChars != TF.showNChars || !RESIZE(tfw))) {
	 /* not that `adjust' may be False and we still need to change the
	  * core width
	  *
	  * - if they changed the showNChars (only evaluated if RESIZE())
	  * - if they switched policy (from !RESIZE to RESIZE).
	  */
     ntfw->core.width = (NTF.showNChars > 0 ?
							NTF.showNChars*NTF.font->max_bounds.width :
							XTextWidth(NTF.font,NTF.buffer,strlen(NTF.buffer)))
			  +LABEL_WIDTH(ntfw)
		      +2*(SHADOW_WIDTH(ntfw)+WIDTH+NTF.internalWidth);
     NTF.visible_end=nlen;
     NTF.visible_start=0;
   }
   adj_right(ntfw); adj_left(ntfw);

   /* handle changes to modified_by_user */
   if (NTF.modified_by_user != TF.modified_by_user) {
		if (NTF.modified_by_user) {
			/* they must not set it 'True' */
			NTF.modified_by_user=False;
		} else {
			if (NTF.auto_blink || TF.auto_blink) blinkerOff(ntfw);
			if (NTF.backup) {
				XtFree(NTF.backup);
				NTF.backup = (String)0;
			}
			if ( ! new_string ) reason = tfwCommitted;
		}
   }

   /* handle changes to the blinker */

   /* range checking of the timer values.
	* Note: we don't bother resetting running timers. The new
	*		values will take effect only the next time the
	*		blink timer is triggered.
	*/
   if (NTF.auto_blink_wait_time < 100) NTF.auto_blink_wait_time = 100;
   if (NTF.blink_on_time < 100) NTF.blink_on_time = 100;
   if (NTF.blink_off_time < 100) NTF.blink_off_time = 100;

   /* no need to check new autoBlink value */

   /* blinker value changed */
   if (NTF.blink != TF.blink) {
		if (NTF.blink && ntfw->core.visible && ! NTF.blink_timer) { /* start blinker */
			NTF.blink_timer = XtAppAddTimeOut(XtWidgetToApplicationContext(super),
												(unsigned long)NTF.blink_on_time,
												blinkerCallback,
												(XtPointer)ntfw);
   		}
		if ( ! NTF.blink ) blinkerOff(ntfw);
   }

   /* 
	* Notify them that the string was changed or committed.
	*/
   if (tfwNone != reason) {
		Boolean tmp = NTF.modified_by_user;

		/* we want them to see the last value */
		NTF.modified_by_user=TF.modified_by_user;
   		XtCallCallbackList((Widget)ntfw,NTF.modified_cbl,(XtPointer)reason);
		NTF.modified_by_user=tmp;
   }


   /* general checks if we need an exposure event */
   if (		NTF.visible_end != TF.visible_end
		||  NTF.visible_start != TF.visible_start
		||  NTF.text_cursor != TF.text_cursor
		||	new_string
		||  ntfw->core.width != tfw->core.width
		||  ntfw->core.height != tfw->core.height
		||  adjust )
		needsExpose = True;

   return needsExpose;
/* the following does not work correctly; no exposure events
 * are generated, if parent does not allow the new
 * geometry
 * return((ntfw->core.width==tfw->core.width)&&
 *        (ntfw->core.height==tfw->core.height));
 */
}
#undef NTF

static XtGeometryResult QueryGeometry
#if NeedFunctionPrototypes
(Widget w, XtWidgetGeometry *proposed, XtWidgetGeometry *answer)
#else
(w,proposed,answer)
Widget w;
XtWidgetGeometry *proposed, *answer;
#endif
{
 TextFieldWidget tfw=(TextFieldWidget)w;
 Boolean ok=False;
 answer->request_mode=CWWidth|CWHeight;

 answer->width=(TF.showNChars > 0 ?
			   TF.showNChars * TF.font->max_bounds.width :
			   XTextWidth(TF.font,TF.buffer,strlen(TF.buffer)))
		+ LABEL_WIDTH(tfw)
		+ 2*( SHADOW_WIDTH(tfw)+TF.internalWidth+WIDTH);

 answer->height= MAX_FONTH(tfw)+2*(SHADOW_WIDTH(tfw)+TF.internalHeight);

 if ((proposed->request_mode&CWWidth) &&
      proposed->width==answer->width) ok=True;

 if ((proposed->request_mode&CWHeight) && proposed->width!=answer->width) ok=False;
 
 if (ok) 				return(XtGeometryYes);

 if (answer->width==tfw->core.width &&
     answer->height==tfw->core.height)  return(XtGeometryNo);
 else					return(XtGeometryAlmost);
}


/*ARGSUSED*/
static Boolean AcceptFocus
#if NeedFunctionPrototypes
(Widget widget, Time time)
#else
(widget,time)
Widget widget;
Time   time;
#endif
{
/*
 * XtSetKeyboardFocus(XtParent(widget),widget);
 *
 * Doesn't work; The composite parent must
 * call XtSetKeyboardFocus. I dont want to use
 * XSetInputFocus(), since it interferes with 
 * XtSetKeyboardFocus.
 *
 * The Xt programmers manual is not consistent about
 * this method. Somewhere I read the method itself has
 * to set the KB focus, somewhere else was said, that
 * the method only tells the parent if the widget is 
 * willing to accept the focus. The latter is what we do.
 */
 return(XtIsSensitive(widget)&&XtIsManaged(widget));
}

/* Public Procedures */

/* convert the text string to the type indicated
 * by the valueType resource (valueSize must be set correctly)
 * and store the converted value at address valueAddress
 */

Boolean TextFieldString2Value
#if NeedFunctionPrototypes
(Widget w)
#else
(w)
Widget w;
#endif
{
 TextFieldWidget tfw=(TextFieldWidget)w;
 XrmValue from;
 if (! XtIsSubclass(w,textFieldWidgetClass)) {
   XtWarning("TextFieldString2Value: operand not a subclass of 'TextField'");
 }
 if (TF.value.addr==NULL) {
  XtWarning("TextFieldString2Value: valueAddress == NULL");
  return(False);
 }
 from.size=strlen(from.addr=TF.buffer);
 return(XtConvertAndStore((Widget)tfw,XtRString,&from,TF.value_type,&TF.value));
}

Boolean TextFieldValue2String
#if NeedFunctionPrototypes
(Widget w)
#else
(w)
Widget w;
#endif
{
 TextFieldWidget tfw=(TextFieldWidget)w;
 XrmValue to;
 Boolean  rval;
 int      len;

 if (! XtIsSubclass(w,textFieldWidgetClass)) {
   XtWarning("TextFieldValue2String: operand not a subclass of 'TextField'");
 }
 if (TF.value.addr==NULL) {
  XtWarning("TextFieldValue2String: valueAddress == NULL");
  return(False);
 }
 to.size=TF.maxlen; to.addr=TF.buffer;

 if (0!=(rval=XtConvertAndStore((Widget)tfw,
		TF.value_type,&TF.value,XtRString,&to))) {
   Unselect(tfw,(Atom*)0);
   len=strlen(TF.buffer);
   if (TF.text_cursor>len) TF.text_cursor=len;
   TF.visible_start=0; TF.visible_end=len;
   
   if (!resize_width(tfw)) {
     if (TF.justify==XtJustifyRight) {
       adj_left (tfw);
       adj_right(tfw);
     } else {
       adj_right(tfw);
       adj_left (tfw);
     }
   }
   ReDisplay(tfw,True);
 }
 return(rval);
}

/* Type converters */

static Boolean CvtByFormat
#if NeedFunctionPrototypes
(Display *di, XrmValuePtr args, Cardinal *nargs,
 XrmValuePtr from, XrmValuePtr to, XtPointer *my_data)
#else
(di,args,nargs,from,to,my_data)
Display 	*di;
XrmValuePtr	args;
Cardinal	*nargs;
XrmValuePtr	from;
XrmValuePtr	to;
XtPointer	*my_data;
#endif
{
 String  fmt;
 Boolean rval,fromString;
 char    mess[256];
 int	 typ=0;

 int     iarg=0;
 long	 larg=0;
 double  darg=0;

 if (*nargs != 2) {
  XtErrorMsg("wrong Parameters", "CvtByFormat",
	     "XtToolkitError",
	     "String to CFormat conversion needs 1 extra arg",
	     (String*)NULL,(Cardinal*)NULL);
 }
 (void)sprintf(mess,"CvtByFormat: ");

 fmt=*(String*)args[0].addr;
 fromString=(Boolean)(unsigned)(*(XtPointer*)args[1].addr);
 rval=True;

 if (fmt==NULL) {
  (void)sprintf(mess+13,"%s","no format string");
  XtWarning(mess); rval=False;
 } else if ((typ=strlen(fmt)-1)>80) {
  (void)sprintf(mess+13,"%s","format too long");
  XtWarning(mess); rval=False;
 } else if (fmt[0]!='%') {
  (void)sprintf(mess+13,"%s","possibly no format (missing %%)");
  XtWarning(mess); rval=False;
 }
 
 
 if (from->addr==NULL) {
  (void)sprintf(mess+13,"%s","no source address");
  XtWarning(mess); rval=False;
 } 
 if (to->addr==NULL) {
  (void)sprintf(mess+13,"%s","no target address");
  XtWarning(mess); rval=False;
 } 
 if (rval) 
  if (fromString) {
   rval=(sscanf(from->addr,fmt,to->addr)==1);
  } else
   switch(fmt[typ]) {
    case 'c':
    case 'd':
    case 'o':
    case 'u':
    case 'x':
    case 'X':  if (from->size==sizeof(char)) {
			if   (fmt[typ-1]=='l')	larg=(long)(*((char*)from->addr));
		   	else			iarg=(int) (*((char*)from->addr));
		} else if (from->size==sizeof(short)) {
			if   (fmt[typ-1]=='l')	larg=(long)(*((short*)from->addr));
		   	else			iarg=(int) (*((short*)from->addr));
		} else if (from->size==sizeof(int)) {
			if   (fmt[typ-1]=='l')	larg=(long)(*((int*)from->addr));
		   	else			iarg=(int) (*((int*)from->addr));
		} else if (from->size==sizeof(long)) {
			if   (fmt[typ-1]=='l')	larg=(long)(*((long*)from->addr));
		   	else			rval=False;
		} else {
			rval=False;
	       }
	       if (rval) {
                 if (fmt[typ-1]=='l') (void)sprintf(to->addr,fmt,larg);
                 else		      (void)sprintf(to->addr,fmt,iarg);
		
		 if (strlen(to->addr) > to->size) { /* !! String overflow !! */
  			XtErrorMsg("buffer overflow", "CvtByFormat",
	     			"XtToolkitError",
	     			"The target string is too small",
	     			(String*)NULL,(Cardinal*)NULL);

		 }

	       } 
	       break;

    case 'f':
    case 'e':
    case 'E':
    case 'g':
    case 'G':  if (from->size==sizeof(float)) {
		  darg=(double)(* ((float*)from->addr));
		} else if (from->size==sizeof(double)) {
		  darg=(double)(* ((double*)from->addr));
		} else {
			rval=False;
	       }
	       if (rval) {
		(void)sprintf(to->addr,fmt,darg);
		if (strlen(to->addr) > to->size) { /* !! String overflow !! */
  			XtErrorMsg("buffer overflow", "CvtByFormat",
	     			"XtToolkitError",
	     			"The target string is too small",
	     			(String*)NULL,(Cardinal*)NULL);

		}
	       }
	       break;

    default:   (void)sprintf(mess+13,"%s","wrong conversion character");
  	       XtWarning(mess); rval=False;
	       break;
 }

 if (!rval) {
  if (fromString) {
    (void)sprintf(mess,"%s using format '%s'",XtRCFormat,fmt);
    XtDisplayStringConversionWarning(di,(char*)from->addr,mess);
  } else {
    (void)sprintf(mess,"Cannot convert type %s to string %s using format %s",
	XtRCFormat,(char*)from->addr,fmt);
    XtWarning(mess);
  }
 }
 return(rval);
}

#ifdef TEXT_COMPAT
/* Do this to be compatible with the Text widget's 'resize' resource */
/* ARGSUSED */
static Boolean CvtStringToResizeMode
#if NeedFunctionPrototypes
(Display *di, XrmValuePtr args, /* unused */ Cardinal *num_args,
 /* unused */ XrmValuePtr fromVal, XrmValuePtr toVal, XtPointer *my_data)
#else
(di, args, num_args, fromVal, toVal, my_data)
Display			*di;
XrmValuePtr 	args;		/* unused */
Cardinal		*num_args;	/* unused */
XrmValuePtr		fromVal;
XrmValuePtr		toVal;
XtPointer		*my_data;
#endif
{
  static XawTextResizeMode resizeMode;
  static  XrmQuark  QResizeNever, QResizeWidth, QResizeHeight, QResizeBoth;
  XrmQuark    q;
  char        lowerName[BUFSIZ];
  static Boolean inited = FALSE;
    
  if ( !inited ) {
    QResizeNever      = XrmPermStringToQuark(XtEtextResizeNever);
    QResizeWidth      = XrmPermStringToQuark(XtEtextResizeWidth);
    QResizeHeight     = XrmPermStringToQuark(XtEtextResizeHeight);
    QResizeBoth       = XrmPermStringToQuark(XtEtextResizeBoth);
    inited = TRUE;
  }

  XmuCopyISOLatin1Lowered (lowerName, (char *)fromVal->addr);
  q = XrmStringToQuark(lowerName);

  if      (q == QResizeNever)          resizeMode = XawtextResizeNever;
  else if (q == QResizeWidth)          resizeMode = XawtextResizeWidth;
  else if (q == QResizeHeight)         resizeMode = XawtextResizeHeight;
  else if (q == QResizeBoth)           resizeMode = XawtextResizeBoth;
  else {
    return False;
  }
  toVal->size=sizeof(XawTextResizeMode);
  toVal->addr=(XtPointer)&resizeMode;
  return True;
}
#endif

/* Event handlers */

/* we need an event handler to workaround a bug (or is this considered a feature?)
 * in Core. If a widget is unmapped, 'visible' is not set to false apparently.
 */

/*ARGSUSED*/
static void handleVisibilityEvents
#if NeedFunctionPrototypes
(Widget w, XtPointer cld, XEvent *ev, Boolean *ctd)
#else
(w, cld, ev, ctd)
Widget		w;
XtPointer	cld;
XEvent		*ev;
Boolean		*ctd;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;

	if (ev->xvisibility.state != VisibilityFullyObscured) {
		w->core.visible = True;
		/* check if the blinker needs to be restarted */
		if (TF.blink && ! TF.blink_timer) {
			TF.blink_timer = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
									(unsigned long)TF.blink_on_time,
									blinkerCallback,
									(XtPointer)tfw);
		}
	} else {
		w->core.visible = False;
	}
}

/* Blinker Support */

/* helper routines */

/* toggle the string state and paint it */
static void blinkerToggle
#if NeedFunctionPrototypes
(TextFieldWidget tfw)
#else
(tfw)
TextFieldWidget tfw;
#endif
{
	TF.blink_state_on = ! TF.blink_state_on;
	/* clear if the new state is 'off' */
	ReDisplay(tfw, ! TF.blink_state_on);
}

static void blinkerOff
#if NeedFunctionPrototypes
(TextFieldWidget tfw)
#else
(tfw)
TextFieldWidget tfw;
#endif
{
	if (TF.blink_timer) {
		XtRemoveTimeOut(TF.blink_timer);
		TF.blink_timer = (XtIntervalId)0;
	}
	TF.blink=False;
	/* if the string was off, we need a redisplay */
	if (!TF.blink_state_on) blinkerToggle(tfw);
}

static void autoBlinkerOn
#if NeedFunctionPrototypes
(TextFieldWidget tfw)
#else
(tfw)
TextFieldWidget tfw;
#endif
{
	/* cancel running timer */
	if (TF.blink_timer) {
		XtRemoveTimeOut(TF.blink_timer);
	}
	TF.blink=True;
	/* if the string is switched off, display it */
	if (!TF.blink_state_on) blinkerToggle(tfw);

	/* restart the auto timeout */
	TF.blink_timer = tfw->core.visible ?
						XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)tfw),
										(unsigned long)TF.auto_blink_wait_time,
										blinkerCallback,
										(XtPointer)tfw):	
						(XtIntervalId)0;
}

static void blinkerCallback
#if NeedFunctionPrototypes
(XtPointer cld, XtIntervalId *id)
#else
(cld, id)
XtPointer		cld;
XtIntervalId	*id;
#endif
{
TextFieldWidget tfw =(TextFieldWidget)cld;

	/* if we are not visible, we don't blink
	 * 
	 * just reset the timer id, event handler
	 * will restart blinking.
	 */
	if ( ! tfw->core.visible ) {
		TF.blink_state_on = True; /* at first, the string is shown */
		TF.blink_timer = 0;
	} else {
		unsigned long timeout = TF.blink_state_on ? TF.blink_on_time : TF.blink_off_time;
		/* toggle the state */
		blinkerToggle(tfw);
		/* retrigger the timeout */
		TF.blink_timer = XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)tfw),
											timeout,
											blinkerCallback,
											cld);
	}
}

/* the backup/restore facility */
static void Backup
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
(w, ev, args, nargs)
Widget w; XEvent *ev; String *args; Cardinal *nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
if (! TF.backup) {
	TF.backup = XtMalloc(TF.maxlen+1);
	TF.backup[TF.maxlen]=(char)0;
}
strncpy(TF.backup,TF.buffer,TF.maxlen);
}

static void Restore
#if NeedFunctionPrototypes
(Widget w, XEvent *ev, String *args, Cardinal *nargs)
#else
(w, ev, args, nargs)
Widget w; XEvent *ev; String *args; Cardinal *nargs;
#endif
{
TextFieldWidget tfw=(TextFieldWidget)w;
	if (!TF.backup) return;

	if ( ! strcmp(TF.buffer,TF.backup) ) {
		if (TF.modified_by_user) {
			/* notify them, that the string was 'restored by typing' */
   			XtCallCallbackList(w,TF.modified_cbl,(XtPointer)tfwStringRestored);
		} else {
			/* should not get here */
			XtFree(TF.backup); TF.backup=0;
			return;
		}
	}

	/* ok, the strings differ; so we must call 'SetValues' to change
	 * it and update all internal state.
	 */

	if (TF.use_string_in_place) {
		/* need to copy it first */
		strncpy(TF.buffer,TF.backup,TF.maxlen);
		TF.buffer[TF.maxlen-1]=0;
		XtVaSetValues(w,XtNstring,TF.buffer,0);
	} else {
		XtVaSetValues(w,XtNstring,TF.backup,0);
	}

	TF.modified_by_user = False;
	if (TF.auto_blink) blinkerOff(tfw);
	ReDisplay(tfw,True);
}

#undef TF
#undef SetPoint
