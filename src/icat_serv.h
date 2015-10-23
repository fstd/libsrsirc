/* icat_serv.h - Server I/O (i.e. IRC) handling
 * icat - IRC netcat on top of libsrsirc - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_ICAT_SERV_H
#define LIBSRSIRC_ICAT_SERV_H 1


#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include <libsrsirc/defs.h>


void lsi_serv_init(void);
void lsi_serv_destroy(void);

bool lsi_serv_operate(void);

bool lsi_serv_canread(void);
int lsi_serv_read(tokarr *t);
int lsi_serv_printf(const char *fmt, ...);
int lsi_serv_fd(void);

bool lsi_serv_online(void);
int lsi_serv_casemap(void);
uint64_t lsi_serv_sentquit(void);


#endif /* LIBSRSIRC_ICAT_SERV_H */
