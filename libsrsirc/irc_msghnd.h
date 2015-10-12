/* irc_msghnd.h - handles the raw TCP (or proxy) connection
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_MSGHND_H
#define LIBSRSIRC_IRC_MSGHND_H 1


#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include "intdefs.h"


bool lsi_imh_regall(irc *hnd, bool dumb);
void lsi_imh_unregall(irc *hnd);


#endif /* LIBSRSIRC_IRC_MSGHND_H */
