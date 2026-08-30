#ifndef _FileList_h
#define _FileList_h
/*
 *  Enhancements to the X-File Manager XFM-1.3.2 (The FileList Widget)
 *  ------------------------------------------------------------------
 
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

#include "FmStringDefs.h"

/****************************************************************
 *
 * FileList widget
 *
 ****************************************************************/

/* Resources:

 Name		     Class			RepType		Default Value
 ----		     -----			-------		-------------
 font		     Font			FontStruct	XtDefaultFont
 files		     Files			FileList	0
 nFiles		     NFiles			Int		-1
 foreground	     Foreground		Pixel		XtDefaultForeground
 highlightColor	 HighlightColor	Pixel		XtDefaultForeground
 borderWidth	 BorderWidth	Dimension	2
 userData	     UserData	    Pointer		NULL
 callback	     Callback		Callback	NULL
 dragTimeout	 DragTimeout    Int			200

*/


/* declare specific FileListWidget class and instance datatypes */

typedef struct _FileListClassRec*	FileListWidgetClass;
typedef struct _FileListRec*		FileListWidget;

/* declare the class constant */

extern WidgetClass fileListWidgetClass;

/* Actions
 *
 * dispatchDirExeFile(  <dirActionName>, <exeActionName>, <fileActionName>,
 *		        <noneActionName>,
 *			[optional args to specific action] )
 *
 * 	dispatch the event to a file type dependent action procedure or
 * 	to <noneActionName> if the event was triggered when the pointer
 * 	was not on a file item.
 *
 * dispatchDirExeFilePopup(..)
 *
 * 	identical to dispatchDirExeFile() but registered as a grab action; use
 * 	this to popup menus.
 *
 * notify(arg)
 *
 *	the callbacks on the callbacklist given by resource XtNcallback are
 *      called with the 'callData' parameter set to point to the argument string 
 *      (or NULL when there was no argument).
 */


/* get the position where ev occurred */
void FileListEventPosition(XEvent *, int *x, int *y);

/* find the entry where ev occurred */
int FileListFindEntry(FileListWidget, XEvent*);

/* get the position of the upper left corner of an entry as
 * well as its width and height 
 */
void FileListEntryPosition(
	FileListWidget flw,
	int entry,
	Position *x, Position *y,
	Dimension *width, Dimension *height);

/* redraw one entry.  */
void FileListRefreshItem(FileListWidget flw, int entry);

#endif /* _FileList_h */
