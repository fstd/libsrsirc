/* irc_basic.h - Front-end (main) interface.
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef SRS_IRC_BASIC_H
#define SRS_IRC_BASIC_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/ircdefs.h>

ibhnd_t ircbas_init(void);
bool ircbas_reset(ibhnd_t hnd);
bool ircbas_dispose(ibhnd_t hnd);
bool ircbas_connect(ibhnd_t hnd);
bool ircbas_online(ibhnd_t hnd);
int ircbas_read(ibhnd_t hnd, char *(*tok)[MAX_IRCARGS], 
    unsigned long to_us);
bool ircbas_write(ibhnd_t hnd, const char *line);

const char *ircbas_mynick(ibhnd_t hnd);

bool ircbas_set_server(ibhnd_t hnd, const char *host, unsigned short port);
bool ircbas_set_pass(ibhnd_t hnd, const char *srvpass);
bool ircbas_set_uname(ibhnd_t hnd, const char *uname);
bool ircbas_set_fname(ibhnd_t hnd, const char *fname);
bool ircbas_set_nick(ibhnd_t hnd, const char *nick);
#endif /* SRS_IRC_BASIC_H */
