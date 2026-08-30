#ifndef FMHISTORY_H
#define FMHISTORY_H

/*
 *  Enhancements to the X-File Manager XFM-1.3.2 (Path History)
 *  -----------------------------------------------------------
 
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

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

/* Create a menu (Class SimpleMenu) with recently used paths
 * the last path in use appears first in the list.
 *
 * On top some fixed paths (eg. $HOME) can be displayed. These strings
 * are given as a NULL terminated list to FmCreateHistoryList();
 *
 * the menu is named "fm_history", if no name is specified. This name
 * can be used in translation tables in order to popup the menu.
 *
 */

typedef struct HistoryList_s *HistoryList;

HistoryList FmCreateHistoryList(char* name, Widget parent, char *fixed_paths[]);

/* insert path on top */
void FmInsertHistoryPath(HistoryList hl, char *path);

/* delete entry (no action if path not found in list) */
void FmDeleteHistoryPath(HistoryList hl, char *path);

/* chop the list to n entries */
void FmChopHistoryList(HistoryList hl, int n);

/* destroy a list */
void FmDestroyHistoryList(HistoryList hl);
#endif
