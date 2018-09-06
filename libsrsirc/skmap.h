/* skmap.h - hashmap with string keys, interface
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-18, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_HASHMAP_H
#define LIBSRSIRC_HASHMAP_H 1


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef size_t (*skmap_hash_fn)(const char *elem, const uint8_t *cmap);
typedef void (*skmap_op_fn)(const void *elem);
typedef void *(*skmap_keydup_fn)(const char *key);
typedef bool (*skmap_eq_fn)(const void *elem1, const void *elem2);
typedef struct skmap skmap;


skmap *lsi_skmap_init(size_t bucketsz, int cmap);
void lsi_skmap_clear(skmap *m);
void lsi_skmap_dispose(skmap *m);
bool lsi_skmap_put(skmap *m, const char *key, void *elem);
void *lsi_skmap_get(skmap *m, const char *key);
void *lsi_skmap_del(skmap *m, const char *key);
size_t lsi_skmap_count(skmap *m);

bool lsi_skmap_first(skmap *m, char **key, void **val);
bool lsi_skmap_next(skmap *m, char **key, void **val);
void lsi_skmap_del_iter(skmap *h);

void lsi_skmap_dump(skmap *m, skmap_op_fn valop);
void lsi_skmap_stat(skmap *h, size_t *nbuck, size_t *nbuckused, size_t *nitems,
    double *loadfac, double *avglistlen, size_t *maxlistlen);
void lsi_skmap_dumpstat(skmap *m, const char *dbgname);
//void skmap_test(void);


#endif /* LIBSRSIRC_HASHMAP_H */
