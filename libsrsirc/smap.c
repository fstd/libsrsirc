/* smap.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_SMAP

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "intlog.h"

#include "ptrlist.h"
#include "smap.h"

static bool streq(const void *s1, const void *s2);
static void* strkeydup(const char *s);
static size_t strhash_small(const char *s);

struct smap {
	ptrlist_t *keybucket;
	ptrlist_t *valbucket;
	size_t bucketsz;
	size_t count;

	bool iterating;
	size_t buckiter;
	size_t listiter;

	smap_hash_fn hfn;
	smap_eq_fn efn;
	smap_keydup_fn keydupfn;
};


smap
smap_init(size_t bucketsz)
{
	struct smap *h = malloc(sizeof *h);
	if (!h)
		return NULL;

	h->bucketsz = bucketsz;
	h->count = 0;
	h->iterating = false;
	h->keybucket = h->valbucket = NULL;

	h->keybucket = malloc(h->bucketsz * sizeof *h->keybucket);
	if (!h->keybucket)
		goto smap_init_fail;

	h->valbucket = malloc(h->bucketsz * sizeof *h->valbucket);
	if (!h->valbucket)
		goto smap_init_fail;

	for (size_t i = 0; i < h->bucketsz; i++) {
		h->valbucket[i] = NULL;
		h->keybucket[i] = NULL;
	}

	h->hfn = strhash_small;
	h->efn = streq;
	h->keydupfn = strkeydup;

	return h;

smap_init_fail:
	if (h) {
		free(h->keybucket);
		free(h->valbucket);
	}

	free(h);

	return NULL;
}

void
smap_clear(smap h)
{
	if (!h)
		return;

	void *e;
	for (size_t i = 0; i < h->bucketsz; i++) {
		if (h->keybucket[i]) {
			if ((e = ptrlist_first(h->keybucket[i])))
				do {
					free(e);
				} while ((e = ptrlist_next(h->keybucket[i])));
			ptrlist_dispose(h->keybucket[i]);
			h->keybucket[i] = NULL;
		}
		if (h->valbucket[i]) {
			ptrlist_dispose(h->valbucket[i]);
			h->valbucket[i] = NULL;
		}
	}

	h->count = 0;
}

void
smap_dispose(smap h)
{
	if (!h)
		return;

	smap_clear(h);

	free(h->keybucket);
	free(h->valbucket);
	free(h);
}

bool
smap_put(smap h, const char *key, void *elem)
{
	if (!h || !key || !elem)
		return false;

	bool allocated = false;
	bool halfinserted = false;
	size_t ind = h->hfn(key) % h->bucketsz;
	char *kd = NULL;

	ptrlist_t kl = h->keybucket[ind];
	ptrlist_t vl = h->valbucket[ind];
	if (!kl) {
		allocated = true;
		if (!(kl = h->keybucket[ind] = ptrlist_init()))
			goto smap_put_fail;

		if (!(vl = h->valbucket[ind] = ptrlist_init()))
			goto smap_put_fail;
	}

	ssize_t i = ptrlist_findeqfn(kl, h->efn, key);
	if (i == -1) {
		kd = h->keydupfn(key);
		if (!kd)
			goto smap_put_fail;

		if (!ptrlist_insert(kl, 0, kd))
			goto smap_put_fail;

		halfinserted = true;

		if (!ptrlist_insert(vl, 0, elem))
			goto smap_put_fail;


		h->count++;
	} else
		ptrlist_replace(vl, i, elem);

	return true;

smap_put_fail:
	if (allocated) {
		ptrlist_dispose(kl);
		ptrlist_dispose(vl);
		h->keybucket[ind] = NULL;
		h->valbucket[ind] = NULL;
	}

	if (halfinserted)
		ptrlist_remove(kl, 0);

	free(kd);

	return false;
}

void*
smap_get(smap h, const char *key)
{
	if (!h)
		return NULL;

	size_t ind = h->hfn(key) % h->bucketsz;

	ptrlist_t kl = h->keybucket[ind];
	if (!kl)
		return NULL;
	ptrlist_t vl = h->valbucket[ind];

	ssize_t i = ptrlist_findeqfn(kl, h->efn, key);

	return i == -1 ? NULL : ptrlist_get(vl, i);
}

bool
smap_del(smap h, const char *key)
{
	if (!h)
		return false;

	size_t ind = h->hfn(key) % h->bucketsz;

	ptrlist_t kl = h->keybucket[ind];
	if (!kl)
		return false;
	ptrlist_t vl = h->valbucket[ind];

	ssize_t i = ptrlist_findeqfn(kl, h->efn, key);

	if (i == -1)
		return false;

	free(ptrlist_get(kl, i));
	ptrlist_remove(kl, i);
	ptrlist_remove(vl, i);
	h->count--;
	return true;
}

size_t
smap_count(smap h)
{
	return !h ? 0 : h->count;
}

bool
smap_first(smap h, const char **key, void **val)
{
	if (!h)
		return false;

	h->buckiter = 0;

	while (h->buckiter < h->bucketsz && (!h->keybucket[h->buckiter]
	    || ptrlist_count(h->keybucket[h->buckiter]) == 0))
		h->buckiter++;

	if (h->buckiter == h->bucketsz) {
		if (key)
			*key = NULL;
		if (val)
			*val = NULL;

		h->iterating = false;

		return false;
	}

	void *k = ptrlist_get(h->keybucket[h->buckiter], 0);
	void *v = ptrlist_get(h->valbucket[h->buckiter], 0);

	h->listiter = 1;

	if (key)
		*key = k;
	if (val)
		*val = v;

	h->iterating = true;

	return true;
}

bool
smap_next(smap h, const char **key, void **val)
{
	if (!h || !h->iterating)
		return false;

	void *k = ptrlist_get(h->keybucket[h->buckiter], h->listiter);

	if (!k) {
		h->buckiter++;
		h->listiter = 0;

		while (h->buckiter < h->bucketsz && (!h->keybucket[h->buckiter]
		    || ptrlist_count(h->keybucket[h->buckiter]) == 0))
			h->buckiter++;

		if (h->buckiter == h->bucketsz) {
			if (key)
				*key = NULL;
			if (val)
				*val = NULL;

			h->iterating = false;
			return false;
		}

		k = ptrlist_get(h->keybucket[h->buckiter], h->listiter);
	}

	void *v = ptrlist_get(h->valbucket[h->buckiter], h->listiter);

	h->listiter++;

	if (key)
		*key = k;
	if (val)
		*val = v;

	return true;

}

void
smap_dump(smap h, smap_op_fn keyop, smap_op_fn valop)
{
	#define M(X, A...) fprintf(stderr, X, ##A)
	M("===hashmap dump (count: %zu)===\n", h->count);
	if (!h)
		M("nullpointer...\n");

	for (size_t i = 0; i < h->bucketsz; i++) {
		if (h->keybucket[i] && ptrlist_count(h->keybucket[i])) {
			fprintf(stderr, "[%zu]: ", i);
			ptrlist_t kl = h->keybucket[i];
			ptrlist_t vl = h->valbucket[i];
			void *key = ptrlist_first(kl);
			void *val = ptrlist_first(vl);
			while (key) {
				keyop(key);
				fprintf(stderr, " --> ");
				valop(val);
				key = ptrlist_next(kl);
				val = ptrlist_next(vl);
			}
			fprintf(stderr, "\n");
		}
	}
	M("===end of hashmap dump===\n");
	#undef M
}

void
smap_dumpstat(smap h)
{
	size_t used = 0;
	size_t usedlen = 0;
	size_t empty = 0;
	for (size_t i = 0; i < h->bucketsz; i++) {
		if (h->keybucket[i]) {
			size_t c = ptrlist_count(h->keybucket[i]);
			if (c > 0) {
				used++;
				usedlen += c;
			} else
				empty++;
		}
	}

	fprintf(stderr, "hashmap stat: bucksz: %zu, used: %zu (%f%%) "
	    "avg listlen: %f\n", h->bucketsz, used,
	    ((double)used/h->bucketsz)*100.0, ((double)usedlen/used));
}


static bool
streq(const void *s1, const void *s2)
{
	return strcmp((const char*)s1, (const char*)s2) == 0;
}

static void*
strkeydup(const char *s)
{
	return strdup(s);
}

static size_t
strhash_small(const char *s)
{
	uint8_t res = 0xaa;
	uint8_t cur;
	bool shift = false;
	char *str = (char*)s;

	while ((cur = *str++)) {
		if ((shift = !shift))
			cur <<= 3;

		res ^= cur;
	}

	return res;
}

