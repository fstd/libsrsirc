/* base_io.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_BASEIO

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "base_io.h"

#include <limits.h>
#include <stdio.h>
#include <time.h>

#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <logger/intlog.h>


long
lsi_b_stdin_read(void *buf, size_t nbytes)
{
#if HAVE_FILENO
	int fd = fileno(stdin);
#else
	W("assuming stdin to be fd 0...");
	int fd = 0;
#endif

#if HAVE_READ
	D("reading from stdin");
	ssize_t r = read(fd, buf, nbytes);
	if (r > LONG_MAX) {
		W("read too long, capping return value");
		r = LONG_MAX;
	} else if (r < 0) {
		EE("read stdin");
	}

	return (long)r;
#else
	E("We need something like read()");
	return -1;
#endif
}


int
lsi_b_stdin_canread(void)
{
#if HAVE_FILENO
	int fd = fileno(stdin);
#else
	W("assuming stdin to be fd 0...");
	int fd = 0;
#endif

	int r = -1;

#if HAVE_SELECT
	struct timeval tout = {0, 0};

	V("select()ing stdin");

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	r = select(fd+1, &fds, NULL, NULL, &tout);

	if (r < 0) {
		int e = errno;
		EE("select");
		return e == EINTR ? 0 : -1;
	}

	V("select: %d", r);
#else
	E("We need something like select() or poll()");
#endif

	return r;

}
