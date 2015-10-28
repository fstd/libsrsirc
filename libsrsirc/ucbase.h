/* ucbase.h - user and channel base, interface (lib-internal)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_UCBASE_H
#define LIBSRSIRC_UCBASE_H 1


#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include "intdefs.h"


typedef struct chan chan;
typedef struct member memb;
typedef struct user user;

struct chan {
	char name[MAX_CHAN_LEN];
	char *topic;
	char *topicnick;
	uint64_t tscreate;
	uint64_t tstopic;
	skmap *memb; //map lnick to struct member
	bool desync;
	char **modes; //one modechar per elem, i.e. "s" or "l 123"
	size_t modes_sz;
	void *tag;
	bool freetag;
};

struct member {
	user *u;
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

bool   lsi_ucb_init(irc *ctx);
void   lsi_ucb_deinit(irc *ctx);
void   lsi_ucb_clear(irc *ctx);
void   lsi_ucb_dump(irc *ctx, bool full);

user  *lsi_ucb_add_user(irc *ctx, const char *ident);
bool   lsi_ucb_drop_user(irc *ctx, user *u);
size_t lsi_ucb_num_users(irc *ctx);
user  *lsi_ucb_get_user(irc *ctx, const char *ident, bool complain);
user  *lsi_ucb_touch_user(irc *ctx, const char *ident, bool complain);
bool   lsi_ucb_rename_user(irc *ctx, const char *ident, const char *newnick,
                           bool *allocerr);

chan  *lsi_ucb_add_chan(irc *ctx, const char *name);
bool   lsi_ucb_drop_chan(irc *ctx, chan *c);
size_t lsi_ucb_num_chans(irc *ctx);
chan  *lsi_ucb_get_chan(irc *ctx, const char *name, bool complain);

void   lsi_ucb_clear_chanmodes(irc *ctx, chan *c);
bool   lsi_ucb_add_chanmode(irc *ctx, chan *c, const char *modestr);
bool   lsi_ucb_drop_chanmode(irc *ctx, chan *c, const char *modestr);

size_t lsi_ucb_num_memb(irc *ctx, chan *c);
memb  *lsi_ucb_get_memb(irc *ctx, chan *c, const char *nick, bool complain);
bool   lsi_ucb_add_memb(irc *ctx, chan *c, user *u, const char *mpfxstr);
bool   lsi_ucb_drop_memb(irc *ctx, chan *c, user *u, bool purge, bool complain);
void   lsi_ucb_clear_memb(irc *ctx, chan *c);
memb  *lsi_ucb_alloc_memb(irc *ctx, user *u, const char *mpfxstr);
bool   lsi_ucb_update_modepfx(irc *ctx, chan *c, const char *nick, char sym,
                              bool enab);

/* these might be dangerous to use, be sure to complete the iteration
 * before any other state might change */
chan *lsi_ucb_first_chan(irc *ctx);
chan *lsi_ucb_next_chan(irc *ctx);
user *lsi_ucb_first_user(irc *ctx);
user *lsi_ucb_next_user(irc *ctx);
memb *lsi_ucb_first_memb(irc *ctx, chan *c);
memb *lsi_ucb_next_memb(irc *ctx, chan *c);
void  lsi_ucb_tag_chan(chan *c, void *tag, bool autofree);
void  lsi_ucb_tag_user(user *u, void *tag, bool autofree);


#endif /* LIBSRSIRC_UCBASE_H */
