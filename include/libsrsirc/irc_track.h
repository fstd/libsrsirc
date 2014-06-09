/* irc_track.h - (optionally) track users and channels
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_TRACK_H
#define LIBSRSIRC_IRC_TRACK_H 1

#include <libsrsirc/defs.h>

struct chanrep {
	const char *name;
	const char *topic;
	const char *topicnick;
	uint64_t tscreate;
	uint64_t tstopic;
};

struct userrep {
	const char *modepfx;
	const char *nick;
	const char *uname;
	const char *host;
	const char *fname;
};

typedef struct chanrep chanrep;
typedef struct userrep userrep;

/* enable tracking for the given irc object.  call between irc_init() and
 * irc_connect(), don't call multiple times for the same `hnd' */
bool trk_init(irc hnd);
void trk_deinit(irc hnd);

/* the results of these functions (i.e. the strings pointed to by the various
 * members of the structs above is only valid until the next time irc_read
 * is called.  if you need persistence, copy the data. */

size_t irc_num_chans(irc h);
size_t irc_all_chans(irc h, chanrep *chanarr, size_t chanarr_cnt);
chanrep* irc_chan(irc h, chanrep *dest, const char *name);
bool irc_tag_chan(irc h, const char *chname, void *tag, bool autofree);

size_t irc_num_users(irc h);
size_t irc_all_users(irc h, userrep *userarr, size_t userarr_cnt);
userrep* irc_user(irc h, userrep *dest, const char *ident);
bool irc_tag_user(irc h, const char *ident, void *tag, bool autofree);

size_t irc_num_members(irc h, const char *chname);
size_t irc_all_members(irc h, const char *chname, userrep *userarr, size_t userarr_cnt);
userrep* irc_member(irc h, userrep *dest, const char *chname, const char *ident);

/* for debugging */
void trk_dump(irc hnd, bool full);

#endif /* LIBSRSIRC_IRC_TRACK_H */