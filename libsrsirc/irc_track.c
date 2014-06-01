/* irc_track.c - (optionally) track users and channels
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_TRACK

#include <string.h>

#include <intlog.h>

#include <libsrsirc/util.h>
#include <libsrsirc/irc_ext.h>

#include "intdefs.h"
#include "common.h"
#include "msg.h"
#include "ucbase.h"

#include <libsrsirc/irc_track.h>

static uint8_t h_JOIN(irc h, tokarr *msg, size_t nargs, bool logon);
static uint8_t h_332(irc h, tokarr *msg, size_t nargs, bool logon);
static uint8_t h_333(irc h, tokarr *msg, size_t nargs, bool logon);
static uint8_t h_353(irc h, tokarr *msg, size_t nargs, bool logon);
static uint8_t h_366(irc h, tokarr *msg, size_t nargs, bool logon);
static uint8_t h_PART(irc h, tokarr *msg, size_t nargs, bool logon);
static uint8_t h_QUIT(irc h, tokarr *msg, size_t nargs, bool logon);
static uint8_t h_NICK(irc h, tokarr *msg, size_t nargs, bool logon);
static uint8_t h_KICK(irc h, tokarr *msg, size_t nargs, bool logon);

bool
trk_init(irc h)
{
	bool fail = false;
	fail = fail || !msg_reghnd(h, "JOIN", h_JOIN, "track");
	fail = fail || !msg_reghnd(h, "332", h_332, "track");
	fail = fail || !msg_reghnd(h, "333", h_333, "track");
	fail = fail || !msg_reghnd(h, "353", h_353, "track");
	fail = fail || !msg_reghnd(h, "366", h_366, "track");
	fail = fail || !msg_reghnd(h, "PART", h_PART, "track");
	fail = fail || !msg_reghnd(h, "QUIT", h_QUIT, "track");
	fail = fail || !msg_reghnd(h, "NICK", h_NICK, "track");
	fail = fail || !msg_reghnd(h, "KICK", h_KICK, "track");

	if (fail)
		return false;

	h->endofnames = true;

	return ucb_init(h);
}


static uint8_t
h_JOIN(irc h, tokarr *msg, size_t nargs, bool logon)
{
	if (!(*msg)[0] || nargs < 3)
		return PROTO_ERR;

	char nick[MAX_NICK_LEN];
	ut_pfx2nick(nick, sizeof nick, (*msg)[0]);

	chan c = get_chan(h, (*msg)[2]);

	if (ut_istrcmp(nick, h->mynick, h->casemap) == 0) {
		if (!c && !add_chan(h, (*msg)[2])) {
			E("out of memory, not tracking chan '%s'", (*msg)[2]);
			return ALLOC_ERR;
		}
	} else {
		if (!c) {
			W("we don't know channel '%s'!", (*msg)[2]);
			return 0;
		}

		if (!add_memb(h, c, nick, "")) {
			E("out of memory, chan '%s' desynced", c->extname);
			return ALLOC_ERR;
		}
	}

	return 0;
}

static uint8_t
h_332(irc h, tokarr *msg, size_t nargs, bool logon)
{
	return 0;
}

static uint8_t
h_333(irc h, tokarr *msg, size_t nargs, bool logon)
{
	return 0;
}

static uint8_t
h_353(irc h, tokarr *msg, size_t nargs, bool logon)
{
	if (!(*msg)[0] || nargs < 6)
		return PROTO_ERR;

	chan c = get_chan(h, (*msg)[4]);
	if (!c) {
		W("we don't know channel '%s'!", (*msg)[2]);
		return 0;
	}

	if (h->endofnames) {
		clear_memb(h, c);
		h->endofnames = false;
	}

	char nick[MAX_NICK_LEN];
	const char *p = (*msg)[5];
	for (;;) {
		char mpfx[2] = {'\0', '\0'};

		if (strchr(h->m005modepfx[1], *p))
			mpfx[0] = *p++;

		const char *end = strchr(p, ' ');
		if (!end)
			end = p + strlen(p);

		size_t len = end - p;
		if (len >= sizeof nick)
			len = sizeof nick - 1;

		com_strNcpy(nick, p, len + 1);

		if (!add_memb(h, c, nick, mpfx))
			return ALLOC_ERR;

		if (!*end)
			break;
		p = end + 1;
	}

	return 0;
}

static uint8_t
h_366(irc h, tokarr *msg, size_t nargs, bool logon)
{
	h->endofnames = true;
	if (!(*msg)[0] || nargs < 3)
		return PROTO_ERR;

	chan c = get_chan(h, (*msg)[3]);
	if (!c) {
		W("we don't know channel '%s'!", (*msg)[2]);
		return 0;
	}
	c->desync = false;

	return 0;
}

static uint8_t
h_PART(irc h, tokarr *msg, size_t nargs, bool logon)
{
	if (!(*msg)[0] || nargs < 3)
		return PROTO_ERR;

	char nick[MAX_NICK_LEN];
	ut_pfx2nick(nick, sizeof nick, (*msg)[0]);

	chan c = get_chan(h, (*msg)[2]);
	if (!c) {
		W("we don't know channel '%s'!", (*msg)[2]);
		return 0;
	}

	if (ut_istrcmp(nick, h->mynick, h->casemap) == 0)
		c->desync = true;

	drop_memb(h, c, nick);

	return 0;
}

static uint8_t
h_QUIT(irc h, tokarr *msg, size_t nargs, bool logon)
{
	if (!(*msg)[0])
		return PROTO_ERR;

	char nick[MAX_NICK_LEN];
	ut_pfx2nick(nick, sizeof nick, (*msg)[0]);

	void *e;
	if (!smap_first(h->chans, NULL, &e))
		return 0;

	do {
		chan c = e;
		drop_memb(h, c, nick);
	} while (smap_next(h->chans, NULL, &e));

	return 0;
}

static uint8_t
h_KICK(irc h, tokarr *msg, size_t nargs, bool logon)
{
	if (!(*msg)[0] || nargs < 4)
		return PROTO_ERR;

	chan c = get_chan(h, (*msg)[2]);
	if (!c) {
		W("we don't know channel '%s'!", (*msg)[2]);
		return 0;
	}

	if (ut_istrcmp((*msg)[3], h->mynick, h->casemap)) == 0)
		c->desync = true;

	drop_memb(h, c, (*msg)[3]);

	return 0;
}

static uint8_t
h_NICK(irc h, tokarr *msg, size_t nargs, bool logon)
{
	uint8_t res = 0;

	if (!(*msg)[0] || nargs < 3)
		return PROTO_ERR;

	char nick[MAX_NICK_LEN];
	ut_pfx2nick(nick, sizeof nick, (*msg)[0]);

	void *e;
	if (!smap_first(h->chans, NULL, &e))
		return 0;

	do {
		chan c = e;
		memb m = get_memb(h, c, nick);
		if (!m)
			continue;

		if (!add_memb(h, c, (*msg)[2], m->modepfx)) {
			E("out of memory, chan '%s' desynced", c->extname);
			res |= ALLOC_ERR;
			c->desync = true;
		}

		drop_memb(h, c, nick);
	} while (smap_next(h->chans, NULL, &e));

	return res;
}

void
trk_dump(irc h)
{
	ucb_dump(h);
}

// JOIN #srsbsns redacted
// :mynick!~myuser@example.org JOIN #srsbsns

/* only if topic is present*/
// :port80c.se.quakenet.org 332 mynick #srsbsns :topic text
// :port80c.se.quakenet.org 333 mynick #srsbsns topicnick 1401460661

/* always */
// :port80c.se.quakenet.org 353 mynick @ #srsbsns :mynick fisted @foo +bar
// :port80c.se.quakenet.org 366 mynick #srsbsns :End of /NAMES list.

// -------

// MODE #srsbsns
// :port80c.se.quakenet.org 324 mynick #srsbsns +sk *
// :port80c.se.quakenet.org 329 mynick #srsbsns 1313083493
