/*
 * Miscellaneous definitions and prototypes, including those from misc.c.
 */

/* $Id: misc.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

#if defined(HAVE_PARAM_H)
#include <sys/param.h>
#endif
#include <stdio.h>

/*
**  By default everything uses drand48(),
**    I was making more exceptions than inclusions.
 */
#if !defined(__EMX__) && !defined(__CYGWIN__)
#define USE_DRAND
#endif

#if defined(SVR4) || defined(__osf__)
#define SHORT_RANGE
#else
#if !defined(__GLIBC__) & !defined(random) & !defined(__CYGWIN__) & !defined(__APPLE__)
long random(void);
#endif

#if !defined(__VMS) & !defined(linux) & !defined(__EMX__) & !defined(__FreeBSD__) & !defined(__CYGWIN__) & !defined(__NetBSD__) & !defined(__DragonFly__) & !defined(__GLIBC__) & !defined(__APPLE__)
#if defined(BSD4_4) || defined(HPArchitecture) || defined(SGIArchitecture) || defined(_AIX) || defined(_SCO_DS)
void srandom(unsigned int);
#else
int srandom(unsigned int);
#endif	/* BSD4_4 */
#endif	/* linux */
#endif

#ifdef USE_DRAND
#ifdef DECLARE_DRAND48
extern double drand48();
extern long lrand48();
#endif
#define RANDOMI()	lrand48()
#define RANDOMI2(s, f)	(drand48() * ((f) - (s)) + (s))
#define SRANDOM(seed)	srand48((long) (seed))
#else
#ifdef SHORT_RANGE
#define RANGE		0x00000fff
#else
#define RANGE		0x0fffffff
#endif
#define RANDOMI()	random()
#define RANDOMI2(s, f)	(((double)(random() % RANGE) / \
			  (double)RANGE) * ((f) - (s)) + (s))
#define SRANDOM(seed)	srandom((unsigned) (seed))
#endif

#ifdef __EMX__
#define strcasecmp stricmp
#endif

#define ICONWIDTH 48
#define ICONHEIGHT 40

/* brushOp.c */
extern void setStandardCursor(Widget w);

/* dialog.c */
extern void AlertBox(Widget parent, char *msg, XtCallbackProc okProc,
	             XtCallbackProc nokProc, void *data);
extern void Notice(Widget w,...);

/* fatBitsEdit.c */
extern void FatCursorSet(Widget w, Pixmap cursor);
extern void FatCursorAddZoom(int zoom, Widget winwid);
extern void FatCursorRemoveZoom(Widget winwid);
extern void FatCursorDestroyCallback(Widget w, XtPointer arg, XtPointer junk);
extern void FatCursorOff(Widget w);
extern void FatbitsUpdate(Widget w, int zoom);
extern void FatbitsEditDestroy(Widget paint);
extern void FatbitsEdit(Widget paint);

/* fileBrowser.c */
extern void *GetFileNameGetLastId(void);
extern void StdSaveRegionFile(Widget w, XtPointer paintArg, XtPointer junk);
extern void StdSaveAsFile(Widget w, XtPointer paintArg, XtPointer junk);
extern void StdSaveFile(Widget w, XtPointer paintArg, XtPointer junk);
extern void *ReadMagic(char *file);
extern void *getArgType(Widget w);
extern void GetFileName(Widget w, int type, char *def,
		        XtCallbackProc okFunc, XtPointer data);

/* fontOp.c */
extern Drawable GetLocalInfoDrawable(void *info);
extern void WriteText(Widget w, void **info, char *text);

/* fontSelect.c */
extern void FontSelect(Widget w, Widget paint);
extern void * setDefaultGlobalFont(Display *dpy, char *name);
extern int GetCharLength(char *buf);

/* iprocess.c */
extern Boolean isFilterDefined();
extern void * ScriptEditor(Widget w, Widget paint);

/* grab.c */
#ifdef __IMAGE_H__
extern Image *DoGrabImage(Widget w, int width, int height);
#endif
extern void DoGrabPixel(Widget w, Pixel * p, Colormap * cmap);
extern XColor *DoGrabColor(Widget w);

/* help.c */
extern char *matchGet(char *line, char *pat);
extern void HelpDialog(Widget parent, String name);
#if defined( _STDIO_H ) || defined( __VMS ) 
extern void HelpTextOutput(FILE* fd, String name);
#endif
extern void HelpInit(Widget top);

/* lupe.c */
void StartMagnifier(Widget w);

/* main.c */
extern int  GetDefaultUndosize();
extern void SetDefaultWHZ(int w, int h, int zoom);
extern void GetDefaultWH(int *w, int *h);
extern void GetPaintWH(int *w, int *h);
extern int GetInitZoom();
extern char *GetShareDir(void);
extern char *GetDefaultRC(void);
extern double GetDpi(void);
extern Boolean GetAsText(void);
extern void SetIconImage(Widget w);
extern void SetMenuBarVisibility(Widget w, Boolean flag);
extern Boolean IsMenuBarVisible(Widget w);
extern Boolean IsMenuBarGlobal();
extern void SetFullMenu(Widget w, Boolean flag);
extern Boolean IsFullMenuSet(Widget w);
extern Boolean IsFullMenuGlobal();
extern Boolean ToolsAreHorizontal();

/* menu.c */
extern void PopdownMenusGlobal();

/* misc.c */
extern char * ZoomToStr(int zoom);
extern int StrToZoom(char *str);
extern void XtSetMinSizeHints(Widget w, int u, int v);
extern int privateXErrorHandler(Display *dpy, XErrorEvent *myerr);
extern Widget GetToplevel(Widget w);
extern Widget GetShell(Widget w);
extern void RaiseWindow(Display *dpy, Window win);
extern void SetIBeamCursor(Widget w);
extern void SetCrossHairCursor(Widget w);
extern void SetPencilCursor(Widget w);
extern void SetCapAndJoin(Widget w, GC gc, int cap, int join);
extern void EnlargePixmap(Display * dpy, Pixmap cursor, int zoom,
		          Pixmap * data, Pixmap * mask);
extern XRectangle *RectUnion(XRectangle * a, XRectangle * b);
extern XRectangle *RectIntersect(XRectangle * a, XRectangle * b);
extern void GetPixmapWHD(Display * dpy, Drawable d, int *wth, int *hth, int *dth);
extern Pixmap dupPixmap(Display * dpy, Pixmap pix);
extern Pixmap GetBackgroundPixmap(Widget w);
extern GC GetGCX(Widget w);
extern void StrToArgv(char *str, int *argc, char **argv);
extern XImage *NewXImage(Display * dpy, Visual * visual,
		         int depth, int width, int height);
extern double gauss(void);
extern int gaussclamp(int range);
extern void *xmalloc(size_t n);
extern void AutoCrop(Widget paint);
extern Widget XtVisCreatePopupShell(String name,  WidgetClass widget_class,
                                    Widget parent, ArgList args, Cardinal num_args);
extern void clickFocusCallback(Widget w, XtPointer arg, XEvent * event, 
			       Boolean * flg);

/* graphic.c */
extern Widget GetNonDirtyCanvas();
extern void StoreName(Widget w, char *name);
extern void loadPrescribedFile(Widget w, char *file);
extern void CreateCanvas(Widget w, int width, int height, int zoom);
extern void SizeSelect(Widget w, XtPointer arg, int new);
extern void loadClipboard(Widget w, XtPointer junk, XtPointer junk2);
extern void changeDashStyleAction(Widget w, XEvent * event);
extern void setZoomButtonLabel(Widget paint, int zoom);
extern void setToolIconPixmap(Widget paint, void *ptr);
extern void setBrushIconPixmap(Widget paint, void *ptr);
extern void setFontIcon(Widget paint);
extern void setCanvasColorsIcon(Widget paint, void *ptr);
extern void changeBackground(Widget w, XtPointer paintArg, XtPointer junk2);
extern void AddItemToCanvasPalette(Widget paint, Pixel p, Pixmap pix);
extern Boolean inCanvasPixmaps(Widget paint, Pixmap pix);
extern void AddFileToGlobalList(char * file);
extern void RemoveFileFromGlobalList(char * file);
extern void setWriteTextSensitive(Widget w, Boolean b);
extern void StdWriteText(Widget w, XtPointer infoArg, XtPointer junk);
extern void StdOpenFile(Widget w, XtPointer paintArg, XtPointer junk);

/* pattern.c */
extern void PatternEdit(Widget w, Pixel *pixels,
                        Pixmap *patterns, void *brushes,
                        int npixels, int npatterns, int nbrushes);
extern void checkPatternLink(Widget w, int mode);
extern void setPatternLineWidth(void * ptr, int width);

/* print.c */
extern void PrintPopup(Widget w, XtPointer paintArg);
extern void ExternPopup(Widget w, XtPointer paintArg);
extern void checkExternalLink(Widget pw);

/* readRC */
void openTempDir(char *buf);
FILE * openTempFile(char **np);

/* typeConvert.c */
extern void InitTypeConverters(void);

/* operation.c */
extern void takeScreenshot(Widget w, XtPointer junk, XtPointer junk2);
extern void exitPaint(Widget w, XtPointer junk, XtPointer junk2);
extern void setToolIconOnWidget(Widget w);
extern void OperationSet(String names[], int num);
extern int getIndexOp();

/* snapshot.c */
extern void ScreenshotImage(Widget w, XtPointer paintArg, int flag);


/* brushOp.c */
extern void BrushSelect(Widget w);
extern void setBrushIconOnWidget(Widget w);

/* rw/readWriteXPL.c */
extern char * ArchiveFile(char *);
extern void * LoadLayers(char **);

/* rw/readWritePNM.c */
#ifdef __IMAGE_H__
extern int WritePNM(char *file, Image * image);
#endif

extern void mousewheelScroll(Widget w, void * l, XEvent * event, Boolean * flg);
