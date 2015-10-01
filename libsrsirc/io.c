/* io.c - i/o processing, protocol tokenization
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_IIO

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "io.h"


#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <platform/base_net.h>
#include <platform/base_time.h>

#include <logger/intlog.h>

#include "common.h"

#include <libsrsirc/util.h>


#define ISDELIM(C) ((C) == '\n' || (C) == '\r')


/* local helpers */
static char* find_delim(struct readctx *ctx);
static int read_more(sckhld sh, struct readctx *ctx, uint64_t to_us);
static bool write_str(sckhld sh, const char *buf);
static long read_wrap(sckhld sh, void *buf, size_t sz);
static long send_wrap(sckhld sh, const void *buf, size_t len);


/* Documented in io.h */
int
lsi_io_read(sckhld sh, struct readctx *ctx, tokarr *tok, uint64_t to_us)
{ T("trace");
	uint64_t tsend = to_us ? lsi_b_tstamp_us() + to_us : 0;

	while (ctx->wptr < ctx->eptr && ISDELIM(*ctx->wptr))
		ctx->wptr++; /* skip leading line delimiters */
	if (ctx->wptr == ctx->eptr) /* empty buffer, use the opportunity.. */
		ctx->wptr = ctx->eptr = ctx->workbuf;

	size_t linelen;
	char *delim;
	char *linestart;
	do {
		while (!(delim = find_delim(ctx))) {
			uint64_t trem = 0;
			if (lsi_com_check_timeout(tsend, &trem))
				return 0;

			int r = read_more(sh, ctx, trem);
			if (r <= 0)
				return r;
		}

		linestart = ctx->wptr;
		linelen = delim - linestart;
		ctx->wptr++;
	} while (linelen == 0);
	ctx->wptr += linelen;

	*delim = '\0';

	I("read: '%s'", linestart);

	return lsi_ut_tokenize(linestart, tok) ? 1 : -1;
}

/* Documented in io.h */
bool
lsi_io_write(sckhld sh, const char *line)
{ T("trace");
	size_t len = strlen(line);
	int needbr = len < 2 || line[len-2] != '\r' || line[len-1] != '\n';

	I("write: '%s%s'", line, needbr ? "\r\n" : "");

	return write_str(sh, line) && (!needbr || write_str(sh, "\r\n"));
}



/* return pointer to first line delim in our receive buffer, or NULL if none */
static char*
find_delim(struct readctx *ctx)
{ T("trace");
	for (char *ptr = ctx->wptr; ptr < ctx->eptr; ptr++)
		if (ISDELIM(*ptr))
			return ptr;
	return NULL;
}

/* attempt to read more data from the ircd into our read buffer.
 * returns 1 if something was read; 0 on timeout; -1 on failure */
static int
read_more(sckhld sh, struct readctx *ctx, uint64_t to_us)
{ T("trace");
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

	int r = lsi_b_select(sh.sck, true, to_us);
	if (r != 1)
		return r;

	long n = read_wrap(sh, ctx->eptr, remain);
	if (n <= 0) {
		n == 0 ?  W("read: EOF") : WE("read failed");
		return n == 0 ? -2 : -1; // FIXME: magic numbers
	}

	ctx->eptr += n;
	return 1;
}


/* write a string to a socket, ensure that everything is sent, well, buffered.
 * returns true on success, false on failure */
static bool
write_str(sckhld sh, const char *buf)
{ T("trace");
	size_t cnt = 0;
	size_t len = strlen(buf);
	while (cnt < len) {
		long n = send_wrap(sh, buf + cnt, len - cnt);
		if (n < 0)
			return false;
		cnt += n;
	}
	return true;
}

/* wrap around either read() or SSL_read(), depending on whether
 * or not SSL is compiled-in and enabled (or not) */
static long
read_wrap(sckhld sh, void *buf, size_t sz)
{ T("trace");
	if (sh.shnd)
		return lsi_b_read_ssl(sh.shnd, buf, sz, NULL);
	return lsi_b_read(sh.sck, buf, sz, NULL);
}

/* likewise for send()/SSL_write() */
static long
send_wrap(sckhld sh, const void *buf, size_t len)
{ T("trace");
	if (sh.shnd)
		return lsi_b_write_ssl(sh.shnd, buf, len);
	return lsi_b_write(sh.sck, buf, len);
}
