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
	if (!(h->chans = smap_init(256, h->casemap)))
		return false;

	if (!(h->users = smap_init(4096, h->casemap)))
		return smap_dispose(h->chans), false;
	
	return true;
}

chan
add_chan(irc h, const char *name)
{
	chan c = com_malloc(sizeof *c);
	if (!c)
		goto add_chan_fail;

	com_strNcpy(c->name, name, sizeof c->name);
	c->topic = c->topicnick = NULL;
	c->tscreate = c->tstopic = 0;
	c->desync = false;
	c->modes = NULL;

	if (!(c->memb = smap_init(256, h->casemap)))
		goto add_chan_fail;

	c->modes_sz = 16; //grows
	if (!(c->modes = com_malloc(c->modes_sz * sizeof *c->modes)))
		goto add_chan_fail;
	
	for (size_t i = 0; i < c->modes_sz; i++)
		c->modes[i] = NULL;
	
	if (!smap_put(h->chans, name, c))
		goto add_chan_fail;

	D("added chan '%s'", c->name);

	return c;

add_chan_fail:
	if (c) {
		if (c->modes)
			free(c->modes[0]);

		free(c->modes);
		smap_dispose(c->memb);
	}

	free(c);
	return NULL;
}

bool
drop_chan(irc h, chan c)
{
	if (!smap_del(h->chans, c->name)) {
		W("channel '%s' not in channel map", c->name);
		return false;
	}

	void *e;
	if (smap_first(c->memb, NULL, &e)) {
		do {
			memb m = e;
			if (--m->u->nchans == 0) {
				if (!smap_del(h->users, m->u->nick))
					W("user '%s' not in user map", m->u->nick);
				D("implicitly dropped user '%s'", m->u->nick);
				free(m->u->nick);
				free(m->u->uname);
				free(m->u->host);
				free(m->u->fname);
				free(m->u);
			}
			free(m);
		} while (smap_next(c->memb, NULL, &e));
		smap_clear(c->memb);
	}
	smap_dispose(c->memb);

	D("dropped channel '%s'", c->name);

	free(c->topic);
	free(c->topicnick);
	for (size_t i = 0; i < c->modes_sz; i++)
		free(c->modes[i]);
	free(c->modes);
	free(c);
	return true;
}

chan
get_chan(irc h, const char *name, bool complain)
{
	chan c = smap_get(h->chans, name);
	if (!c && complain)
		W("no such channel '%s' in chanmap", name); 
	return c;
}

memb
get_memb(irc h, chan c, const char *nick, bool complain)
{
	memb m = smap_get(c->memb, nick);
	if (!m && complain)
		W("no such member '%s' in channel '%s'", nick, c->name); 
	return m;
}

bool
add_memb(irc h, chan c, user u, const char *mpfxstr)
{
	memb m = alloc_memb(h, u, mpfxstr);
	if (!m || !smap_put(c->memb, u->nick, m)) {
		free(m);
		return false;
	}

	u->nchans++;
	D("added member '%s' to chan '%s'", u->nick, c->name);
	return true;
}

bool
drop_memb(irc h, chan c, user u, bool purge, bool complain)
{
	memb m = smap_del(c->memb, u->nick);
	if (m) {
		D("dropped '%s' from '%s'", m->u->nick, c->name);
		if (--m->u->nchans == 0 && purge) {
			if (!smap_del(h->users, m->u->nick))
				W("user '%s' not in user map", m->u->nick);
			D("implicitly dropped user '%s'", m->u->nick);
			free(m->u->nick);
			free(m->u->uname);
			free(m->u->host);
			free(m->u->fname);
			free(m->u);
		}
	} else if (complain)
		W("no such member '%s' in channel '%s'", u->nick, c->name);

	free(m);
	return m;
}

void
clear_memb(irc h, chan c)
{
	void *e;
	if (!smap_first(c->memb, NULL, &e))
		return;
	
	do {
		memb m = e;
		if (--m->u->nchans== 0) {
			if (!smap_del(h->users, m->u->nick))
				W("user '%s' not in user map", m->u->nick);
			D("implicitly dropped user '%s'", m->u->nick);
			free(m->u->nick);
			free(m->u->uname);
			free(m->u->host);
			free(m->u->fname);
			free(m->u);
		}
		free(m);
	} while (smap_next(c->memb, NULL, &e));
	smap_clear(c->memb);
	D("cleared members of channel '%s'", c->name);
}

memb
alloc_memb(irc h, user u, const char *mpfxstr)
{
	memb m = com_malloc(sizeof *m);
	if (!m)
		goto alloc_memb_fail;

	m->u = u;
	com_strNcpy(m->modepfx, mpfxstr, sizeof m->modepfx);

	return m;

alloc_memb_fail:
	free(m);
	return NULL;
}

bool
update_modepfx(irc h, chan c, const char *nick, char mpfxsym, bool enab)
{
	memb m = get_memb(h, c, nick, true);
	if (!m)
		return false;

	char *p = strchr(m->modepfx, mpfxsym);

	if (!!enab == !!p) {
		W("enab: %d, p: '%s' (c: '%s', n: '%s', mpfx: '%s', mpfxsym: '%c')",
		    enab, p, c->name, m->u->nick, m->modepfx, mpfxsym);
		return false;
	}
	
	if (!enab) {
		*p++ = '\0';
		while (*p) {
			p[-1] = *p;
			*p++ = '\0';
		}
	} else {
		if (strlen(m->modepfx) + 1 >= sizeof m->modepfx) {
			E("not enough space for yet another modepfx!");
			return false;
		}
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
		char **nmodes = com_malloc(nsz * sizeof *nmodes);
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

	return (c->modes[ind] = com_strdup(modestr));
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
		D("chanmode '%s' not found (for dropping)", modestr);
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
touch_user_int(user u, const char *ident)
{
	if (!u->uname && strchr(ident, '!')) {
		char unam[MAX_UNAME_LEN];
		ut_pfx2uname(unam, sizeof unam, ident);
		u->uname = com_strdup(unam); //pointless to check
	}

	if (!u->host && strchr(ident, '@')) {
		char host[MAX_HOST_LEN];
		ut_pfx2host(host, sizeof host, ident);
		u->host = com_strdup(host); //pointless to check
	}
}

user
touch_user(irc h, const char *ident, bool complain)
{
	user u = get_user(h, ident, complain);
	if (u)
		touch_user_int(u, ident);
	return u;
}


user
add_user(irc h, const char *ident) //ident may be a nick, or nick!uname@host style
{
	char nick[MAX_NICK_LEN];
	ut_pfx2nick(nick, sizeof nick, ident);

	user u = com_malloc(sizeof *u);
	if (!u)
		goto add_user_fail;

	u->uname = u->host = u->fname = NULL;
	u->nchans = 0;

	if (!(u->nick = com_strdup(nick)))
		goto add_user_fail;

	if (!smap_put(h->users, nick, u))
		goto add_user_fail;

	touch_user_int(u, ident);

	D("added user '%s' ('%s@%s')", u->nick, u->uname, u->host);

	return u;

add_user_fail:
	if (u) {
		free(u->nick);
		free(u->uname);
		free(u->host);
	}

	free(u);
	return NULL;
}

bool
drop_user(irc h, user u)
{
	if (!smap_del(h->users, u->nick)) {
		W("no such user '%s' to drop", u->nick);
		return false;
	}

	void *e;
	if (smap_first(h->chans, NULL, &e))
		do {
			drop_memb(h, e, u, false, false);
		} while (smap_next(h->chans, NULL, &e));
	else
		W("dropping dangling user '%s'", u->nick);

	D("dropped user '%s'", u->nick);

	free(u->nick);
	free(u->uname);
	free(u->host);
	free(u->fname);
	free(u);

	return true;
}

void
ucb_deinit(irc h)
{
	ucb_clear(h);
	smap_dispose(h->chans);
	smap_dispose(h->users);
	h->chans = h->users = NULL;
}

void
ucb_clear(irc h)
{
	void *e;
	if (h->chans) {
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

	if (h->users) {
		if (!smap_first(h->users, NULL, &e))
			return;
		
		do {
			user u = e;
			free(u->nick);
			free(u->uname);
			free(u->host);
			free(u->fname);
			free(u);
		} while (smap_next(h->users, NULL, &e));
		smap_clear(h->users);
	}
}

void
ucb_dump(irc h)
{
	N("=== ucb dump (%zu chans, %zu users) ===",
	    h->chans ? smap_count(h->chans) : 0,
	    h->users ? smap_count(h->users) : 0);
	char *key;
	void *e1, *e2;
	if (smap_first(h->users, &key, &e1))
		do {
			user u = e1;
			u->dangling = true;
		} while (smap_next(h->users, &key, &e1));

	if (smap_first(h->chans, &key, &e1)) {
		do {
			chan c = e1;
			N("channel '%s' (%zu membs) [topic: '%s' (by %s)"
			    ", tsc: %"PRIu64", tst: %"PRIu64"]", c->name,
			    smap_count(c->memb), c->topic, c->topicnick, 
			    c->tscreate, c->tstopic);

			for (size_t i = 0; i < c->modes_sz; i++) {
				if (!c->modes[i])
					break;

				N("  mode '%s'", c->modes[i]);
			}

			char *k;
			if (!smap_first(c->memb, &k, &e2))
				continue;
			do {
				memb m = e2;
				I("    member '%s!%s@%s' ('%s')", m->u->nick, m->u->uname, m->u->host, m->modepfx);
				m->u->dangling = false;
			} while (smap_next(c->memb, &k, &e2));
			smap_dumpstat(c->memb);
		} while (smap_next(h->chans, &key, &e1));
		smap_dumpstat(h->chans);
	}

	if (smap_first(h->users, &key, &e1)) {
		do {
			user u = e1;
			if (u->dangling)
				N("dangling user '%s!%s@%s'", u->nick, u->uname, u->host);
		} while (smap_next(h->users, &key, &e1));
		smap_dumpstat(h->users);
	}
	N("=== end of dump ===");


}

user
get_user(irc h, const char *ident, bool complain)
{
	user u = smap_get(h->users, ident);
	if (!u && complain)
		W("no such user '%s'", ident);
	return u;
}

bool
rename_user(irc h, const char *ident, const char *newnick, bool *allocerr) //mh.
{
	char nick[MAX_NICK_LEN];
	ut_pfx2nick(nick, sizeof nick, ident);

	bool justcase = ut_istrcmp(nick, newnick, h->casemap) == 0;

	if (allocerr)
		*allocerr = false;

	user u = get_user(h, ident, true);
	if (!u)
		return false;

	char *nn = NULL;
	if (justcase) {
		com_strNcpy(u->nick, newnick, strlen(u->nick) + 1);
		return true;
	} else {
		if (!(nn = com_strdup(newnick)))
			return false; //oh shit.
		free(u->nick);
		u->nick = nn;
	}

	if (!smap_put(h->users, newnick, u)) {
		if (allocerr)
			*allocerr = true;
		return false;
	}
	
	smap_del(h->users, ident);

	void *e;
	if (smap_first(h->chans, NULL, &e))
		do {
			chan c = e;
			memb m = smap_get(c->memb, ident);
			if (!m)
				continue;

			if (!smap_put(c->memb, newnick, m)) {
				if (allocerr)
					*allocerr = true;
				return false;
			}

			smap_del(c->memb, ident);
		} while (smap_next(h->chans, NULL, &e));

	return true;
}
