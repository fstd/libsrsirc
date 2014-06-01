/* msg.h - protocol message handlers
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IMSG_H
#define LIBSRSIRC_IMSG_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include "intdefs.h"


/* these bits comprise the return value of protocol message handler functions */
#define CANT_PROCEED   (1<<0) // we encountered something we can't deal with
# define OUT_OF_NICKS  (CANT_PROCEED|(1<<1)) // logon: nick in use and no alt
# define AUTH_ERR      (CANT_PROCEED|(1<<2)) // wrong server password
# define PROTO_ERR     (CANT_PROCEED|(1<<3)) // illegal protocol message
# define IO_ERR        (CANT_PROCEED|(1<<4)) // failure doing i/o (conn_write())
# define ALLOC_ERR     (CANT_PROCEED|(1<<5)) // out of memory
# define SHITCODEZ_ERR (CANT_PROCEED|(1<<6)) // condition which must be a bug
#define LOGON_COMPLETE (1<<7) // we now consider ourselves logged on

bool msg_reghnd(irc hnd, const char *cmd, hnd_fn hndfn, const char *dbginfo);


/* returns the bitwise OR of one or more of the above
 * bitmasks, or 0 for nothing special */
uint8_t msg_handle(irc hndfn, tokarr *msg, bool logon);

#endif /* LIBSRSIRC_IMSG_H */