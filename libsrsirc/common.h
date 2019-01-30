/* common.h - lib-internal IRC-unrelated common routines
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-18, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_COMMON_H
#define LIBSRSIRC_COMMON_H 1


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define COUNTOF(ARR) (sizeof (ARR) / sizeof (ARR)[0])

#define MIN(A, B) ((A) < (B) ? (A) : (B))

enum hosttypes {
	HOSTTYPE_IPV4,
	HOSTTYPE_IPV6,
	HOSTTYPE_DNS
};


size_t lsi_com_strCchr(const char *dst, char c);

bool lsi_com_check_timeout(uint64_t tend, uint64_t *trem);

int lsi_com_consocket(const char *host, uint16_t port, const char *laddr,
    uint16_t lport, char *remaddr, size_t remaddr_sz, uint16_t *peerport,
    uint64_t softto, uint64_t hardto);

bool lsi_com_update_strprop(char **field, const char *val);

enum hosttypes lsi_com_guess_hosttype(const char *host);

size_t lsi_com_base64(char *dest, size_t destsz,
    const uint8_t *input, size_t len);

char * lsi_com_next_tok(char *buf, char delim);

#endif /* LIBSRSIRC_COMMON_H */
