/* ucbase.h -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_UCBASE_H
#define LIBSRSIRC_UCBASE_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include "intdefs.h"

typedef struct chan *chan;
typedef struct member *memb;
typedef struct user *user;

struct chan {
	char name[MAX_CHAN_LEN];
	char *topic;
	char *topicnick;
	uint64_t tscreate;
	uint64_t tstopic;
	smap memb; //map lnick to struct member
	bool desync;
	char **modes; //one modechar per elem, i.e. "s" or "l 123"
	size_t modes_sz;
};


struct member {
	user u;
	char modepfx[MAX_MODEPFX];
};

struct user {
	char *nick;
	char *uname;
	char *host;
	char *fname;
	size_t nchans;
	bool dangling; //debug
};

bool ucb_init(irc h);
chan add_chan(irc h, const char *name);
bool drop_chan(irc h, chan c);
chan get_chan(irc h, const char *name);
memb get_memb(irc h, chan c, const char *nick);
bool add_memb(irc h, chan c, user u, const char *mpfxstr);
bool drop_memb(irc h, chan c, user u);
void clear_memb(irc h, chan c);
memb alloc_memb(irc h, user u, const char *mpfxstr);
bool update_modepfx(irc h, chan c, const char *nick, char mpfxsym, bool enab);
void clear_chanmodes(irc h, chan c);
bool add_chanmode(irc h, chan c, const char *modestr);
bool drop_chanmode(irc h, chan c, const char *modestr);
user touch_user(irc h, const char *ident);
user add_user(irc h, const char *ident);
bool drop_user(irc h, user u);
void ucb_deinit(irc h);
void ucb_clear(irc h);
void ucb_dump(irc h);
user get_user(irc h, const char *ident);
bool rename_user(irc h, const char *ident, const char *newnick);
#endif /* LIBSRSIRC_UCBASE_H */
