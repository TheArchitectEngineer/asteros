/*-----------------------------------------------------------------------------
  FmComms.c

  (c) Simon Marlow 1990-1993
  (c) Albert Graef 1994

  support for receiving instructions from other X processes
------------------------------------------------------------------------------*/

#include <X11/Xatom.h>
#include <X11/Intrinsic.h>

#include "Fm.h"
#include "Am.h"
#include "FmComms.h"

Atom xfm_open_window, xfm_update_window, wm_delete_window, wm_protocols,
	 wm_save_yourself, kwm_save_yourself;

void clientMessageHandler(Widget w, XtPointer closure, XEvent *e)
{
  /* The client message handler must be re-entrant because the invokation of
     the callbacks in response to a wm_delete_window message can cause more
     events to be dispatched. We handle this by just ignoring these recursive
     calls. */

  static int in_use = 0;
  XClientMessageEvent *c = (XClientMessageEvent *)e;

  if (in_use || e->type != ClientMessage || (c->message_type != wm_protocols &&
      freeze))
    return;
  in_use = 1;

  if (c->message_type == xfm_open_window)
    ; /**/ /* to be implemented */
  else if (c->message_type == xfm_update_window)
    ; /**/ /* to be implemented */
  else if (c->message_type == wm_protocols) {
	  if (w == aw.shell) {
		  if (c->data.l[0] == wm_delete_window) {
			  appCloseCb(w, file_windows, (XtPointer)NULL);
		  } else {
			  /* must be `save yourself'; they are waiting for
			   * a property change event
			   */
			  XChangeProperty(XtDisplay(w),XtWindow(w),
					  XA_WM_COMMAND, XA_STRING,
					  8,
					  PropModeAppend,
					  NULL,0);
		  }
	  } else {
      FileWindowRec *fw;
      for (fw = file_windows; fw; fw = fw->next)
	if (w == fw->shell) break;
      if (!fw)
	error("Internal error:", "Widget not found in clientMessageHandler");
      else
	fileCloseCb(w, fw, (XtPointer)NULL);
    }
  }

  in_use = 0;
}

void initComms(void)
{
  /* Make up some new atoms */
  xfm_open_window = XInternAtom(XtDisplay(aw.shell), XFM_OPEN_WINDOW,
				 False);
  xfm_update_window = XInternAtom(XtDisplay(aw.shell), XFM_UPDATE_WINDOW,
				  False);
  wm_delete_window = XInternAtom(XtDisplay(aw.shell), WM_DELETE_WINDOW,
				 False);
  wm_protocols = XInternAtom(XtDisplay(aw.shell), WM_PROTOCOLS,
			     False);
  /* participate on this also; kwm works better with it */
  wm_save_yourself = XInternAtom(XtDisplay(aw.shell), WM_SAVE_YOURSELF,
			     False);
  /* let's see, whether kwm is up */
  kwm_save_yourself = XInternAtom(XtDisplay(aw.shell), KWM_SAVE_YOURSELF,
			     False);

  if (xfm_open_window == None || xfm_update_window == None ||
      wm_delete_window == None || wm_protocols == None)
    abortXfm("Couldn't initialize client message handler");
}
