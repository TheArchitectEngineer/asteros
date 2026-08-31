/* xtermcfg.h.  Generated from xtermcfg.hin by configure.  */
/* $XTermId: xtermcfg.hin,v 1.238 2026/01/22 00:17:20 tom Exp $ */

/*
 * Copyright 1997-2025,2026 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

#ifndef included_xtermcfg_h
#define included_xtermcfg_h 1

/* This is a template for <xtermcfg.h> */

/*
 * There are no configure options for these features:
 * ALLOWLOGFILECHANGES
 * ALLOWLOGFILEONOFF
 * CANT_OPEN_DEV_TTY
 * DEBUG* (any debug-option)
 * DUMP_* (mostly in the ReGIS/SIXEL code)
 * HAS_LTCHARS
 * PUCC_PTYD
 * USE_LOGIN_DASH_P
 * USE_X11TERM
 */

/* #undef ALLOWLOGFILEEXEC */
/* #undef ALLOWLOGGING */
/* #undef CC_HAS_PROTOS */
/* #undef CSRG_BASED */
/* #undef DECL_ERRNO */
#define DEFDELETE_DEL Maybe
#define DEF_ALT_SENDS_ESC False
#define DEF_BACKARO_BS True
#define DEF_BACKARO_ERASE False
#define DEF_INITIAL_ERASE False
#define DEF_META_SENDS_ESC False
/* #undef DFT_COLORMODE */
#define DFT_DECID "420"
#define DFT_TERMTYPE "xterm"
/* #undef DISABLE_SETGID */
#define DISABLE_SETUID 1
#define HAVE_CLOCK_GETTIME 1
/* #undef HAVE_ENDUSERSHELL */
#define HAVE_GETHOSTNAME 1
#define HAVE_GETLOGIN 1
/* #undef HAVE_GETTIMEOFDAY */
/* #undef HAVE_GETUSERSHELL */
/* #undef HAVE_GRANTPT */
/* #undef HAVE_GRANTPT_PTY_ISATTY */
#define HAVE_INITGROUPS 1
/* #undef HAVE_LANGINFO_CODESET */
/* #undef HAVE_LASTLOG_H */
/* #undef HAVE_LIBUTIL_H */
/* #undef HAVE_LIBXPM */
/* #undef HAVE_LIB_NEXTAW */
/* #undef HAVE_LIB_PCRE */
/* #undef HAVE_LIB_PCRE2 */
#define HAVE_LIB_XAW 1
/* #undef HAVE_LIB_XAW3D */
/* #undef HAVE_LIB_XAW3DXFT */
/* #undef HAVE_LIB_XAWPLUS */
#define HAVE_LIB_XCURSOR 1
/* #undef HAVE_MKDTEMP */
#define HAVE_MKSTEMP 1
/* #undef HAVE_NCURSES_CURSES_H */
/* #undef HAVE_NCURSES_TERM_H */
#define HAVE_PATHS_H 1
/* #undef HAVE_PCRE2POSIX_H */
/* #undef HAVE_PCRE2REGCOMP */
/* #undef HAVE_PCREPOSIX_H */
/* #undef HAVE_POSIX_OPENPT */
/* #undef HAVE_POSIX_SAVED_IDS */
/* #undef HAVE_PTSNAME */
/* #undef HAVE_PTY_H */
#define HAVE_PUTENV 1
#define HAVE_SCHED_YIELD 1
#define HAVE_SETITIMER 1
#define HAVE_SETPGID 1
#define HAVE_SETSID 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
/* #undef HAVE_STDNORETURN_H */
#define HAVE_STRFTIME 1
/* #undef HAVE_STROPTS_H */
#define HAVE_SYS_PARAM_H 1
/* #undef HAVE_SYS_PTEM_H */
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TTYDEFAULTS_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_TCGETATTR 1
/* #undef HAVE_TERMCAP_H */
#define HAVE_TERMIOS_H 1
/* #undef HAVE_TERMIO_C_ISPEED */
/* #undef HAVE_TERM_H */
/* #undef HAVE_TIGETSTR */
#define HAVE_UNISTD_H 1
#define HAVE_UNSETENV 1
/* #undef HAVE_USE_EXTENDED_NAMES */
/* #undef HAVE_UTIL_H */
/* #undef HAVE_UTMP */
/* #undef HAVE_UTMP_UT_HOST */
/* #undef HAVE_UTMP_UT_SESSION */
/* #undef HAVE_UTMP_UT_SYSLEN */
/* #undef HAVE_UTMP_UT_XSTATUS */
/* #undef HAVE_UTMP_UT_XTIME */
#define HAVE_WAITPID 1
#define HAVE_WCHAR_H 1
/* #undef HAVE_WCSWIDTH */
/* #undef HAVE_WCWIDTH */
#define HAVE_X11_DECKEYSYM_H 1
/* #undef HAVE_X11_EXTENSIONS_XDBE_H */
/* #undef HAVE_X11_EXTENSIONS_XINERAMA_H */
#define HAVE_X11_EXTENSIONS_XKB_H 1
#define HAVE_X11_SUNKEYSYM_H 1
/* #undef HAVE_X11_TRANSLATEI_H */
#define HAVE_X11_XF86KEYSYM_H 1
#define HAVE_X11_XKBLIB_H 1
/* #undef HAVE_X11_XPOLL_H */
/* #undef HAVE_XDBESWAPBUFFERS */
/* #undef HAVE_XFTDRAWSETCLIP */
/* #undef HAVE_XFTDRAWSETCLIPRECTANGLES */
/* #undef HAVE_XKBKEYCODETOKEYSYM */
/* #undef HAVE_XKBQUERYEXTENSION */
/* #undef HAVE_XKB_BELL_EXT */
#define LUIT_PATH "/opt/X11/bin/luit"
/* #undef NO_ACTIVE_ICON */
/* #undef NO_LEAKS */
#define OPT_256_COLORS 1
/* #undef OPT_88_COLORS */
/* #undef OPT_AIX_COLORS */
/* #undef OPT_BLINK_CURS */
/* #undef OPT_BLINK_TEXT */
#define OPT_BLOCK_SELECT 0
/* #undef OPT_BOX_CHARS */
#define OPT_BROKEN_OSC 0
/* #undef OPT_BROKEN_ST */
/* #undef OPT_BUILTIN_XPMS */
/* #undef OPT_C1_PRINT */
/* #undef OPT_COLOR_CLASS */
/* #undef OPT_DABBREV */
/* #undef OPT_DEC_CHRSET */
/* #undef OPT_DEC_LOCATOR */
/* #undef OPT_DEC_RECTOPS */
#define OPT_DIRECT_COLOR 1
/* #undef OPT_DOUBLE_BUFFER */
/* #undef OPT_EXEC_SELECTION */
/* #undef OPT_EXEC_XTERM */
#define OPT_GRAPHICS 1
/* #undef OPT_HIGHLIGHT_COLOR */
/* #undef OPT_HP_FUNC_KEYS */
/* #undef OPT_I18N_SUPPORT */
/* #undef OPT_INITIAL_ERASE */
#define OPT_INPUT_METHOD 0
/* #undef OPT_ISO_COLORS */
/* #undef OPT_LOAD_VTFONTS */
#define OPT_LUIT_PROG 1
/* #undef OPT_MAXIMIZE */
/* #undef OPT_MINI_LUIT */
/* #undef OPT_NUM_LOCK */
#define OPT_PASTE64 1
/* #undef OPT_PC_COLORS */
/* #undef OPT_PRINT_GRAPHICS */
#define OPT_PTY_HANDSHAKE 1
#define OPT_READLINE 1
/* #undef OPT_REGIS_GRAPHICS */
#define OPT_RESIZE_ADJUST 1
/* #undef OPT_SAME_NAME */
/* #undef OPT_SCO_FUNC_KEYS */
/* #undef OPT_SCREEN_DUMPS */
/* #undef OPT_SELECTION_OPS */
#define OPT_SELECT_REGEX 1
/* #undef OPT_SESSION_MGT */
#define OPT_SIXEL_GRAPHICS 1
/* #undef OPT_STATUS_LINE */
/* #undef OPT_SUN_FUNC_KEYS */
/* #undef OPT_TCAP_FKEYS */
/* #undef OPT_TCAP_QUERY */
/* #undef OPT_TEK4014 */
/* #undef OPT_TOOLBAR */
/* #undef OPT_VT52_MODE */
/* #undef OPT_WIDER_ICHAR */
/* #undef OPT_WIDE_ATTRS */
#define OPT_WIDE_CHARS 1
/* #undef EXP_XMC_GLITCH */
/* #undef OPT_ZICONBEEP */
/* #undef OWN_TERMINFO_DIR */
/* #undef OWN_TERMINFO_ENV */
/* #undef PROCFS_ROOT */
#define SCROLLBAR_RIGHT 1
#define SIG_ATOMIC_T volatile sig_atomic_t
/* #undef STDC_NORETURN */
/* #undef SVR4 */
/* #undef SYSV */
#define TIME_WITH_SYS_TIME 1
#define TTY_GROUP_NAME "no"
/* #undef USE_LASTLOG */
#define USE_POSIX_WAIT 1
#define USE_STRUCT_LASTLOG 1
/* #undef USE_SYSV_UTMP */
/* #undef USE_SYS_SELECT_H */
/* #undef USE_TERMCAP */
/* #undef USE_TERMINFO */
#define USE_TTY_GROUP 1
/* #undef USE_UTEMPTER */
/* #undef USE_UTMP_SETGID */
/* #undef UTMPX_FOR_UTMP */
/* #undef XRENDERFONT */
#define XTERM_ERASE ANSI_DEL
/* #undef cc_t */
/* #undef gid_t */
/* #undef inline */
/* #undef mode_t */
/* #undef nfds_t */
/* #undef off_t */
/* #undef pid_t */
/* #undef time_t */
/* #undef uid_t */
/* #undef ut_name */
/* #undef ut_xstatus */
/* #undef ut_xtime */

/*
 * Ifdef'd to make it simple to override.
 */
#ifndef OPT_TRACE
/* #undef OPT_TRACE */
/* #undef OPT_TRACE_FLAGS */
#endif

/*
 * g++ support for __attribute__() is haphazard.
 */
#ifndef __cplusplus
#define GCC_PRINTF 1
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#define GCC_NORETURN __attribute__((noreturn))
#define GCC_UNUSED __attribute__((unused))
#endif

#ifndef HAVE_X11_XPOLL_H
#define NO_XPOLL_H	/* X11R6.1 & up use Xpoll.h for select() definitions */
#endif

/* vile:cmode
 */
#endif /* included_xtermcfg_h */
