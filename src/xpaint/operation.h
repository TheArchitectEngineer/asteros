/* $Id: operation.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

/* operation.c */
void OperationSelectCall(int n);
void OperationSet(String names[], int num);
void OperationSetPaint(Widget paint);
void OperationAddArg(Arg arg);
void OperationInit(Widget toplevel);
#ifdef __IMAGE_H__
Image *OperationIconImage(Widget w, char *name);
#endif
