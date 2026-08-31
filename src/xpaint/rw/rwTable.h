/* $Id: rwTable.h,v 1.21 2005/03/20 20:15:34 demailly Exp $ */

#define MAGIC_READER	"Best Guess"

#ifdef __IMAGE_H__
typedef Image *(*RWreadFunc) (char *);
typedef int (*RWwriteFunc) (char *, Image *);
#endif
typedef int (*RWtestFunc) (char *);

void *RWtableGetEntry(char *);
#ifdef __IMAGE_H__
int writeMagic(char *, Image *);
Image *readMagic(char *);
RWreadFunc RWtableGetReader(void *);
RWwriteFunc RWtableGetWriter(void *);
#endif
char **RWtableGetReaderList(void);
char **RWtableGetWriterList(void);
char *RWGetMsg(void);
void *RWtableGetReaderID(void);
char *RWtableGetId(void *);

#ifdef __IMAGE_H__
int WriteAsciiPNMfd(FILE * fd, Image * image);
#endif

#ifdef MISSING_STDARG_H
void RWSetMsg(...);
#else
void RWSetMsg(char *,...);
#endif
