/* imsg.h - protocl message handler prototypes
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef MSGHANDLE_H
#define MSGHANDLE_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/irc_defs.h>

uint8_t handle(irc hnd, char *(*msg)[MAX_IRCARGS]);

#endif /* MSGHANDLE_H */

