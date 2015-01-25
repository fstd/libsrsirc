/* icat_serv.h - Server I/O (i.e. IRC) handling
 * icat - IRC netcat on top of libsrsirc - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_ICAT_SERV_H
#define LIBSRSIRC_ICAT_SERV_H 1


#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include <libsrsirc/defs.h>


void serv_init(void);
void serv_destroy(void);

bool serv_operate(void);

bool serv_canread(void);
int serv_read(tokarr *t);
int serv_printf(const char *fmt, ...);

bool serv_online(void);
int serv_casemap(void);
uint64_t serv_sentquit(void);


#endif /* LIBSRSIRC_ICAT_SERV_H */
