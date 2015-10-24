/* ucbase.c - user and channel base
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_UCBASE

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "ucbase.h"


#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <platform/base_string.h>

#include <logger/intlog.h>

#include "skmap.h"
#include "common.h"

#include <libsrsirc/util.h>


static int compare_modepfx(irc *ctx, char c1, char c2);


bool
lsi_ucb_init(irc *ctx)
{
	if (!(ctx->chans = lsi_skmap_init(256, ctx->casemap)))
		return false;

	if (!(ctx->users = lsi_skmap_init(4096, ctx->casemap)))
		return lsi_skmap_dispose(ctx->chans), false;

	return true;
}

chan *
lsi_add_chan(irc *ctx, const char *name)
{
	chan *c = lsi_com_malloc(sizeof *c);
	if (!c)
		goto add_chan_fail;

	lsi_com_strNcpy(c->name, name, sizeof c->name);
	c->topic = c->topicnick = NULL;
	c->tscreate = c->tstopic = 0;
	c->desync = false;
	c->modes = NULL;
	c->tag = NULL;
	c->freetag = false;

	if (!(c->memb = lsi_skmap_init(256, ctx->casemap)))
		goto add_chan_fail;

	c->modes_sz = 16; //grows
	if (!(c->modes = lsi_com_malloc(c->modes_sz * sizeof *c->modes)))
		goto add_chan_fail;

	for (size_t i = 0; i < c->modes_sz; i++)
		c->modes[i] = NULL;

	if (!lsi_skmap_put(ctx->chans, name, c))
		goto add_chan_fail;

	D("added chan '%s'", c->name);

	return c;

add_chan_fail:
	if (c) {
		if (c->modes)
			free(c->modes[0]);

		free(c->modes);
		lsi_skmap_dispose(c->memb);
	}

	free(c);
	return NULL;
}

bool
lsi_drop_chan(irc *ctx, chan *c)
{
	if (!lsi_skmap_del(ctx->chans, c->name)) {
		W("channel '%s' not in channel map", c->name);
		return false;
	}

	void *e;
	if (lsi_skmap_first(c->memb, NULL, &e)) {
		do {
			memb *m = e;
			if (--m->u->nchans == 0) {
				if (!lsi_skmap_del(ctx->users, m->u->nick))
					W("user '%s' not in umap", m->u->nick);
				D("implicitly dropped user '%s'", m->u->nick);
				free(m->u->nick);
				free(m->u->uname);
				free(m->u->host);
				free(m->u->fname);
				if (m->u->freetag)
					free(m->u->tag);
				free(m->u);
			}
			free(m);
		} while (lsi_skmap_next(c->memb, NULL, &e));
		lsi_skmap_clear(c->memb);
	}
	lsi_skmap_dispose(c->memb);

	D("dropped channel '%s'", c->name);

	free(c->topic);
	free(c->topicnick);
	for (size_t i = 0; i < c->modes_sz; i++)
		free(c->modes[i]);
	free(c->modes);
	if (c->freetag)
		free(c->tag);
	free(c);
	return true;
}

size_t
lsi_num_chans(irc *ctx)
{
	return lsi_skmap_count(ctx->chans);
}

chan *
lsi_get_chan(irc *ctx, const char *name, bool complain)
{
	chan *c = lsi_skmap_get(ctx->chans, name);
	if (!c && complain)
		W("no such channel '%s' in chanmap", name);
	return c;
}

memb *
lsi_get_memb(irc *ctx, chan *c, const char *nick, bool complain)
{
	memb *m = lsi_skmap_get(c->memb, nick);
	if (!m && complain)
		W("no such member '%s' in channel '%s'", nick, c->name);
	return m;
}

size_t
lsi_num_memb(irc *ctx, chan *c)
{
	return lsi_skmap_count(c->memb);
}

bool
lsi_add_memb(irc *ctx, chan *c, user *u, const char *mpfxstr)
{
	memb *m = lsi_alloc_memb(ctx, u, mpfxstr);
	if (!m || !lsi_skmap_put(c->memb, u->nick, m)) {
		free(m);
		return false;
	}

	u->nchans++;
	D("added member '%s' to chan '%s'", u->nick, c->name);
	return true;
}

bool
lsi_drop_memb(irc *ctx, chan *c, user *u, bool purge, bool complain)
{
	memb *m = lsi_skmap_del(c->memb, u->nick);
	if (m) {
		D("dropped '%s' from '%s'", m->u->nick, c->name);
		if (--m->u->nchans == 0 && purge) {
			if (!lsi_skmap_del(ctx->users, m->u->nick))
				W("user '%s' not in user map", m->u->nick);
			D("implicitly dropped user '%s'", m->u->nick);
			free(m->u->nick);
			free(m->u->uname);
			free(m->u->host);
			free(m->u->fname);
			if (m->u->freetag)
				free(m->u->tag);
			free(m->u);
		}
	} else if (complain)
		W("no such member '%s' in channel '%s'", u->nick, c->name);

	free(m);
	return m;
}

void
lsi_clear_memb(irc *ctx, chan *c)
{
	void *e;
	if (!lsi_skmap_first(c->memb, NULL, &e))
		return;

	do {
		memb *m = e;
		if (--m->u->nchans== 0) {
			if (!lsi_skmap_del(ctx->users, m->u->nick))
				W("user '%s' not in user map", m->u->nick);
			D("implicitly dropped user '%s'", m->u->nick);
			free(m->u->nick);
			free(m->u->uname);
			free(m->u->host);
			free(m->u->fname);
			if (m->u->freetag)
				free(m->u->tag);
			free(m->u);
		}
		free(m);
	} while (lsi_skmap_next(c->memb, NULL, &e));
	lsi_skmap_clear(c->memb);
	D("cleared members of channel '%s'", c->name);
	return;
}

memb *
lsi_alloc_memb(irc *ctx, user *u, const char *mpfxstr)
{
	memb *m = lsi_com_malloc(sizeof *m);
	if (!m)
		goto alloc_memb_fail;

	m->u = u;
	lsi_com_strNcpy(m->modepfx, mpfxstr, sizeof m->modepfx);

	return m;

alloc_memb_fail:
	free(m);
	return NULL;
}

bool
lsi_update_modepfx(irc *ctx, chan *c, const char *nick, char mpfxsym, bool enab)
{
	memb *m = lsi_get_memb(ctx, c, nick, true);
	if (!m)
		return false;

	char *p = strchr(m->modepfx, mpfxsym);

	if (!!enab == !!p) {
		W("enab: %d, p: '%s' (c: '%s', n: '%s', mpfx: '%s', sym: '%c')",
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
		while (*p && compare_modepfx(ctx, *p, mpfxsym) > 0)
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
compare_modepfx(irc *ctx, char c1, char c2)
{
	const char *p1 = strchr(ctx->m005modepfx[1], c1);
	const char *p2 = strchr(ctx->m005modepfx[1], c2);

	ptrdiff_t o1 = p1 - ctx->m005modepfx[1];
	ptrdiff_t o2 = p2 - ctx->m005modepfx[1];

	return o1 < o2 ? 1 : o1 > o2 ? -1 : 0;
}

void
lsi_clear_chanmodes(irc *ctx, chan *c)
{
	for (size_t i = 0; i < c->modes_sz; i++)
		free(c->modes[i]), c->modes[i] = NULL;
	return;
}

bool
lsi_add_chanmode(irc *ctx, chan *c, const char *modestr)
{
	size_t ind = 0;
	for (; ind < c->modes_sz; ind++)
		if (!c->modes[ind])
			break;

	if (ind == c->modes_sz) {
		size_t nsz = c->modes_sz * 2;
		char **nmodes = lsi_com_malloc(nsz * sizeof *nmodes);
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

	return (c->modes[ind] = lsi_b_strdup(modestr));
}

bool
lsi_drop_chanmode(irc *ctx, chan *c, const char *modestr)
{
	size_t i;
	int cls = lsi_ut_classify_chanmode(ctx, modestr[0]);
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
lsi_touch_user_int(user *u, const char *ident)
{
	if (!u->uname && strchr(ident, '!')) {
		char unam[MAX_UNAME_LEN];
		lsi_ut_ident2uname(unam, sizeof unam, ident);
		u->uname = lsi_b_strdup(unam); //pointless to check
	}

	if (!u->host && strchr(ident, '@')) {
		char host[MAX_HOST_LEN];
		lsi_ut_ident2host(host, sizeof host, ident);
		u->host = lsi_b_strdup(host); //pointless to check
	}
	return;
}

user *
lsi_touch_user(irc *ctx, const char *ident, bool complain)
{
	user *u = lsi_get_user(ctx, ident, complain);
	if (u)
		lsi_touch_user_int(u, ident);
	return u;
}


user *
lsi_add_user(irc *ctx, const char *ident) //ident may be a nick, or nick!uname@host
{
	char nick[MAX_NICK_LEN];
	lsi_ut_ident2nick(nick, sizeof nick, ident);

	user *u = lsi_com_malloc(sizeof *u);
	if (!u)
		goto add_user_fail;

	u->uname = u->host = u->fname = NULL;
	u->nchans = 0;
	u->tag = NULL;
	u->freetag = false;

	if (!(u->nick = lsi_b_strdup(nick)))
		goto add_user_fail;

	if (!lsi_skmap_put(ctx->users, nick, u))
		goto add_user_fail;

	lsi_touch_user_int(u, ident);

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
lsi_drop_user(irc *ctx, user *u)
{
	if (!lsi_skmap_del(ctx->users, u->nick)) {
		W("no such user '%s' to drop", u->nick);
		return false;
	}

	void *e;
	if (lsi_skmap_first(ctx->chans, NULL, &e))
		do {
			lsi_drop_memb(ctx, e, u, false, false);
		} while (lsi_skmap_next(ctx->chans, NULL, &e));
	else
		W("dropping dangling user '%s'", u->nick);

	D("dropped user '%s'", u->nick);

	free(u->nick);
	free(u->uname);
	free(u->host);
	free(u->fname);
	if (u->freetag)
		free(u->tag);
	free(u);

	return true;
}

void
lsi_ucb_deinit(irc *ctx)
{
	lsi_ucb_clear(ctx);
	lsi_skmap_dispose(ctx->chans);
	lsi_skmap_dispose(ctx->users);
	ctx->chans = ctx->users = NULL;
	return;
}

void
lsi_ucb_clear(irc *ctx)
{
	void *e;
	if (ctx->chans) {
		if (!lsi_skmap_first(ctx->chans, NULL, &e))
			return;

		do {
			chan *c = e;
			lsi_clear_memb(ctx, c);
			lsi_skmap_dispose(c->memb);
			free(c->topicnick);
			free(c->topic);
			for (size_t i = 0; i < c->modes_sz; i++)
				free(c->modes[i]);
			free(c->modes);
			free(c);
		} while (lsi_skmap_next(ctx->chans, NULL, &e));
		lsi_skmap_clear(ctx->chans);
	}

	if (ctx->users) {
		if (!lsi_skmap_first(ctx->users, NULL, &e))
			return;

		do {
			user *u = e;
			free(u->nick);
			free(u->uname);
			free(u->host);
			free(u->fname);
			free(u);
		} while (lsi_skmap_next(ctx->users, NULL, &e));
		lsi_skmap_clear(ctx->users);
	}
	return;
}

void
lsi_ucb_dump(irc *ctx, bool full)
{
	lsi_skmap_dumpstat(ctx->chans, "channels");
	lsi_skmap_dumpstat(ctx->users, "global users");

	char *key;
	void *e1, *e2;
	if (lsi_skmap_first(ctx->chans, NULL, &e1))
		do {
			chan *c = e1;
			lsi_skmap_dumpstat(c->memb, c->name);
		} while (lsi_skmap_next(ctx->chans, NULL, &e1));

	if (!full)
		return;

	if (lsi_skmap_first(ctx->users, &key, &e1))
		do {
			user *u = e1;
			u->dangling = true;
		} while (lsi_skmap_next(ctx->users, &key, &e1));

	if (lsi_skmap_first(ctx->chans, &key, &e1))
		do {
			chan *c = e1;
			A("channel '%s' (%zu membs) [topic: '%s' (by %s)"
			    ", tsc: %"PRIu64", tst: %"PRIu64"]", c->name,
			    lsi_skmap_count(c->memb), c->topic, c->topicnick,
			    c->tscreate, c->tstopic);

			for (size_t i = 0; i < c->modes_sz; i++) {
				if (!c->modes[i])
					break;

				A("  mode '%s'", c->modes[i]);
			}

			char *k;
			if (!lsi_skmap_first(c->memb, &k, &e2))
				continue;
			do {
				memb *m = e2;
				A("    member ('%s') '%s!%s@%s' ['%s']",
				    m->modepfx, m->u->nick, m->u->uname,
				    m->u->host, m->u->fname);

				m->u->dangling = false;
			} while (lsi_skmap_next(c->memb, &k, &e2));
		} while (lsi_skmap_next(ctx->chans, &key, &e1));

	if (lsi_skmap_first(ctx->users, &key, &e1))
		do {
			user *u = e1;
			if (u->dangling)
				A("dangling user '%s!%s@%s'",
				    u->nick, u->uname, u->host);
		} while (lsi_skmap_next(ctx->users, &key, &e1));
	return;
}

user *
lsi_get_user(irc *ctx, const char *ident, bool complain)
{
	user *u = lsi_skmap_get(ctx->users, ident);
	if (!u && complain)
		W("no such user '%s'", ident);
	return u;
}

size_t
lsi_num_users(irc *ctx)
{
	return lsi_skmap_count(ctx->users);
}

bool
lsi_rename_user(irc *ctx, const char *ident, const char *newnick, bool *allocerr)
{
	char nick[MAX_NICK_LEN];
	lsi_ut_ident2nick(nick, sizeof nick, ident);

	bool justcase = lsi_ut_istrcmp(nick, newnick, ctx->casemap) == 0;

	if (allocerr)
		*allocerr = false;

	user *u = lsi_get_user(ctx, ident, true);
	if (!u)
		return false;

	char *nn = NULL;
	if (justcase) {
		lsi_com_strNcpy(u->nick, newnick, strlen(u->nick) + 1);
		return true;
	} else {
		if (!(nn = lsi_b_strdup(newnick)))
			return false; //oh shit.
		free(u->nick);
		u->nick = nn;
	}

	if (!lsi_skmap_put(ctx->users, newnick, u)) {
		if (allocerr)
			*allocerr = true;
		return false;
	}

	lsi_skmap_del(ctx->users, ident);

	void *e;
	if (lsi_skmap_first(ctx->chans, NULL, &e))
		do {
			chan *c = e;
			memb *m = lsi_skmap_get(c->memb, ident);
			if (!m)
				continue;

			if (!lsi_skmap_put(c->memb, newnick, m)) {
				if (allocerr)
					*allocerr = true;
				return false;
			}

			lsi_skmap_del(c->memb, ident);
		} while (lsi_skmap_next(ctx->chans, NULL, &e));

	return true;
}

chan *
lsi_first_chan(irc *ctx)
{
	void *e;
	if (!lsi_skmap_first(ctx->chans, NULL, &e))
		return NULL;
	return e;
}

chan *
lsi_next_chan(irc *ctx)
{
	void *e;
	if (!lsi_skmap_next(ctx->chans, NULL, &e))
		return NULL;
	return e;
}

user *
lsi_first_user(irc *ctx)
{
	void *e;
	if (!lsi_skmap_next(ctx->users, NULL, &e))
		return NULL;
	return e;
}

user *
lsi_next_user(irc *ctx)
{
	void *e;
	if (!lsi_skmap_next(ctx->users, NULL, &e))
		return NULL;
	return e;
}

memb *
lsi_first_memb(irc *ctx, chan *c)
{
	void *e;
	if (!lsi_skmap_next(c->memb, NULL, &e))
		return NULL;
	return e;
}

memb *
lsi_next_memb(irc *ctx, chan *c)
{
	void *e;
	if (!lsi_skmap_next(c->memb, NULL, &e))
		return NULL;
	return e;
}

void
lsi_tag_chan(chan *c, void *tag, bool autofree)
{
	if (c->freetag)
		free(c->tag);
	c->tag = tag;
	c->freetag = autofree;
	return;
}


void
lsi_tag_user(user *u, void *tag, bool autofree)
{
	if (u->freetag)
		free(u->tag);
	u->tag = tag;
	u->freetag = autofree;
	return;
}
