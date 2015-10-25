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
static char *find_delim(struct readctx *rctx);
static int read_more(sckhld sh, struct readctx *rctx, uint64_t to_us);
static bool write_str(sckhld sh, const char *str);
static long read_wrap(sckhld sh, void *buf, size_t sz, bool *tryagain);
static long send_wrap(sckhld sh, const void *buf, size_t len);


/* Documented in io.h */
int
lsi_io_read(sckhld sh, struct readctx *rctx, tokarr *tok, uint64_t to_us)
{
	uint64_t tsend = to_us ? lsi_b_tstamp_us() + to_us : 0;

	while (rctx->wptr < rctx->eptr && ISDELIM(*rctx->wptr))
		rctx->wptr++; /* skip leading line delimiters */
	if (rctx->wptr == rctx->eptr) /* empty buffer, use the opportunity.. */
		rctx->wptr = rctx->eptr = rctx->workbuf;

	size_t linelen;
	char *delim;
	char *linestart;
	do {
		while (!(delim = find_delim(rctx))) {
			uint64_t trem = 0;
			if (lsi_com_check_timeout(tsend, &trem))
				return 0;

			int r = read_more(sh, rctx, trem);
			if (r <= 0)
				return r;
		}

		linestart = rctx->wptr;
		linelen = delim - linestart;
		rctx->wptr++;
	} while (linelen == 0);
	rctx->wptr += linelen;

	*delim = '\0';

	I("read: '%s'", linestart);

	return lsi_ut_tokenize(linestart, tok) ? 1 : -1;
}

bool
lsi_io_can_read(sckhld sh, struct readctx *rctx)
{
	while (rctx->wptr < rctx->eptr && ISDELIM(*rctx->wptr))
		rctx->wptr++; /* skip leading line delimiters */

	if (rctx->wptr == rctx->eptr) /* empty buffer, use the opportunity.. */
		rctx->wptr = rctx->eptr = rctx->workbuf;

	if (find_delim(rctx))
		return true;
	
	if (sh.shnd) {
		E("we cannot reliably tell whether we can read ssl data");
		/* return true so the user does read.  it might
		 * block, cause drama and tears, but is better than
		 * them never reading at all.  this will be fixed
		 * when we're either nonblocking or have found a way
		 * to reliably select() an ssl socket.. XXX */
		return true;
	}

	int r = lsi_b_select(&sh.sck, 1, true, true, 1);

	return r == 1;
}

/* Documented in io.h */
bool
lsi_io_write(sckhld sh, const char *line)
{
	size_t len = strlen(line);
	int needbr = len < 2 || line[len-2] != '\r' || line[len-1] != '\n';

	I("write: '%s%s'", line, needbr ? "\r\n" : "");

	return write_str(sh, line) && (!needbr || write_str(sh, "\r\n"));
}



/* return pointer to first line delim in our receive buffer, or NULL if none */
static char *
find_delim(struct readctx *rctx)
{
	*rctx->eptr = '\0'; // for tracing
	for (char *ptr = rctx->wptr; ptr < rctx->eptr; ptr++)
		if (ISDELIM(*ptr))
			return ptr;
	return NULL;
}

/* attempt to read more data from the ircd into our read buffer.
 * returns 1 if something was read; 0 on timeout; -1 on failure */
static int
read_more(sckhld sh, struct readctx *rctx, uint64_t to_us)
{
	/* no sizeof rctx->workbuf here because it's one bigger than WORKBUF_SZ
	 * and we don't want to fill the last byte with data; it's a dummy */
	size_t remain = WORKBUF_SZ - (rctx->eptr - rctx->workbuf);
	if (!remain) { /* no more space left in receive buffer */
		if (rctx->wptr == rctx->workbuf) { /* completely full */
			E("input too long");
			return -1;
		}

		/* make additional room by moving data to the beginning */
		size_t datalen = (size_t)(rctx->eptr - rctx->wptr);
		memmove(rctx->workbuf, rctx->wptr, datalen);
		rctx->wptr = rctx->workbuf;
		rctx->eptr = &rctx->workbuf[datalen];

		remain = WORKBUF_SZ - (rctx->eptr - rctx->workbuf);
	}

	int r = lsi_b_select(&sh.sck, 1, true, true, to_us);
	if (r != 1)
		return r;

	bool tryagain = false;
	long n = read_wrap(sh, rctx->eptr, remain, &tryagain);
	if (n <= 0) {
		if (tryagain)
			return 0;
		n == 0 ?  W("read: EOF") : WE("read failed");
		return n == 0 ? -2 : -1; // FIXME: magic numbers
	}

	rctx->eptr += n;
	return 1;
}


/* write a string to a socket, ensure that everything is sent, well, buffered.
 * returns true on success, false on failure */
static bool
write_str(sckhld sh, const char *str)
{
	size_t cnt = 0;
	size_t len = strlen(str);
	while (cnt < len) {
		long n = send_wrap(sh, str + cnt, len - cnt);
		if (n < 0)
			return false;
		cnt += n;
	}
	return true;
}

/* wrap around either read() or SSL_read(), depending on whether
 * or not SSL is compiled-in and enabled (or not) */
static long
read_wrap(sckhld sh, void *buf, size_t sz, bool *tryagain)
{
	if (sh.shnd)
		return lsi_b_read_ssl(sh.shnd, buf, sz, tryagain);
	return lsi_b_read(sh.sck, buf, sz, tryagain);
}

/* likewise for send()/SSL_write() */
static long
send_wrap(sckhld sh, const void *buf, size_t len)
{
	if (sh.shnd)
		return lsi_b_write_ssl(sh.shnd, buf, len);
	return lsi_b_write(sh.sck, buf, len);
}
