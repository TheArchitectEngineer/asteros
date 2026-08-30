#ifndef _TextFileListP_h
#define _TextFileListP_h
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

#include "Fm.h"
#include "TextFileList.h"
/* include superclass private header file */
#include "FileListP.h"

typedef struct {
    int empty;
} TextFileListClassPart;

typedef struct _TextFileListClassRec {
    CoreClassPart		core_class;
    FileListClassPart		fileList_class;
    TextFileListClassPart	textFileList_class;
} TextFileListClassRec;

extern TextFileListClassRec textFileListClassRec;

typedef struct {
    /* resources */
    int			entry_sep,left,top,tabsep;
    Boolean		ino_s,dev_s,nlink_s,umode_s,uid_s,gid_s,size_s,time_s;
    /* private state */
    int			entry_height;
    int			hil_entry;
    int			ino_w,dev_w,nlink_w,umode_w,uid_w,gid_w,size_w,time_w;
    int			perm_w[9];
#ifdef VIEWPORT_HACK
    int			hack;
#endif
} TextFileListPart;

typedef struct _TextFileListRec {
    CorePart		core;
    FileListPart	fileList;
    TextFileListPart	textFileList;
} TextFileListRec;

#endif /* _TextFileListP_h */
