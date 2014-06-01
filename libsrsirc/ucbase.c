/* ucbase.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_UCBASE

#include <stdlib.h>

#include <inttypes.h>

#include <intlog.h>

#include "smap.h"
#include "common.h"
#include <libsrsirc/util.h>
#include "ucbase.h"

bool
ucb_init(irc h)
{
	return (h->chans = smap_init(256));
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
	c->topic = c->modestr = NULL;
	c->tscreate = c->tstopic = 0;
	c->memb = smap_init(256);
	c->desync = false;

	if (!smap_put(h->chans, lname, c))
		goto add_chan_fail;

	D("added chan '%s' ('%s')", c->extname, c->name);

	return c;

add_chan_fail:
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

	return smap_del(c->memb, m->nick);
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

void
ucb_dump(irc h)
{
	N("=== ucb dump (%zu chans) ===", smap_count(h->chans));
	const char *key;
	void *e1, *e2;
	if (!smap_first(h->chans, &key, &e1))
		return;

	do {
		chan c = e1;
		N("channel '%s' ('%s', %zu membs) [topic: '%s', modestr: '%s', "
		    "tsc: %"PRIu64", tst: %"PRIu64"]", c->extname, c->name,
		    smap_count(c->memb), c->topic, c->modestr, c->tscreate,
		    c->tstopic);

		const char *k;
		if (!smap_first(c->memb, &k, &e2))
			continue;
		do {
			memb m = e2;
			N("\tmember '%s' ('%s', '%s')",
			    m->extnick, m->nick, m->modepfx);
		} while (smap_next(c->memb, &k, &e2));

		N("\t-- end of member --");
	} while (smap_next(h->chans, &key, &e1));

	N("=== end of dump ===");


}
