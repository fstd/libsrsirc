/* bucklist.h - helper structure for hashmap
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-19, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_PTRLIST_H
#define LIBSRSIRC_PTRLIST_H 1


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/* simple and stupid single linked list with void* data elements */
typedef struct bucklist bucklist;
typedef bool (*bucklist_find_fn)(const void *e);
typedef void (*bucklist_op_fn)(const void *e);
typedef bool (*bucklist_eq_fn)(const void *e1, const void *e2);

/* alloc/dispose/count/clear */
bucklist *lsi_bucklist_init(const uint8_t *cmap);
void lsi_bucklist_dispose(bucklist *l);
size_t lsi_bucklist_count(bucklist *l);
bool lsi_bucklist_isempty(bucklist *l);
void lsi_bucklist_clear(bucklist *l);

/* insert/replace/get by index */
bool lsi_bucklist_insert(bucklist *l, size_t i, char *key, void *val);
bool lsi_bucklist_get(bucklist *l, size_t i, char **key, void **val);

/* linear search */
void *lsi_bucklist_find(bucklist *l, const char *key, char **origkey);
void *lsi_bucklist_remove(bucklist *l, const char *key, char **origkey);
bool lsi_bucklist_replace(bucklist *l, const char *key, void *val);

/* iteration */
bool lsi_bucklist_first(bucklist *l, char **key, void **val);
bool lsi_bucklist_next(bucklist *l, char **key, void **val);
void lsi_bucklist_del_iter(bucklist *l);

/* debug */
void lsi_bucklist_dump(bucklist *l, bucklist_op_fn op);


#endif /* LIBSRSIRC_PTRLIST_H */
