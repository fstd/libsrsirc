/* irc_track.h - (optionally) track users and channels
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_TRACK_H
#define LIBSRSIRC_IRC_TRACK_H 1

#include <libsrsirc/defs.h>
#include "smap.h"

typedef struct chan chan;
typedef struct user user;

struct chan {
	char *name;
	char *topic;
	char *modestr;
	uint64_t tscreate;
	uint64_t tstopic;
	smap memb;
};

struct user {
	char *ident; //may be just a nick, or nick!uname@host
};

#endif /* LIBSRSIRC_IRC_TRACK_H */
