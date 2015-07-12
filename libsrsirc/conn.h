/* conn.h - handles the raw TCP (or proxy) connection
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_ICONN_H
#define LIBSRSIRC_ICONN_H 1


#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include "intdefs.h"


iconn conn_init(void);
void conn_reset(iconn hnd);
void conn_dispose(iconn hnd);
bool conn_connect(iconn hnd, uint64_t softto_us, uint64_t hardto_us);
int conn_read(iconn hnd, tokarr *tok, uint64_t to_us);
bool conn_write(iconn hnd, const char *line);
bool conn_online(iconn hnd);
bool conn_eof(iconn hnd);

const char *conn_get_host(iconn hnd);
uint16_t conn_get_port(iconn hnd);
const char *conn_get_px_host(iconn hnd);
uint16_t conn_get_px_port(iconn hnd);
int conn_get_px_type(iconn hnd);
bool conn_set_server(iconn hnd, const char *host, uint16_t port);
bool conn_set_px(iconn hnd, const char *host, uint16_t port, int ptype);
bool conn_set_ssl(iconn hnd, bool on);
bool conn_get_ssl(iconn hnd);

/* TODO: replace these by something less insane */
bool conn_colon_trail(iconn hnd);
int conn_sockfd(iconn hnd);


#endif /* LIBSRSIRC_ICONN_H */
