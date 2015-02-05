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
	skmap memb; //map lnick to struct member
	bool desync;
	char **modes; //one modechar per elem, i.e. "s" or "l 123"
	size_t modes_sz;
	void *tag;
	bool freetag;
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
	void *tag;
	bool freetag;
};

bool ucb_init(irc h);
chan add_chan(irc h, const char *name);
bool drop_chan(irc h, chan c);
size_t num_chans(irc h);
chan get_chan(irc h, const char *name, bool complain);
size_t num_memb(irc h, chan c);
memb get_memb(irc h, chan c, const char *nick, bool complain);
bool add_memb(irc h, chan c, user u, const char *mpfxstr);
bool drop_memb(irc h, chan c, user u, bool purge, bool complain);
void clear_memb(irc h, chan c);
memb alloc_memb(irc h, user u, const char *mpfxstr);
bool update_modepfx(irc h, chan c, const char *nick, char mpfxsym, bool enab);
void clear_chanmodes(irc h, chan c);
bool add_chanmode(irc h, chan c, const char *modestr);
bool drop_chanmode(irc h, chan c, const char *modestr);
user touch_user(irc h, const char *ident, bool complain);
user add_user(irc h, const char *ident);
bool drop_user(irc h, user u);
void ucb_deinit(irc h);
void ucb_clear(irc h);
void ucb_dump(irc h, bool full);
user get_user(irc h, const char *ident, bool complain);
size_t num_users(irc h);
bool rename_user(irc h, const char *ident, const char *newnick, bool *allocerr);
/* these might be dangerous to use, be sure to complete the iteration
 * before any other state might change */
chan first_chan(irc h);
chan next_chan(irc h);
user first_user(irc h);
user next_user(irc h);
memb first_memb(irc h, chan c);
memb next_memb(irc h, chan c);
void tag_chan(chan c, void *tag, bool autofree);
void tag_user(user u, void *tag, bool autofree);


#endif /* LIBSRSIRC_UCBASE_H */
