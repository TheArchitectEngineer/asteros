/* $Id: region.h,v 1.7 2005/03/20 20:15:32 demailly Exp $ */

/* cutCopyPaste.c */
void StdCopyCallback(Widget w, XtPointer paintArg, String * nm, XEvent * event);
void StdPasteCallback(Widget w, XtPointer paintArg, XtPointer junk);
void StdScreenshotCallback(Widget w, XtPointer paintArg, XtPointer junk);
void StdMemorySetCallback(Widget w, XtPointer paintArg, XtPointer junk);
void StdMemoryRecallCallback(Widget w, XtPointer paintArg, XtPointer junk);
void StdMemoryRemoveCallback(Widget w, XtPointer paintArg, XtPointer junk);
void StdClearCallback(Widget w, XtPointer paintArg, XtPointer junk2);
void StdCutCallback(Widget w, XtPointer paintArg, String * nm, XEvent * event);
void StdDuplicateCallback(Widget w, XtPointer paintArg, XtPointer junk2);
void StdSelectAllCallback(Widget w, XtPointer paintArg, XtPointer junk2);
void StdEraseAllCallback(Widget w, XtPointer paintArg, XtPointer junk2);
void StdUndoCallback(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRedoCallback(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRefreshCallback(Widget w, XtPointer paintArg, XtPointer junk2);
#ifdef __IMAGE_H__
void ClipboardSetImage(Widget w, Image * image);
void MemorySetImage(Widget w, Image * image);
#endif
void StdRegionFlipX(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionFlipY(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionDelimit(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionComplement(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionClone(Widget w, XtPointer paintArg, XtPointer junk2);
void StdLastImgProcess(Widget paint);

void StdRegionInvert(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionSharpen(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionSmooth(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionEdge(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionEmboss(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionOilPaint(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionAddNoise(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionSpread(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionBlend(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionPixelize(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionDespeckle(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionNormContrast(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionSolarize(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionModifyRGB(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionGrey(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionBWMask(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionQuantize(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionExpand(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionDownscale(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionUserDefined(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionDirFilt(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionUndo(Widget w, XtPointer paintArg, XtPointer junk2);
void StdRegionTilt(Widget w, XtPointer paintArg, XtPointer junk2);
#ifdef __IMAGE_H__
Image *ImgProcessSetup(Widget paint);
Image *ImgProcessFinish(Widget paint, Image * in, Image * (*func) (Image *));
#endif
void ccpAddUndo(Widget w, Widget paint);
void ccpAddRedo(Widget w, Widget paint);
void ccpAddRefresh(Widget w, Widget paint);
void ccpAddCut(Widget w, Widget paint);
void ccpAddCopy(Widget w, Widget paint);
void ccpAddPaste(Widget w, Widget paint);
void ccpAddClear(Widget w, Widget paint);
void ccpAddScreenshot(Widget w, Widget paint);
void ccpAddDuplicate(Widget w, Widget paint);
void ccpAddSelectAll(Widget w, Widget paint);
void ccpAddEraseAll(Widget w, Widget paint);
void ccpAddSaveRegion(Widget w, Widget paint);
void ccpAddCloneRegion(Widget w, Widget paint);
void ccpAddOCRRegion(Widget w, Widget paint);
void ccpAddRegionFlipX(Widget w, Widget paint);
void ccpAddRegionFlipY(Widget w, Widget paint);
void ccpAddRegionInvert(Widget w, Widget paint);
void ccpAddRegionSharpen(Widget w, Widget paint);
void ccpAddRegionEdge(Widget w, Widget paint);
void ccpAddRegionEmboss(Widget w, Widget paint);
void ccpAddRegionOilPaint(Widget w, Widget paint);
void ccpAddRegionNoise(Widget w, Widget paint);
void ccpAddRegionBlend(Widget w, Widget paint);
void ccpAddStdPopup(Widget paint, void *info);

void RegionCrop(PaintWidget paint);
void RegionMove(PaintWidget pw, int dx, int dy);
void RegionTransparency(PaintWidget pw);

