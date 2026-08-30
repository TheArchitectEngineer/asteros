#ifndef _IconFileListP_h
#define _IconFileListP_h
/*
 *  Enhancements to the X-File Manager XFM-1.3.2 (The IconFileList Widget)
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

#include "Fm.h"
#include "IconFileList.h"
/* include superclass private header file */
#include "FileListP.h"

typedef struct {
    int empty;
} IconFileListClassPart;

typedef struct _IconFileListClassRec {
    CoreClassPart		core_class;
    FileListClassPart		fileList_class;
    IconFileListClassPart	iconFileList_class;
} IconFileListClassRec;

extern IconFileListClassRec iconFileListClassRec;

typedef struct {
    /* resources */
    int			entry_sep,left,top,labelsep;
    int			user_n_horiz,min_icon_width,min_icon_height;
    /* private state */
    int			entry_height;
    int			entry_width;
    int			liney;
    int			hil_entry;
    int			n_horiz;
} IconFileListPart;

typedef struct _IconFileListRec {
    CorePart		core;
    FileListPart	fileList;
    IconFileListPart	iconFileList;
} IconFileListRec;

#endif /* _IconFileListP_h */
