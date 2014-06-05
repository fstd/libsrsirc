/* smap.h -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_HASHMAP_H
#define LIBSRSIRC_HASHMAP_H 1

#include <stddef.h>
#include <stdbool.h>

typedef size_t (*smap_hash_fn)(const char *elem, const char *cmap);
typedef void (*smap_op_fn)(const char *elem);
typedef void* (*smap_keydup_fn)(const char *key);
typedef bool (*smap_eq_fn)(const void *elem1, const void *elem2);
typedef struct smap *smap;

smap smap_init(size_t bucketsz, int cmap);
void smap_clear(smap m);
void smap_dispose(smap m);
bool smap_put(smap m, const char *key, void *elem);
void* smap_get(smap m, const char *key);
void* smap_del(smap m, const char *key);
size_t smap_count(smap m);

bool smap_first(smap m, char **key, void **val);
bool smap_next(smap m, char **key, void **val);
void smap_del_iter(smap h);

void smap_dump(smap m, smap_op_fn valop);
void smap_dumpstat(smap m);

#endif /* LIBSRSIRC_HASHMAP_H */
