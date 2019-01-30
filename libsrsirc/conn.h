/* conn.h - handles the raw TCP (or proxy) connection
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-18, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_CONN_H
#define LIBSRSIRC_CONN_H 1


#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include "intdefs.h"


iconn *lsi_conn_init(void);
void lsi_conn_reset(iconn *ctx);
void lsi_conn_dispose(iconn *ctx);
bool lsi_conn_connect(iconn *ctx, uint64_t softto_us, uint64_t hardto_us);
int lsi_conn_read(iconn *ctx, tokarr *tok, char **tags, size_t *ntags,
    uint64_t to_us); // XXX
bool lsi_conn_write(iconn *ctx, const char *line);
bool lsi_conn_online(iconn *ctx);
bool lsi_conn_eof(iconn *ctx);

const char *lsi_conn_get_host(iconn *ctx);
uint16_t lsi_conn_get_port(iconn *ctx);
const char *lsi_conn_get_px_host(iconn *ctx);
uint16_t lsi_conn_get_px_port(iconn *ctx);
int lsi_conn_get_px_type(iconn *ctx);
bool lsi_conn_set_server(iconn *ctx, const char *host, uint16_t port);
bool lsi_conn_set_px(iconn *ctx, const char *host, uint16_t port, int ptype);
bool lsi_conn_set_localaddr(iconn *ctx, const char *addr, uint16_t port);
bool lsi_conn_set_ssl(iconn *ctx, bool on);
bool lsi_conn_get_ssl(iconn *ctx);

/* TODO: replace these by something less insane */
bool lsi_conn_colon_trail(iconn *ctx);
int lsi_conn_sockfd(iconn *ctx);

void irc_conn_dump(iconn *ctx);

#endif /* LIBSRSIRC_CONN_H */
