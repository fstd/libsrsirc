/* ptrlist.h - (C) 2014, Timo Buhrmester
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_PTRLIST_H
#define LIBSRSIRC_PTRLIST_H 1

/* simple and stupid single linked list with void* data elements */


#include <stdbool.h>

typedef struct ptrlist *ptrlist_t;
typedef bool (*ptrlist_find_fn)(const void *e);
typedef void (*ptrlist_op_fn)(const void *e);
typedef bool (*ptrlist_eq_fn)(const void *e1, const void *e2);

/* alloc/dispose/count/clear */
ptrlist_t ptrlist_init(void);
void ptrlist_dispose(ptrlist_t l);
void ptrlist_clear(ptrlist_t l);
size_t ptrlist_count(ptrlist_t l);

/* insert/remove/get by index */
bool ptrlist_insert(ptrlist_t l, ssize_t i, void *data);
bool ptrlist_replace(ptrlist_t l, size_t i, void *data);
bool ptrlist_remove(ptrlist_t l, size_t i);
void* ptrlist_get(ptrlist_t l, size_t i);

/* linear search */
ssize_t ptrlist_findraw(ptrlist_t, void *data);
ssize_t ptrlist_findfn(ptrlist_t, ptrlist_find_fn fndfn);
ssize_t ptrlist_findeqfn(ptrlist_t l, ptrlist_eq_fn eqfn, const void *needle);

/* iteration */
void* ptrlist_first(ptrlist_t l);
void* ptrlist_next(ptrlist_t l);
void ptrlist_dump(ptrlist_t l, ptrlist_op_fn op);

#endif /* LIBSRSIRC_PTRLIST_H */
