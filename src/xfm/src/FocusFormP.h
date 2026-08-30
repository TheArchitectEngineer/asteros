#ifndef _FocusFormP_h
#define _FocusFormP_h
/*
 *  $id$
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
 *  $Log: FocusFormP.h,v $
 *  Revision 1.1.1.1.4.1  2000/02/23 16:21:52  till
 *   - merged albert graef's changes from p3 to p4
 *
 *  Revision 1.2  1998/04/19 11:00:25  till
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

#include "FocusForm.h"
#include <X11/Xaw/FormP.h>

typedef struct _FocusFormClassRec {
    CoreClassPart	core_class;
    CompositeClassPart	composite_class;
    ConstraintClassPart	constraint_class;
    FormClassPart	form_class;
} FocusFormClassRec;

extern FocusFormClassRec focusFormClassRec;


typedef struct _FocusFormPart {
			/* which child you want to give the focus first
			 * (can only be evaluated _after_ children have
			 * been created)
			 */
    String			focus_init_child;
/* private */
    Widget			focus_child; /* This _has_ the focus */
    Boolean			initialized;
} FocusFormPart;

typedef struct _FocusFormRec {
    CorePart		core;
    CompositePart	composite;
    ConstraintPart	constraint;
    FormPart		form;
    FocusFormPart	focusForm;
} FocusFormRec;

typedef struct _FocusFormConstraintsPart {
  Boolean focus_interest;
  Boolean sensitive;		/* try to override core resource */
} FocusFormConstraintsPart;

typedef struct _FocusFormConstraintsRec {
    FormConstraintsPart	     form;
    FocusFormConstraintsPart focusForm;
} FocusFormConstraintsRec, *FocusFormConstraints;

#endif /* _XawFormP_h */
