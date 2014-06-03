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
	char *topicnick;
	uint64_t tscreate;
	uint64_t tstopic;
	smap memb; //map lnick to struct member
	bool desync;
	char **modes; //one modechar per elem, i.e. "s" or "l 123" 
	size_t modes_sz;
};


struct member {
	char nick[MAX_NICK_LEN];
	char extnick[MAX_NICK_LEN];
	char modepfx[MAX_MODEPFX];
};


bool ucb_init(irc h);
void ucb_deinit(irc h);
void ucb_clear(irc h);
chan add_chan(irc h, const char *name);
chan get_chan(irc h, const char *name);
void clear_memb(irc h, chan c);
memb get_memb(irc h, chan c, const char *nick);
bool add_memb(irc h, chan c, const char *nick, const char *mpfxstr);
bool drop_memb(irc h, chan c, const char *nick);
memb alloc_memb(irc h, const char *nick, const char *mpfxstr);
bool update_modepfx(irc h, chan c, const char *nick, char mpfxsym, bool enab);
void clear_chanmodes(irc h, chan c);
bool add_chanmode(irc h, chan c, const char *modestr);
bool drop_chanmode(irc h, chan c, const char *modestr);
void ucb_dump(irc h);

#endif /* LIBSRSIRC_UCBASE_H */
