/* See curses.h -- xterm/resize.c both check the tgetent() return value and
 * fall back to compiled-in defaults, so returning "no database" here is a
 * real, supported code path for them, not a crash. */
#include <curses.h>
#include <stddef.h>

char *UP = NULL;
char *BC = NULL;
char PC = 0;
short ospeed = 0;

int tgetent(char *bp, const char *name) { (void)bp; (void)name; return -1; }
int tgetflag(const char *id) { (void)id; return 0; }
int tgetnum(const char *id) { (void)id; return -1; }
char *tgetstr(const char *id, char **area) { (void)id; (void)area; return NULL; }
char *tgoto(const char *cap, int col, int row) { (void)cap; (void)col; (void)row; return NULL; }
int tputs(const char *str, int affcnt, int (*putc)(int)) { (void)str; (void)affcnt; (void)putc; return 0; }
