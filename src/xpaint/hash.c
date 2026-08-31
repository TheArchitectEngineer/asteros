/* +-------------------------------------------------------------------+ */
/* | Copyright 1992, David Koblas.                                     | */
/* | Copyright 1995, 1996 Torsten Martinsen (bullestock@dk-online.dk)  | */
/* |                                                                   | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

/* $Id: hash.c,v 1.17 2005/03/20 20:15:32 demailly Exp $ */

/*
**  A simple useful set of hash routines:
**
**   void *HashCreate(int (*cmp)(void *, void*), void (*free)(void *), int nelem)
**     -- Create a hash table with cmp() function, and optional free() function.
**   void     HashDestroy(HashTable *tbl)
**     -- Destroy the whole hash table
**   void    *HashFind(HashTable *tbl, int value, void *val)
**     -- Find an element in the hash table, returns NULL if not found
**   int      HashAdd(HashTable *tbl, int value, void *val)
**     -- Add an element to the table, returns non-zero on failure
**   int      HashRemove(HashTable *tbl, int value, void *elem)
**     -- Remove a particular hash entry from the table, pointer comparison
 */


#include <stdio.h>
#include <stdlib.h>

#include <X11/Intrinsic.h>
#include "misc.h"
#include "hash.h"


typedef struct hash_s {
    int *val;
    struct hash_s *left, *right, *same, **parentp;
} HashElement;

typedef struct {
    int (*cmp) (void *, void *);
    void (*free) (void *);
    HashElement **table;
    int nelem;
} HashTable;

/*
**  Create a hashtable, with cmp() compare function, and free() free function
**       with a hash table width of nelem, free can be null in which case the
**       system free function will be used.
 */
void *
HashCreate(int (*cmp) (void *, void *), void (*free) (void *), int nelem)
{
    HashTable *new;
    int i;

    new = (HashTable *) xmalloc(sizeof(HashTable));
    new->nelem = nelem;
    new->cmp = cmp;
    new->free = free;
    new->table = (HashElement **) xmalloc(nelem * sizeof(HashElement *));

    for (i = 0; i < nelem; i++)
	new->table[i] = NULL;

    return (void *) new;
}

/*
 * Helper function for HashDestroy()
 */
static void 
hashDestory(void (*func) (void *), HashElement * entry)
{
    if (entry->left != NULL) {
	hashDestory(func, entry->left);
	free(entry->left);
    }
    if (entry->right != NULL) {
	hashDestory(func, entry->right);
	free(entry->right);
    }
    if (entry->same != NULL) {
	hashDestory(func, entry->same);
	free(entry->same);
    }
    if (entry->val)
	func(entry->val);
}

/*
**  Free the entire hash table, and all elements
 */
void 
HashDestroy(void *t)
{
    int i;
    HashTable *tbl = (HashTable *) t;

    if (tbl == NULL)
	return;

    for (i = 0; i < tbl->nelem; i++) {
	if (tbl->table[i] != NULL) {
	    hashDestory(tbl->free ? tbl->free : free, tbl->table[i]);
	    free(tbl->table[i]);
	}
    }
    free(tbl->table);
    free(tbl);
}

/*
**  Find a particular element in the hash table, returns NULL if it isn't found
 */
void *
HashFind(void *t, int value, void *val)
{
    HashTable *tbl = (HashTable *) t;
    HashElement *cur;
    int v;

    if (tbl == NULL)
	return NULL;

    cur = tbl->table[value];

    while (cur != NULL) {
	if ((v = tbl->cmp(cur->val, val)) == 0)
	    return cur->val;

	if (v < 0)
	    cur = cur->left;
	else
	    cur = cur->right;
    }

    return NULL;
}

/*
**  Add a new element to the hash table, returns non-zero on failure
 */
int 
HashAdd(void *t, int value, void *val)
{
    HashTable *tbl = (HashTable *) t;
    HashElement *new;
    HashElement *cur = tbl->table[value];
    HashElement **prev;
    int v;

    if (tbl == NULL)
	return 1;

    new = (HashElement *) xmalloc(sizeof(HashElement));

    new->left = NULL;
    new->right = NULL;
    new->same = NULL;
    new->val = val;
    new->parentp = NULL;

    prev = &tbl->table[value];
    while (cur != NULL) {
	if ((v = tbl->cmp(cur->val, val)) < 0) {
	    prev = &cur->left;
	    cur = cur->left;
	} else if (v > 0) {
	    prev = &cur->right;
	    cur = cur->right;
	} else {
	    prev = &cur->same;
	    new->same = cur->same;
	    if (cur->same != NULL)
		cur->same->parentp = &new->same;
	    break;
	}
    }
    *prev = new;
    new->parentp = prev;

    return 0;
}

static HashElement *
find(HashElement * pos, void *elem)
{
    HashElement *v;

    if (pos == NULL)
	return NULL;

    if (pos->val == elem)
	return pos;
    if (pos->left != NULL && (v = find(pos->left, elem)) != NULL)
	return v;
    if (pos->right != NULL && (v = find(pos->right, elem)) != NULL)
	return v;
    if (pos->same != NULL && (v = find(pos->same, elem)) != NULL)
	return v;
    return NULL;
}

/*
**  Remove a particular element from the hash table, done using pointer compares
 */
int 
HashRemove(void *t, int value, void *elem)
{
    HashTable *tbl = (HashTable *) t;
    HashElement *v;

    if (elem == NULL || tbl == NULL)
	return 1;

    if ((v = find(tbl->table[value], elem)) == NULL)
	return 0;		/* The element was not actually in the table */

    if (v->same != NULL) {
	/*
	**  Same links are not allowed to have left & right children
	**   so just inherit the parent's children.
	 */
	if (v->left != NULL)
	    v->left->parentp = &v->same->left;
	if (v->right != NULL)
	    v->right->parentp = &v->same->right;
	v->same->left = v->left;
	v->same->right = v->right;

	*v->parentp = v->same;
	v->same->parentp = v->parentp;
    } else if (v->left != NULL) {
	*v->parentp = v->left;
	v->left->parentp = v->parentp;
	if (v->right != NULL) {
	    HashElement *cur = v->left, **prev = &v->left;
	    while (cur != NULL) {
		if (tbl->cmp(cur->val, v->right->val) < 0) {
		    prev = &cur->left;
		    cur = cur->left;
		} else {
		    prev = &cur->right;
		    cur = cur->right;
		}
	    }
	    *prev = v->right;
	    v->right->parentp = prev;
	}
    } else {
	/* no left side, just attach the right */
	*v->parentp = v->right;
	if (v->right != NULL)
	    v->right->parentp = v->parentp;
    }
    free(v);

    return 1;
}
