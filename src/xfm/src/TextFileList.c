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
#include "TextFileListP.h"
#include <X11/Xaw/XawInit.h>

static XtResource tl_resources[] = {
#define offset(field) XtOffsetOf(TextFileListRec, textFileList.field)
    /* {name, class, type, size, offset, default_type, default_addr}, */
    { XtNentrySep, XtCEntrySep, XtRInt, sizeof(int),
	offset(entry_sep), XtRImmediate, (XtPointer)2},
    { XtNtopSep, XtCTopSep, XtRInt, sizeof(int),
	offset(top), XtRImmediate, (XtPointer)2},
    { XtNleftSep, XtCLeftSep, XtRInt, sizeof(int),
	offset(left), XtRImmediate, (XtPointer)2},
    { XtNtabSep, XtCTabSep, XtRInt, sizeof(int),
	offset(tabsep), XtRImmediate, (XtPointer)4},
    { XtNshowInode, XtCShowInode, XtRBoolean, sizeof(Boolean),
	offset(ino_s), XtRImmediate, (XtPointer)True},
    { XtNshowType, XtCShowType, XtRBoolean, sizeof(Boolean),
	offset(dev_s), XtRImmediate, (XtPointer)True},
    { XtNshowLinks, XtCShowLinks, XtRBoolean, sizeof(Boolean),
	offset(nlink_s), XtRImmediate, (XtPointer)True},
    { XtNshowPermissions, XtCShowPermissions, XtRBoolean, sizeof(Boolean),
	offset(umode_s), XtRImmediate, (XtPointer)True},
    { XtNshowOwner, XtCShowOwner, XtRBoolean, sizeof(Boolean),
	offset(uid_s), XtRImmediate, (XtPointer)True},
    { XtNshowGroup, XtCShowGroup, XtRBoolean, sizeof(Boolean),
	offset(gid_s), XtRImmediate, (XtPointer)True},
    { XtNshowLength, XtCShowLength, XtRBoolean, sizeof(Boolean),
	offset(size_s), XtRImmediate, (XtPointer)True},
    { XtNshowDate, XtCShowDate, XtRBoolean, sizeof(Boolean),
	offset(time_s), XtRImmediate, (XtPointer)True},
#undef offset
};

/* Methods */
static void tlInitialize(Widget,Widget,ArgList,Cardinal*);
static void tlExpose(Widget,XEvent*,Region);
static Boolean tlSetValues(Widget,Widget,Widget,ArgList,Cardinal*);
static XtGeometryResult tlQueryGeometry(Widget,XtWidgetGeometry*,XtWidgetGeometry*);
static int  tlFindEntry(FileListWidget,int,int,XEvent*);
static void tlEntryPosition(FileListWidget,int,Position*,Position*,Dimension*,Dimension*);
#ifdef VIEWPORT_HACK
static void tlSetValuesAlmost(Widget,Widget,XtWidgetGeometry*,XtWidgetGeometry*);
#endif

/* private procedures */
static void drawPart(TextFileListWidget,int,int);
static int CalcWidth(TextFileListWidget);
static int GetUidWidth(TextFileListWidget);
static int GetGidWidth(TextFileListWidget);
static int GetTimeWidth(TextFileListWidget);
static int GetSizeWidth(TextFileListWidget);
static int GetInodeWidth(TextFileListWidget);

/* Actions */
static void highlight(Widget, XEvent*, String*, Cardinal*);
static void unhighlight(Widget, XEvent*, String*, Cardinal*);


static XtActionsRec actions[] =
{
  /* {name, procedure}, */
    {"highlight",	highlight},
    {"unhighlight",	unhighlight},
};

static char translations[] ="";
/*
"<Key>:		textFileList()	\n\
";
*/

TextFileListClassRec textFileListClassRec = {
  { /* core fields */
    /* superclass		*/	(WidgetClass) &fileListClassRec,
    /* class_name		*/	"TextFileList",
    /* widget_size		*/	sizeof(TextFileListRec),
    /* class_initialize		*/	XawInitializeWidgetSet,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	tlInitialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	XtInheritRealize,
    /* actions			*/	actions,
    /* num_actions		*/	XtNumber(actions),
    /* resources		*/	tl_resources,
    /* num_resources		*/	XtNumber(tl_resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	XtExposeCompressMultiple,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	NULL,
    /* resize			*/	NULL,
    /* expose			*/	tlExpose,
    /* set_values		*/	tlSetValues,
    /* set_values_hook		*/	NULL,
#ifdef VIEWPORT_HACK
    /* set_values_almost	*/	tlSetValuesAlmost,
#else
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
#endif
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	translations,
    /* query_geometry		*/	tlQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
  { /* FileList fields */
    /* findEntry		*/	tlFindEntry,
    /* entryPosition		*/	tlEntryPosition
  },
  { /* textFileList fields */
    /* empty			*/	0
  }
};

WidgetClass textFileListWidgetClass = (WidgetClass)&textFileListClassRec;

#define TL (tlw->textFileList)
#define FL (tlw->fileList)

static void tlInitialize(treq,tnew,args,n_args)
 Widget	  treq,tnew;
 ArgList  args;
 Cardinal *n_args;
{
 int  width,height,tmp;
 TextFileListWidget tlw=(TextFileListWidget)tnew;

 TL.hil_entry=-1;

 height=FL.font->max_bounds.ascent+FL.font->max_bounds.descent+
	2*FL.border_width+TL.entry_sep;
 TL.entry_height=height;

 tmp=XTextWidth(FL.font,"m",1);

 TL.ino_w=-1;
 TL.dev_w=tmp;
 TL.nlink_w=2*tmp;
 TL.umode_w=-1;
 TL.uid_w=-1;
 TL.gid_w=-1;
 TL.size_w=-1;
 TL.time_w=-1;

#ifdef VIEWPORT_HACK
 TL.hack=1;
#endif

 width=CalcWidth(tlw);

 height*=FL.n_files;
 height+=TL.top-TL.entry_sep;
 if ((int)tlw->core.width<width) tlw->core.width=width;
 if ((int)tlw->core.height<height) tlw->core.height=height;
}

static void tlExpose(Widget w,XEvent *ev,Region reg)
{
 TextFileListWidget tlw=(TextFileListWidget)w;
 int imin,imax;
 if (ev->xexpose.count>0) return;
 imin=(ev->xexpose.y-TL.top)/TL.entry_height;
 imax=(ev->xexpose.y+ev->xexpose.height-TL.top)/TL.entry_height;
 drawPart(tlw,imin,imax);
}

static void drawPart(TextFileListWidget tlw, int imin, int imax)
{
 int i,j,h,left,leftbr,len,cur;
 int ino_x,dev_x,nlink_x,umode_x,uid_x,gid_x,size_x,time_x,fill;
 unsigned int rectw, recth;
 FileRec *file;
 int liney,recty,rectx;
 Display *di=XtDisplay((Widget)tlw);
 Window  win=XtWindow((Widget)tlw);
 GC	 gc,gcnorm=FL.gc_norm;
 char    buf[BUFSIZ], *chpt;
 struct  passwd *pw;
 struct  group  *gr;

 if (!XtIsRealized((Widget)tlw)) return;

 ino_x=dev_x=nlink_x=umode_x=uid_x=gid_x=size_x=time_x=0;
 rectx=TL.left+FL.border_width/2;
 recty=TL.top+FL.border_width/2 + imin*TL.entry_height;
 rectw=FL.name_w-FL.border_width; /* name_w contains 2*bw */
 recth=TL.entry_height-FL.border_width-TL.entry_sep;
 liney=TL.top+FL.font->max_bounds.ascent+FL.border_width + imin*TL.entry_height;
 h=TL.entry_height;
 left=TL.left+FL.border_width;
 leftbr=left+XTextWidth(FL.font,"[",1);

 /* calculate the position of the other fields */
 cur=TL.left+TL.tabsep+FL.name_w;
 if (TL.ino_s) {
	ino_x=cur; cur+=TL.ino_w+TL.tabsep;
 }
 if (TL.dev_s) {
	dev_x=cur; cur+=TL.dev_w+TL.tabsep;
 }
 if (TL.umode_s) {
	umode_x=cur; cur+=TL.umode_w+TL.tabsep;
 }
 if (TL.nlink_s) {
	nlink_x=cur; cur+=TL.nlink_w+TL.tabsep;
 }
 if (TL.uid_s) {
	uid_x=cur; cur+=TL.uid_w+TL.tabsep;
 }
 if (TL.gid_s) {
	gid_x=cur; cur+=TL.gid_w+TL.tabsep;
 }
 if (TL.size_s) {
	size_x=cur; cur+=TL.size_w+TL.tabsep;
 }
 if (TL.time_s) {
	time_x=cur; cur+=TL.time_w+TL.tabsep;
 }

 if (imin<0) imin=0;
 if (imax>=FL.n_files) imax=FL.n_files-1;
 if (TL.hil_entry>=imin && TL.hil_entry<=imax) {
	XFillRectangle(di,win,
			FL.gc_highlight,
			TL.left+FL.border_width/2,
			TL.top+FL.border_width/2+TL.hil_entry*h,
			rectw,recth);
 }
 for (i=imin; i<=imax; i++) {
        file=FL.files[i];

	gc= (i==TL.hil_entry?FL.gc_invert:FL.gc_highlight);
	len=strlen(file->name);
	if (S_ISDIR(file->stats.st_mode)) {
		XDrawString(di,win,
			gc,
			left, liney, "[", 1);
		file->name[len]=(char)']';
		XDrawString(di,win,
			gc,
			leftbr, liney,
			file->name,
			len+1);
	} else {
		XDrawString(di,win,
			gc,
			left, liney,
			file->name,
			len);
	}
	file->name[len]=(char)0;

	if (file->selected) 
		XDrawRectangle(di,win,FL.gc_highlight,
			rectx,recty,
			rectw,recth);

	if (TL.ino_s) {
	   sprintf(buf,"%lu",(unsigned long)file->stats.st_ino);
	   fill=TL.ino_w-XTextWidth(FL.font,buf,len=strlen(buf));
	   XDrawString(di,win,gcnorm,ino_x+fill,liney,buf,len);
	}
	if (TL.dev_s) {
	   if      (file->sym_link) 		   chpt="l";
	   else if (S_ISDIR(file->stats.st_mode))  chpt="d";
	   else if (S_ISCHR(file->stats.st_mode))  chpt="c";
	   else if (S_ISBLK(file->stats.st_mode))  chpt="b";
	   else if (S_ISFIFO(file->stats.st_mode)) chpt="p";
	   else if (S_ISSOCK(file->stats.st_mode)) chpt="s";
	   else 				   chpt="-";

	   fill=TL.dev_w-XTextWidth(FL.font,chpt,1);
	   XDrawString(di,win,gcnorm,dev_x+fill,liney,chpt,1);
	}
	if (TL.umode_s) {
	   makePermissionsString(buf,file->stats.st_mode);
	   fill=0;
	   for (j=0;j<9;j++) {
		XDrawString(
		  di,win,gcnorm,
		  umode_x+fill+((TL.perm_w[j]-XTextWidth(FL.font,buf+j,1))/2),
		  liney,buf+j,1);
		fill+=TL.perm_w[j];
	   }
	}
	if (TL.nlink_s) {
	   sprintf(buf,"%lu",(unsigned long)file->stats.st_nlink);
	   fill=TL.nlink_w-XTextWidth(FL.font,buf,len=strlen(buf));
	   XDrawString(di,win,gcnorm,nlink_x+fill,liney,buf,len);
	}
	if (TL.uid_s)   {
	   if ((pw=getpwuid(file->stats.st_uid))==0) {
		sprintf(buf,"%lu",(unsigned long)file->stats.st_uid);
		chpt=buf;
	   } else chpt=pw->pw_name;
	   XDrawString(di,win,gcnorm,uid_x,liney,chpt,strlen(chpt));
	}
	if (TL.gid_s)   {
	   if ((gr=getgrgid(file->stats.st_gid))==0) {
		sprintf(buf,"%lu",(unsigned long)file->stats.st_gid);
		chpt=buf;
	   } else chpt=gr->gr_name;
	   XDrawString(di,win,gcnorm,gid_x,liney,chpt,strlen(chpt));
	}
	if (TL.size_s) {
	   sprintf(buf,"%lu",(unsigned long)file->stats.st_size);
	   fill=TL.size_w-XTextWidth(FL.font,buf,len=strlen(buf));
	   XDrawString(di,win,gcnorm,size_x+fill,liney,buf,len);
	}
	if (TL.time_s) {
	   len=strlen(chpt=ctime(&file->stats.st_mtime));
           if (*(chpt+len-1)==(char)'\n') len--;
	   XDrawString(di,win,gcnorm,time_x,liney,chpt,len);
	}

	recty+=h;
	liney+=h;
 }

}


#define TN (new->textFileList)
#define TC (current->textFileList)

static Boolean tlSetValues(Widget c, Widget r, Widget super,
	ArgList args, Cardinal *nargs)
{
int rval=False;
TextFileListWidget current=(TextFileListWidget)c;
TextFileListWidget new=(TextFileListWidget)super;

TN.entry_sep=TC.entry_sep;
TN.top=TC.top;
TN.left=TC.left;
TN.tabsep=TC.tabsep;

if ( TN.ino_s != TC.ino_s ||
     TN.dev_s != TC.dev_s ||
     TN.nlink_s != TC.nlink_s ||
     TN.umode_s != TC.umode_s ||
     TN.uid_s != TC.uid_s ||
     TN.gid_s != TC.gid_s ||
     TN.size_s != TC.size_s ||
     TN.time_s != TC.time_s ) {
  super->core.width=CalcWidth(new);
#ifdef VIEWPORT_HACK
  super->core.height+=TN.hack; 
  TN.hack=-TN.hack;
  /* This is a hack to force the viewport parent to change the height
   * (workaround a bug)
   */
#endif
  rval=True;
}

return rval;
}

#undef TN
#undef TC

#ifdef VIEWPORT_HACK
/* The viewport computes wrong the layout if not both, a height as well as a
 * width change is requested. So we use this method to reset the
 * height modified by tlSetValues.
 */
static void tlSetValuesAlmost(
	Widget old, 
	Widget new, 
	XtWidgetGeometry *request,
	XtWidgetGeometry *compr)
{
/* This is a hack to workaround a bug in the viewport parent
 */
 *request =*compr;
 request->height=old->core.height;
}
#endif

static XtGeometryResult tlQueryGeometry(Widget w,
	XtWidgetGeometry* intended,XtWidgetGeometry* preferred)
{
 TextFileListWidget tlw=(TextFileListWidget)w;

 preferred->request_mode = CWWidth | CWHeight;
 preferred->width = CalcWidth(tlw);
 preferred->height = TL.entry_height*FL.n_files-TL.entry_sep+2*TL.top;

 if ( ((intended->request_mode & (CWWidth | CWHeight))
	   	== (CWWidth | CWHeight)) &&
	  intended->width >= preferred->width &&
	  intended->height >= preferred->height) {
	return XtGeometryYes;
 } else if (preferred->width == w->core.width &&
	  preferred->height == w->core.height)
	return XtGeometryNo;
 else
	return XtGeometryAlmost;
}

/*ARGSUSED*/
static void highlight(Widget w, XEvent* ev, String* args, Cardinal* nargs)
{
TextFileListWidget tlw=(TextFileListWidget)w;
int i,x,y;

FileListEventPosition(ev,&x,&y);
/* this excludes the case of being an event other
 * than Motion, Button- Enter-/Leave or Key- event 
 * (x == -1 in this case).
 */

i=tlFindEntry((FileListWidget)w,x,y,ev);
if (i==TL.hil_entry) return; /* is already highlighted */
if (TL.hil_entry>=0) unhighlight(w,ev,args,nargs);
TL.hil_entry=i;
if (i>=0)    drawPart(tlw,i,i);
}

/*ARGSUSED*/
static void unhighlight(Widget w, XEvent* ev, String* args, Cardinal* nargs)
{
TextFileListWidget tlw=(TextFileListWidget)w;
int i=TL.hil_entry;
int rectx,recty;
unsigned int rectw,recth;

if (TL.hil_entry<0) return;

rectx=TL.left;
recty=TL.top + i*TL.entry_height;
rectw=FL.name_w+2*FL.border_width;
recth=TL.entry_height-TL.entry_sep;

XFillRectangle(XtDisplay(w),XtWindow(w),
		FL.gc_invert,
		rectx,recty,
		rectw,recth);
TL.hil_entry=-1;
drawPart(tlw,i,i);
}

/* private procedures */

static int tlFindEntry(FileListWidget flw, int x, int y, XEvent *ev)
{
 TextFileListWidget tlw=(TextFileListWidget)flw;
 int i;

 if (x<TL.left || x> TL.left+FL.name_w) return -1;
 i=(y-TL.top)/TL.entry_height;
 if (i<0 || i>=FL.n_files) return -1;
 else return i;
}

static void tlEntryPosition(	
	FileListWidget flw,
	int entry,
	Position *x, Position *y,
	Dimension *width, Dimension *height)
{
 TextFileListWidget tlw=(TextFileListWidget)flw;
 
 *x=(Position)TL.left;
 *y=(Position)(TL.top+TL.entry_height*entry);
 *width=(Dimension)CalcWidth(tlw);
 *height=(Dimension)(TL.entry_height-TL.entry_sep);
}

static int GetUidWidth(TextFileListWidget tlw)
{
int i,width=0,tmp;
uid_t uid=0,tmpuid=0;
char buf[128], *chpt=0;
struct passwd  *pw;

for (i=0;i<FL.n_files;i++) {
   if (chpt && uid==(tmpuid=FL.files[i]->stats.st_uid)) continue;
   pw=getpwuid(uid=tmpuid);
   if (!pw) {
	sprintf(buf,"%lu",(unsigned long)tmpuid);
	chpt=buf;
   } else chpt=pw->pw_name;
   if ((tmp=XTextWidth(FL.font,chpt,strlen(chpt)))>width) width=tmp;
}
return width;
}

static int GetGidWidth(TextFileListWidget tlw)
{
int i,width=0,tmp;
gid_t gid=0,tmpgid=0;
char buf[128], *chpt=0;
struct group *grp;

for (i=0;i<FL.n_files;i++) {
   if (gid==(tmpgid=FL.files[i]->stats.st_gid) && chpt) continue;
   grp=getgrgid(gid=tmpgid);
   if (!grp) {
	sprintf(buf,"%lu",(unsigned long)tmpgid);
	chpt=buf;
   } else chpt=grp->gr_name;
   if ((tmp=XTextWidth(FL.font,chpt,strlen(chpt)))>width) width=tmp;
}
return width;
}

static int GetSizeWidth(TextFileListWidget tlw)
{
int i,width=0,tmp;
char buf[128];

for (i=0;i<FL.n_files;i++) {
   sprintf(buf,"%lu",(unsigned long)FL.files[i]->stats.st_size);
   if ((tmp=XTextWidth(FL.font,buf,strlen(buf)))>width) width=tmp;
}
return width;
}

static int GetInodeWidth(TextFileListWidget tlw)
{
int i,width=0,tmp;
char buf[128];

for (i=0;i<FL.n_files;i++) {
   sprintf(buf,"%lu",(unsigned long)FL.files[i]->stats.st_ino);
   if ((tmp=XTextWidth(FL.font,buf,strlen(buf)))>width) width=tmp;
}
return width;
}
static int GetTimeWidth(TextFileListWidget tlw)
{
int i,width=0,tmp,len;
char *chpt;

for (i=0;i<FL.n_files;i++) {
   len=strlen(chpt=ctime(&FL.files[i]->stats.st_mtime));
   if (*(chpt+len-1)==(char)'\n') len--;
   if ((tmp=XTextWidth(FL.font,chpt,len))>width) width=tmp;
}
return width;
}

static int CalcWidth(TextFileListWidget tlw)
{
 int width, tmp,tmp1,tmp2;

 width=TL.left+FL.name_w;

 if (TL.ino_s) {
   if (TL.ino_w==-1) TL.ino_w=GetInodeWidth(tlw);
   width+=TL.tabsep+TL.ino_w;
 }

 if (TL.dev_s) width+=TL.tabsep+TL.dev_w;
 
 if (TL.nlink_s) width+=TL.tabsep+TL.nlink_w;


 if (TL.umode_s) {
 /* calculate width of the characters of the permission string
  * (do this for every char to make it look nice :-)
  */
  if (TL.umode_w==-1) {
   tmp=XTextWidth(FL.font,"-",1);
   tmp2=((tmp1=XTextWidth(FL.font,"r",1))>tmp? tmp1:tmp);
   TL.perm_w[0]=TL.perm_w[3]=TL.perm_w[6]=tmp2;
   TL.umode_w=3*tmp2;

   tmp2=((tmp1=XTextWidth(FL.font,"w",1))>tmp? tmp1:tmp);
   TL.perm_w[1]=TL.perm_w[4]=TL.perm_w[7]=tmp2;
   TL.umode_w+=3*tmp2;

   if ((tmp1=XTextWidth(FL.font,"x",1))>tmp) tmp=tmp1;
 
   tmp2=( (tmp1=XTextWidth(FL.font,"s",1)) > tmp? tmp1:tmp);
   if ((tmp1=XTextWidth(FL.font,"S",1))>tmp2) tmp2=tmp1;
   TL.perm_w[2]=TL.perm_w[5]=tmp2;
   TL.umode_w+=2*tmp2;

   if ((tmp1=XTextWidth(FL.font,"t",1))>tmp) tmp=tmp1;
   if ((tmp1=XTextWidth(FL.font,"T",1))>tmp) tmp=tmp1;
   TL.umode_w+=(TL.perm_w[8]=tmp);
  }
  width+=TL.tabsep+TL.umode_w;
 }

 if (TL.uid_s) {
   if (TL.uid_w==-1) TL.uid_w=GetUidWidth(tlw);
   width+=TL.tabsep+TL.uid_w;
 }

 if (TL.gid_s) {
   if (TL.gid_w==-1) TL.gid_w=GetGidWidth(tlw);
   width+=TL.tabsep+TL.gid_w;
 }

 if (TL.size_s) {
   if (TL.size_w==-1) TL.size_w=GetSizeWidth(tlw);
   width+=TL.tabsep+TL.size_w;
 }

 if (TL.time_s) {
   if (TL.time_w==-1) TL.time_w=GetTimeWidth(tlw);
   width+=TL.tabsep+TL.time_w;
 }

 width+=TL.left;
 
 return width;
}
