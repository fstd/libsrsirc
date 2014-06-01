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

struct chan {
	char name[MAX_CHAN_LEN];
	char extname[MAX_CHAN_LEN];
	char *topic;
	char *modestr;
	uint64_t tscreate;
	uint64_t tstopic;
	smap memb; //map lnick to struct member
	bool desync;
};

struct member {
	char nick[MAX_NICK_LEN];
	char extnick[MAX_NICK_LEN];
	char modepfx[MAX_MODEPFX];
};


bool ucb_init(irc h);
chan add_chan(irc h, const char *name);
chan get_chan(irc h, const char *name);
void clear_memb(irc h, chan c);
memb get_memb(irc h, chan c, const char *nick);
bool add_memb(irc h, chan c, const char *nick, const char *mpfxstr);
bool drop_memb(irc h, chan c, const char *nick);
memb alloc_memb(irc h, const char *nick, const char *mpfxstr);
void ucb_dump(irc h);

#endif /* LIBSRSIRC_UCBASE_H */
