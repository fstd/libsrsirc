/* common.c - routines commonly used throughout the lib
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE 1

#include <common.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
/* POSIX */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

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

void*
ic_xmalloc(size_t num)
{
	void *new = malloc(num);
	if (!new)
		exit(1);
	return new;
}

void*
ic_xrealloc(void *p, size_t num)
{
	void *new;

	if (!p)
		return ic_xmalloc(num);

	new = realloc(p, num);
	if (!new)
		exit(1);

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
	else
	{
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


int ic_mksocket(const char *host, unsigned short port,
		struct sockaddr *sockaddr, size_t *addrlen)
{
	struct addrinfo *ai_list = NULL;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_NUMERICSERV;
	char portstr[6];
	snprintf(portstr, sizeof portstr, "%hu", port);

	int r = getaddrinfo(host, portstr, &hints, &ai_list);

	if (r != 0) {
		//warnx("%s", gai_strerror(r));
		return -1;
	}

	if (!ai_list) {
		//warnx("result address list empty");
		return -1;
	}

	int sck = -1;

	for (struct addrinfo *ai = ai_list; ai; ai = ai->ai_next)
	{
		sck = socket(ai->ai_family, ai->ai_socktype,
				ai->ai_protocol);
		if (sck < 0) {
			//warn("cannot create socket");
			continue;
		}
		if (sockaddr)
			*sockaddr = *(ai->ai_addr);
		if (addrlen)
			*addrlen = ai->ai_addrlen;

		break;
	}

	freeaddrinfo(ai_list);

	return sck;
}

