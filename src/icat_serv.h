/* icat_serv.h - Server I/O (i.e. IRC) handling
 * icat - IRC netcat on top of libsrsirc - (C) 2012-2024, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_ICAT_SERV_H
#define LIBSRSIRC_ICAT_SERV_H 1


#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include <libsrsirc/defs.h>


void icat_serv_init(void);
void icat_serv_destroy(void);

bool icat_serv_operate(void);

int icat_serv_read(tokarr *t, uint64_t to_us);
int icat_serv_printf(const char *fmt, ...);
int icat_serv_fd(void);
bool icat_serv_ssl(void);

bool icat_serv_online(void);
int icat_serv_casemap(void);
uint64_t icat_serv_sentquit(void);
uint64_t icat_serv_attention_at(void);

void icat_serv_dump(void);

#endif /* LIBSRSIRC_ICAT_SERV_H */
