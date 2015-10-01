/* bucklist.h - helper structure for hashmap
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_PTRLIST_H
#define LIBSRSIRC_PTRLIST_H 1


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/* simple and stupid single linked list with void* data elements */
typedef struct bucklist *bucklist_t;
typedef bool (*bucklist_find_fn)(const void *e);
typedef void (*bucklist_op_fn)(const void *e);
typedef bool (*bucklist_eq_fn)(const void *e1, const void *e2);

/* alloc/dispose/count/clear */
bucklist_t lsi_bucklist_init(const uint8_t *cmap);
void lsi_bucklist_dispose(bucklist_t l);
size_t lsi_bucklist_count(bucklist_t l);
bool lsi_bucklist_isempty(bucklist_t l);
void lsi_bucklist_clear(bucklist_t l);

/* insert/replace/get by index */
bool lsi_bucklist_insert(bucklist_t l, size_t i, char *key, void *val);
bool lsi_bucklist_get(bucklist_t l, size_t i, char **key, void **val);

/* linear search */
void* lsi_bucklist_find(bucklist_t l, const char *key, char **origkey);
void* lsi_bucklist_remove(bucklist_t l, const char *key, char **origkey);
bool lsi_bucklist_replace(bucklist_t l, const char *key, void *val);

/* iteration */
bool lsi_bucklist_first(bucklist_t l, char **key, void **val);
bool lsi_bucklist_next(bucklist_t l, char **key, void **val);
void lsi_bucklist_del_iter(bucklist_t l);

/* debug */
void lsi_bucklist_dump(bucklist_t l, bucklist_op_fn op);


#endif /* LIBSRSIRC_PTRLIST_H */
