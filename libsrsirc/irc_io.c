/* irc_io.c - Back-end, i/o processing
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* C */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

/* POSIX */
#include <unistd.h>
#include <sys/socket.h>

#include <common.h>
#include <log.h>
/* pub if */
#include <libsrsirc/irc_io.h>

#define ISDELIM(C) ((C)=='\n' || (C) == '\r')

/* local helpers */
static int tokenize(char *buf, char **tok, size_t tok_len); //tokenize proto msg
static char *skip2lws(char *s, bool tab_is_ws); //forward pointer until whitespace
static int writeall(int sck, const char* buf); //write a full string

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
//	D("hexdump '%s'", name);
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
//	D("end of hexdump '%s'", name);
//}

int
ircio_read(int sck, char *tokbuf, size_t tokbuf_sz, char *workbuf, size_t workbuf_sz, char **mehptr, char **tok, size_t tok_len, unsigned long to_us)
{
	int64_t tsend = to_us ? timestamp_us() + to_us : 0;
	//D("invoke(sck:%d, tokbuf: %p, tokbuf_sz: %zu, workbuf: %p, workbuf_sz: %zu, mehptr: %p (*:%p), to_us: %lu, tsend: %lld)", sck, tokbuf, tokbuf_sz, workbuf, workbuf_sz, mehptr, *mehptr, to_us, tsend);
	if (!*mehptr) {
		//D("fresh invoke (*mehptr is NULL). initializing workbuf, pointing *mehptr to it");
		*mehptr = workbuf;
		workbuf[0] = '\0';
	} else {
		//D("first ten of *mehptr: '%.10s'", *mehptr);
		//hexdump(workbuf, workbuf_sz, "workbuf");
	}

	while(ISDELIM(**mehptr)) {
		//D("skipping a leading delim");
		(*mehptr)++;
	}
	
	char *end = *mehptr;

	while(*end && !ISDELIM(*end))
		end++;

	if (!*end) {
		//D("didn't find delim in workbuf, need moar dataz");
		size_t len = (size_t)(end - *mehptr);
		//D("%zu bytes already in workbuf", len);
		if (*mehptr != workbuf) {
			size_t mehdist = (size_t)(*mehptr - workbuf);
			//D("*mehptr doesn't point at workbuf's start (dist: %zu), shifting %zu bytes to the beginning", mehdist, len);
			//hexdump(workbuf, workbuf_sz, "workbuf before shift");
			memmove(workbuf, *mehptr, len);
			//D("zeroing %zu bytes", workbuf_sz - len);
			memset(workbuf + len, 0, workbuf_sz - len);
			*mehptr = workbuf;
			end -= mehdist;
			//D("end is %p (%hhx (%c))", end, *end, *end);
			//hexdump(workbuf, workbuf_sz, "workbuf after shift");
		}

		for(;;) {
			if (len + 1 >= workbuf_sz) {
				W("(sck:%d) input too long", sck);
				return -1;
			}
			for(;;) {
				struct timeval tout;
				if (tsend) {
					int64_t trem = tsend - timestamp_us();
					if (trem > 1000000)
						trem = 1000000; //limiting time spent in read so we can cancel w/o much delay
					if (trem <= 0) {
						//D("(sck:%d) timeout reached while selecting for read", sck);
						return 0;
					}

					tconv(&tout, &trem, false);
				}
				fd_set fds;
				FD_ZERO(&fds);
				FD_SET(sck, &fds);
				errno = 0;
				//D("selecting...");
				int r = select(sck+1, &fds, NULL, NULL, tsend ? &tout : NULL);
				if (r < 0) {
					if (errno == EINTR) {
						NE("(sck:%d) received signal while selecting", sck);
						return 0;
					}
					WE("(sck:%d) select() failed", sck);
					return -1;
				} else if (r == 1) {
					break;
				} else if (r != 0)
					W("wtf select returned %d", r);
			}
			//D("reading max %zu byte", workbuf_sz - len - 1);
			errno = 0;
			ssize_t n = read(sck, end, workbuf_sz - len - 1);
			if (n <= 0) {
				if (n == 0) W("(sck%d) read: EOF", sck);
				else        WE("(sck%d) read failed (%d)", sck, n);
				return -1;
			}
			//D("read returned %d", n);
			bool gotdelim = false;
			char *delim = NULL;
			while(n--) {
				if (!gotdelim && ISDELIM(*end)) {
					delim = end;
					//D("found a delim");
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
			//D("no delim found so far");
		}
	}
	//hexdump(workbuf, workbuf_sz, "workbuf aftr both loops");
		
	assert (*end);
	size_t len = (size_t)(end - *mehptr);
	//D("got %zu bytes till delim in workbuf", len);
	if (len + 1 >= tokbuf_sz)
		len = tokbuf_sz - 1;
	//D("copying %zu bytes into tokbuf", len);
	strncpy(tokbuf, *mehptr, len);
	tokbuf[len] = '\0';
	*mehptr = end+1;
	//D("first ten of *mehptr: '%.10s'", *mehptr);
	//D("tokenizing, then done!");
	return tokenize(tokbuf, tok, tok_len);
}
/* pub if implementation */

//int
//ircio_read(int sck, char *buf, char *overbuf, size_t bufoverbuf_sz, char **tok, size_t tok_len, unsigned long to_us)
//{
//	if (sck < 0 || !buf || !bufoverbuf_sz || tok_len < 2)
//		return -1;
//	//D("(sck:%d) wanna read (timeout: %lu)", sck, to_us);
//
//	int64_t tsend = to_us ? timestamp_us() + to_us : 0;
//	/* read a \r\n terminated line into buf */
//	size_t c = strlen(overbuf);
//	int r;
//	while(c < bufoverbuf_sz && (c<2 || overbuf[c-2] != '\r' || overbuf[c-1] != '\n')) {
//		if (tsend)
//		{
//			fd_set fds;
//			for(;;) {
//				FD_ZERO(&fds);
//				FD_SET(sck, &fds);
//
//				int64_t trem = tsend - timestamp_us();
//				if (trem > 500000)
//					trem = 500000; //limiting time spent in read so we can cancel w/o much delay
//				if (trem <= 0) {
//					if (c > 0)
//						W("(sck:%d) timeout reached while selecting for read, we already had %zu bytes:/", sck, c);
//					return 0;
//				}
//
//				struct timeval tout;
//				tout.tv_sec = 0;
//				tout.tv_usec = 0;
//				tconv(&tout, &trem, false);
//
//				errno = 0;
//				//D("selecting");
//				r = select(sck+1, &fds, NULL, NULL, tsend ? &tout : NULL);
//				//D("select: %d", r);
//				if (r < 0)
//				{
//					WE("(sck:%d) select() failed", sck);
//					return -1;
//				}
//				if (r == 1) {
//					break;
//				}
//				if (r != 0)
//					W("wtf select returned %d", r);
//			}
//		}
//		//D("read()ing a byte");
//		errno = 0;
//		if ((r = read(sck, &overbuf[c++], 1)) <= 0) {
//			if (r == 0) W("(sck%d) read: EOF", sck);
//			else        WE("(sck%d) read failed (%d)", sck, r);
//			return -1;
//		}// else
//			//D("read returned %d (%hhx (%c))", r, overbuf[c-1], overbuf[c-1]);
//	}
//	//D("(sck%d) done reading", sck);
//	if (c == bufoverbuf_sz) {
//		char last = overbuf[c-1];
//		overbuf[c-1] = '\0';
//		W("line too long: '%s' (last char was %hhx (%c))", overbuf, last, last);
//		return -1;
//	}
//	
//	overbuf[c - 2] = '\0'; // cut off \r\n
//	strcpy(buf, overbuf);
//	overbuf[0] = '\0';
//	D("read a line: '%s'", buf);
//	return tokenize(buf, tok, tok_len);
//}

int
ircio_write(int sck, const char *line)
{
	if (sck < 0 || !line)
		return -1;

	size_t len = strlen(line);
	int needbr = len < 2 || line[len - 2] != '\r' || line[len - 1] != '\n';

	if (!writeall(sck, line) || (needbr && !writeall(sck, "\r\n"))) {
		W("(sck%d) writeall() failed", sck);
		return -1;
	}

	return (int)(len + (needbr ? 2 : 0));
}

/* local helpers implementation */
static int
writeall(int sck, const char* buf)
{
	size_t cnt = 0;
	size_t len = strlen(buf);
	while(cnt < len)
	{
	//	int n = write(sck, buf + cnt, len - cnt);
		errno = 0;
		ssize_t n = send(sck, buf + cnt, len - cnt, MSG_NOSIGNAL);
		if (n < 0) {
			WE("(sck%d) send() failed", sck);
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
	
	//D("tokenizing: %s", buf);
	for(size_t i = 0; i < tok_len; ++i)
		tok[i] = NULL;

	while(isspace(*buf))
		*buf++ = '\0';

	size_t len = strlen(buf);
	//D("len is %zu, jumped over ws: %s", len, buf);
	if (len == 0)
		return 0;

	if (*buf == ':')
	{
		tok[0] = buf + 1;
		buf = skip2lws(buf, true);
		if (!buf) {
			W("parse erro, pfx but no cmd");
			return -1;//parse err, pfx but no cmd
		}
		while(isspace(*buf))
			*buf++ = '\0';
		//D("extracted pfx: %s, rest: %s", tok[0], buf);
	}

	tok[1] = buf;
	buf = skip2lws(buf, true);
	if (buf) {
		while(isspace(*buf))
			*buf++ = '\0';
	}
	//D("extracted cmd: %s, rest: %s", tok[1], buf);

	size_t argc = 2;
	while(buf && *buf && argc < tok_len)
	{
		//D("iter (argc: %zu) buf is: %s", argc, buf);
		if (*buf == ':')
		{
			*buf = '\0';
			tok[argc++] = buf + 1;
			//D("extracted trailing (len: %zu), arg[%zu]: %s", strlen(buf+1), argc-1, tok[argc-1]);
			break;
		}
		tok[argc++] = buf;
		buf = skip2lws(buf, false); //have seen a channel with <Tab> in its name
		if (buf) {
			while(isspace(*buf))
				*buf++ = '\0';
			//D("jumped over ws: %s", buf);
		}
		//D("extracted arg[%zu]: %s, rest: %s", argc-1, tok[argc-1], buf);
	}

	//D("done!");
	return 1;
}

static char*
skip2lws(char *s, bool tab_is_ws)
{
	while(*s && (!isspace(*s) || (*s == '\t' && !tab_is_ws)))
		s++;
	return *s ? s : NULL;
}

void
ircio_log_init(void)
{
	if (!LOG_ISINIT())
		LOG_INITX("irc_io", LOGLVL_WARN, stderr, false);
}

void
ircio_log_set_loglvl(int loglevel)
{
	LOG_LEVEL(loglevel);
}
void
ircio_log_set_target(FILE *str)
{
	LOG_TARGET(str);
}
void
ircio_log_set_fancy(bool fancy)
{
	LOG_COLORS(fancy);
}
