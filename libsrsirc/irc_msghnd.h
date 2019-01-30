/* irc_msghnd.h - core message handlers (see irc_msghnd.c for actual handlers)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-19, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_MSGHND_H
#define LIBSRSIRC_IRC_MSGHND_H 1


#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include "intdefs.h"


bool lsi_imh_regall(irc *ctx, bool dumb);
void lsi_imh_unregall(irc *ctx);


#endif /* LIBSRSIRC_IRC_MSGHND_H */
