/* $Id: color.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

/* chroma.c */
void ChromaDialog(Widget w, Palette * map);
void GetChromaBackground(int *rb, int *gb, int *bb);
void GetChromaDelta(int *rd, int *gd, int *bd);

/* color.c */
Widget ColorPickerPalette(Widget parent, Palette * map, Pixel * pixval);
Widget ColorPicker(Widget parent, Colormap cmap, Pixel * pixval);
Widget ColorPickerGetSelectWidget(Widget w);
Widget ColorPickerGetAsBgWidget(Widget w);
void ColorPickerShell(Widget w);
void ColorPickerSetXColor(Widget w, XColor * xcol);
void ColorPickerSetPixel(Widget w, Pixel pix);
void ColorPickerSetFunction(Widget w, XtCallbackProc func, XtPointer data);
Pixel ColorPickerGetPixel(Widget w);
XColor *ColorPickerGetXColor(Widget w);
void ColorPickerUpdateMap(Widget w, Palette * map);

/* colorEdit.c */
void ColorEditor(Widget w, Pixel pixel, Palette * map, Boolean allowWrite,
		 XtCallbackProc okProc, XtPointer closure);
