/* bucklist.c - (C) 2014, Timo Buhrmester
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_PLST

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "bucklist.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <logger/intlog.h>
#include "cmap.h"
#include "common.h"


struct pl_node {
	char *key;
	void *val;
	struct pl_node *next;
};

struct bucklist {
	struct pl_node *head;
	struct pl_node *iter;
	struct pl_node *previter; //for delete while iteration
	const char *cmap;
};

static bool pfxeq(const char *n1, const char *n2, const char *cmap);

bucklist_t
bucklist_init(const char *cmap)
{
	struct bucklist *l = com_malloc(sizeof *l);
	if (!l)
		return NULL;

	l->head = NULL;
	l->iter = NULL;
	l->cmap = cmap;
	return l;
}

void
bucklist_dispose(bucklist_t l)
{
	bucklist_clear(l);
	free(l);
}

size_t
bucklist_count(bucklist_t l)
{
	if (!l || !l->head)
		return 0;

	/* XXX cache? */
	struct pl_node *n = l->head;
	size_t c = 1;
	while ((n = n->next))
		c++;

	return c;
}

bool
bucklist_isempty(bucklist_t l)
{
	return !l->head;
}

void
bucklist_clear(bucklist_t l)
{
	if (!l)
		return;

	struct pl_node *n = l->head;
	while (n) {
		struct pl_node *tmp = n->next;
		free(n);
		n = tmp;
	}

	l->head = NULL;
}

bool
bucklist_insert(bucklist_t l, ssize_t i, char *key, void *val)
{
	struct pl_node *n = l->head;
	struct pl_node *prev = NULL;

	struct pl_node *newnode = com_malloc(sizeof *newnode);
	if (!newnode)
		return false;

	newnode->key = key;
	newnode->val = val;

	if (!n) { //special case: list is empty
		newnode->next = NULL;
		l->head = newnode;
		return true;
	}

	while (n->next && i) {
		i--;
		prev = n;
		n = n->next;
	}

	if (i) {
		n->next = newnode;
		newnode->next = NULL;
	} else {
		newnode->next = n;
		if (prev)
			prev->next = newnode;
		else
			l->head = newnode;
	}

	return true;
}

/* key or val == NULL means don't touch */
bool
bucklist_replace(bucklist_t l, const char *key, void *val)
{
	if (!val)
		return false;

	struct pl_node *n = l->head;
	while (n) {
		if (pfxeq(n->key, key, l->cmap)) {
			n->val = val;
			return true;
		}
		n = n->next;
	}

	return false;
}

void*
bucklist_remove(bucklist_t l, const char *key, char **origkey)
{
	struct pl_node *n = l->head;
	struct pl_node *prev = NULL;
	while (n) {
		if (pfxeq(n->key, key, l->cmap)) {
			if (origkey)
				*origkey = n->key;
			void *val = n->val;

			if (!prev)
				l->head = n->next;
			else
				prev->next = n->next;

			free(n);
			return val;
		}
		prev = n;
		n = n->next;
	}

	return NULL;
}

bool
bucklist_get(bucklist_t l, size_t i, char **key, void **val)
{
	if (!l->head)
		return false;

	struct pl_node *n = l->head;

	while (n->next && i > 0) {
		i--;
		n = n->next;
	}

	if (i > 0)
		return false;

	if (key) *key = n->key;
	if (val) *val = n->val;

	return true;
}

void*
bucklist_find(bucklist_t l, const char *key, char **origkey)
{
	struct pl_node *n = l->head;
	while (n) {
		if (pfxeq(n->key, key, l->cmap)) {
			if (origkey)
				*origkey = n->key;
			return n->val;
		}
		n = n->next;
	}

	return NULL;
}

bool
bucklist_first(bucklist_t l, char **key, void **val)
{
	if (!l->head)
		return false;

	l->previter = NULL;
	l->iter = l->head;

	if (key) *key = l->iter->key;
	if (val) *val = l->iter->val;

	return true;
}

bool
bucklist_next(bucklist_t l, char **key, void **val)
{
	if (!l->iter || !l->iter->next)
		return false;

	l->previter = l->iter;
	l->iter = l->iter->next;

	if (key) *key = l->iter->key;
	if (val) *val = l->iter->val;

	return true;
}

void
bucklist_del_iter(bucklist_t l)
{
	struct pl_node *next = l->iter->next;
	if (!l->previter)
		l->head = next;
	else
		l->previter->next = next;
	
	free(l->iter);
	l->iter = l->previter;
}

void
bucklist_dump(bucklist_t l, bucklist_op_fn op)
{
	#define M(X, A...) fprintf(stderr, X, ##A)
	if (!l)
		return;

	size_t c = bucklist_count(l);
	M("%zu elements: [", c);
	struct pl_node *n = l->head;
	while(n) {
		op(n->key);
		if (n->next)
			M(" --> ");
		n=n->next;
	}

	M("]\n");
	#undef M
}


bool
pfxeq(const char *n1, const char *n2, const char *cmap)
{
	unsigned char c1, c2;
	while ((c1 = cmap[(unsigned char)*n1]) & /* avoid short circuit */
	    (c2 = cmap[(unsigned char)*n2])) {
		if (c1 != c2)
			return false;

		n1++; n2++;
	}

	return c1 == c2;
}
