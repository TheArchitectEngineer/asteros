/*---------------------------------------------------------------------------
  Module FmBitmaps

  (c) Simon Marlow 1990-92
  (c) Albert Graef 1994
  
  modified 7-1997 by strauman@sun6hft.ee.tu-berlin.de to add
  different enhancements (see README-NEW).

  Functions & data for handling the bitmaps and cursors.
---------------------------------------------------------------------------*/

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xmu/Drawing.h>

#ifdef XPM
#include <X11/xpm.h>
#endif

#include "../lib/bitmaps/xfm_file.xbm"
#include "../lib/bitmaps/xfm_filemsk.xbm"
#include "../lib/bitmaps/xfm_files.xbm"
#include "../lib/bitmaps/xfm_filesmsk.xbm"
#ifdef ENHANCE_CURSOR
#include "../lib/bitmaps/xfm_noentry32.xbm"
#include "../lib/bitmaps/xfm_noentrymsk32.xbm"
#else
#include "../lib/bitmaps/xfm_noentry.xbm"
#include "../lib/bitmaps/xfm_noentrymsk.xbm"
#endif
#include "../lib/bitmaps/xfm_dir.xbm"
#include "../lib/bitmaps/xfm_dirmsk.xbm"
#include "../lib/bitmaps/xfm_exec.xbm"
#include "../lib/bitmaps/xfm_execmsk.xbm"
#include "../lib/bitmaps/xfm_watch.xbm"
#include "../lib/bitmaps/xfm_watchmsk.xbm"
#include "../lib/bitmaps/xfm_lline.xbm"
#include "../lib/bitmaps/xfm_tline.xbm"
#include "../lib/bitmaps/xfm_fline.xbm"
#include "../lib/bitmaps/xfm_cline.xbm"
#include "../lib/bitmaps/xfm_larrow.xbm"
#include "../lib/bitmaps/xfm_rarrow.xbm"
#include "../lib/bitmaps/xfm_wavy_arrow.xbm"
#include "../lib/bitmaps/xfm_tick.xbm"
#include "../lib/bitmaps/xfm_notick.xbm"
#include "../lib/bitmaps/xfm_excl.xbm"
#ifdef ENHANCE_PERMS
/* xfm_suid.xbm/xfm_Suid.xbm and xfm_sticky.xbm/xfm_Sticky.xbm are real,
   distinct files upstream (differing only in case) that collide and
   silently lose one on this Mac's case-insensitive APFS `git clone` --
   same gotcha WindowMaker's own vendoring hit (TODO.md Phase 36).
   Recovered both via GitHub's raw blob API (case-sensitive at the git
   object level) and renamed to case-safe filenames; the *_bits/_width/
   _height symbol names inside are untouched, so nothing below needs to
   change. */
#include "../lib/bitmaps/xfm_suid_lower.xbm"
#include "../lib/bitmaps/xfm_suid_upper.xbm"
#include "../lib/bitmaps/xfm_sticky_lower.xbm"
#include "../lib/bitmaps/xfm_sticky_upper.xbm"
#endif
#ifdef ENHANCE_CURSOR
#include "../lib/bitmaps/xfm_file_s.xbm"
#include "../lib/bitmaps/xfm_filemsk_s.xbm"
#include "../lib/bitmaps/xfm_files_s.xbm"
#include "../lib/bitmaps/xfm_filesmsk_s.xbm"
#include "../lib/bitmaps/xfm_noentry_s.xbm"
#include "../lib/bitmaps/xfm_noentrymsk_s.xbm"
#include "../lib/bitmaps/xfm_dir_s.xbm"
#include "../lib/bitmaps/xfm_dirmsk_s.xbm"
#include "../lib/bitmaps/xfm_exec_s.xbm"
#include "../lib/bitmaps/xfm_execmsk_s.xbm"
#endif

#ifndef XPM
#include "../lib/bitmaps/xfm_symlnk.xbm"
#include "../lib/bitmaps/xfm_dirlnk.xbm"
#include "../lib/bitmaps/xfm_execlnk.xbm"
#include "../lib/bitmaps/xfm_blackhole.xbm"
#include "../lib/bitmaps/xfm_icon.xbm"
#include "../lib/bitmaps/xfm_appmgr.xbm"
#else
#ifndef ENHANCE_3DICONS
#include "../lib/pixmaps/xfm_file.xpm"
#include "../lib/pixmaps/xfm_dir.xpm"
#include "../lib/pixmaps/xfm_updir.xpm"
#include "../lib/pixmaps/xfm_exec.xpm"
#include "../lib/pixmaps/xfm_files.xpm"
#include "../lib/pixmaps/xfm_symlnk.xpm"
#include "../lib/pixmaps/xfm_dirlnk.xpm"
#include "../lib/pixmaps/xfm_execlnk.xpm"
#include "../lib/pixmaps/xfm_blackhole.xpm"
#include "../lib/pixmaps/xfm_icon.xpm"
#include "../lib/pixmaps/xfm_appmgr.xpm"
#else
#include "../contrib/3dicons/icons/file.xpm"
#include "../contrib/3dicons/icons/folder.xpm"
#include "../contrib/3dicons/icons/folder_up.xpm"
#include "../contrib/3dicons/icons/app.xpm"
#include "../contrib/3dicons/icons/files.xpm"
#include "../contrib/3dicons/icons/file_link.xpm"
#include "../contrib/3dicons/icons/folder_link.xpm"
#include "../contrib/3dicons/icons/app_link.xpm"
#include "../contrib/3dicons/icons/file_link_bad.xpm"
#include "../contrib/3dicons/icons/suitcase.xpm"
#endif /* ENHANCE_3DICONS */
#endif

#include "Am.h"
#include "Fm.h"

/*-----------------------------------------------------------------------------
  STATIC DATA
-----------------------------------------------------------------------------*/

typedef struct {
  char *bits;
  int width, height;
} BitmapRec;


#ifdef __STDC__
#define ICON(x) { x##_bits, x##_width, x##_height }
#else
#define ICON(x) { x/**/_bits, x/**/_width, x/**/_height }
#endif

static BitmapRec bitmaps[] = {
  ICON(xfm_file), ICON(xfm_filemsk), ICON(xfm_files), ICON(xfm_filesmsk),
  ICON(xfm_noentry), ICON(xfm_noentrymsk), ICON(xfm_dir), ICON(xfm_dirmsk),
  ICON(xfm_exec), ICON(xfm_execmsk), ICON(xfm_watch), ICON(xfm_watchmsk),
  ICON(xfm_lline), ICON(xfm_tline), ICON(xfm_fline), ICON(xfm_cline),
  ICON(xfm_larrow), ICON(xfm_rarrow), ICON(xfm_wavy_arrow), ICON(xfm_tick),
  ICON(xfm_notick), ICON(xfm_excl),
#ifdef ENHANCE_PERMS
  ICON(xfm_suid),ICON(xfm_Suid),ICON(xfm_sticky),ICON(xfm_Sticky),
#endif
#ifdef ENHANCE_CURSOR
  ICON(xfm_file_s), ICON(xfm_filemsk_s),
  ICON(xfm_files_s), ICON(xfm_filesmsk_s),
  ICON(xfm_noentry_s), ICON(xfm_noentrymsk_s),
  ICON(xfm_dir_s), ICON(xfm_dirmsk_s),
  ICON(xfm_exec_s), ICON(xfm_execmsk_s),
#endif
#ifndef XPM
  ICON(xfm_files), ICON(xfm_dir), ICON(xfm_dir), ICON(xfm_file),
  ICON(xfm_exec), ICON(xfm_symlnk), ICON(xfm_dirlnk), ICON(xfm_execlnk),
  ICON(xfm_blackhole),
#endif
};

#ifdef XPM
static char **pixmaps[] = {
#ifndef ENHANCE_3DICONS
  xfm_files_xpm, xfm_dir_xpm, xfm_updir_xpm, xfm_file_xpm,
  xfm_exec_xpm, xfm_symlnk_xpm, xfm_dirlnk_xpm, xfm_execlnk_xpm,
  xfm_blackhole_xpm,
#else
  files_xpm, folder_xpm, folder_up_xpm, file_xpm,
  app_xpm, file_link_xpm, folder_link_xpm, app_link_xpm,
  file_link_bad_xpm,
#endif /* ENHANCE_3DICONS */
};
#endif

typedef struct {
  int source, mask;
} CursorRec;

static CursorRec cursors[] = {
  { FILE_CBM, FILEMSK_CBM },
  { FILES_CBM, FILESMSK_CBM },
  { NOENTRY_CBM, NOENTRYMSK_CBM },
  { DIR_CBM, DIRMSK_CBM },
  { EXEC_CBM, EXECMSK_CBM },
  { WATCH_CBM, WATCHMSK_CBM }
};

#ifdef ENHANCE_CURSOR
/* Bitmaps of cursors not bigger than 16x16 bit */
static CursorRec safe_cursors[] = {
  { S_FILE_CBM, S_FILEMSK_CBM },
  { S_FILES_CBM, S_FILESMSK_CBM },
  { S_NOENTRY_CBM, S_NOENTRYMSK_CBM },
  { S_DIR_CBM, S_DIRMSK_CBM },
  { S_EXEC_CBM, S_EXECMSK_CBM },
  { WATCH_CBM, WATCHMSK_CBM }
};
#endif

static char nothing[]={(char)0};

/*-----------------------------------------------------------------------------
  PUBLIC DATA
-----------------------------------------------------------------------------*/

Pixmap *bm;
Cursor *curs;

TypeRec builtin_types[]={
  {nothing,
#ifdef MAGIC_HEADERS
   nothing,
#endif
  nothing,nothing,nothing,0,0,(Pixmap)0,(unsigned int)0,(unsigned int)0},
  {nothing,
#ifdef MAGIC_HEADERS
   nothing,
#endif
  nothing,nothing,nothing,0,0,(Pixmap)0,(unsigned int)0,(unsigned int)0},
  {nothing,
#ifdef MAGIC_HEADERS
   nothing,
#endif
  nothing,nothing,nothing,0,0,(Pixmap)0,(unsigned int)0,(unsigned int)0},
  {nothing,
#ifdef MAGIC_HEADERS
   nothing,
#endif
  nothing,nothing,nothing,0,0,(Pixmap)0,(unsigned int)0,(unsigned int)0},
  {nothing,
#ifdef MAGIC_HEADERS
   nothing,
#endif
  nothing,nothing,nothing,0,0,(Pixmap)0,(unsigned int)0,(unsigned int)0},
  {nothing,
#ifdef MAGIC_HEADERS
   nothing,
#endif
  nothing,nothing,nothing,0,0,(Pixmap)0,(unsigned int)0,(unsigned int)0},
  {nothing,
#ifdef MAGIC_HEADERS
   nothing,
#endif
  nothing,nothing,nothing,0,0,(Pixmap)0,(unsigned int)0,(unsigned int)0},
  {nothing,
#ifdef MAGIC_HEADERS
   nothing,
#endif
  nothing,nothing,nothing,0,0,(Pixmap)0,(unsigned int)0,(unsigned int)0},
};

#ifdef XPM
#ifdef ENHANCE_CMAP
/* PRIVATE FUNCTIONS */
static int readPixmap(Widget top, Boolean from_file, char **data, Pixmap *pm, Pixmap *msk, XpmAttributes *atts);
static Boolean switchColormap(Widget wid);
#endif
#endif

/*-----------------------------------------------------------------------------
  PUBLIC FUNCTIONS
-----------------------------------------------------------------------------*/


void readBitmaps()
{
  int i,j;
  Display *dpy;
  int scrn;
  Colormap cmp;
  Window win;
  XColor black, white;
#ifdef ENHANCE_CURSOR
  Pixmap   *cpm;
  unsigned int w,h,rw,rh;
#endif
#ifdef XPM
  XpmAttributes xpm_attr;
  static XpmColorSymbol none_color = { NULL, "None", (Pixel)0 };
#endif

  dpy = XtDisplay(aw.shell);
  win = DefaultRootWindow(dpy);
  scrn = DefaultScreen(dpy);
  cmp = DefaultColormap(dpy, scrn);

  black.pixel = BlackPixel(dpy, scrn);
  XQueryColor(dpy, cmp, &black);
  white.pixel = WhitePixel(dpy, scrn);
  XQueryColor(dpy, cmp, &white);

  bm = (Pixmap *) XtMalloc(END_BM * sizeof(Pixmap *));
  curs = (Cursor *) XtMalloc(XtNumber(cursors) * sizeof(Cursor *));

  /* create the hardcoded bitmaps */

  for (i=0; i<XtNumber(bitmaps); i++)
    bm[i] = XCreateBitmapFromData(dpy, win, bitmaps[i].bits,
				  bitmaps[i].width, bitmaps[i].height);
#ifdef XPM
#ifdef ENHANCE_CMAP
  xpm_attr.valuemask = XpmColormap|XpmColorSymbols|XpmSize|XpmCloseness;
#else
  xpm_attr.valuemask = XpmReturnPixels|XpmColorSymbols|XpmSize;
#endif
  xpm_attr.colorsymbols = &none_color;
  xpm_attr.numsymbols = 1;
#ifdef ENHANCE_CMAP
  xpm_attr.closeness = (unsigned int) resources.color_closeness;
  XtVaGetValues(aw.shell, XtNbackground, &none_color.pixel,
			  XtNcolormap,&xpm_attr.colormap, NULL);
#else
  XtVaGetValues(aw.shell, XtNbackground, &none_color.pixel, NULL);
#endif
  for (i=XtNumber(bitmaps); i<DIR_BM; i++)
#ifdef ENHANCE_CMAP
    readPixmap(aw.shell,False,pixmaps[i-XtNumber(bitmaps)],&bm[i],NULL,&xpm_attr);
#else
    XpmCreatePixmapFromData(dpy, win, pixmaps[i-XtNumber(bitmaps)], &bm[i],
			    NULL, &xpm_attr);
#endif

  j=0;
  for (i=DIR_BM; i<ICON_BM; i++) {
#ifdef ENHANCE_CMAP
    readPixmap(aw.shell,False,pixmaps[i-XtNumber(bitmaps)],&bm[i],NULL,&xpm_attr);
#else
    XpmCreatePixmapFromData(dpy, win, pixmaps[i-XtNumber(bitmaps)], &bm[i],
			    NULL, &xpm_attr);
#endif
    builtin_types[j].icon_bm=bm[i];
    builtin_types[j].bm_width=xpm_attr.width;
    builtin_types[j].bm_height=xpm_attr.height;
    j++;
  }
#else
  j=0;
  for (i=DIR_BM; i<ICON_BM; i++) {
    builtin_types[j].icon_bm=bm[i];
    builtin_types[j].bm_width=bitmaps[i].width;
    builtin_types[j].bm_height=bitmaps[i].height;
    j++;
  }
#endif

  /* create the cursors */
#ifdef ENHANCE_CURSOR
  /* Check if the server supports our maximal cursor
   * size; 
   * Note: we assume the bitmap size can be found in
   * 'bitmaps'.
   */
  w=h=0;
  for (i=0; i<XtNumber(cursors); i++) {
    if ((rw=bitmaps[cursors[i].source].width)>w) w=rw;
    if ((rh=bitmaps[cursors[i].source].height)>h) h=rh;
  }
  XQueryBestCursor(dpy,win,w,h,&rw,&rh);
  if ( rw<w || rh<h ) { 
   /* switch to 16 bit cursors which are safe */
   for (i=0; i<XtNumber(cursors); i++)
     curs[i] = XCreatePixmapCursor(dpy, bm[safe_cursors[i].source], 
				        bm[safe_cursors[i].mask],
					&black, &white, 8, 8);
  } else
#endif
  for (i=0; i<XtNumber(cursors); i++)
    curs[i] = XCreatePixmapCursor(dpy, bm[cursors[i].source], 
				  bm[cursors[i].mask], &black, &white, 16, 16);

#ifdef ENHANCE_CURSOR
  /* cursor pixmaps are no longer needed */
  for (i=0; i<XtNumber(cursors); i++) {
    cpm=&bm[safe_cursors[i].source];
    if (*cpm) { XFreePixmap(dpy,*cpm); *cpm=(Pixmap)0; }
    cpm=&bm[safe_cursors[i].mask];
    if (*cpm) { XFreePixmap(dpy,*cpm); *cpm=(Pixmap)0; }
#ifdef XPM
    cpm=&bm[cursors[i].source];
    if (*cpm) { XFreePixmap(dpy,*cpm); *cpm=(Pixmap)0; }
    cpm=&bm[cursors[i].mask];
    if (*cpm) { XFreePixmap(dpy,*cpm); *cpm=(Pixmap)0; }
#endif
  } 
#endif

  /* create the application icons */

#ifdef XPM
#ifdef ENHANCE_CMAP
#ifndef ENHANCE_3DICONS
  readPixmap(aw.shell,False,xfm_icon_xpm,&bm[ICON_BM],&bm[ICONMSK_BM],NULL);
  readPixmap(aw.shell,False,xfm_appmgr_xpm,&bm[APPMGR_BM],&bm[APPMGRMSK_BM],NULL);
#else
  readPixmap(aw.shell,False,folder_xpm,&bm[ICON_BM],&bm[ICONMSK_BM],NULL);
  readPixmap(aw.shell,False,suitcase_xpm,&bm[APPMGR_BM],&bm[APPMGRMSK_BM],NULL);
#endif /* ENHANCE_3DICONS */
#else
#ifndef ENHANCE_3DICONS
  XpmCreatePixmapFromData(dpy, win, xfm_icon_xpm, &bm[ICON_BM],
#else
  XpmCreatePixmapFromData(dpy, win, folder_xpm, &bm[ICON_BM],
#endif /* ENHANCE_3DICONS */
			  &bm[ICONMSK_BM], NULL);
#ifndef ENHANCE_3DICONS
  XpmCreatePixmapFromData(dpy, win, xfm_appmgr_xpm, &bm[APPMGR_BM],
#else
  XpmCreatePixmapFromData(dpy, win, suitcase_xpm, &bm[APPMGR_BM],
#endif /* ENHANCE_3DICONS */
			  &bm[APPMGRMSK_BM], NULL);
#endif
#else
  bm[ICON_BM] = XCreateBitmapFromData(dpy, win, xfm_icon_bits,
				      xfm_icon_width, xfm_icon_height);
  bm[ICONMSK_BM] = None;
  bm[APPMGR_BM] = XCreateBitmapFromData(dpy, win, xfm_appmgr_bits,
				      xfm_appmgr_width, xfm_appmgr_height);
  bm[APPMGRMSK_BM] = None;
#endif
}

Pixmap readIcon(char *name, unsigned int *pm_width, unsigned int *pm_height)
{
  Display *dpy = XtDisplay(aw.shell);
  Window win = DefaultRootWindow(dpy);
  Screen *scrn = XtScreen(aw.shell);
  Pixmap icon_bm;
  char fullname[MAXPATHLEN];
  unsigned int w, h;
  int x, y;

#ifdef XPM

  XpmAttributes xpm_attr;
  static XpmColorSymbol none_color = { NULL, "None", (Pixel)0 };

  /* first search for xpm icons: */

#ifdef ENHANCE_CMAP
  xpm_attr.valuemask = XpmColormap|XpmColorSymbols|XpmSize|XpmCloseness;
#else
  xpm_attr.valuemask = XpmReturnPixels|XpmColorSymbols|XpmSize;
#endif
  xpm_attr.colorsymbols = &none_color;
  xpm_attr.numsymbols = 1;
#ifdef ENHANCE_CMAP
  xpm_attr.closeness = (unsigned int) resources.color_closeness;
  XtVaGetValues(aw.shell, XtNbackground, &none_color.pixel,
			  XtNcolormap, &xpm_attr.colormap, NULL);

  (void) searchPath(fullname, resources.pixmap_path, name);
  if (XpmSuccess==readPixmap(aw.shell,True,(char**)fullname,&icon_bm,NULL,&xpm_attr)) {
#else
  XtVaGetValues(aw.shell, XtNbackground, &none_color.pixel, NULL);

  if (XpmReadFileToPixmap(dpy, win,
			  searchPath(fullname, resources.pixmap_path, name),
			  &icon_bm, NULL, &xpm_attr) == XpmSuccess) {
#endif
    if (pm_width) *pm_width=(unsigned int)xpm_attr.width;
    if (pm_height) *pm_height=(unsigned int)xpm_attr.height;
    return icon_bm;
  }
#endif

  /* now search bitmap in standard locations (*bitmapFilePath): */

  icon_bm = XmuLocateBitmapFile(scrn, name, NULL, 0, (int *)&w, (int *)&h,
				 &x, &y);
  if (icon_bm != None) {
    if (pm_width) *pm_width=w;
    if (pm_height) *pm_height=h;
    return icon_bm;
  }

  /* finally search along *bitmapPath: */

  if (XReadBitmapFile(dpy, win,
		      searchPath(fullname, resources.bitmap_path, name),
		      &w, &h, &icon_bm, &x, &y) == BitmapSuccess) {
    if (pm_width) *pm_width=w;
    if (pm_height) *pm_height=h;
    return icon_bm;
   }
  if (pm_width) *pm_width=(unsigned int)0;
  if (pm_height) *pm_height=(unsigned int)0;
  return None;
}

#ifdef XPM
#ifdef ENHANCE_CMAP

static Boolean switchColormap(Widget wid)
{
static Boolean has_private_cm=False;

XtAppContext app=XtWidgetToApplicationContext(wid);
Colormap     new,old;
Display	     *di=XtDisplay(wid);
Widget	     top,wtmp;

 if (has_private_cm) {
   XtAppWarning(app,"switchColormap failed: have private colormap already");
   return False;
 }
 top=wid;
 while(wtmp=XtParent(top)) top=wtmp;

 XtVaGetValues(top,XtNcolormap,&old,0);
 
 if (0==(new=XCopyColormapAndFree(di,old))) {
   XtAppWarning(app,"switchColormap failed: cannot allocate new colormap");
   return False;
 }

 has_private_cm=True;
 XtVaSetValues(top,XtNcolormap,new,0);
 XtSetWMColormapWindows(top,&top,1);

 XtAppWarning(app,"switching to private colormap...");
 return True;
}

static int readPixmap(Widget top, Boolean from_file, char **data, Pixmap *pm, Pixmap *msk, XpmAttributes *atts)
{
Display *dpy=XtDisplay(top);
Window   win=DefaultRootWindow(dpy);
int      rval;

rval=(from_file? XpmReadFileToPixmap(dpy,win,(String)data,pm,msk,atts) :
		 XpmCreatePixmapFromData(dpy,win,data,pm,msk,atts));

if (rval==XpmColorFailed && switchColormap(top)) {
    /* get the new colormap */
    XtVaGetValues(top,XtNcolormap,&atts->colormap,0);
    /* and repeat */
    rval=(from_file? XpmReadFileToPixmap(dpy,win,(String)data,pm,msk,atts) :
		     XpmCreatePixmapFromData(dpy,win,data,pm,msk,atts));
}
return rval;
}
#endif
#endif
