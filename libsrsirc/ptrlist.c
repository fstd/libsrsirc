/* ptrlist.c - (C) 2014, Timo Buhrmester
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_PLST

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "intlog.h"
#include "ptrlist.h"

struct pl_node {
	void *data;
	struct pl_node *next;
};

struct ptrlist {
	struct pl_node *head;
	struct pl_node *iter;
};

ptrlist_t
ptrlist_init(void)
{
	struct ptrlist *l = malloc(sizeof *l);
	if (!l) {
		EE("malloc");
		return NULL;
	}

	l->head = NULL;
	l->iter = NULL;
	return l;
}

void
ptrlist_dispose(ptrlist_t l)
{
	ptrlist_clear(l);
	free(l);
}

size_t
ptrlist_count(ptrlist_t l)
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

void
ptrlist_clear(ptrlist_t l)
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
ptrlist_insert(ptrlist_t l, ssize_t i, void *data)
{
	if (!l || !data)
		return false;

	struct pl_node *n = l->head;
	struct pl_node *prev = NULL;

	struct pl_node *newnode = malloc(sizeof *newnode);
	if (!newnode)
		return false;

	newnode->data = data;

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

bool
ptrlist_replace(ptrlist_t l, size_t i, void *data)
{
	if (!l || !l->head || !data)
		return false;

	struct pl_node *n = l->head;
	struct pl_node *prev = NULL;

	while (n->next && i > 0) {
		i--;
		prev = n;
		n = n->next;
	}

	if (i > 0)
		return false;

	n->data = data;

	return true;
}

bool
ptrlist_remove(ptrlist_t l, size_t i)
{
	if (!l || !l->head)
		return false;

	struct pl_node *n = l->head;
	struct pl_node *prev = NULL;

	while (n->next && i > 0) {
		i--;
		prev = n;
		n = n->next;
	}

	if (i > 0)
		return false;

	if (!prev)
		l->head = n->next;
	else
		prev->next = n->next;

	free(n);

	return true;
}

void*
ptrlist_get(ptrlist_t l, size_t i)
{
	if (!l || !l->head)
		return false;

	struct pl_node *n = l->head;

	while (n->next && i > 0) {
		i--;
		n = n->next;
	}

	if (i > 0)
		return false;

	return n->data;
}


ssize_t
ptrlist_findraw(ptrlist_t l, void *data)
{
	if (!l || !l->head)
		return -1;

	ssize_t c = 0;
	struct pl_node *n = l->head;
	while (n) {
		if (n->data == data)
			return c;
		c++;
		n = n->next;
	}

	return -1;
}

ssize_t
ptrlist_findfn(ptrlist_t l, ptrlist_find_fn fndfn)
{
	if (!l || !l->head)
		return -1;

	ssize_t c = 0;
	struct pl_node *n = l->head;
	while (n) {
		if (fndfn(n->data))
			return c;
		c++;
		n = n->next;
	}

	return -1;
}

ssize_t
ptrlist_findeqfn(ptrlist_t l, ptrlist_eq_fn eqfn, const void *needle)
{
	if (!l || !l->head)
		return -1;

	ssize_t c = 0;
	struct pl_node *n = l->head;
	while (n) {
		if (eqfn(n->data, needle))
			return c;
		c++;
		n = n->next;
	}

	return -1;

}


void
ptrlist_dump(ptrlist_t l, ptrlist_op_fn op)
{
	#define M(X, A...) fprintf(stderr, X, ##A)
	if (!l)
		return;

	size_t c = ptrlist_count(l);
	M("%zu elements: [", c);
	struct pl_node *n = l->head;
	while(n) {
		op(n->data);
		if (n->next)
			M(" --> ");
		n=n->next;
	}

	M("]\n");
	#undef M
}

void*
ptrlist_first(ptrlist_t l)
{
	if (!l || !l->head)
		return NULL;

	l->iter = l->head;
	return l->iter->data;
}

void*
ptrlist_next(ptrlist_t l)
{
	if (!l || !l->iter || !l->iter->next)
		return NULL;

	l->iter = l->iter->next;
	return l->iter->data;
}
