/* proxy.h - Proxy subroutines (socks4/socks5/http)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_PROXY_H
#define LIBSRSIRC_PROXY_H 1

#include <stdbool.h>

bool proxy_logon_http(int sck, const char *host, uint16_t port,
    uint64_t to_us);

bool proxy_logon_socks4(int sck, const char *host, uint16_t port,
    uint64_t to_us);

bool proxy_logon_socks5(int sck, const char *host, uint16_t port,
    uint64_t to_us);

#endif /* LIBSRSIRC_PROXY_H */
