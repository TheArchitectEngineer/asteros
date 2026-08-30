#ifndef _FileListP_h
#define _FileListP_h
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

#include "Fm.h"
#include "FileList.h"
/* include superclass private header file */
#include <X11/CoreP.h>

typedef struct {
    int (*findEntry)(FileListWidget,int,int,XEvent*);
    void (*entryPosition)(FileListWidget,
			  int,
			  Position *,Position *,
			  Dimension*,Dimension*);
} FileListClassPart;

typedef struct _FileListClassRec {
    CoreClassPart	core_class;
    FileListClassPart	fileList_class;
} FileListClassRec;

extern FileListClassRec fileListClassRec;

typedef struct {
    /* resources */
    XFontStruct 	*font;
    FileList		files;
    int			n_files;
    Pixel		foreground,highlight_pixel;
    Dimension		border_width; 
    XtPointer		user_data;
    XtCallbackList	notify_cbl;
    int			drag_timeout;
    /* private state */
    int			name_w;
    int			pointer_entry;
    int			enter_x,enter_y;
    GC			gc_norm,gc_invert,gc_highlight;
    XtIntervalId	timeout_id;
    Boolean		timer_running;
} FileListPart;

typedef struct _FileListRec {
    CorePart		core;
    FileListPart	fileList;
} FileListRec;

#endif /* _FileListP_h */
