/*
 * Prototypes for all *Op.c files.
 */

/* $Id: ops.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

/* arcOp.c */
void ArcSetMode(int value);
void *ArcAdd(Widget w);
void ArcRemove(Widget w, void *l);
/* blobOp.c */
void *FreehandAdd(Widget w);
void FreehandRemove(Widget w, void *l);
void *FilledFreehandAdd(Widget w);
void FilledFreehandRemove(Widget w, void *l);
/* boxOp.c */
void DrawBox(Display *dpy, Drawable d, GC gc, XRectangle *rect, Boolean fill);
void *BoxAdd(Widget w);
void BoxRemove(Widget w, void *l);
void *FilledBoxAdd(Widget w);
void FilledBoxRemove(Widget w, void *l);
void BoxSetStyle(int mode);
int  BoxGetStyle(void);
void BoxSetParameter(int b_size, float b_ratio);
/* brushOp.c */
Boolean EraseGetMode(void);
void EraseSetMode(Boolean mode);
void BrushSetMode(int mode);
void BrushSetParameters(float opacity);
void DashSetStyle(char *dashstyle);
void *BrushAdd(Widget w);
void BrushRemove(Widget w, void *l);
void *EraseAdd(Widget w);
void EraseRemove(Widget w, void *l);
void *SmearAdd(Widget w);
void SmearRemove(Widget w, void *l);
void BrushInit(Widget toplevel);
void BrushSelect(Widget w);
/* circleOp.c */
void *CircleAdd(Widget w);
void CircleRemove(Widget w, void *p);
void *FilledCircleAdd(Widget w);
void FilledCircleRemove(Widget w, void *p);
void *OvalAdd(Widget w);
void OvalRemove(Widget w, void *p);
void *FilledOvalAdd(Widget w);
void FilledOvalRemove(Widget w, void *p);
void CircleSetStyle(Boolean value);
Boolean CircleGetStyle(void);
/* dynPenOp.c */
void *DynPencilAdd(Widget w);
void DynPencilRemove(Widget w, void *p);
void DynPencilSetParameters(float w, float m, float d);
void DynPencilSetFinish(Boolean flag);
Boolean DynPencilGetFinish();
/* fillOp.c */
void *FillAdd(Widget w);
void FillRemove(Widget w, void *l);
void FillSetMode(int value);
void FillSetTolerance(int value);
void *GradientFillAdd(Widget w);
void GradientFillRemove(Widget w, void *l);
void GradientFillSetMode(int value);
void GradientFillSetParameters(float voffset, float hoffset, float pad,
			       float angle, int steps);
void *FractalFillAdd(Widget w);
void FractalFillRemove(Widget w, void *l);
void FractalFillSetMode(int value);
/* fontOp.c */
void *FontAdd(Widget w);
void FontRemove(Widget w, void *p);
void FontChanged(Widget w);
/* lineOp.c */
void *LineAdd(Widget w);
void LineRemove(Widget w, void *p);
void *ArrowAdd(Widget w);
void ArrowRemove(Widget w, void *p);
void ArrowHeadSetParameters(int t, int s, float a);
/* pencilOp.c */
void *PencilAdd(Widget w);
void PencilRemove(Widget w, void *l);
void *DotPencilAdd(Widget w);
void DotPencilRemove(Widget w, void *l);
/* polygonOp.c */
void *PolygonAdd(Widget w);
void PolygonRemove(Widget w, void *l);
void *FilledPolygonAdd(Widget w);
void FilledPolygonRemove(Widget w, void *p);
void *SelectPolygonAdd(Widget w);
void SelectPolygonRemove(Widget w, void *p);
void *BrokenlineAdd(Widget w);
void BrokenlineRemove(Widget w, void *p);
void PolygonSetParameters(int t, int s, float a);
void PolygonGetParameters(int *t, int *s, float *a);
void CreatePolygonalRegion(Widget w, XPoint *xp, int n);

/* splineOp.c */
void SplineSetMode(int mode);
void *SplineAdd(Widget w);
void SplineRemove(Widget w, void *p);
void *FilledSplineAdd(Widget w);
void FilledSplineRemove(Widget w, void *p);
/* selectOp.c */
void *SelectBoxAdd(Widget w);
void SelectBoxRemove(Widget w, void *p);
void *SelectOvalAdd(Widget w);
void SelectOvalRemove(Widget w, void *p);
void *SelectSplineAdd(Widget w);
void SelectSplineRemove(Widget w, void *p);
void *SelectFreehandAdd(Widget w);
void SelectFreehandRemove(Widget w, void *p);
void SelectSetCutMode(int value);
int SelectGetCutMode(void);
Boolean chromaCut(Widget w, XRectangle * r, Pixmap * mask);
void selectKeyPress(Widget w, void * l, XKeyEvent * event, void * info);
void selectKeyRelease(Widget w, void * l, XKeyEvent * event, void * info);

/* sprayOp.c */
void SpraySetParameters(int r, int d, int sp);
Boolean SprayGetStyle(void);
void SpraySetStyle(Boolean flag);
Boolean MultiGetStyle(void);
void MultiSetStyle(Boolean flag);
Boolean ArrowGetStyle(void);
void ArrowSetStyle(Boolean flag);
void *SprayAdd(Widget w);
void SprayRemove(Widget w, void *p);
