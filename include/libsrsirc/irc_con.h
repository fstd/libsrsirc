/* irc_con.h - Semi back-end interface. Can also be front-end w/ ltd. use
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef SRS_IRC_CON_H
#define SRS_IRC_CON_H 1

#define _GNU_SOURCE 1

#include <stdbool.h>
#include <stddef.h>

typedef struct ichnd* ichnd_t;

ichnd_t irccon_init(void);
bool irccon_reset(ichnd_t hnd);
bool irccon_dispose(ichnd_t hnd);
bool irccon_connect(ichnd_t hnd,
    unsigned long softto_us, unsigned long hardto_us);
int irccon_read(ichnd_t hnd, char **tok, size_t tok_len, unsigned long to_us);
bool irccon_write(ichnd_t hnd, const char *line);
bool irccon_online(ichnd_t hnd);
bool irccon_valid(ichnd_t hnd);
bool irccon_colon_trail(ichnd_t hnd);
const char *irccon_get_host(ichnd_t hnd);
unsigned short irccon_get_port(ichnd_t hnd);
const char *irccon_get_proxy_host(ichnd_t hnd);
unsigned short irccon_get_proxy_port(ichnd_t hnd);
int irccon_get_proxy_type(ichnd_t hnd);
bool irccon_set_server(ichnd_t hnd, const char *host, unsigned short port);
bool irccon_set_proxy(ichnd_t hnd, const char *host, unsigned short port, int ptype);
#ifdef WITH_SSL
bool irccon_set_ssl(ichnd_t hnd, bool on);
bool irccon_get_ssl(ichnd_t hnd);
#endif
int irccon_sockfd(ichnd_t hnd);

#endif /* SRS_IRC_CON_H */
