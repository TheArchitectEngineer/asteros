#ifndef _Paint_h
#define _Paint_h

/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, 1993, David Koblas (koblas@netcom.com)            | */
/* | Copyright 1995, 1996 Torsten Martinsen (bullestock@dk-online.dk)  | */
/* |                                                                   | */
/* | Permission to use, copy, modify, and to distribute this software  | */
/* | and its documentation for any purpose is hereby granted without   | */
/* | fee, provided that the above copyright notice appear in all       | */
/* | copies and that both that copyright notice and this permission    | */
/* | notice appear in supporting documentation.  There is no           | */
/* | representations about the suitability of this software for        | */
/* | any purpose.  this software is provided "as is" without express   | */
/* | or implied warranty.                                              | */
/* |                                                                   | */
/* +-------------------------------------------------------------------+ */

/* $Id: Paint.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

/****************************************************************
 *
 * Paint widget
 *
 ****************************************************************/

/* Resources:

   Name              Class              RepType         Default Value
   ----              -----              -------         -------------
   background        Background         Pixel           XtDefaultBackground
   border            BorderColor        Pixel           XtDefaultForeground
   borderWidth       BorderWidth        Dimension       1
   destroyCallback   Callback           Pointer         NULL
   height            Height             Dimension       0
   mappedWhenManaged MappedWhenManaged  Boolean         True
   sensitive         Sensitive          Boolean         True
   width             Width              Dimension       0
   x                 Position           Position        0
   y                 Position           Position        0

 */

/* define any special resource names here that are not in <X11/StringDefs.h> */

#define XtNpaintResource "paintResource"
#define XtNpattern	 "pattern"
#define XtNmenubar	 "menubar"
#define XtNfullmenu	 "fullmenu"
#define XtNgrid	 	 "grid"
#define XtNgridmode 	 "gridmode"
#define XtNgridcolor 	 "gridcolor"
#define XtNinterpolation "interpolation"
#define XtNtransparent   "transparent"
#define XtNsnapX 	 "snapX"
#define XtNsnapY 	 "snapY"
#define XtNsnapOn 	 "snapOn"
#define XtNlineWidth	 "lineWidth"
#define XtNfatBack	 "fatBack"
#define XtNsizeChanged	 "sizeChanged"
#define XtNundoSize	 "undoSize"
#define XtNlocked 	 "locked"
#define XtNzoom	 	 "zoom"
#define XtNpaint	 "paint"
#define XtNzoomX	 "zoomX"
#define XtNzoomY	 "zoomY"
#define XtNdrawWidth	 "drawWidth"
#define XtNdrawHeight	 "drawHeight"
#define XtNcompress	 "compress"
#define XtNdirty	 "dirty"
#define XtNfillRule	 "fillRule"
#define XtNregionCallback	"regionSetCallback"
#define XtNdownX		"downX"
#define XtNdownY		"downY"
#define XtNlineForeground	"lineForeground"
#define XtNlinePattern		"linePattern"
#define XtNlineFillRule		"lineFillRule"
#ifndef XtNreadOnly
#define XtNreadOnly		"readOnly"
#endif
#ifndef XtNcursor
#define XtNcursor		"cursor"
#endif
#define XtNfilename		"filename"
#define XtNmenuwidgets		"menuwidgets"

#define XtCPaintResource "PaintResource"
#define XtCPattern	 "Pattern"
#define XtCMenubar	 "Menubar"
#define XtCFullMenu	 "FullMenu"
#define XtCGrid		 "Grid"
#define XtCGridMode	 "GridMode"
#define XtCGridColor	 "GridColor"
#define XtCInterpolation "Interpolation"
#define XtCTransparent   "Transparent"
#define XtCSnapX	 "SnapX"
#define XtCSnapY	 "SnapY"
#define XtCSnapOn	 "SnapOn"
#define XtCLineWidth	 "LineWidth"
#define XtCFatBack	 "FatBack"
#define XtCSizeChanged	 "SizeChanged"
#define XtCLocked	 "Locked"
#define XtCUndoSize	 "UndoSize"
#define XtCZoom	  	 "Zoom"
#define XtCPaint	 "Paint"
#define XtCZoomX	 "ZoomX"
#define XtCZoomY	 "ZoomY"
#define XtCDrawWidth	 "DrawWidth"
#define XtCDrawHeight	 "DrawHeight"
#define XtCCompress	 "Compress"
#define XtCDirty	 "Dirty"
#define XtCEditable	 "Editable"
#define XtCFillRule	 "FillRule"
#define XtCRegionCallback	"RegionSetCallback"
#define XtCDownX		"DownX"
#define XtCDownY		"DownY"
#define XtCLineForeground	"LineForeground"
#define XtCLinePattern		"LinePattern"
#define XtCLineFillRule		"LineFillRule"
#ifndef XtCReadOnly
#define XtCReadOnly		"ReadOnly"
#endif
#define XtCfilename		"Filename"
#define XtCmenuwidgets		"Menuwidgets"

#define XtRXFTFontStruct "XFTFontStruct"

/* declare specific PaintWidget class and instance datatypes */

typedef struct _PaintClassRec *PaintClass;
typedef struct _PaintRec *PaintWidget;

/* declare the class constant */

extern WidgetClass paintWidgetClass;

/*
**  Operation callback information
 */

typedef struct {
    Pixmap pixmap;
    unsigned char *alpha;
} AlphaPixmap;

typedef enum {
    opPixmap = 0x01, opWindow = 0x02
} OpSurface;

typedef struct {
    int refCount;
    OpSurface surface;
    Drawable drawable;
    GC first_gc, second_gc, base_gc;
    void *data;
    int isFat;
    int x, y;
    int realX, realY;
    int zoom;
    Pixmap base;
} OpInfo;

typedef float pwMatrix[2][2];

/* Brush Item structure */
typedef struct {
    int width, height;
    int numpixels;		/* Total number of set pixels */
    char *brushbits;
    Pixmap pixmap, bw;
    Cursor cursor;
    Widget icon;
} BrushItem;

typedef void (*OpEventProc) (Widget, void *, XEvent *, OpInfo *);
void OpRemoveEventHandler(Widget, int, int, Boolean, OpEventProc, void *);
void OpAddEventHandler(Widget, int, int, Boolean, OpEventProc, void *);
void UndoStart(Widget, OpInfo *);
void UndoStartRectangle(Widget, OpInfo *, XRectangle *);
void UndoStartPoint(Widget, OpInfo *, int, int);
void UndoSetRectangle(Widget, XRectangle *);
void UndoGrow(Widget, int, int);

AlphaPixmap PwUndoStart(Widget, XRectangle *);
void PwUndoSetRectangle(Widget, XRectangle *);
void PwUndoAddRectangle(Widget, XRectangle *);
void Undo(Widget);
void Redo(Widget);
void UndoInitialize(PaintWidget pw, int n);

#define PwZoomParent	((int) -10000)

/*
**  Public functions
 */

void PwUpdate(Widget, XRectangle *, Boolean);
void PwUpdateDrawable(Widget, Drawable, XRectangle *);
void PwSetDrawn(Widget, Boolean);
void PwGetPixmap(Widget, Pixmap *, int *, int *);
void PwPutPixmap(Widget w, Pixmap pix);
XRectangle *PwScaleRectangle(Widget, XRectangle *);
XImage *PwGetImage(Widget, XRectangle *);
void zoomUpdate(PaintWidget pw, Boolean isExpose, XRectangle * rect);
Pixel AlphaTwistPixel(Pixel p, unsigned char *u);
Pixel AlphaTwistPixel8(Pixel p, unsigned char *u);
void AlphaTwistImage(PaintWidget w, XImage *xim, 
                     unsigned char *alpha, int width,
                     int dx, int dy, int ax, int ay);

/*
**  Region routines
 */
void PwDrawVisibleGrid(PaintWidget, Widget, Boolean, int, int, int, int);
void PwRegionExpose(Widget, PaintWidget, XEvent *, Boolean *);
void PwRegionSet(Widget, XRectangle *, Pixmap, Pixmap);
void PwRegionSetRawPixmap(Widget, Pixmap);
void PwRegionTear(Widget);
Boolean PwRegionGet(Widget, Pixmap *, Pixmap *);
Pixmap PwGetRawPixmap(Widget);
void PwRegionSetMatrix(Widget, pwMatrix);
void PwRegionAppendMatrix(Widget, pwMatrix);
void PwRegionAddScale(Widget, float *, float *);
void PwRegionSetScale(Widget, float *, float *);
void PwRegionReset(Widget, Boolean);
void PwRegionClear(Widget);
void PwRegionFinish(Widget, Boolean);
Boolean PwRegionOff(Widget w, Boolean flag);

typedef Pixmap(*pwRegionDoneProc) (Widget, XImage *, pwMatrix);

void PwRegionSetDone(Widget, pwRegionDoneProc);

#endif				/* _Paint_h */
