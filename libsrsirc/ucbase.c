/* ucbase.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_UCBASE

#include <stdlib.h>
#include <string.h>

#include <inttypes.h>

#include <intlog.h>

#include "smap.h"
#include "common.h"
#include <libsrsirc/util.h>
#include "ucbase.h"

static int compare_modepfx(irc h, char c1, char c2);

bool
ucb_init(irc h)
{
	return (h->chans = smap_init(256));
}

void
ucb_deinit(irc h)
{
	if (h->chans)
		ucb_clear(h);
	smap_dispose(h->chans);
	h->chans = NULL;
}

void
ucb_clear(irc h)
{
	void *e;
	if (!smap_first(h->chans, NULL, &e))
		return;
	
	do {
		chan c = e;
		clear_memb(h, c);
		smap_dispose(c->memb);
		free(c->topicnick);
		free(c->topic);
		for (size_t i = 0; i < c->modes_sz; i++)
			free(c->modes[i]);
		free(c->modes);
		free(c);
	} while (smap_next(h->chans, NULL, &e));
	
	smap_clear(h->chans);
}

chan
add_chan(irc h, const char *name)
{
	char lname[MAX_CHAN_LEN];
	ut_strtolower(lname, sizeof lname, name, h->casemap);

	chan c = malloc(sizeof *c);
	if (!c)
		goto add_chan_fail;

	com_strNcpy(c->extname, name, sizeof c->extname);
	com_strNcpy(c->name, lname, sizeof c->name);
	c->topic = c->topicnick = NULL;
	c->tscreate = c->tstopic = 0;
	c->desync = false;
	c->modes = NULL;

	if (!(c->memb = smap_init(256)))
		goto add_chan_fail;

	c->modes_sz = 16; //grows
	if (!(c->modes = malloc(c->modes_sz * sizeof *c->modes)))
		goto add_chan_fail;
	
	for (size_t i = 0; i < c->modes_sz; i++)
		c->modes[i] = NULL;
	
	if (!smap_put(h->chans, lname, c))
		goto add_chan_fail;

	D("added chan '%s' ('%s')", c->extname, c->name);

	return c;

add_chan_fail:
	if (c) {
		if (c->modes)
			free(c->modes[0]);

		free(c->modes);
		smap_dispose(c->memb);
	}

	free(c);
	E("out of memory");
	return NULL;
}

chan
get_chan(irc h, const char *name)
{
	char lname[MAX_CHAN_LEN];
	ut_strtolower(lname, sizeof lname, name, h->casemap);

	return smap_get(h->chans, lname);
}

void
clear_memb(irc h, chan c)
{
	void *e;
	if (!smap_first(c->memb, NULL, &e))
		return;
	
	do {
		free(e);
	} while (smap_next(c->memb, NULL, &e));
	smap_clear(c->memb);
}

memb
get_memb(irc h, chan c, const char *nick)
{
	char lnick[MAX_NICK_LEN];
	ut_strtolower(lnick, sizeof lnick, nick, h->casemap);

	return smap_get(c->memb, lnick);
}

bool
add_memb(irc h, chan c, const char *nick, const char *mpfxstr)
{
	memb m = alloc_memb(h, nick, mpfxstr);
	if (!m || !smap_put(c->memb, m->nick, m))
		goto add_memb_fail;

	D("added member '%s' ('%s') to chan '%s'", m->extnick, m->nick, c->extname);
	return true;

add_memb_fail:
	free(m);
	E("out of memory");
	return false;
}


bool
drop_memb(irc h, chan c, const char *nick)
{
	memb m = get_memb(h, c, nick);
	if (!m)
		return false;

	bool deleted = smap_del(c->memb, m->nick);
	if (deleted)
		free(m);
	return deleted;
}

memb
alloc_memb(irc h, const char *nick, const char *mpfxstr)
{
	char lnick[MAX_NICK_LEN];
	ut_strtolower(lnick, sizeof lnick, nick, h->casemap);

	memb m = malloc(sizeof *m);
	if (!m)
		goto alloc_memb_fail;

	com_strNcpy(m->extnick, nick, sizeof m->extnick);
	com_strNcpy(m->nick, lnick, sizeof m->nick);
	com_strNcpy(m->modepfx, mpfxstr, sizeof m->modepfx);

	return m;

alloc_memb_fail:
	free(m);
	E("out of memory");
	return NULL;
}

bool
update_modepfx(irc h, chan c, const char *nick, char mpfxsym, bool enab)
{
	memb m = get_memb(h, c, nick);
	if (!m)
		return false;

	char *p = strchr(m->modepfx, mpfxsym);

	if (!!enab == !!p)
		return false;
	
	if (!enab) {
		*p++ = '\0';
		while (*p) {
			p[-1] = *p;
			*p++ = '\0';
		}
	} else {
		if (strlen(m->modepfx) + 1 >= sizeof m->modepfx)
			return false;
		p = m->modepfx;
		while (*p && compare_modepfx(h, *p, mpfxsym) > 0)
			p++;

		char *o = p + strlen(p);
		while (o >= p) {
			o[1] = o[0];
			o--;
		}

		*p = mpfxsym;
	}

	return true;
}

/* returns positive if c1 is stronger than c2.  be sure only to call 
 * for valid arguments (modepfx symbols like '@') */
static int
compare_modepfx(irc h, char c1, char c2)
{
	const char *p1 = strchr(h->m005modepfx[1], c1);
	const char *p2 = strchr(h->m005modepfx[1], c2);

	ptrdiff_t o1 = p1 - h->m005modepfx[1];
	ptrdiff_t o2 = p2 - h->m005modepfx[1];
	
	return o1 < o2 ? 1 : o1 > o2 ? -1 : 0; 
}

void
clear_chanmodes(irc h, chan c)
{
	for (size_t i = 0; i < c->modes_sz; i++)
		free(c->modes[i]), c->modes[i] = NULL;
}

bool
add_chanmode(irc h, chan c, const char *modestr)
{
	size_t ind = 0;
	for (; ind < c->modes_sz; ind++)
		if (!c->modes[ind])
			break;
	
	if (ind == c->modes_sz) {
		size_t nsz = c->modes_sz * 2;
		char **nmodes = malloc(nsz * sizeof *nmodes);
		if (!nmodes)
			return false;
		size_t i;
		for (i = 0; i < c->modes_sz; i++)
			nmodes[i] = c->modes[i];

		for (; i < nsz; i++)
			nmodes[i] = NULL;

		free(c->modes);
		c->modes = nmodes;
		c->modes_sz = nsz;
	}

	return (c->modes[ind] = strdup(modestr));
}

bool
drop_chanmode(irc h, chan c, const char *modestr)
{
	size_t i;
	int cls = ut_classify_chanmode(h, modestr[0]);
	switch (cls) {
	case CHANMODE_CLASS_A: //always has an argument (list-modes)
		for (i = 0; i < c->modes_sz; i++)
			if (c->modes[i] && strcmp(c->modes[i], modestr) == 0)
				break;
		break;
	case CHANMODE_CLASS_B: //has argument when unset but it's irrelevant
	case CHANMODE_CLASS_C: //no argument when beig unset
	case CHANMODE_CLASS_D: //never has an argument
		for (i = 0; i < c->modes_sz; i++)
			if (c->modes[i] && c->modes[i][0] == modestr[0])
				break;
		break;
	default:
		E("huh? illegal modestr '%s'", modestr);
		return false;
	}

	if (i == c->modes_sz) {
		E("chanmode '%s' not found (for dropping)", modestr);
		return false;
	}

	size_t last;
	for (last = c->modes_sz - 1; last > 0; last--)
		if (c->modes[last])
			break;
	
	free(c->modes[i]);
	if (last == i)
		c->modes[i] = NULL;
	else {
		c->modes[i] = c->modes[last];
		c->modes[last] = NULL;
	}

	return true;
}



void
ucb_dump(irc h)
{
	N("=== ucb dump (%zu chans) ===", h->chans ? smap_count(h->chans) : 0);
	const char *key;
	void *e1, *e2;
	if (!smap_first(h->chans, &key, &e1))
		return;

	do {
		chan c = e1;
		N("channel '%s' ('%s', %zu membs) [topic: '%s' (by %s)"
		    ", tsc: %"PRIu64", tst: %"PRIu64"]", c->extname, c->name,
		    smap_count(c->memb), c->topic, c->topicnick, 
		    c->tscreate, c->tstopic);

		for (size_t i = 0; i < c->modes_sz; i++) {
			if (!c->modes[i])
				break;

			N("  mode '%s'", c->modes[i]);
		}

		const char *k;
		if (!smap_first(c->memb, &k, &e2))
			continue;
		do {
			memb m = e2;
			N("    member '%s' ('%s', '%s')",
			    m->extnick, m->nick, m->modepfx);
		} while (smap_next(c->memb, &k, &e2));
	} while (smap_next(h->chans, &key, &e1));

	N("=== end of dump ===");


}
