/* common.c - lib-internal IRC-unrelated common routines
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_COMMON

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "common.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <errno.h>

/* POSIX */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>

/* local */
#include <logger/intlog.h>

#include <libsrsirc/defs.h>


static size_t com_resolve(const char *host, uint16_t port,
    struct addrinfo **result);
static int tryhost(struct addrinfo *ai, char *peeraddr, size_t peeraddr_sz,
    uint16_t *peerport, uint64_t to_us);
static void addr_from_sockaddr(struct addrinfo *ai, char *addr, size_t addr_sz,
    uint16_t *port);
static bool connect_with_timeout(int sck, struct addrinfo *ai, uint64_t to_us);

void
com_strNcat(char *dest, const char *src, size_t destsz)
{
	size_t len = strlen(dest);
	if (len + 1 >= destsz)
		return;
	size_t rem = destsz - (len + 1);

	char *ptr = dest + len;
	while (rem-- && *src) {
		*ptr++ = *src++;
	}
	*ptr = '\0';
}

size_t
com_strCchr(const char *dst, char c)
{
	size_t r = 0;
	while (*dst)
		if (*dst++ == c)
			r++;
	return r;
}

uint64_t
com_timestamp_us(void)
{
	struct timeval t;
	uint64_t ts = 0;
	if (gettimeofday(&t, NULL) != 0)
		EE("gettimeofday");
	else
		com_tconv(&t, &ts, true);

	return ts;
}

void
com_tconv(struct timeval *tv, uint64_t *ts, bool tv_to_ts)
{
	if (tv_to_ts)
		*ts = (uint64_t)tv->tv_sec * 1000000 + tv->tv_usec;
	else {
		tv->tv_sec = *ts / 1000000;
		tv->tv_usec = *ts % 1000000;
	}
}

char*
com_strNcpy(char *dst, const char *src, size_t dst_sz)
{
	dst[dst_sz-1] = '\0';
	while (--dst_sz)
		if (!(*dst++ = *src++))
			break;
	return dst;
}

int
com_consocket(const char *host, uint16_t port, char *peeraddr,
    size_t peeraddr_sz, uint16_t *peerport, uint64_t softto, uint64_t hardto)
{
	uint64_t hardtsend = hardto ? com_timestamp_us() + hardto : 0;

	struct addrinfo *ai_list;
	size_t count = com_resolve(host, port, &ai_list);
	if (!count)
		return -1;

	if (softto && hardto && softto * count < hardto)
		softto = hardto / count;

	int sck = -1;
	for (struct addrinfo *ai = ai_list; ai; ai = ai->ai_next) {
		uint64_t trem = 0;

		if (com_check_timeout(hardtsend, &trem)) {
			W("hard timeout");
			return false;
		}

		if (trem > softto)
			trem = softto;

		sck = tryhost(ai, peeraddr, peeraddr_sz, peerport, trem);

		if (sck != -1)
			break;
	}

	freeaddrinfo(ai_list);

	return sck;
}

static size_t
com_resolve(const char *host, uint16_t port, struct addrinfo **result)
{
	struct addrinfo *ai_list = NULL;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_NUMERICSERV;
	char portstr[6];
	snprintf(portstr, sizeof portstr, "%"PRIu16"", port);

	D("calling getaddrinfo on '%s:%s' (AF_UNSPEC, STREAM)", host, portstr);

	int r = getaddrinfo(host, portstr, &hints, &ai_list);

	if (r != 0) {
		W("getaddrinfo() failed: %s", gai_strerror(r));
		return 0;
	}

	size_t count = 0;
	for (struct addrinfo *ai = ai_list; ai; ai = ai->ai_next)
		count++;

	if (!count) {
		W("getaddrinfo result address list empty");
		return 0;
	}

	*result = ai_list;

	return count;
}


static int
tryhost(struct addrinfo *ai, char *peeraddr, size_t peeraddr_sz,
    uint16_t *peerport, uint64_t to_us)
{
	D("creating socket (fam=%d, styp=%d, prot=%d)",
	    ai->ai_family, ai->ai_socktype, ai->ai_protocol);

	int sck = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (sck < 0) {
		WE("cannot create socket");
		return -1;
	}

	if (connect_with_timeout(sck, ai, to_us)) {
		if (peeraddr && peeraddr_sz)
			addr_from_sockaddr(ai, peeraddr, peeraddr_sz, peerport);

		return sck;
	}

	close(sck);
	return -1;
}

static void
addr_from_sockaddr(struct addrinfo *ai, char *addr, size_t addr_sz,
    uint16_t *port)
{

	if (ai->ai_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in*)ai->ai_addr;

		if (addr && addr_sz)
			inet_ntop(AF_INET, &sin->sin_addr, addr, addr_sz);
		if (port)
			*port = ntohs(sin->sin_port);
	} else if (ai->ai_family == AF_INET6) {
		struct sockaddr_in6 *sin = (struct sockaddr_in6*)ai->ai_addr;


		if (addr && addr_sz)
			inet_ntop(AF_INET6, &sin->sin6_addr, addr, addr_sz);
		if (port)
			*port = ntohs(sin->sin6_port);
	} else {
		if (addr && addr_sz)
			com_strNcpy(addr, "(non-IPv4/IPv6)", addr_sz);
		if (port)
			*port = 0;
	}
}

static bool
connect_with_timeout(int sck, struct addrinfo *ai, uint64_t to_us)
{
	uint64_t tsend = to_us ? com_timestamp_us() + to_us : 0;

	if (fcntl(sck, F_SETFL, O_NONBLOCK) == -1) {
		WE("failed to enable nonblocking mode");
		return false;
	}

	int r = connect(sck, ai->ai_addr, ai->ai_addrlen);
	if (r == -1 && (errno != EINPROGRESS)) {
		WE("connect() failed");
		return false;
	}

	struct timeval tout;

	for (;;) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sck, &fds);
		uint64_t trem = 0;

		if (com_check_timeout(tsend, &trem)) {
			W("timeout reached while in 3WHS");
			return false;
		}

		if (tsend)
			com_tconv(&tout, &trem, false);

		r = select(sck+1, NULL, &fds, NULL, tsend ? &tout : NULL);
		if (r < 0) {
			WE("select() failed");
			return false;
		}
		if (r == 1)
			break;
	}

	int opt = 1;
	socklen_t optlen = sizeof opt;
	if (getsockopt(sck, SOL_SOCKET, SO_ERROR, &opt, &optlen) != 0) {
		W("getsockopt failed");
		return false;
	}

	if (opt == 0) {
		I("socket connected!");
		return true;
	}

	char berr[128];
	strerror_r(opt, berr, sizeof berr);
	W("could not connect socket (%d: %s)", opt, berr);

	return false;
}

bool
com_update_strprop(char **field, const char *val)
{
	char *n = NULL;
	if (val && !(n = com_strdup(val)))
		return false;

	free(*field);
	*field = n;

	return true;
}

bool
com_check_timeout(uint64_t tsend, uint64_t *trem)
{
	if (!tsend) {
		if (trem)
			*trem = 0;
		return false;
	}

	uint64_t now = com_timestamp_us();
	if (now >= tsend) {
		if (trem)
			*trem = 0;
		return true;
	}

	if (trem)
		*trem = tsend - now;

	return false;
}

void*
com_malloc(size_t sz)
{
	void *r = malloc(sz);
	if (!r)
		EE("malloc");
	return r;
}

char*
com_strdup(const char *s)
{
	char *r = strdup(s);
	if (!r)
		EE("strdup");
	return r;
}
