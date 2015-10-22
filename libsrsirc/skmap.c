/* skmap.c - hashmap with string keys
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_SKMAP

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "skmap.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <platform/base_string.h>

#include <logger/intlog.h>

#include "bucklist.h"
#include "cmap.h"
#include "common.h"


struct skmap {
	bucklist **buck;
	size_t bsz;
	size_t count;

	bool iterating;
	size_t bit;
	size_t listiter;

	skmap_hash_fn hfn;

	const uint8_t *cmap;
};


static size_t strhash_small(const char *s, const uint8_t *cmap);
static size_t strhash_mid(const char *s, const uint8_t *cmap);


skmap *
lsi_skmap_init(size_t bsz, int cmap)
{
	skmap *h = lsi_com_malloc(sizeof *h);
	if (!h)
		return NULL;

	h->bsz = bsz;
	h->count = 0;
	h->iterating = false;
	h->cmap = g_cmap[cmap];
	h->hfn = bsz <= 256 ? strhash_small : strhash_mid;

	h->buck = lsi_com_malloc(h->bsz * sizeof *h->buck);
	if (!h->buck)
		goto skmap_init_fail;

	for (size_t i = 0; i < h->bsz; i++)
		h->buck[i] = NULL;

	return h;

skmap_init_fail:
	if (h)
		free(h->buck);
	free(h);

	return NULL;
}

void
lsi_skmap_clear(skmap *h)
{
	char *k;
	for (size_t i = 0; i < h->bsz; i++) {
		if (!h->buck[i])
			continue;

		if (lsi_bucklist_first(h->buck[i], &k, NULL))
			do {
				free(k);
			} while (lsi_bucklist_next(h->buck[i], &k, NULL));

		lsi_bucklist_dispose(h->buck[i]);
		h->buck[i] = NULL;
	}

	h->count = 0;
}

void
lsi_skmap_dispose(skmap *h)
{
	if (!h)
		return;

	lsi_skmap_clear(h);

	free(h->buck);
	free(h);
}

bool
lsi_skmap_put(skmap *h, const char *key, void *elem)
{
	if (!key || !elem)
		return false;

	bool allocated = false;
	size_t ind = h->hfn(key, h->cmap) % h->bsz;
	char *kd = NULL;

	bucklist *kl = h->buck[ind];
	if (!kl) {
		allocated = true;
		if (!(kl = h->buck[ind] = lsi_bucklist_init(h->cmap)))
			goto skmap_put_fail;
	}

	void *e = lsi_bucklist_find(kl, key, NULL);
	if (!e) {
		kd = lsi_b_strdup(key);
		if (!kd)
			goto skmap_put_fail;

		if (!lsi_bucklist_insert(kl, 0, kd, elem))
			goto skmap_put_fail;

		h->count++;
	} else
		lsi_bucklist_replace(kl, key, elem);

	return true;

skmap_put_fail:
	if (allocated) {
		lsi_bucklist_dispose(kl);
		h->buck[ind] = NULL;
	}

	free(kd);

	return false;
}

void *
lsi_skmap_get(skmap *h, const char *key)
{
	size_t ind = h->hfn(key, h->cmap) % h->bsz;

	bucklist *kl = h->buck[ind];
	if (!kl)
		return NULL;

	return lsi_bucklist_find(kl, key, NULL);
}

void *
lsi_skmap_del(skmap *h, const char *key)
{
	size_t ind = h->hfn(key, h->cmap) % h->bsz;

	bucklist *kl = h->buck[ind];
	if (!kl)
		return NULL;

	char *okey;
	void *e = lsi_bucklist_remove(kl, key, &okey);

	if (!e)
		return NULL;

	free(okey);
	h->count--;
	return e;
}

size_t
lsi_skmap_count(skmap *h)
{
	return h->count;
}

bool
lsi_skmap_first(skmap *h, char **key, void **val)
{
	h->bit = 0;
	while (h->bit < h->bsz &&
	    (!h->buck[h->bit] || lsi_bucklist_isempty(h->buck[h->bit])))
		h->bit++;

	if (h->bit == h->bsz) {
		if (key) *key = NULL;
		if (val) *val = NULL;

		return h->iterating = false;
	}

	lsi_bucklist_first(h->buck[h->bit], key, val);

	return h->iterating = true;
}

bool
lsi_skmap_next(skmap *h, char **key, void **val)
{
	if (!h->iterating)
		return false;

	if (lsi_bucklist_next(h->buck[h->bit], key, val))
		return true;

	do {
		h->bit++;
	} while (h->bit < h->bsz &&
	    (!h->buck[h->bit] || lsi_bucklist_isempty(h->buck[h->bit])));

	if (h->bit == h->bsz) {
		if (key) *key = NULL;
		if (val) *val = NULL;

		return h->iterating = false;
	}

	lsi_bucklist_first(h->buck[h->bit], key, val);

	return true;
}

void
lsi_skmap_del_iter(skmap *h)
{
	if (h->iterating)
		lsi_bucklist_del_iter(h->buck[h->bit]);
}

void
lsi_skmap_dump(skmap *h, skmap_op_fn valop)
{
	#define M(...) fprintf(stderr, __VA_ARGS__)
	M("===hashmap dump (count: %zu)===\n", h->count);
	if (!h)
		M("nullpointer...\n");

	for (size_t i = 0; i < h->bsz; i++) {
		if (h->buck[i] && lsi_bucklist_count(h->buck[i])) {
			fprintf(stderr, "[%zu]: ", i);
			bucklist *kl = h->buck[i];
			char *key;
			void *val;
			if (lsi_bucklist_first(kl, &key, &val))
				do {
					fprintf(stderr, "'%s' --> ", key);
					if (valop)
						valop(val);
					fputs(", ", stderr);
				} while (lsi_bucklist_next(kl, &key, &val));
			fputc('\n', stderr);
		}
	}
	M("===end of hashmap dump===\n");
	#undef M
}

void
lsi_skmap_stat(skmap *h, size_t *nbuck, size_t *nbuckused, size_t *nitems,
    double *loadfac, double *avglistlen, size_t *maxlistlen)
{
	size_t used = 0;
	size_t usedlen = 0;
	size_t empty = 0;
	size_t maxlen = 0;
	for (size_t i = 0; i < h->bsz; i++) {
		if (h->buck[i]) {
			size_t c = lsi_bucklist_count(h->buck[i]);
			if (c > maxlen)
				maxlen = c;
			if (c > 0) {
				used++;
				usedlen += c;
			} else
				empty++;
		}
	}

	*nbuck = h->bsz;
	*nbuckused = used;
	*nitems = h->count;
	*loadfac = (double)h->count / h->bsz;
	if (used)
		*avglistlen = (double)usedlen / used;
	else
		*avglistlen = 0;
	*maxlistlen = maxlen;
}

void
lsi_skmap_dumpstat(skmap *h, const char *dbgname)
{
	size_t nbuck;
	size_t nbuckused;
	size_t nitems;
	double loadfac;
	double avglistlen;
	size_t maxlistlen;

	lsi_skmap_stat(h, &nbuck, &nbuckused, &nitems, &loadfac, &avglistlen,
	    &maxlistlen);

	A("hashmap '%s' stat: bucksz: %zu, buckused: %zu (%f%%), items: %zu, "
	    "loadfac: %f, avg listlen: %f, max listlen: %zu",
	    dbgname, nbuck, nbuckused, 100.0*nbuckused/nbuck, nitems,
	    loadfac, avglistlen, maxlistlen);
}


static size_t
strhash_small(const char *s, const uint8_t *cmap)
{
	uint8_t res = 0xaa;
	uint8_t cur;
	unsigned shift = 0;

	while ((cur = cmap[(uint8_t)*s++]))
		res ^= (cur << (++shift % 3));

	return res;
}

static size_t
strhash_mid(const char *s, const uint8_t *cmap)
{
	uint8_t res[2] = { 0xaa, 0xaa };
	uint8_t cur;
	unsigned shift = 0;
	bool first = true;

	while ((cur = cmap[(uint8_t)*s++]))
		res[first = !first] ^= cur << (++shift % 3);

	return (res[0] << 8) | res[1];
}

/*void skmap_test(void) {

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
