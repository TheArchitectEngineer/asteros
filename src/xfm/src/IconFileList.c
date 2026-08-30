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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include "IconFileListP.h"

static XtResource il_resources[] = {
#define offset(field) XtOffsetOf(IconFileListRec, iconFileList.field)
    /* {name, class, type, size, offset, default_type, default_addr}, */
    { XtNentrySep, XtCEntrySep, XtRInt, sizeof(int),
	offset(entry_sep), XtRImmediate, (XtPointer)2},
    { XtNtopSep, XtCTopSep, XtRInt, sizeof(int),
	offset(top), XtRImmediate, (XtPointer)2},
    { XtNleftSep, XtCLeftSep, XtRInt, sizeof(int),
	offset(left), XtRImmediate, (XtPointer)2},
    { XtNlabelSep, XtCLabelSep, XtRInt, sizeof(int),
	offset(labelsep), XtRImmediate, (XtPointer)2},
    { XtNnHoriz, XtCNHoriz, XtRInt, sizeof(int),
	offset(user_n_horiz), XtRImmediate, (XtPointer)0},
    { XtNminIconWidth, XtCMinIconWidth, XtRInt, sizeof(int),
	offset(min_icon_width), XtRImmediate, (XtPointer)0},
    { XtNminIconHeight, XtCMinIconHeight, XtRInt, sizeof(int),
	offset(min_icon_height), XtRImmediate, (XtPointer)0},
#undef offset
};

/* Methods */
static void ilInitialize(Widget,Widget,ArgList,Cardinal*);
static void ilExpose(Widget,XEvent*,Region);
static void ilResize(Widget);
static Boolean ilSetValues(Widget,Widget,Widget,ArgList,Cardinal*);
static XtGeometryResult ilQueryGeometry(Widget,XtWidgetGeometry*,XtWidgetGeometry*);
static int  ilFindEntry(FileListWidget,int,int,XEvent*);
static void ilEntryPosition(FileListWidget,int,Position*,Position*,Dimension*,Dimension*);

/* private procedures */
static void drawPart(IconFileListWidget,int,int,int,int);

/* Actions */
static void highlight(Widget, XEvent*, String*, Cardinal*);
static void unhighlight(Widget, XEvent*, String*, Cardinal*);


static XtActionsRec actions[] =
{
  /* {name, procedure}, */
    {"highlight",	highlight},
    {"unhighlight",	unhighlight},
};

static char translations[] = "";
/*
"<Key>:		highlight()	\n\
";
*/

IconFileListClassRec iconFileListClassRec = {
  { /* core fields */
    /* superclass		*/	(WidgetClass) &fileListClassRec,
    /* class_name		*/	"IconFileList",
    /* widget_size		*/	sizeof(IconFileListRec),
    /* class_initialize		*/	NULL,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	ilInitialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	XtInheritRealize,
    /* actions			*/	actions,
    /* num_actions		*/	XtNumber(actions),
    /* resources		*/	il_resources,
    /* num_resources		*/	XtNumber(il_resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	XtExposeCompressMultiple,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	NULL,
    /* resize			*/	ilResize,
    /* expose			*/	ilExpose,
    /* set_values		*/	ilSetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	translations,
    /* query_geometry		*/	ilQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
  { /* FileList fields */
    /* findEntry		*/	ilFindEntry,
    /* entryPosition		*/	ilEntryPosition
  },
  { /* iconFileList fields */
    /* empty			*/	0
  }
};

WidgetClass iconFileListWidgetClass = (WidgetClass)&iconFileListClassRec;

#define IL (ilw->iconFileList)
#define FL (ilw->fileList)

static void ilInitialize(treq,tnew,args,n_args)
 Widget	  treq,tnew;
 ArgList  args;
 Cardinal *n_args;
{
 int  i,width,height,tmp,bm_w,bm_h,bm_wmax,bm_hmax;
 IconFileListWidget ilw=(IconFileListWidget)tnew;
 TypeRec *type;

 IL.hil_entry=-1;

 
 bm_wmax=IL.min_icon_width;
 bm_hmax=IL.min_icon_height;
 for (i=0; i<FL.n_files; i++) {
    type=FL.files[i]->type;
    if ((bm_w=type->bm_width) > bm_wmax) bm_wmax=bm_w;
    if ((bm_h=type->bm_height) > bm_hmax) bm_hmax=bm_h;
 }
 bm_wmax+=2*FL.border_width;
 IL.entry_width=IL.entry_sep+
	(FL.name_w>bm_wmax?FL.name_w:bm_wmax);

 IL.liney=bm_hmax+IL.labelsep+FL.font->max_bounds.ascent+FL.border_width;

 IL.entry_height=IL.liney+FL.font->max_bounds.descent+
	FL.border_width+IL.entry_sep;

 if (IL.user_n_horiz==0) {
   if (ilw->core.width==0) {
     if (ilw->core.height==0) {
       IL.n_horiz=3;
       XtAppWarning(XtWidgetToApplicationContext(tnew),
	  "IconFileListWidget: width, height and nHoriz==0 (set to 3)");
     } else {
       tmp=((int)ilw->core.height-2*IL.top+IL.entry_sep)/IL.entry_height;
       if (tmp==0) tmp=1;
       IL.n_horiz=FL.n_files/tmp;
     }
   } else {
     IL.n_horiz=((int)ilw->core.width-2*IL.left+IL.entry_sep)/IL.entry_width;
     if (IL.n_horiz==0) IL.n_horiz=1;
   }
 } else {
   IL.n_horiz=IL.user_n_horiz;
 }

 tmp=FL.n_files/IL.n_horiz +1;
 width=2*IL.left+IL.entry_width*IL.n_horiz-IL.entry_sep;
 height=2*IL.top+tmp*IL.entry_height-IL.entry_sep;

 if ((int)ilw->core.width<width) ilw->core.width=width;
 if ((int)ilw->core.height<height) ilw->core.height=height;
}

static void ilResize(Widget w)
{
 IconFileListWidget ilw=(IconFileListWidget)w;
 if (IL.user_n_horiz==0) {
   IL.n_horiz=((int)ilw->core.width-2*IL.left+IL.entry_sep)/IL.entry_width;
   if (IL.n_horiz==0) IL.n_horiz=1;
 }
}

static void ilExpose(Widget w,XEvent *ev,Region reg)
{
 IconFileListWidget ilw=(IconFileListWidget)w;
 int xmin,ymin,xmax,ymax;
 if (ev->xexpose.count>0) return;
 xmin=(ev->xexpose.x-IL.left)/IL.entry_width;
 if (xmin<0) xmin=0;
 if (xmin>=IL.n_horiz) xmin=IL.n_horiz-1;
 ymin=(ev->xexpose.y-IL.top)/IL.entry_height;
 if (ymin<0) ymin=0;
 xmax=(ev->xexpose.x+ev->xexpose.width-IL.top)/IL.entry_width;
 if (xmax>=IL.n_horiz) xmax=IL.n_horiz-1;
 if (xmax<0) xmax=0;
 ymax=(ev->xexpose.y+ev->xexpose.height-IL.top)/IL.entry_height;
 if (ymax<0) ymax=0;

 drawPart(ilw,xmin,xmax,ymin,ymax);
}

static void drawPart(IconFileListWidget ilw, 
	int xmin, int xmax, int ymin, int ymax)
{
 int index,i,j,left,len,ulx,uly,bm_h,bm_w;
 unsigned int rectw, recth,w,h;
 FileRec *file;
 int recty,rectx;
 Display *di=XtDisplay((Widget)ilw);
 Window  win=XtWindow((Widget)ilw);
 GC	 gc;

 if (!XtIsRealized((Widget)ilw)) return;

 rectx=FL.border_width/2;
 recty=FL.border_width/2;
 rectw=IL.entry_width-FL.border_width-IL.entry_sep;
 recth=IL.entry_height-FL.border_width-IL.entry_sep;

 bm_w=IL.entry_width-IL.entry_sep;
 bm_h=IL.liney-FL.font->max_bounds.ascent-IL.labelsep+FL.border_width;
 index=ymin*IL.n_horiz+xmin;
 left=IL.left+xmin*IL.entry_width;
 uly=IL.top +ymin*IL.entry_height;
 for (i=ymin; i<=ymax; i++) {
   ulx=left;
   for (j=xmin; j<=xmax; j++) {
     if (index>=FL.n_files) { index=-2; return; }
	/* Draw one element here */

	file=FL.files[index];
	len=strlen(file->name);

	gc=FL.gc_norm;
	if (IL.hil_entry==index) {
	  XFillRectangle(di,win,
			FL.gc_highlight,
			ulx+rectx,uly+recty,
			rectw,recth);
	  gc=FL.gc_invert;
        }

	XDrawString(di,win,
		gc,
		ulx+(bm_w-XTextWidth(FL.font,file->name,len))/2,
		uly+IL.liney,
		file->name,
		len);

	w=file->type->bm_width;h=file->type->bm_height;
	XCopyArea(di,file->type->icon_bm,win,FL.gc_highlight,
		0,0,w,h,
		ulx+(bm_w-(int)w)/2,uly+(bm_h-(int)h)/2);

	if (file->selected) 
		XDrawRectangle(di,win,FL.gc_highlight,
			ulx+rectx,uly+recty,
			rectw,recth);

	/* end drawing */
     ulx+=IL.entry_width;
     index++;
   }
 uly+=IL.entry_height;
 index+=IL.n_horiz-1-(xmax-xmin); 
 }
}

#define IN (new->iconFileList)
#define IC (current->iconFileList)

static Boolean ilSetValues(Widget c, Widget r, Widget super,
	ArgList args, Cardinal *nargs)
{
int rval=False,tmp;
IconFileListWidget current=(IconFileListWidget)c;
IconFileListWidget new=(IconFileListWidget)super;

IN.entry_sep=IC.entry_sep;
IN.top=IC.top;
IN.left=IC.left;
IN.labelsep=IC.labelsep;
IN.min_icon_width=IC.min_icon_width;
IN.min_icon_height=IC.min_icon_height;

if (IN.user_n_horiz != IC.user_n_horiz) {
  if (IN.user_n_horiz<=0) {
     IN.user_n_horiz=0;
     IN.n_horiz=((int)new->core.width-2*IN.left+IN.entry_sep)/IN.entry_width;
     if (IN.n_horiz==0) IN.n_horiz=1;
  } else {
     IN.n_horiz=IN.user_n_horiz;
     tmp=new->fileList.n_files/IN.n_horiz+1; 
     new->core.width=2*IN.left+IN.entry_width*IN.n_horiz-IN.entry_sep;
     new->core.height=2*IN.top+tmp*IN.entry_height-IN.entry_sep;
  }
}

return rval;
}

#undef IN
#undef IC

static XtGeometryResult ilQueryGeometry(Widget w,
	XtWidgetGeometry* intended,XtWidgetGeometry* preferred)
{
 IconFileListWidget ilw=(IconFileListWidget)w;
 int n_vert,new_nhoriz;

 preferred->request_mode = CWWidth | CWHeight;

 if (IL.user_n_horiz>0) {
	preferred->width=w->core.width;
	preferred->height=w->core.height;
	return XtGeometryNo;
 }

 preferred->width = intended->width;
 new_nhoriz=((int)intended->width-2*IL.left+IL.entry_sep)/IL.entry_width;
 if (new_nhoriz==0) new_nhoriz=1;

 n_vert=FL.n_files/new_nhoriz+1;
 preferred->height = IL.entry_height*n_vert-IL.entry_sep+2*IL.top;

 if ( ((intended->request_mode & (CWWidth | CWHeight))
	   	== (CWWidth | CWHeight)) &&
	  intended->height >= preferred->height)
	return XtGeometryYes;
 else if (preferred->width == w->core.width &&
	  preferred->height == w->core.height)
	return XtGeometryNo;
 else
	return XtGeometryAlmost;
}


/*ARGSUSED*/
static void highlight(Widget w, XEvent* ev, String* args, Cardinal* nargs)
{
IconFileListWidget ilw=(IconFileListWidget)w;
int i,x,y;

FileListEventPosition(ev,&x,&y);
/* this excludes the case of being an event other
 * than Motion, Button- Enter-/Leave or Key- event 
 * (x == -1 in this case).
 */

i=ilFindEntry((FileListWidget)w,x,y,ev);
if (i==IL.hil_entry) return; /* is already highlighted */
if (IL.hil_entry>=0) unhighlight(w,ev,args,nargs);
IL.hil_entry=i;
x=i%IL.n_horiz;
y=i/IL.n_horiz;
if (i>=0)    drawPart(ilw,x,x,y,y);
}

/*ARGSUSED*/
static void unhighlight(Widget w, XEvent* ev, String* args, Cardinal* nargs)
{
IconFileListWidget ilw=(IconFileListWidget)w;
int i=IL.hil_entry,x,y;
int rectx,recty;
unsigned int rectw,recth;

if (IL.hil_entry<0) return;

x=i%IL.n_horiz;
y=i/IL.n_horiz;

rectx=IL.left+ x*IL.entry_width;
recty=IL.top + y*IL.entry_height;
rectw=IL.entry_width-IL.entry_sep;
recth=IL.entry_height-IL.entry_sep;

XFillRectangle(XtDisplay(w),XtWindow(w),
		FL.gc_invert,
		rectx,recty,
		rectw,recth);
IL.hil_entry=-1;
drawPart(ilw,x,x,y,y);
}


static int ilFindEntry(FileListWidget flw, int x, int y, XEvent *ev)
{
 IconFileListWidget ilw=(IconFileListWidget)flw;
 int i,nx,ny;

 if (x<IL.left || x>= IL.left+IL.n_horiz*IL.entry_width) return -1;

 nx=(x-IL.left)/IL.entry_width;
 ny=(y-IL.top)/IL.entry_height;
 i=ny*IL.n_horiz+nx;
 if (i<0 || i>=FL.n_files) return -1;
 else return i;
}

static void ilEntryPosition(
	FileListWidget flw,
	int entry,
	Position *x, Position *y,
	Dimension *width, Dimension *height)
{
 IconFileListWidget ilw=(IconFileListWidget)flw;
 *x=(Position)(IL.left+IL.entry_width*(entry%IL.n_horiz));
 *y=(Position)(IL.top+IL.entry_height*(entry/IL.n_horiz));
 *width=(Dimension)(IL.entry_width-IL.entry_sep);
 *height=(Dimension)(IL.entry_height-IL.entry_sep);
}
