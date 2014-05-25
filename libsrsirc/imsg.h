/* imsg.h - protocol message handlers
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IMSG_H
#define LIBSRSIRC_IMSG_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/irc_defs.h>

uint8_t handle(irc hnd, tokarr *msg);

#endif /* LIBSRSIRC_IMSG_H */
