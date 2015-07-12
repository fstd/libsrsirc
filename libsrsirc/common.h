/* common.h - lib-internal IRC-unrelated common routines
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_COMMON_H
#define LIBSRSIRC_COMMON_H 1


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define COUNTOF(ARR) (sizeof (ARR) / sizeof (ARR)[0])


enum hosttypes {
	HOSTTYPE_IPV4,
	HOSTTYPE_IPV6,
	HOSTTYPE_DNS
};


void com_strNcat(char *dest, const char *src, size_t destsz);
char* com_strNcpy(char *dst, const char *src, size_t len);
size_t com_strCchr(const char *dst, char c);

bool com_check_timeout(uint64_t tsend, uint64_t *trem);

int com_consocket(const char *host, uint16_t port, char *peeraddr,
    size_t peeraddr_sz, uint16_t *peerport, uint64_t softto, uint64_t hardto);

bool com_update_strprop(char **field, const char *val);

void* com_malloc(size_t sz);
char* com_strdup(const char *s);

enum hosttypes guess_hosttype(const char *host);


#endif /* LIBSRSIRC_COMMON_H */
