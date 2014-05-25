/* common.h - lib-internal IRC-unrelated common routines
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_COMMON_H
#define LIBSRSIRC_COMMON_H 1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <sys/time.h>

#define COUNTOF(ARR) (sizeof (ARR) / sizeof (ARR)[0])

void ic_strNcat(char *dest, const char *src, size_t destsz);
char* ic_strNcpy(char *dst, const char *src, size_t len);
size_t ic_strCchr(const char *dst, char c);

uint64_t ic_timestamp_us(void);
bool ic_check_timeout(uint64_t tsend, uint64_t *trem);
void ic_tconv(struct timeval *tv, uint64_t *ts, bool tv_to_ts);

int ic_consocket(const char *host, uint16_t port,
    struct sockaddr *sockaddr, size_t *addrlen,
    uint64_t softto, uint64_t hardto);
bool update_strprop(char **field, const char *val);

#endif /* LIBSRSIRC_COMMON_H */
