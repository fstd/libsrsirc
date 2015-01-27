/* bucklist.h - (C) 2014, Timo Buhrmester
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_PTRLIST_H
#define LIBSRSIRC_PTRLIST_H 1

/* simple and stupid single linked list with void* data elements */


#include <stdbool.h>
#include <stddef.h>

typedef struct bucklist *bucklist_t;
typedef bool (*bucklist_find_fn)(const void *e);
typedef void (*bucklist_op_fn)(const void *e);
typedef bool (*bucklist_eq_fn)(const void *e1, const void *e2);

/* alloc/dispose/count/clear */
bucklist_t bucklist_init(const char *cmap);
void bucklist_dispose(bucklist_t l);
size_t bucklist_count(bucklist_t l);
bool bucklist_isempty(bucklist_t l);
void bucklist_clear(bucklist_t l);

/* insert/replace/get by index */
bool bucklist_insert(bucklist_t l, ssize_t i, char *key, void *val);
bool bucklist_get(bucklist_t l, size_t i, char **key, void **val);

/* linear search */
void* bucklist_find(bucklist_t l, const char *key, char **origkey);
void* bucklist_remove(bucklist_t l, const char *key, char **origkey);
bool bucklist_replace(bucklist_t l, const char *key, void *val);

/* iteration */
bool bucklist_first(bucklist_t l, char **key, void **val);
bool bucklist_next(bucklist_t l, char **key, void **val);
void bucklist_del_iter(bucklist_t l);

/* debug */
void bucklist_dump(bucklist_t l, bucklist_op_fn op);

#endif /* LIBSRSIRC_PTRLIST_H */
