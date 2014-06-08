/* irc_track.h - (optionally) track users and channels
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_TRACK_H
#define LIBSRSIRC_IRC_TRACK_H 1

#include <libsrsirc/defs.h>

/* enable tracking for the given irc object.  call between irc_init() and
 * irc_connect(), don't call multiple times for the same `hnd' */
bool trk_init(irc hnd);
void trk_deinit(irc hnd);

/* for debugging */
void trk_dump(irc hnd, bool full);

#endif /* LIBSRSIRC_IRC_TRACK_H */
