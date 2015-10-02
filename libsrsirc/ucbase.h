/* ucbase.h - user and channel base, interface (lib-internal)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
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

bool lsi_ucb_init(irc h);
chan lsi_add_chan(irc h, const char *name);
bool lsi_drop_chan(irc h, chan c);
size_t lsi_num_chans(irc h);
chan lsi_get_chan(irc h, const char *name, bool complain);
size_t lsi_num_memb(irc h, chan c);
memb lsi_get_memb(irc h, chan c, const char *nick, bool complain);
bool lsi_add_memb(irc h, chan c, user u, const char *mpfxstr);
bool lsi_drop_memb(irc h, chan c, user u, bool purge, bool complain);
void lsi_clear_memb(irc h, chan c);
memb lsi_alloc_memb(irc h, user u, const char *mpfxstr);
bool lsi_update_modepfx(irc h, chan c, const char *nick, char sym, bool enab);
void lsi_clear_chanmodes(irc h, chan c);
bool lsi_add_chanmode(irc h, chan c, const char *modestr);
bool lsi_drop_chanmode(irc h, chan c, const char *modestr);
user lsi_touch_user(irc h, const char *ident, bool complain);
user lsi_add_user(irc h, const char *ident);
bool lsi_drop_user(irc h, user u);
void lsi_ucb_deinit(irc h);
void lsi_ucb_clear(irc h);
void lsi_ucb_dump(irc h, bool full);
user lsi_get_user(irc h, const char *ident, bool complain);
size_t lsi_num_users(irc h);
bool lsi_rename_user(irc h, const char *ident, const char *newnick,
    bool *allocerr);

/* these might be dangerous to use, be sure to complete the iteration
 * before any other state might change */
chan lsi_first_chan(irc h);
chan lsi_next_chan(irc h);
user lsi_first_user(irc h);
user lsi_next_user(irc h);
memb lsi_first_memb(irc h, chan c);
memb lsi_next_memb(irc h, chan c);
void lsi_tag_chan(chan c, void *tag, bool autofree);
void lsi_tag_user(user u, void *tag, bool autofree);


#endif /* LIBSRSIRC_UCBASE_H */
