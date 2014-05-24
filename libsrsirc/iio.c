/* iio.c - Back-end, i/o processing
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
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
static int wait_for_readable(int sck, uint64_t to_us);
static bool writeall(sckhld sh, const char *buf);
static ssize_t readwrap(sckhld sh, void *buf, size_t sz);
static ssize_t sendwrap(sckhld sh, const void *buf, size_t len, int flags);
static int tokenize(char *buf, tokarr *tok);
static char* skip2lws(char *s, bool tab_is_ws);

/* pub if implementation */
int
ircio_read(sckhld sh, struct readctx *ctx, tokarr *tok, uint64_t to_us)
{
	uint64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;

	while (ctx->wptr < ctx->eptr && ISDELIM(*ctx->wptr)) /* skip leading delims */
		ctx->wptr++;

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
	ctx->wptr += linelen + 1;
	return tokenize(linestart, tok);
}

bool
ircio_write(sckhld sh, const char *line)
{
	size_t len = strlen(line);
	int needbr = len < 2 || line[len-2] != '\r' || line[len-1] != '\n';

	return writeall(sh, line) && (!needbr || writeall(sh, "\r\n"));
}

/* local helpers implementation */
static char*
find_delim(struct readctx *ctx)
{
	for (char *ptr = ctx->wptr; ptr < ctx->eptr; ptr++)
		if (ISDELIM(*ptr))
			return ptr;
	return NULL;
}

static int
read_more(sckhld sh, struct readctx *ctx, uint64_t to_us)
{
	size_t remain = sizeof ctx->workbuf - (ctx->eptr - ctx->workbuf);
	if (!remain) {
		if (ctx->wptr == ctx->workbuf) {
			E("input too long");
			return -1;
		}
		
		W("moving");
		size_t datalen = (size_t)(ctx->eptr - ctx->wptr);
		memmove(ctx->workbuf, ctx->wptr, datalen);
		ctx->wptr = ctx->workbuf;
		ctx->eptr = &ctx->workbuf[datalen];
		remain = sizeof ctx->workbuf - (ctx->eptr - ctx->workbuf);
	}

	int r = wait_for_readable(sh.sck, to_us);
	if (r != 1)
		return r;

	ssize_t n = readwrap(sh, ctx->eptr, remain);
	if (n <= 0) {
		n == 0 ?  W("read: EOF") : WE("read failed");
		return -1;
	}

	ctx->eptr += n;
	return 1;
}

static int
wait_for_readable(int sck, uint64_t to_us)
{
	uint64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;

	for(;;) {
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
			return e == EINTR ? 0 : 1;
		} else if (r == 1 && FD_ISSET(sck, &fds)) {
			return 1;
		}
	}
}

static bool
writeall(sckhld sh, const char *buf)
{
	size_t cnt = 0;
	size_t len = strlen(buf);
	while(cnt < len) {
		ssize_t n = sendwrap(sh, buf + cnt, len - cnt, MSG_NOSIGNAL);
		if (n < 0)
			return false;
		cnt += n;
	}
	return true;
}

static ssize_t
readwrap(sckhld sh, void *buf, size_t sz)
{
#ifdef WITH_SSL
	if (sh.shnd)
		return (ssize_t)SSL_read(sh.shnd, buf, sz);
#endif
	return read(sh.sck, buf, sz);
}

static ssize_t
sendwrap(sckhld sh, const void *buf, size_t len, int flags)
{
#ifdef WITH_SSL
	if (sh.shnd)
		return (ssize_t)SSL_write(sh.shnd, buf, len);
#endif
	return send(sh.sck, buf, len, flags);
}

static int
tokenize(char *buf, tokarr *tok)
{
	if (!buf || !tok)
		return -1;

	V("tokenizing: %s", buf);
	for(size_t i = 0; i < COUNTOF(*tok); ++i)
		(*tok)[i] = NULL;

	while(isspace((unsigned char)*buf))
		*buf++ = '\0';

	size_t len = strlen(buf);
	V("len is %zu, jumped over ws: %s", len, buf);
	if (len == 0)
		return 0;

	if (*buf == ':') {
		(*tok)[0] = buf + 1;
		buf = skip2lws(buf, true);
		if (!buf) {
			W("parse erro, pfx but no cmd");
			return -1;//parse err, pfx but no cmd
		}
		while(isspace((unsigned char)*buf))
			*buf++ = '\0';
		V("extracted pfx: %s, rest: %s", (*tok)[0], buf);
	}

	(*tok)[1] = buf;
	buf = skip2lws(buf, true);
	if (buf) {
		while(isspace((unsigned char)*buf))
			*buf++ = '\0';
	}
	V("extracted cmd: %s, rest: %s", (*tok)[1], buf);

	size_t argc = 2;
	while(buf && *buf && argc < COUNTOF(*tok)) {
		V("iter (argc: %zu) buf is: %s", argc, buf);
		if (*buf == ':') {
			(*tok)[argc++] = buf + 1;
			V("extracted trailing (len: %zu), arg[%zu]: %s",
			    strlen(buf+1), argc-1, (*tok)[argc-1]);
			break;
		}
		(*tok)[argc++] = buf;

		/* have seen a channel with <Tab> in its name */
		buf = skip2lws(buf, false);
		if (buf) {
			while(isspace((unsigned char)*buf))
				*buf++ = '\0';
			V("jumped over ws: %s", buf);
		}
		V("extracted arg[%zu]: %s, rest: %s",
		    argc-1, (*tok)[argc-1], buf);
	}

	V("done!");
	return 1;
}

static char*
skip2lws(char *s, bool tab_is_ws)
{
	while(*s &&
	    (!isspace((unsigned char)*s) || (*s == '\t' && !tab_is_ws)))
		s++;
	return *s ? s : NULL;
}
