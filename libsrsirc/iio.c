/* iio.c - i/o processing, protocol tokenization
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_IIO

/* C */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

/* POSIX */
#include <unistd.h>
#include <sys/socket.h>
#include <inttypes.h>

#ifdef WITH_SSL
/* ssl */
# include <openssl/err.h>
#endif

/* locals */
#include "common.h"
#include <intlog.h>

#include "iio.h"

#define ISDELIM(C) ((C) == '\n' || (C) == '\r')

/* local helpers */
static char* find_delim(struct readctx *ctx);
static int read_more(sckhld sh, struct readctx *ctx, uint64_t to_us);
static bool tokenize(char *buf, tokarr *tok);
static char* next_tok(char *buf);
static int wait_for_readable(int sck, uint64_t to_us);
static bool write_str(sckhld sh, const char *buf);
static ssize_t read_wrap(sckhld sh, void *buf, size_t sz);
static ssize_t send_wrap(sckhld sh, const void *buf, size_t len, int flags);


/* Documented in iio.h */
int
iio_read(sckhld sh, struct readctx *ctx, tokarr *tok, uint64_t to_us)
{
	uint64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;

	while (ctx->wptr < ctx->eptr && ISDELIM(*ctx->wptr))
		ctx->wptr++; /* skip leading line delimiters */

	if (ctx->wptr == ctx->eptr) /* empty buffer, use the opportunity.. */
		ctx->wptr = ctx->eptr = ctx->workbuf;

	char *delim;
	while (!(delim = find_delim(ctx))) {
		uint64_t trem = 0;
		if (ic_check_timeout(tsend, &trem))
			return 0;

		int r = read_more(sh, ctx, trem);
		if (r <= 0)
			return r;
	}

	char *linestart = ctx->wptr;
	size_t linelen = delim - linestart;
	*delim = '\0';
	D("read a line: '%s'", linestart);
	ctx->wptr += linelen + 1;
	return tokenize(linestart, tok) ? 1 : -1;
}

/* Documented in iio.h */
bool
iio_write(sckhld sh, const char *line)
{
	size_t len = strlen(line);
	int needbr = len < 2 || line[len-2] != '\r' || line[len-1] != '\n';

	return write_str(sh, line) && (!needbr || write_str(sh, "\r\n"));
}



/* return pointer to first line delim in our receive buffer, or NULL if none */
static char*
find_delim(struct readctx *ctx)
{
	for (char *ptr = ctx->wptr; ptr < ctx->eptr; ptr++)
		if (ISDELIM(*ptr))
			return ptr;
	return NULL;
}

/* attempt to read more data from the ircd into our read buffer.
 * returns 1 if something was read; 0 on timeout; -1 on failure */
static int
read_more(sckhld sh, struct readctx *ctx, uint64_t to_us)
{
	size_t remain = sizeof ctx->workbuf - (ctx->eptr - ctx->workbuf);
	if (!remain) { /* no more space left in receive buffer */
		if (ctx->wptr == ctx->workbuf) { /* completely full */
			E("input too long");
			return -1;
		}

		/* make additional room by moving data to the beginning */
		size_t datalen = (size_t)(ctx->eptr - ctx->wptr);
		memmove(ctx->workbuf, ctx->wptr, datalen);
		ctx->wptr = ctx->workbuf;
		ctx->eptr = &ctx->workbuf[datalen];

		remain = sizeof ctx->workbuf - (ctx->eptr - ctx->workbuf);
	}

	int r = wait_for_readable(sh.sck, to_us);
	if (r != 1)
		return r;

	ssize_t n = read_wrap(sh, ctx->eptr, remain);
	if (n <= 0) {
		n == 0 ?  W("read: EOF") : WE("read failed");
		return -1;
	}

	ctx->eptr += n;
	return 1;
}

/* in-place tokenize an IRC protocol message pointed to by `buf'
 * the array pointed to by `tok' is populated with pointers to the identified
 * tokens; if there are less tokens than elements in the array, the remaining
 * elements are set to NULL.  returns true on success, false on failure */
static bool
tokenize(char *buf, tokarr *tok)
{
	for (size_t i = 0; i < COUNTOF(*tok); ++i)
		(*tok)[i] = NULL;

	if (*buf == ':') { /* message has a prefix */
		(*tok)[0] = buf + 1; /* disregard the colon */
		if (!(buf = next_tok(buf))) {
			E("protocol error (no more tokens after prefix)");
			return false;
		}
	} else if (*buf == ' ') { /* this would lead to parsing issues */
		E("protocol error (leading whitespace)");
		return false;
	} else if (!*buf) {
		E("protocol error (empty line)");
		return false;
	}

	(*tok)[1] = buf; /* command */

	size_t argc = 2;
	while (argc < COUNTOF(*tok) && (buf = next_tok(buf))) {
		if (*buf == ':') { /* `trailing' arg */
			(*tok)[argc++] = buf + 1; /* disregard the colon */
			break;
		}

		(*tok)[argc++] = buf;
	}

	return true;
}

/* \0-terminate the (to-be)-token `buf' points to, then locate the next token,
 * if any, and return pointer to it (or NULL) */
static char*
next_tok(char *buf)
{
	while (*buf && *buf != ' ') /* walk until end of (former) token */
		buf++;

	if (!*buf)
		return NULL; /* there's no next token */

	while (*buf == ' ') /* walk over token delimiter, zero it out */
		*buf++ = '\0';

	if (!*buf)
		return NULL; /* trailing whitespace, but no next token */

	return buf; /* return pointer to beginning of the next token */
}

/* wait up to `to_us' microseconds for `sck' to become readable
 * returns 1 if the socket is readable; 0 on timeout; -1 on failure */
static int
wait_for_readable(int sck, uint64_t to_us)
{
	uint64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;

	for (;;) {
		struct timeval tout;
		uint64_t trem = 0;
		if (ic_check_timeout(tsend, &trem))
			return 0;

		if (tsend)
			ic_tconv(&tout, &trem, false);

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sck, &fds);
		int r = select(sck+1, &fds, NULL, NULL, tsend ? &tout : NULL);

		if (r < 0) {
			int e = errno;
			WE("select");
			return e == EINTR ? 0 : -1;
		} else if (r == 1 && FD_ISSET(sck, &fds)) {
			return 1;
		}
	}
}

/* write a string to a socket, ensure that everything is sent, well, buffered.
 * returns true on success, false on failure */
static bool
write_str(sckhld sh, const char *buf)
{
	size_t cnt = 0;
	size_t len = strlen(buf);
	while (cnt < len) {
		ssize_t n = send_wrap(sh, buf + cnt, len - cnt, MSG_NOSIGNAL);
		if (n < 0)
			return false;
		cnt += n;
	}
	return true;
}

/* wrap around either read() or SSL_read(), depending on whether
 * or not SSL is compiled-in and enabled (or not) */
static ssize_t
read_wrap(sckhld sh, void *buf, size_t sz)
{
#ifdef WITH_SSL
	if (sh.shnd)
		return (ssize_t)SSL_read(sh.shnd, buf, sz);
#endif
	return read(sh.sck, buf, sz);
}

/* likewise for send()/SSL_write() */
static ssize_t
send_wrap(sckhld sh, const void *buf, size_t len, int flags)
{
#ifdef WITH_SSL
	if (sh.shnd)
		return (ssize_t)SSL_write(sh.shnd, buf, len);
#endif
	return send(sh.sck, buf, len, flags);
}
