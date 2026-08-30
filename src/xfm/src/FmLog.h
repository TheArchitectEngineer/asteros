#ifndef FmLog_h
#define FmLog_h

Widget FmCreateLog(Widget parent, XFontStruct *font);
extern Widget fmLogPopupShell;
void   logPopup(Widget, FileWindowRec *, XtPointer);

#endif
