/* $Id: graphic.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

typedef void (*GraphicAllProc) (Widget, void *);

/* graphic.c */
void GraphicRemove(Widget paint, XtPointer junk, XtPointer junk2);
void GraphicAdd(Widget paint);
void GraphicAll(GraphicAllProc func, void *data);
void GraphicSetOp(void (*stop) (Widget, void *), void *(*start) (Widget));
void *GraphicGetData(Widget w);
void zoomCallback(Widget w, XtPointer paintArg, XtPointer junk2);
void lineStyleCallback(Widget w, XEvent * event);
void EnableLast(Widget paint);
void EnableRevert(Widget paint);
Widget makeGraphicShell(Widget wid);
Widget graphicCreate(Widget shell, int width, int height, int zoom,
		     Pixmap pix, Colormap cmap, unsigned char *alpha);
void *GraphicGetReaderId(Widget paint);
void GraphicOpenFile(Widget w, XtPointer fileArg, XtPointer imageArg);
Widget GraphicOpenFileZoom(Widget w, char *file, XtPointer imageArg, int zoom);
void GraphicCreate(Widget wid, int value);
void setEditMenuPasteSensitive();
void * PopupMenu(char *title, int n, void *values, void * proc);
void RefreshWidget(Widget w);
void ImageRegionProcess(Widget paint, void * func);
WidgetList InitializedWidgetList();
void CheckMarkItems(Widget paint, WidgetList wlist);
void SetAlphaMode(Widget w, int mode);
