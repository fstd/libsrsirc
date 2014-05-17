/* proxy.h - Proxy subroutines (socks4/socks5/http)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef PROXY_H
#define PROXY_H

#include <stdbool.h>

bool proxy_logon_http(int sck, const char *host, unsigned short port,
    unsigned long to_us);

bool proxy_logon_socks4(int sck, const char *host, unsigned short port,
    unsigned long to_us);

bool proxy_logon_socks5(int sck, const char *host, unsigned short port,
    unsigned long to_us);

#endif
