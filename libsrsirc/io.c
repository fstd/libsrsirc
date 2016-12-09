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
static long read_wrap(sckhld sh, void *buf, size_t sz, uint64_t to_us);
static long send_wrap(sckhld sh, const void *buf, size_t len);


/* Documented in io.h */
int
lsi_io_read(sckhld sh, struct readctx *rctx, tokarr *tok,
    char **tags, size_t *ntags, uint64_t to_us)
{
	uint64_t tend = to_us ? lsi_b_tstamp_us() + to_us : 0;
	uint64_t tnow, trem = 0;

	V("Wanna read with%s timeout (%"PRIu64"). %zu bytes in buffer: '%.*s'",
	    tend?"":" no", to_us, rctx->eptr - rctx->wptr,
	    (int)(rctx->eptr - rctx->wptr), rctx->wptr);

	while (rctx->wptr < rctx->eptr && ISDELIM(*rctx->wptr))
		rctx->wptr++; /* skip leading line delimiters */
	if (rctx->wptr == rctx->eptr) { /* empty buffer, use the opportunity.. */
		rctx->wptr = rctx->eptr = rctx->workbuf;
		V("Opportunistical buffer reset");
	}

	size_t linelen;
	char *delim;
	char *linestart;
	do {
		while (!(delim = find_delim(rctx))) {
			if (tend) {
				tnow = lsi_b_tstamp_us();
				trem = tnow >= tend ? 1 : tend - tnow;
			}

			int r = read_more(sh, rctx, trem);
			if (r <= 0)
				return r;
		}

		linestart = rctx->wptr;
		linelen = delim - linestart;
		rctx->wptr++;
		V("Delim found, linelen %zu", linelen);
	} while (linelen == 0);
	rctx->wptr += linelen;

	*delim = '\0';

	I("Read: '%s'", linestart);

	if (linestart[0] == '@') {
		linestart = lsi_ut_extract_tags(linestart + 1,
		    tags, ntags);

		if (!linestart || !linestart[0]) {
			E("protocol error (just tags?)");
			return -1;
		}
	} else if (ntags)
		*ntags = 0;

	return lsi_ut_tokenize(linestart, tok) ? 1 : -1;
}

/* Documented in io.h */
bool
lsi_io_write(sckhld sh, const char *line)
{
	size_t len = strlen(line);
	int needbr = len < 2 || line[len-2] != '\r' || line[len-1] != '\n';

	V("Wanna write: '%s%s'", line, needbr ? "\r\n" : "");

	bool suc = write_str(sh, line) && (!needbr || write_str(sh, "\r\n"));

	if (suc)
		I("Wrote: '%s%s'", line, needbr ? "\r\n" : "");
	else
		W("Failed to write '%s%s'", line, needbr ? "\r\n" : "");

	return suc;
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
		D("Buffer is full");
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
		D("Moved %zu bytes to the front; space: %zu", datalen, remain);
	}

	V("Reading more data (max. %zu bytes, timeout: %"PRIu64, remain, to_us);
	long n = read_wrap(sh, rctx->eptr, remain, to_us);
	// >0: Amount of bytes read
	// 0: timeout
	// -1: Failure
	// -2: EOF
	if (n <= 0) {
		if (n < 0)
			n == -2 ?  W("read: EOF") : WE("read failed");
		else
			V("Timeout");
		return n;
	}

	V("Got %ld more bytes", n);

	rctx->eptr += n;
	return 1;
}


/* write a string to a socket. the underlying write function ensures that
 * everything is sent, well, buffered.
 * returns true on success, false on failure */
static bool
write_str(sckhld sh, const char *str)
{
	return send_wrap(sh, str, strlen(str)) > 0;
}

/* wrap around either read() or SSL_read(), depending on whether
 * or not SSL is compiled-in and enabled (or not) */
static long
read_wrap(sckhld sh, void *buf, size_t sz, uint64_t to_us)
{
	if (sh.shnd)
		return lsi_b_read_ssl(sh.shnd, buf, sz, to_us);
	return lsi_b_read(sh.sck, buf, sz, to_us);
}

/* likewise for send()/SSL_write() */
static long
send_wrap(sckhld sh, const void *buf, size_t len)
{
	if (sh.shnd)
		return lsi_b_write_ssl(sh.shnd, buf, len);
	return lsi_b_write(sh.sck, buf, len);
}
