/* irc_io.c - Back-end, i/o processing
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE 1

/* pub if */
#include <libsrsirc/irc_io.h>

/* locals */
#include <common.h>

/* C */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

/* POSIX */
#include <unistd.h>
#include <sys/socket.h>

#ifdef WITH_SSL
/* ssl */
# include <openssl/err.h>
#endif

#include <debug.h>

#define ISDELIM(C) ((C)=='\n' || (C) == '\r')

/* local helpers */
static int tokenize(char *buf, char **tok, size_t tok_len); //tokenize proto msg
static char *skip2lws(char *s, bool tab_is_ws); //fwd pointer until whitespace
static int writeall(int sck,
#ifdef WITH_SSL
    SSL *shnd,
#endif
    const char* buf); //write a full string

/* pub if implementation */
int
ircio_read(int sck, char *tokbuf, size_t tokbuf_sz, char *workbuf,
    size_t workbuf_sz, char **mehptr, char **tok, size_t tok_len,
    unsigned long to_us)
{
#ifdef WITH_SSL
	return ircio_read_ex(sck, NULL, tokbuf, tokbuf_sz, workbuf,
	    workbuf_sz, mehptr, tok, tok_len, to_us);
#else
	return ircio_read_ex(sck, tokbuf, tokbuf_sz, workbuf,
	    workbuf_sz, mehptr, tok, tok_len, to_us);
#endif
}

int
ircio_read_ex(int sck,
#ifdef WITH_SSL
    SSL *shnd,
#endif
    char *tokbuf, size_t tokbuf_sz, char *workbuf,
    size_t workbuf_sz, char **mehptr, char **tok, size_t tok_len,
    unsigned long to_us)
{
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	//WVX("invoke(sck:%d, tokbuf: %p, tokbuf_sz: %zu, workbuf: %p, "
	//    "workbuf_sz: %zu, mehptr: %p (*:%p), to_us: %lu, tsend: %lld)",
	//    sck, tokbuf, tokbuf_sz, workbuf, workbuf_sz, mehptr, *mehptr,
	//    to_us, tsend);
	if (!*mehptr) {
		//WVX("fresh invoke (*mehptr is NULL). init workbuf, "
		//    "pointing *mehptr to it");
		*mehptr = workbuf;
		workbuf[0] = '\0';
	} else {
		//WVX("first ten of *mehptr: '%.10s'", *mehptr);
		//hexdump(workbuf, workbuf_sz, "workbuf");
	}

	while(ISDELIM(**mehptr)) {
		//WVX("skipping a leading delim");
		(*mehptr)++;
	}

	char *end = *mehptr;

	while(*end && !ISDELIM(*end))
		end++;

	if (!*end) {
		//WVX("didn't find delim in workbuf, need moar dataz");
		size_t len = (size_t)(end - *mehptr);
		//WVX("%zu bytes already in workbuf", len);
		if (*mehptr != workbuf) {
			size_t mehdist = (size_t)(*mehptr - workbuf);
			//WVX("*mehptr doesn't point at workbuf's start "
			//    "(dist: %zu), shifting %zu bytes to the beginning",
			//    mehdist, len);

			//hexdump(workbuf, workbuf_sz, "workbuf before shift");
			memmove(workbuf, *mehptr, len);
			//WVX("zeroing %zu bytes", workbuf_sz - len);
			memset(workbuf + len, 0, workbuf_sz - len);
			*mehptr = workbuf;
			end -= mehdist;
			//WVX("end is %p (%hhx (%c))", end, *end, *end);
			//hexdump(workbuf, workbuf_sz, "workbuf after shift");
		}

		for(;;) {
			if (len + 1 >= workbuf_sz) {
				WX("(sck:%d) input too long", sck);
				return -1;
			}
			for(;;) {
				struct timeval tout;
				if (tsend) {
					int64_t trem = tsend - ic_timestamp_us();
					if (trem <= 0) {
						//WVX("(sck:%d) timeout reached"
						//    " while selecting for read",
						//    sck);
						return 0;
					}

					ic_tconv(&tout, &trem, false);
				}
				fd_set fds;
				FD_ZERO(&fds);
				FD_SET(sck, &fds);
				errno = 0;
				//WVX("selecting...");
				int r = select(sck+1, &fds, NULL, NULL,
				    tsend ? &tout : NULL);

				if (r < 0) {
					if (errno == EINTR) {
						W("(sck:%d) received signal "
						    "while selecting", sck);
						return 0;
					}
					W("(sck:%d) select() failed", sck);
					return -1;
				} else if (r == 1) {
					break;
				} else if (r != 0)
					WX("wtf select returned %d", r);
			}
			//WVX("reading max %zu byte", workbuf_sz - len - 1);
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
					WX("(sck%d) read: EOF", sck);
				else
					W("(sck%d) read failed (%zd)", sck, n);
				return -1;
			}
			//WVX("read returned %d", n);
			bool gotdelim = false;
			char *delim = NULL;
			while(n--) {
				if (!gotdelim && ISDELIM(*end)) {
					delim = end;
					//WVX("found a delim");
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
			//WVX("no delim found so far");
		}
	}
	//hexdump(workbuf, workbuf_sz, "workbuf aftr both loops");

	assert (*end);
	size_t len = (size_t)(end - *mehptr);
	//WVX("got %zu bytes till delim in workbuf", len);
	if (len + 1 >= tokbuf_sz)
		len = tokbuf_sz - 1;
	//WVX("copying %zu bytes into tokbuf", len);
	strncpy(tokbuf, *mehptr, len);
	tokbuf[len] = '\0';
	*mehptr = end+1;
	//WVX("first ten of *mehptr: '%.10s'", *mehptr);
	//WVX("tokenizing, then done!");
	return tokenize(tokbuf, tok, tok_len);
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
	if (sck < 0 || !line)
		return -1;

	size_t len = strlen(line);
	int needbr = len < 2 || line[len - 2] != '\r' || line[len - 1] != '\n';

#ifdef WITH_SSL
	if (!writeall(sck, shnd, line) || (needbr && !writeall(sck, shnd, "\r\n"))) {
#else
	if (!writeall(sck, line) || (needbr && !writeall(sck, "\r\n"))) {
#endif
		WX("(sck%d) writeall() failed", sck);
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
    const char* buf)
{
	size_t cnt = 0;
	size_t len = strlen(buf);
	while(cnt < len)
	{
	//	int n = write(sck, buf + cnt, len - cnt);
		errno = 0;
		ssize_t n;
#ifdef WITH_SSL
		if (shnd)
			n = (ssize_t)SSL_write(shnd, buf + cnt, len - cnt);
		else
			n = send(sck, buf + cnt, len - cnt, MSG_NOSIGNAL);
#else
		n = send(sck, buf + cnt, len - cnt, MSG_NOSIGNAL);
#endif
		if (n < 0) {
			W("(sck%d) send() failed", sck);
			return 0;
		}
		cnt += n;
	}
	return 1;
}

static int
tokenize(char *buf, char **tok, size_t tok_len)
{
	if (!buf || !tok || tok_len < 2)
		return -1;

	//WVX("tokenizing: %s", buf);
	for(size_t i = 0; i < tok_len; ++i)
		tok[i] = NULL;

	while(isspace(*buf))
		*buf++ = '\0';

	size_t len = strlen(buf);
	//WVX("len is %zu, jumped over ws: %s", len, buf);
	if (len == 0)
		return 0;

	if (*buf == ':')
	{
		tok[0] = buf + 1;
		buf = skip2lws(buf, true);
		if (!buf) {
			WX("parse erro, pfx but no cmd");
			return -1;//parse err, pfx but no cmd
		}
		while(isspace(*buf))
			*buf++ = '\0';
		//WVX("extracted pfx: %s, rest: %s", tok[0], buf);
	}

	tok[1] = buf;
	buf = skip2lws(buf, true);
	if (buf) {
		while(isspace(*buf))
			*buf++ = '\0';
	}
	//WVX("extracted cmd: %s, rest: %s", tok[1], buf);

	size_t argc = 2;
	while(buf && *buf && argc < tok_len)
	{
		//WVX("iter (argc: %zu) buf is: %s", argc, buf);
		if (*buf == ':')
		{
			tok[argc++] = buf + 1;
			//WVX("extracted trailing (len: %zu), arg[%zu]: %s",
			//    strlen(buf+1), argc-1, tok[argc-1]);
			break;
		}
		tok[argc++] = buf;

		/* have seen a channel with <Tab> in its name */
		buf = skip2lws(buf, false);
		if (buf) {
			while(isspace(*buf))
				*buf++ = '\0';
			//WVX("jumped over ws: %s", buf);
		}
		//WVX("extracted arg[%zu]: %s, rest: %s", argc-1, tok[argc-1], buf);
	}

	//WVX("done!");
	return 1;
}

static char*
skip2lws(char *s, bool tab_is_ws)
{
	while(*s && (!isspace(*s) || (*s == '\t' && !tab_is_ws)))
		s++;
	return *s ? s : NULL;
}

/*not-quite-ravomavain's h4xdump*/
//static void hexdump(const void *pAddressIn, long  lSize, const char *name)
//{
//	char szBuf[100];
//	long lIndent = 1;
//	long lOutLen, lIndex, lIndex2, lOutLen2;
//	long lRelPos;
//	struct { char *pData; unsigned long lSize; } buf;
//	unsigned char *pTmp,ucTmp;
//	unsigned char *pAddress = (unsigned char *)pAddressIn;
//
//	buf.pData   = (char *)pAddress;
//	buf.lSize   = lSize;
//	WVX("hexdump '%s'", name);
//
//	while (buf.lSize > 0)
//	{
//		pTmp     = (unsigned char *)buf.pData;
//		lOutLen  = (int)buf.lSize;
//		if (lOutLen > 16)
//			lOutLen = 16;
//
//		/* create a 64-character formatted output line: */
//		sprintf(szBuf, " |                            "
//				"                      "
//				"    %08lX", (long unsigned int)(pTmp-pAddress));
//		lOutLen2 = lOutLen;
//
//		for(lIndex = 1+lIndent, lIndex2 = 53-15+lIndent, lRelPos = 0;
//				lOutLen2;
//				lOutLen2--, lIndex += 2, lIndex2++
//		   )
//		{
//			ucTmp = *pTmp++;
//
//			sprintf(szBuf + lIndex, "%02X ", (unsigned short)ucTmp);
//			if(!isprint(ucTmp))  ucTmp = '.'; /* nonprintable char */
//			szBuf[lIndex2] = ucTmp;
//
//			if (!(++lRelPos & 3))     /* extra blank after 4 bytes */
//			{  lIndex++; szBuf[lIndex+2] = ' '; }
//		}
//
//		if (!(lRelPos & 3)) lIndex--;
//
//		szBuf[lIndex  ]   = '|';
//		szBuf[lIndex+1]   = ' ';
//
//		fprintf(stderr, "%s\n", szBuf);
//
//		buf.pData   += lOutLen;
//		buf.lSize   -= lOutLen;
//	}
//	WVX("end of hexdump '%s'", name);
//}

