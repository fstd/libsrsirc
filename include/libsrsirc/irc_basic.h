/* irc_basic.h - Front-end (main) interface.
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef SRS_IRC_BASIC_H
#define SRS_IRC_BASIC_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/ircdefs.h>

irc irc_init(void);
bool irc_reset(irc hnd);
bool irc_dispose(irc hnd);
bool irc_connect(irc hnd);
bool irc_online(irc hnd);
int irc_read(irc hnd, char *(*tok)[MAX_IRCARGS], 
    unsigned long to_us);
bool irc_write(irc hnd, const char *line);

const char *irc_mynick(irc hnd);

bool irc_set_server(irc hnd, const char *host, unsigned short port);
bool irc_set_pass(irc hnd, const char *srvpass);
bool irc_set_uname(irc hnd, const char *uname);
bool irc_set_fname(irc hnd, const char *fname);
bool irc_set_nick(irc hnd, const char *nick);
#endif /* SRS_IRC_BASIC_H */
