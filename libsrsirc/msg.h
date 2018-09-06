/* msg.h - protocol message handler mechanism and dispatch
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-18, Timo Buhrmester
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
# define IO_ERR        (CANT_PROCEED|(1<<4)) // i/o failure (lsi_conn_write())
# define ALLOC_ERR     (CANT_PROCEED|(1<<5)) // out of memory
# define USER_ERR      (CANT_PROCEED|(1<<6)) // user msg handler failed
# define SASL_ERR      (CANT_PROCEED|(1<<7)) // SASL authentication failed
# define CAP_ERR       (CANT_PROCEED|(1<<8)) // >= 1 'musthave'-CAP unavailable
#define LOGON_COMPLETE (1<<9) // we now consider ourselves logged on
#define SASL_COMPLETE  (1<<10) // SASL authentication succeeded
#define MORE_CAPS      (1<<11) // multiline reply to CAP LS
#define STARTTLS_OVER  (1<<12) // early starttls finished (or failed)

bool lsi_msg_reghnd(irc *ctx, const char *cmd, hnd_fn hndfn, const char *module);
void lsi_msg_unregall(irc *ctx, const char *module);

bool lsi_msg_reguhnd(irc *ctx, const char *cmd, uhnd_fn hndfn, bool pre);


/* returns the bitwise OR of one or more of the above
 * bitmasks, or 0 for nothing special */
uint16_t lsi_msg_handle(irc *ctx, tokarr *msg, bool logon);


#endif /* LIBSRSIRC_IMSG_H */
