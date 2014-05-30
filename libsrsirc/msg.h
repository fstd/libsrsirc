/* msg.h - protocol message handlers
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IMSG_H
#define LIBSRSIRC_IMSG_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>

#define CANT_PROCEED (1<<0)
# define OUT_OF_NICKS (1<<1)
# define AUTH_ERR (1<<2)
# define PROTO_ERR (1<<3)
# define IO_ERR (1<<4)
#define LOGON_COMPLETE (1<<5)

typedef uint8_t (*hnd_fn)(irc hnd, tokarr *msg, size_t nargs, bool logon, void *tag);

bool msg_reghnd(const char *cmd, hnd_fn hndfn, const char *dbginfo, void *tag);


/* returns the bitwise OR of one or more of the above
 * bitmasks, or 0 for nothing special */
uint8_t msg_handle(irc hndfn, tokarr *msg, bool logon);

#endif /* LIBSRSIRC_IMSG_H */
