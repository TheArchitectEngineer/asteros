#ifndef FmSelection_h
#define FmSelection_h

Boolean FmOwnSelection(FileWindowRec *fw, Time time);

void FmDisownSelection(FileWindowRec *fw);
void FmSelectionInit(Display *di);

Boolean FmIsSelectionOwner(FileWindowRec *fw);

#endif
