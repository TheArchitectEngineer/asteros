#include <vms_x_fix.h>
#undef XtDisplay
#undef XtScreen
#undef XtWindow
#undef XtClass
#undef XtIsRealized
#undef XtParent

#define fork vfork

#define EDITOR "XPAINT$EDITOR"
#define SHAREDIR "/XPAINT$SHARE"
#define XPAINT_VERSION "2.8.0"
#define XAPPLOADDIR "/XPAINT$APPLOADDIR"
#define PRINTCAP "PRINTCAP"
