/* Stub -- this project has no terminfo/termcap database at all, so a real
 * curses implementation has nothing to read from. Real Darwin's curses.h
 * declares the traditional BSD termcap functions directly (tgetent et al
 * predate terminfo-only curses and are still declared here, not in a
 * separate termcap.h, on Apple's own headers); only referenced by xterm's
 * xtermcap.c/resize.c, which both already handle tgetent() failure as a
 * normal, supported path (xterm falls back to its compiled-in key tables
 * when no termcap entry is found -- see xtermcap.c's TcapInit macro). -1
 * is the real, documented tgetent() return code for "no termcap database
 * available", not a made-up sentinel. */
#ifndef _CURSES_H_
#define _CURSES_H_

#ifdef __cplusplus
extern "C" {
#endif

extern char *UP, *BC, PC;
extern short ospeed;

int tgetent(char *bp, const char *name);
int tgetflag(const char *id);
int tgetnum(const char *id);
char *tgetstr(const char *id, char **area);
char *tgoto(const char *cap, int col, int row);
int tputs(const char *str, int affcnt, int (*putc)(int));

#ifdef __cplusplus
}
#endif

#endif /* _CURSES_H_ */
