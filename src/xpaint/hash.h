/* $Id: hash.h,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

/* hash.c */
void *HashCreate(int (*cmp) (void *, void *), void (*free) (void *), int nelem);
void HashDestroy(void *t);
void *HashFind(void *t, int value, void *val);
int HashAdd(void *t, int value, void *val);
int HashRemove(void *t, int value, void *elem);
