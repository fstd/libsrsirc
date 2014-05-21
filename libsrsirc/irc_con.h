/* irc_con.h - handles the raw TCP (or proxy) connection
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef SRS_IRC_CON_H
#define SRS_IRC_CON_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/ircdefs.h>

iconn icon_init(void);
bool icon_reset(iconn hnd);
bool icon_dispose(iconn hnd);
bool icon_connect(iconn hnd,
    unsigned long softto_us, unsigned long hardto_us);
int icon_read(iconn hnd, char *(*tok)[MAX_IRCARGS],
    unsigned long to_us);
bool icon_write(iconn hnd, const char *line);
bool icon_online(iconn hnd);

const char *icon_get_host(iconn hnd);
unsigned short icon_get_port(iconn hnd);
const char *icon_get_proxy_host(iconn hnd);
unsigned short icon_get_proxy_port(iconn hnd);
int icon_get_proxy_type(iconn hnd);
bool icon_set_server(iconn hnd, const char *host, unsigned short port);
bool icon_set_proxy(iconn hnd, const char *host, unsigned short port,
    int ptype);
bool icon_set_ssl(iconn hnd, bool on);
bool icon_get_ssl(iconn hnd);

/* TODO: replace these by something less insane */
bool icon_colon_trail(iconn hnd);
int icon_sockfd(iconn hnd);

#endif /* SRS_IRC_CON_H */
