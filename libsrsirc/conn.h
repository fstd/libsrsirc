/* conn.h - handles the raw TCP (or proxy) connection
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_ICONN_H
#define LIBSRSIRC_ICONN_H 1


#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include "intdefs.h"


iconn *lsi_conn_init(void);
void lsi_conn_reset(iconn *hnd);
void lsi_conn_dispose(iconn *hnd);
bool lsi_conn_connect(iconn *hnd, uint64_t softto_us, uint64_t hardto_us);
int lsi_conn_read(iconn *hnd, tokarr *tok, uint64_t to_us);
bool lsi_conn_write(iconn *hnd, const char *line);
bool lsi_conn_online(iconn *hnd);
bool lsi_conn_eof(iconn *hnd);

const char *lsi_conn_get_host(iconn *hnd);
uint16_t lsi_conn_get_port(iconn *hnd);
const char *lsi_conn_get_px_host(iconn *hnd);
uint16_t lsi_conn_get_px_port(iconn *hnd);
int lsi_conn_get_px_type(iconn *hnd);
bool lsi_conn_set_server(iconn *hnd, const char *host, uint16_t port);
bool lsi_conn_set_px(iconn *hnd, const char *host, uint16_t port, int ptype);
bool lsi_conn_set_ssl(iconn *hnd, bool on);
bool lsi_conn_get_ssl(iconn *hnd);

/* TODO: replace these by something less insane */
bool lsi_conn_colon_trail(iconn *hnd);
int lsi_conn_sockfd(iconn *hnd);


#endif /* LIBSRSIRC_ICONN_H */
