!***********************************************************************
! Application defaults for xfm.
! Most of these are the defaults anyway, but they are listed here so you can
! change them if necessary.
!***********************************************************************

! Search paths for icon files
Xfm*bitmapPath: LIBDIR/bitmaps:/usr/include/X11/bitmaps
#ifndef ENHANCE_3DICONS
Xfm*pixmapPath: LIBDIR/pixmaps:/usr/include/X11/pixmaps
#else
Xfm*pixmapPath: LIBDIR/icons:LIBDIR/pixmaps:/usr/include/X11/pixmaps
#endif /* ENHANCE_3DICONS */

! Default applications file to be loaded at startup
Xfm*applicationDataFile: ~/.xfm/Apps

! Directory in which new application files are created
Xfm*applicationDataDir: ~/.xfm

! Clipboard for Cut/Copy/Paste operations in application window
Xfm*applicationDataClip: ~/.xfm/.XfmClip

! Configuration file (where the file types are specified)
Xfm*configFile: ~/.xfm/xfmrc

! Device configuration file (floppies and such)
Xfm*devFile: ~/.xfm/xfmdev

! Magic headers configuration file (files you recognize by their header)
Xfm*magicFile: ~/.xfm/magic

! Double click time in milliseconds
Xfm*doubleClickTime:               300

! Time interval in milliseconds for automatic folder updates (set to zero to
! disable this feature)
Xfm*updateInterval:		10000
#ifdef ENHANCE_PERMS
! stat the directory every hardUpdateTicks*updateInterval millisecs
! to discover changed modes or links to deleted files
! (set to zero to disable this feature)
Xfm*hardUpdateTicks:		10
#endif
#ifdef ENHANCE_USERINFO
!You can specify the name of the shell you want XFM to use here.
!Only if this fails (eg. by commenting out) $SHELL will be used.
Xfm*shellCommand: /bin/sh
#endif
! Just two examples -- you should not need this
!Xfm*BourneShells: AUTO
!Xfm*BourneShells: /bin/sh, /usr/local/bin/bash
#ifdef ENHANCE_SELECTION
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!The separator (arbitrary string) to be used when transferring
!the paths of the selected files to the requestor of the PRIMARY
!selection. This string will be used between individual paths,
!the default being a space character (note the quoting by '\')
!
!Xfm*selectionPathsSeparator:\ 
#endif
#ifdef ENHANCE_LOG
! Some settings for the log window
Xfm*fmLog.title: Xfm output log
! 
! You can enable the horizontal scrollbar if you want:
Xfm*fmLog*Text.scrollHorizontal: whenNeeded
!
! The initial value of the 'auto popup' toggle which
! determines if the Log window pops up automatically
! when new data is available.
Xfm*fmLog*Auto Flag.state: True
#define LOG_TRANSLATION Ctrl	<Key>l			: notify("*view*show log")\n
#else
#define LOG_TRANSLATION
#endif
!***********************************************************************
! Preferences
!***********************************************************************

! Geometry for application and initial file manager windows
#ifndef ENHANCE_3DICONS
Xfm.Geometry:			265x515+565+130
#else
Xfm.Geometry:			240x345+565+130
#endif /* ENHANCE_3DICONS */
Xfm.initGeometry:		515x345+5+130
! Geometry for subsequent file windows
Xfm.file window.Geometry:	515x345
#ifdef ENHANCE_LOG
! The dimensions of the Log window on startup
Xfm*fmLog.width:515
Xfm*fmLog.height:200
#endif
#ifdef XPM
#ifdef ENHANCE_CMAP
! Tolerance of allocated colors by Xpmlib. The lower the number the closer
! the colors will be to the requested value --- probably at the cost of
! additional entries in the colormap. A value of 40000 gives good results.
colorCloseness: 40000
#endif
#endif


! If you do not want to go chasing dialog popups around the screen, but rather
! have them in a fixed location, uncomment the following (recommended)
Xfm*TransientShell.geometry: +210+295
 
! The following removes borders from the main menu buttons (my own preference)
Xfm*file window*button box.?.borderWidth: 0

! NOTE: The background color of icons is determined from the Xfm.background
! resource. Make sure that this resource is always given a value appropriate
! for *both* application and file windows.

! The background color for file and application windows
Xfm*background: grey77

! The foreground color for icon toggles; this is also used when an icon toggle
! is highlighted or marked
#ifdef ENHANCE_SELECTION
Xfm.highlightColor: midnight blue
#else
Xfm*viewport.icon box*Toggle.foreground: midnight blue
#ifdef USE_NEW_WIDGETS
Xfm*TextFileList.highlightColor: midnight blue
Xfm*IconFileList.highlightColor: midnight blue
#endif
#endif

#ifdef ENHANCE_LOG
! Some nice settings for the log window
xfm*fmLog*background:			white
xfm*fmLog.form.background:		grey77
xfm*fmLog*Command*background:		grey77
xfm*fmLog*Label*background:		grey77
xfm*fmLog*Toggle*background:		grey77
xfm*fmLog*Scrollbar*background:		grey77
#endif

! Use these only if you  have the 75dpi fonts. (recommended)
Xfm*boldFont:			-adobe-helvetica-bold-r-*-*-12-*
Xfm*iconFont:			-adobe-helvetica-medium-r-*-*-10-*
Xfm*buttonFont:			-adobe-helvetica-medium-r-*-*-12-*
Xfm*menuFont:			-adobe-helvetica-medium-r-*-*-12-*
Xfm*labelFont:			-adobe-helvetica-medium-r-*-*-12-*
Xfm*statusFont:			-adobe-helvetica-medium-r-*-*-12-*
#ifdef ENHANCE_LOG
! The font for the log window's text widget
! you might like afixed width font
Xfm*fmLog*AsciiSink.font:	-*-lucidatypewriter-medium-r-*-*-12-*
#endif
! This must be a fixed width font
Xfm*cellFont:			fixed

! Specify the sizes of the different icon toggles. The following values
! are appropriate for the icons supplied with xfm.
#ifndef ENHANCE_3DICONS
Xfm*appIconWidth: 72
Xfm*appIconHeight: 60
#else
Xfm*appIconWidth: 48
Xfm*appIconHeight: 40
#endif /* ENHANCE_3DICONS */
Xfm*fileIconWidth: 48
Xfm*fileIconHeight: 40
Xfm*treeIconWidth: 48
Xfm*treeIconHeight: 32

! Confirmation for various operations
Xfm*confirmDeletes: 		true
Xfm*confirmDeleteFolder: 	true
Xfm*confirmCopies: 		true
Xfm*confirmMoves: 		true
Xfm*confirmOverwrite:		true
Xfm*confirmQuit:		true

! Echo actions on stderr (useful for debugging purposes)
Xfm*echoActions:		false
#ifdef USE_NEW_WIDGETS
! What should be shown by the text view
! (can be changed interactively by the options menu)
#else
! Directory display in Text type
#endif
Xfm*showOwner: true
Xfm*showDate: true
Xfm*showPermissions: true
Xfm*showLength: true
#ifdef USE_NEW_WIDGETS
Xfm*showInode: false
Xfm*showType:  true
Xfm*showLinks: true
Xfm*showGroup: true
#endif
! The type of the first and subsequent file windows
! valid values are Tree, Icons and Text
Xfm*initialDisplayType:	 	Icons
Xfm*defaultDisplayType:		Icons

! The type of sorting used by default
! valid values are SortBy{Name,Size,Date}
Xfm*defaultSortType: 		SortByName

! The default editor to use
Xfm*defaultEditor: 		exec emacs

! The default viewer to use
Xfm*defaultViewer: 		exec xless

! The default xterm to use
Xfm*defaultXterm: 		exec xterm

#ifdef ENHANCE_HISTORY
! Some settings for the path history menu
Xfm*fm_history.label: Path history
Xfm*fm_history*SmeLine.lineWidth: 1

! This is the max. number of entries
Xfm*historyMaxN: 30
#define HIST_TRANSLATION(do_before,do_after) <Btn3Down>:do_before FmUpdateHistory(fm_history)MenuPopup(fm_history) do_after\n
#else
#define HIST_TRANSLATION(do_before,do_after)
#endif
!***********************************************************************
! Miscellaneous settings (see also FmMain.c)
! Normally you won't have to change these.
!***********************************************************************

Xfm*Command.cursor: hand2
Xfm*MenuButton.cursor: hand2
Xfm*viewport.forceBars: False
!Xfm.awform.viewport.borderWidth : 0
Xfm*popup form*bitmap.borderWidth : 0
Xfm*popup form*label.borderWidth : 0
Xfm*button box.orientation: horizontal
Xfm*button box.borderWidth: 0
!Xfm*file window*viewport.borderWidth: 0
Xfm*viewport.icon box*Label.borderWidth : 0
Xfm*viewport.icon box.Command.borderWidth : 0
Xfm*viewport.icon box.Form.borderWidth : 0
Xfm*viewport.icon box*Toggle.borderWidth : 1
#ifdef USE_NEW_WIDGETS
Xfm*viewport.TextFileList.borderWidth: 1
Xfm*viewport.TextFileList.entrySep: 2
Xfm*viewport.TextFileList.topSep: 2
Xfm*viewport.TextFileList.leftSep: 2
Xfm*viewport.TextFileList.tabSep: 4
Xfm*viewport.TextFileList.dragTimeout: 200
Xfm*viewport.IconFileList.borderWidth: 1
Xfm*viewport.IconFileList.entrySep: 2
Xfm*viewport.IconFileList.topSep: 2
Xfm*viewport.IconFileList.leftSep: 2
Xfm*viewport.IconFileList.labelSep: 2
Xfm*viewport.IconFileList.dragTimeout: 200
#endif
Xfm*chmod*Label.borderWidth : 0
Xfm*info*Label.borderWidth : 0
Xfm*error*Label.borderWidth : 0
Xfm*confirm*Label.borderWidth : 0
#ifndef ENHANCE_TXT_FIELD
Xfm*Text*translations : #override \n\
 <Key>Return: no-op() \n\
 <Key>Linefeed : no-op() \n\
 Ctrl<Key>J : no-op() \n
Xfm*Text*baseTranslations : #override \n\
 <Key>Return: no-op() \n\
 <Key>Linefeed : no-op() \n\
 Ctrl<Key>J : no-op() \n
#else
Xfm*TextField.baseTranslations:#override\n\
HIST_TRANSLATION(FocusSet(), ) \
Shift	<Key>Tab:FocusTraverse(b)\n\
	<Key>Tab:FocusTraverse()\n\
	<FocusIn>:Detail_NotifyAncestor(CursorState,a)\n\
	<FocusOut>:Detail_NotifyAncestor(CursorState,i)\n\
	<Btn1Up>:FocusSet()\
	         HighlightExtend()\
	         MakeSelection(PRIMARY)\n\
	<Btn2Up>:FocusSet()\
		 MoveCursor()\
		 InsertSelection(PRIMARY)\n\
	<Enter>:\n\
	<Leave>:\n\
	<Key>Escape: \n\
	<Key>Return: \n\
	<Key>Linefeed:\n
#endif
#ifdef XAW3D
! shadow widths for Xaw3d
#ifdef ENHANCE_HISTORY
Xfm*file window*folderlabel.shadowWidth: 2
Xfm*file window*folderlabel.borderWidth: 0
#endif
Xfm*icon box*shadowWidth: 0
Xfm*MenuButton.highlightThickness: 0
Xfm*Command.highlightThickness: 0
Xfm*Toggle.highlightThickness: 0
Xfm*Label.ShadowWidth: 0
Xfm*Label*borderWidth: 0
Xfm*status*shadowWidth: 2
#endif
#ifdef ENHANCE_TRANSLATIONS
Xfm*file window*icon box*tree_icon.baseTranslations: #override\n\
  <Enter>             : fileHighlight()\n\
  <Leave>             : resetCursor()\n\
  Shift <Btn1Down>,<Btn1Up> : fileToggle()\n\
  <Btn1Down>,<Btn1Up> : fileSelect()\n\
  <Btn1Down>,<Leave>  : fileBeginDrag(1,move)\n\
  <Btn2Down>,<Btn2Up> : fileToggle()\n\
  <Btn2Down>,<Leave>  : fileBeginDrag(2,copy)\n\
  <Btn3Down>          : dirPopup()\n

Xfm*awform*icon box*icon.baseTranslations:#override\n\
    <Enter>             : appMaybeHighlight()\n\
    <Leave>             : unhighlight()\n\
    <Btn1Up>(2)         : runApp()\n\
    <Btn1Down>,<Btn1Up> : appSelect()\n\
    <Btn1Down>,<Leave>  : appBeginDrag(1,move)\n\
    <Btn2Down>,<Btn2Up> : appToggle()\n\
    <Btn2Down>,<Leave>  : appBeginDrag(2,copy)\n

Xfm*awform*icon box.baseTranslations: #override\n\
    <Btn2Up>            : dummy()\n\
    <Btn3Up>            : dummy()\n\
    <Btn3Down>          : appPopup()\n
#ifdef USE_NEW_WIDGETS
Xfm*file window*folderlabel.baseTranslations: #override\n\
  <Enter>	      : trackCursor()\n\
  <Leave>	      : trackCursor()\n\
HIST_TRANSLATION(,) \
  <Btn1Up>(2)         : fileRefresh()\n
Xfm*file window*viewport.baseTranslations:#override\n\
  <Enter>: trackCursor()\n <Leave>: trackCursor()\n

Xfm*IconFileList*baseTranslations: #override\n\
        <Enter>	        	: dispatchDirExeFile(fileHighlight,fileHighlight,fileMaybeHighlight,"")\n\
        <Leave>	        	: resetCursor()\n\
 Shift  <Btn1Down>,<Btn1Up> 	: fileToggle()\n\
        <Btn1Up>(2)           	: dispatchDirExeFile(fileOpenDir,fileExecFile,fileExecAction,"")\n\
        <Btn1Down>,<Btn1Up>   	: fileSelect()\n\
        <Btn1Down>,<Motion>   	: fileBeginDrag(1,move)\n\
        <Btn2Down>,<Btn2Up>   	: fileToggle()\n\
        <Btn2Down>,<Motion>   	: fileBeginDrag(2,copy)\n\
        <Btn3Down>            	: dispatchDirExeFilePopup(dirPopup,filePopup,filePopup,"")\n\
Shift   <Key>n			: notify(*folder*new)\n\
Shift   <Key>g			: notify(*folder*goto)\n\
Shift   <Key>h			: notify(*folder*home)\n\
Shift   <Key>u			: notify(*folder*up)\n\
Shift   <Key>c			: notify(*folder*clone)\n\
Shift   <Key>q			: notify(*folder*close)\n\
Ctrl	<Key>r			: notify(*view*tree)\n\
Ctrl	<Key>i			: notify(*view*icons)\n\
Ctrl	<Key>t			: notify(*view*text)\n\
Ctrl	<Key>n			: notify("*view*sort by name")\n\
Ctrl	<Key>s			: notify("*view*sort by size")\n\
Ctrl	<Key>d			: notify("*view*sort by mtime")\n\
Ctrl	<Key>f			: notify(*view*filter)\n\
Ctrl	<Key>h			: notify("*view*hide folders")\n\
Ctrl	<Key>m			: notify("*view*mix folders/files")\n\
Ctrl	<Key>u			: notify("*view*show hidden files")\n\
LOG_TRANSLATION \
	<Key>n			: notify(*file*new)\n\
 	<Key>m			: notify(*file*move)\n\
 	<Key>c			: notify(*file*copy)\n\
 	<Key>l			: notify(*file*link)\n\
 	<Key>d			: notify(*file*delete)\n\
	<Key>Delete		: notify(*file*delete)\n\
	<Key>BackSpace		: notify(*file*delete)\n\
 	<Key>s			: notify(*file*select)\n\
 	<Key>a			: notify("*file*select all")\n\
 	<Key>u			: notify("*file*deselect all")\n\
 	<Key>o			: notify("*file*own Selection")\n\
 	<Key>x			: notify(*file*xterm)\n\
 	<Key>q			: notify(*file*quit)\n

Xfm*TextFileList*baseTranslations: #override\n\
        <Enter>	        	: dispatchDirExeFile(fileHighlight,fileHighlight,fileMaybeHighlight,"")\n\
        <Leave>	        	: resetCursor()\n\
 Shift  <Btn1Down>,<Btn1Up> 	: fileToggle()\n\
        <Btn1Up>(2)           	: dispatchDirExeFile(fileOpenDir,fileExecFile,fileExecAction,"")\n\
        <Btn1Down>,<Btn1Up>   	: fileSelect()\n\
        <Btn1Down>,<Motion>   	: fileBeginDrag(1,move)\n\
        <Btn2Down>,<Btn2Up>   	: fileToggle()\n\
        <Btn2Down>,<Motion>   	: fileBeginDrag(2,copy)\n\
        <Btn3Down>            	: dispatchDirExeFilePopup(dirPopup,filePopup,filePopup,"")\n\
Shift   <Key>n			: notify(*folder*new)\n\
Shift   <Key>g			: notify(*folder*goto)\n\
Shift   <Key>h			: notify(*folder*home)\n\
Shift   <Key>u			: notify(*folder*up)\n\
Shift   <Key>c			: notify(*folder*clone)\n\
Shift   <Key>q			: notify(*folder*close)\n\
Ctrl	<Key>r			: notify(*view*tree)\n\
Ctrl	<Key>i			: notify(*view*icons)\n\
Ctrl	<Key>t			: notify(*view*text)\n\
Ctrl	<Key>n			: notify("*view*sort by name")\n\
Ctrl	<Key>s			: notify("*view*sort by size")\n\
Ctrl	<Key>d			: notify("*view*sort by mtime")\n\
Ctrl	<Key>f			: notify(*view*filter)\n\
Ctrl	<Key>h			: notify("*view*hide folders")\n\
Ctrl	<Key>m			: notify("*view*mix folders/files")\n\
Ctrl	<Key>u			: notify("*view*show hidden files")\n\
LOG_TRANSLATION \
	<Key>n			: notify(*file*new)\n\
 	<Key>m			: notify(*file*move)\n\
 	<Key>c			: notify(*file*copy)\n\
 	<Key>l			: notify(*file*link)\n\
 	<Key>d			: notify(*file*delete)\n\
	<Key>Delete		: notify(*file*delete)\n\
	<Key>BackSpace		: notify(*file*delete)\n\
 	<Key>s			: notify(*file*select)\n\
 	<Key>a			: notify("*file*select all")\n\
 	<Key>u			: notify("*file*deselect all")\n\
 	<Key>o			: notify("*file*own Selection")\n\
 	<Key>x			: notify(*file*xterm)\n\
 	<Key>q			: notify(*file*quit)\n
#else
Xfm*file window*folderlabel.baseTranslations: #override\n\
HIST_TRANSLATION(,)\
  <Btn1Up>(2)         : fileRefresh()\n
Xfm*file window*icon box*file_icon.baseTranslations: #override\n\
  <Enter>             : fileMaybeHighlight()\n\
  <Leave>             : unhighlight()\n\
  Shift <Btn1Down>,<Btn1Up> : fileToggle()\n\
  <Btn1Up>(2)         : fileExecAction()\n\
  <Btn1Down>,<Btn1Up> : fileSelect()\n\
  <Btn1Down>,<Leave>  : fileBeginDrag(1,move)\n\
  <Btn2Down>,<Btn2Up> : fileToggle()\n\
  <Btn2Down>,<Leave>  : fileBeginDrag(2,copy)\n\
  <Btn3Down>          : filePopup()\n

Xfm*file window*icon box*dir_icon.baseTranslations: #override\n\
  <Enter>             : fileHighlight()\n\
  <Leave>             : resetCursor()\n\
  Shift <Btn1Down>,<Btn1Up> : fileToggle()\n\
  <Btn1Down>,<Btn1Up> : fileSelect()\n\
  <Btn1Down>,<Leave>  : fileBeginDrag(1,move)\n\
  <Btn1Up>(2)         : fileOpenDir()\n\
  <Btn2Down>,<Btn2Up> : fileToggle()\n\
  <Btn2Down>,<Leave>  : fileBeginDrag(2,copy)\n\
  <Btn3Down>          : dirPopup()\n

Xfm*file window*icon box*exe_icon.baseTranslations: #override\n\
  <Enter>             : fileHighlight()\n\
  <Leave>             : resetCursor()\n\
  Shift <Btn1Down>,<Btn1Up> : fileToggle()\n\
  <Btn1Up>(2)         : fileExecFile()\n\
  <Btn1Down>,<Btn1Up> : fileSelect()\n\
  <Btn1Down>,<Leave>  : fileBeginDrag(1,move)\n\
  <Btn2Down>,<Btn2Up> : fileToggle()\n\
  <Btn2Down>,<Leave>  : fileBeginDrag(2,copy)\n\
  <Btn3Down>          : filePopup()\n

Xfm*file window*icon box*other_icon.baseTranslations: #override\n\
  <Enter>             : fileHighlight()\n\
  <Leave>             : resetCursor()\n\
  Shift <Btn1Down>,<Btn1Up> : fileToggle()\n\
  <Btn1Up>(2)         : fileExecFile()\n\
  <Btn1Down>,<Btn1Up> : fileSelect()\n\
  <Btn1Down>,<Leave>  : fileBeginDrag(1,move)\n\
  <Btn2Down>,<Btn2Up> : fileToggle()\n\
  <Btn2Down>,<Leave>  : fileBeginDrag(2,copy)\n\
  <Btn3Down>          : filePopup()\n

! This is a hack to get the icon box to recognise button events 
Xfm*file window*icon box.baseTranslations: #override\n\
    <Btn2Up> : dummy()\n\
    <Btn3Up> : dummy()\n
#endif
#ifdef ENHANCE_POP_ACCEL
Xfm*button box*cancel.accelerators:#override\n\
<ClientMessage>WM_PROTOCOLS: set() notify() unset()\n\
<Key>Escape: set() notify() unset()\n

Xfm*about*button box*ok.accelerators:#override\n\
<ClientMessage>WM_PROTOCOLS: set() notify() unset()\n\
<Key>Escape: set() notify() unset()\n

Xfm*button box*ok.accelerators:#override\n\
<Key>Return: set() notify() unset()\n\
<Key>Linefeed: set() notify() unset()\n

Xfm*button box*replace.accelerators:#override\n\
<Key>Return: set() notify() unset()\n\
<Key>Linefeed: set() notify() unset()\n

Xfm*button box*install.accelerators:#override\n\
<Key>Return: set() notify() unset()\n\
<Key>Linefeed: set() notify() unset()\n
#ifdef ENHANCE_LOG
Xfm*fmLog*Hide.accelerators:#override\n\
<ClientMessage>WM_PROTOCOLS: set() notify() unset()\n\
<Key>Return: set() notify() unset()\n\
<Key>Linefeed: set() notify() unset()\n
Xfm*fmLog*Clear Log.accelerators:#override\n\
<Key>Delete: set() notify() unset()\n
#endif
#endif
#include "src/FmVersion.h"
! dont change the version number
Xfm.appDefsVersion: XFMVERSION
#endif
