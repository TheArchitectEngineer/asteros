/* $Id: protocol.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

typedef void (*DestroyCallbackFunc) (Widget, void *, XEvent *);

/* protocol.c */
void AddDestroyCallback(Widget w, DestroyCallbackFunc func, void *data);
void StateSetBusyWatch(Boolean flag);
void StateSetBusy(Boolean flag);
void StateShellBusy(Widget w, Boolean flag);
void StateAddParent(Widget w, Widget parent);
void StateTimeStep(void);
