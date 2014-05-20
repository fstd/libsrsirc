/* common.c - routines commonly used throughout the lib
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

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
#include <arpa/inet.h>

#include <intlog.h>

#include <common.h>


void
ic_strNcat(char *dest, const char *src, size_t destsz)
{
	size_t len = strlen(dest);
	if (len + 1 >= destsz)
		return;
	size_t rem = destsz - (len + 1);

	char *ptr = dest + len;
	while(rem-- && *src) {
		*ptr++ = *src++;
	}
	*ptr = '\0';
}

size_t
ic_strCchr(const char *dst, char c)
{
	size_t r = 0;
	while(*dst)
		if (*dst++ == c)
			r++;
	return r;
}

/*
void*
ic_xmalloc(size_t num)
{
	void *new = malloc(num);
	if (!new)
		CE("malloc failed");
	return new;
}

void*
ic_xrealloc(void *p, size_t num)
{
	void *new = realloc(p, num);
	if (!new)
		CE("realloc failed");

	return new;
}

void*
ic_xcalloc(size_t num, size_t size)
{
	void *new = ic_xmalloc(num * size);
	memset(new, 0, num * size);
	return new;
}

char*
ic_xstrdup(const char *str)
{
	if (str)
		return strcpy(ic_xmalloc(strlen(str) + 1), str);
	return NULL;
}
*/

int64_t
ic_timestamp_us()
{
	struct timeval t;
	int64_t ts;
	gettimeofday(&t, NULL);
	ic_tconv(&t, &ts, true);
	return ts;
}

void
ic_tconv(struct timeval *tv, int64_t *ts, bool tv_to_ts)
{
	if (tv_to_ts)
		*ts = (int64_t)tv->tv_sec * 1000000 + tv->tv_usec;
	else {
		tv->tv_sec = *ts / 1000000;
		tv->tv_usec = *ts % 1000000;
	}
}

char*
ic_strNcpy(char *dst, const char *src, size_t len)
{
	char *r = strncpy(dst, src, len);
	dst[len-1] = '\0';
	return r;
}

int ic_consocket(const char *host, unsigned short port,
    struct sockaddr *sockaddr, size_t *addrlen,
    unsigned long softto, unsigned long hardto)
{
	D("ic_consocket() called: host='%s', port=%hu, sto=%lu, hto=%lu)",
	    host, port, softto, hardto);

	struct addrinfo *ai_list = NULL;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_NUMERICSERV;
	char portstr[6];
	snprintf(portstr, sizeof portstr, "%hu", port);

	int64_t hardtsend = hardto ? ic_timestamp_us() + hardto : 0;

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

	int sck = -1;

	D("iterating over result list...");
	for (struct addrinfo *ai = ai_list; ai; ai = ai->ai_next) {
		int64_t softtsend = softto ? ic_timestamp_us() + softto : 0;

		I("next result, creating socket (fam=%d, styp=%d, prot=%d)",
		    ai->ai_family, ai->ai_socktype, ai->ai_protocol);

		sck = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sck < 0) {
			WE("cannot create socket");
			continue;
		}

		char peeraddr[64] = "(non-INET/INET6)";
		unsigned short peerport = 0;

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
		snprintf(portstr, sizeof portstr, ":%hu", peerport);
		ic_strNcat(peeraddr, portstr, sizeof peeraddr);

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
		int64_t trem = 0;

		bool success = false;

		for(;;) {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(sck, &fds);

			if (hardtsend || softtsend) {
				trem = hardtsend < softtsend
				    ? hardtsend : softtsend - ic_timestamp_us();

				if (trem <= 0) {
					W("timeout reached while in 3WHS");
					break;
				}

				ic_tconv(&tout, &trem, false);
			}

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

/*
char*
ic_strmdup(const char *str, size_t minlen)
{
	size_t len = strlen(str);
	char *s = malloc((len > minlen ? len : minlen) + 1);
	if (s)
		strcpy(s, str);
	return s;
}
*/

char**
ic_clonearr(char **arr, size_t nelem)
{
	char **res = malloc((nelem+1) * sizeof *arr);
	if (!res) {
		EE("malloc");
		return NULL;
	}

	for(size_t i = 0; i < nelem; i++) {
		if (arr[i]) {
			if (!(res[i] = strdup(arr[i]))) {
				EE("strdup");
				goto clonearr_fail;
			}
		} else
			res[i] = NULL;
	}
	res[nelem] = NULL;
	return res;

clonearr_fail:

	ic_freearr(res, nelem);
	return NULL;
}


void
ic_freearr(char **arr, size_t nelem)
{
	if (arr) {
		for(size_t i = 0; i < nelem; i++)
			free(arr[i]);
		free(arr);
	}
}
