/* common.c - lib-internal IRC-unrelated common routines
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_COMMON

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
#include "intlog.h"

#include <libsrsirc/defs.h>

#include "common.h"


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
com_strNcpy(char *dst, const char *src, size_t len)
{
	char *r = strncpy(dst, src, len);
	dst[len-1] = '\0';
	return r;
}

int com_consocket(const char *host, uint16_t port,
    struct sockaddr *sockaddr, size_t *addrlen,
    uint64_t softto, uint64_t hardto)
{
	D("com_consocket() called: host='%s', port=%"PRIu16", sto=%"PRIu64", hto=%"PRIu64")",
	    host, port, softto, hardto);

	struct addrinfo *ai_list = NULL;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_NUMERICSERV;
	char portstr[6];
	snprintf(portstr, sizeof portstr, "%"PRIu16"", port);

	uint64_t hardtsend = hardto ? com_timestamp_us() + hardto : 0;

	D("calling getaddrinfo on '%s:%s' (AF_UNSPEC, SOCK_STREAM)",
	    host, portstr);

	int r = getaddrinfo(host, portstr, &hints, &ai_list);

	if (r != 0) {
		W("getaddrinfo() failed: %s", gai_strerror(r));
		return -1;
	}

	if (!ai_list) {
		W("getaddrinfo result address list empty");
		return -1;
	}

	size_t count = 0;
	for (struct addrinfo *ai = ai_list; ai; ai = ai->ai_next)
		count++;

	if (softto && hardto && softto * count < hardto)
		softto = hardto / count;

	int sck = -1;
	D("iterating over result list...");
	for (struct addrinfo *ai = ai_list; ai; ai = ai->ai_next) {
		uint64_t softtsend = softto ? com_timestamp_us() + softto : 0;

		I("next result, creating socket (fam=%d, styp=%d, prot=%d)",
		    ai->ai_family, ai->ai_socktype, ai->ai_protocol);

		sck = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sck < 0) {
			WE("cannot create socket");
			continue;
		}

		char peeraddr[64] = "(non-INET/INET6)";
		uint16_t peerport = 0;

		if (ai->ai_family == AF_INET) {
			struct sockaddr_in *sin =
			    (struct sockaddr_in*)ai->ai_addr;

			inet_ntop(AF_INET, &sin->sin_addr,
			    peeraddr, sizeof peeraddr);

			peerport = ntohs(sin->sin_port);
		} else if (ai->ai_family == AF_INET6) {
			struct sockaddr_in6 *sin =
			    (struct sockaddr_in6*)ai->ai_addr;

			inet_ntop(AF_INET6, &sin->sin6_addr,
			    peeraddr, sizeof peeraddr);

			peerport = ntohs(sin->sin6_port);
		}

		char portstr[7];
		snprintf(portstr, sizeof portstr, ":%"PRIu16"", peerport);
		com_strNcat(peeraddr, portstr, sizeof peeraddr);

		int opt = 1;
		socklen_t optlen = sizeof opt;

		D("peer addr is '%s'. going non-blocking", peeraddr);
		if (fcntl(sck, F_SETFL, O_NONBLOCK) == -1) {
			WE("failed to enable nonblocking mode");
			close(sck);
			sck = -1;
			continue;
		}

		D("set to nonblocking mode, calling connect() now");
		errno = 0;
		int r = connect(sck, ai->ai_addr, ai->ai_addrlen);

		if (r == -1 && (errno != EINPROGRESS)) {
			WE("connect() failed");
			close(sck);
			sck = -1;
			continue;
		}

		struct timeval tout;
		tout.tv_sec = 0;
		tout.tv_usec = 0;

		bool success = false;

		for (;;) {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(sck, &fds);
			uint64_t strem = 0;
			uint64_t htrem = 0;

			if (com_check_timeout(hardtsend, &htrem)
			    || com_check_timeout(softtsend, &strem)) {
				W("timeout reached while in 3WHS");
				break;
			}

			if (hardtsend || softtsend)
				com_tconv(&tout,
				    htrem < strem ? &htrem : &strem, false);

			errno = 0;
			r = select(sck+1, NULL, &fds, NULL,
			    hardtsend || softtsend ? &tout : NULL);
			if (r < 0) {
				WE("select() failed");
				break;
			}
			if (r == 1) {
				success = true;
				D("selected!");
				break;
			}
		}

		if (!success) {
			close(sck);
			sck = -1;
			continue;
		}

		if (getsockopt(sck, SOL_SOCKET, SO_ERROR, &opt, &optlen) != 0) {
			W("getsockopt failed");
			close(sck);
			sck = -1;
			continue;
		}

		if (opt == 0) {
			I("socket connected to '%s'!", peeraddr);
			if (sockaddr)
				*sockaddr = *(ai->ai_addr);
			if (addrlen)
				*addrlen = ai->ai_addrlen;

			break;
		} else {
			char berr[128];
			strerror_r(opt, berr, sizeof berr);
			W("could not connect socket (%d: %s)", opt, berr);
			close(sck);
			sck = -1;
			continue;
		}
	}

	freeaddrinfo(ai_list);

	return sck;
}

bool
com_update_strprop(char **field, const char *val)
{
	char *n = NULL;
	if (val) {
		if (!(n = strdup(val))) {
			EE("strdup");
			return false;
		}
	}

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
