/* irc_io.c - Back-end, i/o processing
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

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
#include <common.h>
#include <intlog.h>

#include "irc_io.h"


#define ISDELIM(C) ((C)=='\n' || (C) == '\r')

/* local helpers */
static void hexdump(const void *pAddressIn, size_t  zSize, const char *name);
static int tokenize(char *buf, char *(*tok)[MAX_IRCARGS]);
static char *skip2lws(char *s, bool tab_is_ws); //fwd ptr until whitespace
static int writeall(int sck,
#ifdef WITH_SSL
    SSL *shnd,
#endif
    const char *buf); //write a full string

/* pub if implementation */
int
ircio_read(int sck, char *tokbuf, size_t tokbuf_sz, char *workbuf,
    size_t workbuf_sz, char **mehptr, char *(*tok)[MAX_IRCARGS],
    uint64_t to_us)
{
#ifdef WITH_SSL
	return ircio_read_ex(sck, NULL, tokbuf, tokbuf_sz, workbuf,
	    workbuf_sz, mehptr, tok, to_us);
#else
	return ircio_read_ex(sck, tokbuf, tokbuf_sz, workbuf,
	    workbuf_sz, mehptr, tok, to_us);
#endif
}

int
ircio_read_ex(int sck,
#ifdef WITH_SSL
    SSL *shnd,
#endif
    char *tokbuf, size_t tokbuf_sz, char *workbuf,
    size_t workbuf_sz, char **mehptr, char *(*tok)[MAX_IRCARGS],
    uint64_t to_us)
{
	uint64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	V("invoke(sck:%d, tokbuf: %p, tokbuf_sz: %zu, workbuf: %p, "
	    "workbuf_sz: %zu, mehptr: %p (*:%p), to_us: %"PRIu64", tsend: %"PRIu64")",
	    sck, tokbuf, tokbuf_sz, workbuf, workbuf_sz, mehptr, *mehptr,
	    to_us, tsend);
	if (!*mehptr) {
		V("fresh invoke (*mehptr is NULL). init workbuf, "
		    "pointing *mehptr to it");
		*mehptr = workbuf;
		workbuf[0] = '\0';
	} else {
		V("first ten of *mehptr: '%.10s'", *mehptr);
		if (ircdbg_getlvl() == LOG_VIVI)
			hexdump(workbuf, workbuf_sz, "workbuf");
	}

	while(ISDELIM(**mehptr)) {
		V("skipping a leading delim");
		(*mehptr)++;
	}

	char *end = *mehptr;

	while(*end && !ISDELIM(*end))
		end++;

	if (!*end) {
		V("didn't find delim in workbuf, need moar dataz");
		size_t len = (size_t)(end - *mehptr);
		V("%zu bytes already in workbuf", len);
		if (*mehptr != workbuf) {
			size_t mehdist = (size_t)(*mehptr - workbuf);
			V("*mehptr doesn't point at workbuf's start "
			    "(dist: %zu), shifting %zu bytes to the start",
			    mehdist, len);

			if (ircdbg_getlvl() == LOG_VIVI)
				hexdump(workbuf, workbuf_sz,
				    "workbuf before shift");
			memmove(workbuf, *mehptr, len);
			V("zeroing %zu bytes", workbuf_sz - len);
			memset(workbuf + len, 0, workbuf_sz - len);
			*mehptr = workbuf;
			end -= mehdist;
			V("end is %p (%"PRIu8" (%c))", end, *end, *end);
			if (ircdbg_getlvl() == LOG_VIVI)
				hexdump(workbuf, workbuf_sz,
				    "workbuf after shift");
		}

		for(;;) {
			if (len + 1 >= workbuf_sz) {
				W("(sck:%d) input too long", sck);
				return -1;
			}
			for(;;) {
				struct timeval tout;
				if (tsend) {
					uint64_t trem =
					    tsend - ic_timestamp_us();

					if (trem <= 0) {
						V("(sck:%d) timeout hit"
						    " while selecting",
						    sck);
						return 0;
					}

					ic_tconv(&tout, &trem, false);
				}
				fd_set fds;
				FD_ZERO(&fds);
				FD_SET(sck, &fds);
				errno = 0;
				V("selecting...");
				int r = select(sck+1, &fds, NULL, NULL,
				    tsend ? &tout : NULL);

				if (r < 0) {
					if (errno == EINTR) {
						WE("(sck:%d) select got "
						    "while selecting",sck);
						return 0;
					}
					WE("(sck:%d) select() failed",sck);
					return -1;
				} else if (r == 1) {
					break;
				} else if (r != 0)
					W("wtf select returned %d", r);
			}
			V("reading max %zu byte", workbuf_sz - len - 1);
			ssize_t n;
			errno = 0;
#ifdef WITH_SSL
			if (shnd)
				n = (ssize_t)SSL_read(shnd, end,
				    workbuf_sz - len - 1);
			else
				n = read(sck, end, workbuf_sz - len - 1);
#else
			n = read(sck, end, workbuf_sz - len - 1);
#endif
			if (n <= 0) {
				if (n == 0)
					W("(sck%d) read: EOF", sck);
				else
					WE("(sck%d) read failed (%zd)",
					    sck, n);
				return -1;
			}
			V("read returned %d", n);
			bool gotdelim = false;
			char *delim = NULL;
			while(n--) {
				if (!gotdelim && ISDELIM(*end)) {
					delim = end;
					V("found a delim");
					gotdelim = true;
				}

				end++;
				len++;
			}
			assert (*end == '\0');

			if (gotdelim) {
				end = delim;
				break;
			}
			V("no delim found so far");
		}
	}
	if (ircdbg_getlvl() == LOG_VIVI)
		hexdump(workbuf, workbuf_sz, "workbuf aftr both loops");

	assert (*end);
	size_t len = (size_t)(end - *mehptr);
	V("got %zu bytes till delim in workbuf", len);
	if (len + 1 >= tokbuf_sz)
		len = tokbuf_sz - 1;
	V("copying %zu bytes into tokbuf", len);
	strncpy(tokbuf, *mehptr, len);
	tokbuf[len] = '\0';
	*mehptr = end+1;
	V("first ten of *mehptr: '%.10s'", *mehptr);
	V("tokenizing, then done!");
	D("read a line: '%s'", tokbuf);
	return tokenize(tokbuf, tok);
}

int
ircio_write(int sck, const char *line)
{
#ifdef WITH_SSL
	return ircio_write_ex(sck, NULL, line);
#else
	return ircio_write_ex(sck, line);
#endif
}

int
ircio_write_ex(int sck,
#ifdef WITH_SSL
    SSL *shnd,
#endif
    const char *line)
{
	V("invoke(sck:%d, line: %s", sck, line);
	if (sck < 0 || !line)
		return -1;

	size_t len = strlen(line);
	int needbr = len < 2 || line[len-2] != '\r' || line[len-1] != '\n';

#ifdef WITH_SSL
	if (!writeall(sck, shnd, line)
	    || (needbr && !writeall(sck, shnd, "\r\n"))) {
#else
	if (!writeall(sck, line) || (needbr && !writeall(sck, "\r\n"))) {
#endif
		W("(sck%d) writeall() failed for line: %s", sck, line);
		return -1;
	}

	return (int)(len + (needbr ? 2 : 0));
}

/* local helpers implementation */
static int
writeall(int sck,
#ifdef WITH_SSL
    SSL *shnd,
#endif
    const char *buf)
{
	V("invoke");
	size_t cnt = 0;
	size_t len = strlen(buf);
	while(cnt < len) {
		errno = 0;
		ssize_t n;
		V("calling send()/SSL_write(), len: %zu", len - cnt);
#ifdef WITH_SSL
		if (shnd)
			n = (ssize_t)SSL_write(shnd, buf + cnt, len - cnt);
		else
			n = send(sck, buf + cnt, len - cnt, MSG_NOSIGNAL);
#else
		n = send(sck, buf + cnt, len - cnt, MSG_NOSIGNAL);
#endif
		if (n < 0) {
			WE("(sck%d) send() failed", sck);
			return 0;
		}
		V("send/SSL_write() returned %zd", n);
		cnt += n;
	}
	V("success");
	return 1;
}

static int
tokenize(char *buf, char *(*tok)[MAX_IRCARGS])
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

/*not-quite-ravomavain's h4xdump*/
static void hexdump(const void *pAddressIn, size_t zSize, const char *name)
{
	char szBuf[100];
	size_t lIndent = 1;
	size_t lOutLen, lIndex, lIndex2, lOutLen2;
	size_t lRelPos;
	struct { char *pData; size_t zSize; } buf;
	unsigned char *pTmp,ucTmp;
	unsigned char *pAddress = (unsigned char *)pAddressIn;

	buf.pData   = (char *)pAddress;
	buf.zSize   = zSize;
	V("hexdump '%s'", name);

	while (buf.zSize > 0) {
		pTmp = (unsigned char *)buf.pData;
		lOutLen = (int)buf.zSize;
		if (lOutLen > 16)
			lOutLen = 16;

		/* create a 64-character formatted output line: */
		sprintf(szBuf, " |                            "
		    "                      "
		    "    %08"PRIX64, (uint64_t)(pTmp-pAddress));
		lOutLen2 = lOutLen;

		for(lIndex = 1+lIndent, lIndex2 = 53-15+lIndent, lRelPos=0;
		    lOutLen2;
		    lOutLen2--, lIndex += 2, lIndex2++) {
			ucTmp = *pTmp++;

			sprintf(szBuf + lIndex, "%02"PRIX16" ", (uint16_t)ucTmp);
			if(!isprint(ucTmp))  ucTmp = '.'; /*nonprintable*/
			szBuf[lIndex2] = ucTmp;

			if (!(++lRelPos & 3)) /*extra blank after 4 bytes*/
			{  lIndex++; szBuf[lIndex+2] = ' '; }
		}

		if (!(lRelPos & 3)) lIndex--;

		szBuf[lIndex  ]   = '|';
		szBuf[lIndex+1]   = ' ';

		V("%s", szBuf);

		buf.pData   += lOutLen;
		buf.zSize   -= lOutLen;
	}
	V("end of hexdump '%s'", name);
}

