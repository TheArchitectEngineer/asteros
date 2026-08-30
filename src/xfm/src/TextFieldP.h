#ifndef _TextFieldP_h
#define _TextFieldP_h

/*
 *  $Id: TextFieldP.h,v 1.1.1.1.4.1 2000/02/23 16:21:52 till Exp $
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
 *  $Log: TextFieldP.h,v $
 *  Revision 1.1.1.1.4.1  2000/02/23 16:21:52  till
 *   - merged albert graef's changes from p3 to p4
 *
 *  Revision 1.8  1999/10/28 18:09:10  cvs
 *   - userData needs #ifndef, not #ifdef!!!
 *
 *  Revision 1.7  1999/10/22 23:32:42  cvs
 *   - added the showNChars resource.
 *
 *  Revision 1.6  1999/10/21 15:28:16  cvs
 *   - added support for a label to TextField.
 *
 *  Revision 1.5  1999/02/05 17:41:54  till
 *   - added modifiedCallback reason (callData parameter).
 *
 *  Revision 1.4  1999/02/04 23:02:11  cvs
 *  - 	Added blinker / modifiedByUser.
 *  	Added the 'autoBlink' 'Commit()' action feature.
 *
 *  Revision 1.3  1998/10/30 22:18:58  till
 *  -fixed some bugs in TextField
 *  -C++ compliance for TextField.h
 *  -replace selected part of string
 *
 *  Revision 1.2  1998/04/19 11:00:25  till
 *  started cvs logging
 *

 */

#include "TextField.h"

#ifdef TEXT_COMPAT
#include <X11/Xaw/TextP.h> /* need XtNRResizeMode */
#endif

/* include superclass private header file */

#ifdef XAW3D
#include <X11/Xaw3d/ThreeDP.h>
#else
#include <X11/Xaw/SimpleP.h>
#endif

/* define unique representation types not found in <X11/StringDefs.h> */

typedef struct {
    int empty;
} TextFieldClassPart;

typedef struct _TextFieldClassRec {
    CoreClassPart	core_class;
    SimpleClassPart	simple_class;
#ifdef XAW3D
    ThreeDClassPart	threeD_class;
#endif
    TextFieldClassPart	textField_class;
} TextFieldClassRec;

extern TextFieldClassRec textFieldClassRec;

#ifdef TEXT_COMPAT
typedef XawTextResizeMode TfResizeMode;
#else
typedef union {
	Boolean b;
	enum { a,b,c,d } pad;
} TfResizeMode;
#endif

typedef struct {
    /* resources */
    XFontStruct*	font;
    Pixel			foreground;
    Pixel			insensitive_foreground;
    Dimension		internalHeight;
    Dimension		internalWidth;
    TfTextPosition	text_cursor;
    TfResizeMode	resize;
    String			string;
    Boolean			use_string_in_place;
    XtJustify		justify;
    int				maxlen;
    Boolean			readonly;
    short			scroll_chars;
    XtCallbackList	notify_cbl,modified_cbl;
    XrmValue		value;
    String			value_type;
    String			input_format;
    String			output_format;
    Boolean			display_caret;
	Boolean			modified_by_user;
	Boolean			blink;
	Boolean			auto_blink;
	int				auto_blink_wait_time; /* note: time in ms for XtTimers is unsigned long.
									   *	   There is no such rep. type however.
									   */
	int				blink_on_time;
	int				blink_off_time;
	Boolean			leftLabel; /* paint the label at the left or right ? */
    XFontStruct*	lblFont;
	String			label;
	int				labelSep;
	Dimension		labelWidth;
	XtJustify		lblJustify;
	int				showNChars;
#ifndef XAW3D
	XtPointer		userData;
#endif


    /* private state */

    GC				lbl,norm, norm_hil, insens, insens_hil;
    short			visible_start,visible_end;
    unsigned long	selections;
    String			buffer;
    short			hil_start,hil_end;
    short			multiclick;
    Time			multiclick_time;
    Boolean			active_cursor;
	XtIntervalId 	blink_timer;
	Boolean			blink_state_on;
	String			backup;
} TextFieldPart;

typedef struct _TextFieldRec {
    CorePart		core;
    SimplePart		simple;
#ifdef XAW3D
    ThreeDPart		threeD;
#endif
    TextFieldPart	textField;
} TextFieldRec;

#endif /* _TextFieldP_h */
