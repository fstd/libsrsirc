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

#include "bucklist.h"
#include "cmap.h"
#include "smap.h"

static size_t strhash_small(const char *s, const char *cmap);
static size_t strhash_mid(const char *s, const char *cmap);

struct smap {
	bucklist_t *buck;
	size_t bsz;
	size_t count;

	bool iterating;
	size_t bit;
	size_t listiter;

	smap_hash_fn hfn;

	const char *cmap;
};


smap
smap_init(size_t bsz, int cmap)
{
	struct smap *h = malloc(sizeof *h);
	if (!h)
		return NULL;

	h->bsz = bsz;
	h->count = 0;
	h->iterating = false;
	h->cmap = g_cmap[cmap];
	h->hfn = bsz <= 256 ? strhash_small : strhash_mid;

	h->buck = malloc(h->bsz * sizeof *h->buck);
	if (!h->buck)
		goto smap_init_fail;

	for (size_t i = 0; i < h->bsz; i++)
		h->buck[i] = NULL;

	return h;

smap_init_fail:
	if (h)
		free(h->buck);
	free(h);

	return NULL;
}

void
smap_clear(smap h)
{
	char *k;
	for (size_t i = 0; i < h->bsz; i++) {
		if (!h->buck[i])
			continue;

		if (bucklist_first(h->buck[i], &k, NULL))
			do {
				free(k);
			} while (bucklist_next(h->buck[i], &k, NULL));

		bucklist_dispose(h->buck[i]);
		h->buck[i] = NULL;
	}

	h->count = 0;
}

void
smap_dispose(smap h)
{
	if (!h)
		return;

	smap_clear(h);

	free(h->buck);
	free(h);
}

bool
smap_put(smap h, const char *key, void *elem)
{
	if (!key || !elem)
		return false;

	bool allocated = false;
	size_t ind = h->hfn(key, h->cmap) % h->bsz;
	char *kd = NULL;

	bucklist_t kl = h->buck[ind];
	if (!kl) {
		allocated = true;
		if (!(kl = h->buck[ind] = bucklist_init(h->cmap)))
			goto smap_put_fail;
	}

	void *e = bucklist_find(kl, key, NULL);
	if (!e) {
		kd = strdup(key);
		if (!kd)
			goto smap_put_fail;

		if (!bucklist_insert(kl, 0, kd, elem))
			goto smap_put_fail;

		h->count++;
	} else
		bucklist_replace(kl, key, elem);

	return true;

smap_put_fail:
	if (allocated) {
		bucklist_dispose(kl);
		h->buck[ind] = NULL;
	}

	free(kd);

	return false;
}

void*
smap_get(smap h, const char *key)
{
	size_t ind = h->hfn(key, h->cmap) % h->bsz;

	bucklist_t kl = h->buck[ind];
	if (!kl)
		return NULL;

	return bucklist_find(kl, key, NULL);
}

void*
smap_del(smap h, const char *key)
{
	size_t ind = h->hfn(key, h->cmap) % h->bsz;

	bucklist_t kl = h->buck[ind];
	if (!kl)
		return NULL;

	char *okey;
	void *e = bucklist_remove(kl, key, &okey);

	if (!e)
		return NULL;

	free(okey);
	h->count--;
	return e;
}

size_t
smap_count(smap h)
{
	return h->count;
}

bool
smap_first(smap h, char **key, void **val)
{
	h->bit = 0;
	while (h->bit < h->bsz &&
	    (!h->buck[h->bit] || bucklist_isempty(h->buck[h->bit])))
		h->bit++;

	if (h->bit == h->bsz) {
		if (key) *key = NULL;
		if (val) *val = NULL;

		return h->iterating = false;
	}

	bucklist_first(h->buck[h->bit], key, val);

	return h->iterating = true;
}

bool
smap_next(smap h, char **key, void **val)
{
	if (!h->iterating)
		return false;

	if (bucklist_next(h->buck[h->bit], key, val))
		return true;

	do {
		h->bit++;
	} while (h->bit < h->bsz &&
	    (!h->buck[h->bit] || bucklist_isempty(h->buck[h->bit])));

	if (h->bit == h->bsz) {
		if (key) *key = NULL;
		if (val) *val = NULL;

		return h->iterating = false;
	}

	bucklist_first(h->buck[h->bit], key, val);

	return true;
}

void
smap_del_iter(smap h)
{
	if (h->iterating)
		bucklist_del_iter(h->buck[h->bit]);
}

void
smap_dump(smap h, smap_op_fn valop)
{
	#define M(X, A...) fprintf(stderr, X, ##A)
	M("===hashmap dump (count: %zu)===\n", h->count);
	if (!h)
		M("nullpointer...\n");

	for (size_t i = 0; i < h->bsz; i++) {
		if (h->buck[i] && bucklist_count(h->buck[i])) {
			fprintf(stderr, "[%zu]: ", i);
			bucklist_t kl = h->buck[i];
			char *key;
			void *val;
			if (bucklist_first(kl, &key, &val))
				do {
					fprintf(stderr, "'%s' --> ", key);
					if (valop)
						valop(val);
				} while (bucklist_next(kl, &key, &val));
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
	size_t maxlen = 0;
	for (size_t i = 0; i < h->bsz; i++) {
		if (h->buck[i]) {
			size_t c = bucklist_count(h->buck[i]);
			if (c > maxlen)
				maxlen = c;
			if (c > 0) {
				used++;
				usedlen += c;
			} else
				empty++;
		}
	}

	fprintf(stderr, "hashmap stat: bucksz: %zu, used: %zu (%f%%) "
	    "avg listlen: %f, max listlen: %zu\n", h->bsz, used,
	    ((double)used/h->bsz)*100.0, ((double)usedlen/used), maxlen);
}


static size_t
strhash_small(const char *s, const char *cmap)
{
	uint8_t res = 0xaa;
	uint8_t cur;
	bool shift = false;
	char *str = (char*)s;

	while ((cur = cmap[(unsigned char)*str++]))
		res ^= (cur << (shift = !shift));

	return res;
}

static size_t
strhash_mid(const char *s, const char *cmap)
{
	uint8_t res[2] = { 0xaa, 0xaa };
	uint8_t cur;
	unsigned shift = 0;
	bool first = true;
	char *str = (char*)s;

	while ((cur = cmap[(unsigned char)*str++]))
		res[first = !first] ^= cur << !!(++shift & 2);

	return (res[0] << 8) | res[1];
}

/*void smap_test(void) {

	char *line = NULL;
	size_t linesize = 0;
	ssize_t linelen;

	while ((linelen = getline(&line, &linesize, stdin)) != -1) {
		char *s = strchr(line, '\n');
		*s = 0;
		size_t shs = strhash_small(line, g_cmap[0]);
		size_t shm = strhash_mid(line, g_cmap[0]);
		size_t shm4 = strhash_mid(line, g_cmap[0]) % 8192;
		
		printf("'%s' %zu %zu %zu\n", line, shs, shm, shm4);
	}

	if (ferror(stdin))
		perror("getline");

} */
