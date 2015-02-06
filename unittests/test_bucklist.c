/* test_bucklist.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
  * See README for contact-, COPYING for license information. */

#include "unittests_common.h"

#include <libsrsirc/bucklist.h>
#include <libsrsirc/cmap.h>
#include <libsrsirc/defs.h>

///* simple and stupid single linked list with void* data elements */
//typedef struct bucklist *bucklist_t;
//typedef bool (*bucklist_find_fn)(const void *e);
//typedef void (*bucklist_op_fn)(const void *e);
//typedef bool (*bucklist_eq_fn)(const void *e1, const void *e2);
//
///* alloc/dispose/count/clear */
//bucklist_t bucklist_init(const uint8_t *cmap);
//void bucklist_dispose(bucklist_t l);
//size_t bucklist_count(bucklist_t l);
//bool bucklist_isempty(bucklist_t l);
//void bucklist_clear(bucklist_t l);
//
///* insert/replace/get by index */
//bool bucklist_insert(bucklist_t l, size_t i, char *key, void *val);
//bool bucklist_get(bucklist_t l, size_t i, char **key, void **val);
//
///* linear search */
//void* bucklist_find(bucklist_t l, const char *key, char **origkey);
//void* bucklist_remove(bucklist_t l, const char *key, char **origkey);
//bool bucklist_replace(bucklist_t l, const char *key, void *val);
//
///* iteration */
//bool bucklist_first(bucklist_t l, char **key, void **val);
//bool bucklist_next(bucklist_t l, char **key, void **val);
//void bucklist_del_iter(bucklist_t l);
//
///* debug */
//void bucklist_dump(bucklist_t l, bucklist_op_fn op);

const char* /*UNITTEST*/
test_basic(void)
{
	bucklist_t l = bucklist_init(g_cmap[CMAP_RFC1459]);
	if (!l)
		return "bucklist alloc failed";

	if (bucklist_count(l) != 0)
		return "newly allocated list not empty";
	
	bucklist_dispose(l);

	return NULL;
}
