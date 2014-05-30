/* irc_track.c - (optionally) track users and channels
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_TRACK

#include <intlog.h>

#include "msg.h"
#include "common.h"
#include <libsrsirc/irc_track.h>

static smap chans; //map lowercase chan name to struct chan
static smap users; //map lowercase nick to struct user,
static bool s_track_users;

static uint8_t h_JOIN(irc h, tokarr *msg, size_t nargs, bool logon, void *tag);
static uint8_t h_332(irc h, tokarr *msg, size_t nargs, bool logon, void *tag);
static uint8_t h_333(irc h, tokarr *msg, size_t nargs, bool logon, void *tag);
static uint8_t h_353(irc h, tokarr *msg, size_t nargs, bool logon, void *tag);
static uint8_t h_366(irc h, tokarr *msg, size_t nargs, bool logon, void *tag);

bool
trk_init(bool track_users)
{
	bool fail = false;
	fail = fail || !msg_reghnd("JOIN", h_JOIN, "track", NULL);
	fail = fail || !msg_reghnd("332", h_332, "track", NULL);
	fail = fail || !msg_reghnd("333", h_333, "track", NULL);
	fail = fail || !msg_reghnd("353", h_353, "track", NULL);
	fail = fail || !msg_reghnd("366", h_366, "track", NULL);

	if (fail)
		return false;

	if (!(chans = smap_init(64)))
		return false;

	if (track_users && !(users = smap_init(256)))
		return false;

	s_track_users = track_users;

	return true;
}


static uint8_t
h_JOIN(irc h, tokarr *msg, size_t nargs, bool logon, void *tag)
{
	return 0;
}

static uint8_t
h_332(irc h, tokarr *msg, size_t nargs, bool logon, void *tag)
{
	return 0;
}

static uint8_t
h_333(irc h, tokarr *msg, size_t nargs, bool logon, void *tag)
{
	return 0;
}

static uint8_t
h_353(irc h, tokarr *msg, size_t nargs, bool logon, void *tag)
{
	return 0;
}

static uint8_t
h_366(irc h, tokarr *msg, size_t nargs, bool logon, void *tag)
{
	return 0;
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
