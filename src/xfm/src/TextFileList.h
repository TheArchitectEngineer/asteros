#ifndef _TextFileList_h
#define _TextFileList_h
/*
 *  Enhancements to the X-File Manager XFM-1.3.2 (The TextFileList Widget)
 *  ----------------------------------------------------------------------
 
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
 * TextFileList widget
 *
 ****************************************************************/

#include "FileList.h"

/* Resources:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 entrySep	     EntrySep		int		2
 topSep 	     TopSep		int		2
 leftSep	     LeftSep		int		2
 tabSep		     TabSep		int		2
 showInode	     ShowInode		Boolean		True
 showType	     ShowType		Boolean		True
 showPermission	     ShowPermission	Boolean		True
 showLinks	     ShowLinks		Boolean		True
 showUser	     ShowUser		Boolean		True
 showGroup	     ShowGroup		Boolean		True
 showSize	     ShowSize		Boolean		True
 showMTime	     ShowMTime		Boolean		True

*/

/* declare specific TextFileListWidget class and instance datatypes */

typedef struct _TextFileListClassRec*	TextFileListWidgetClass;
typedef struct _TextFileListRec*		TextFileListWidget;

/* declare the class constant */

extern WidgetClass textFileListWidgetClass;

#endif /* _TextFileList_h */
