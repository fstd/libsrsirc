/* base_net.h -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_BASE_NET_H
#define LIBSRSIRC_BASE_NET_H 1


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef WITH_SSL
# include <openssl/err.h>
# include <openssl/ssl.h>
#endif


struct addrlist {
	char addrstr[64];
	char reqname[256];
	uint16_t port;
	bool ipv6;

	struct addrlist *next;
};


#ifdef WITH_SSL
typedef SSL *SSLTYPE;
typedef SSL_CTX *SSLCTXTYPE;
#else
typedef void *SSLTYPE;
typedef void *SSLCTXTYPE;
#endif


int b_socket(bool ipv6);
int b_connect(int sck, const struct addrlist *srv);
int b_close(int sck);
int b_select(int sck, bool rdbl, uint64_t to_us);
bool b_blocking(int sck, bool blocking);
bool b_sock_ok(int sck);

long b_read(int sck, void *buf, size_t sz, bool *tryagain);
long b_write(int sck, const void *buf, size_t len);

long b_read_ssl(SSLTYPE ssl, void *buf, size_t sz, bool *tryagain);
long b_write_ssl(SSLTYPE ssl, const void *buf, size_t len);

int b_mkaddrlist(const char *host, uint16_t port, struct addrlist **res);
void b_freeaddrlist(struct addrlist *al);

SSLCTXTYPE b_mksslctx(void);
void b_freesslctx(SSLCTXTYPE ctx);

SSLTYPE b_sslize(int sck, SSLCTXTYPE ctx);
void b_sslfin(SSLTYPE shnd);

uint16_t b_htons(uint16_t h);
uint32_t b_inet_addr(const char *ip4str);
bool b_inet4_addr(unsigned char *dest, size_t destsz, const char *ip4str);
bool b_inet6_addr(unsigned char *dest, size_t destsz, const char *ip6str);

#endif /* LIBSRSIRC_BASE_NET_H */
