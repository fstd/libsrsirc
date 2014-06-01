/* irc_track.h - (optionally) track users and channels
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_TRACK_H
#define LIBSRSIRC_IRC_TRACK_H 1

#include <libsrsirc/defs.h>

bool trk_init(irc hnd); //we always track chans, (if we're enabled)

void trk_dump(irc hnd);

#endif /* LIBSRSIRC_IRC_TRACK_H */
