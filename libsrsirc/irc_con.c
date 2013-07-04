/* irc_io.c - Semi back-end (btw irc_io and irc_basic) implementation
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE 1

/* pub if */
#include <libsrsirc/irc_con.h>

/* locals */
#include <common.h>
#include <libsrsirc/irc_io.h>
#include <libsrsirc/irc_util.h>

//#define _BSD_SOURCE 1

/* C */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

/* POSIX */
#include <unistd.h>
#include <sys/select.h>

#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "debug.h"

#define RXBUF_SZ 4096
#define LINEBUF_SZ 1024
#define OVERBUF_SZ 1024

#define DEF_HOST "localhost"
#define DEF_PORT 6667

#define INV -1
#define OFF 0
#define ON 1

#define HOST_IPV4 0
#define HOST_IPV6 1
#define HOST_DNS 2


#define XCALLOC(num) ic_xcalloc(1, num)
#define XMALLOC(num) ic_xmalloc(num)
#define XREALLOC(p, num) ic_xrealloc((p),(num))
#define XFREE(p) do{  if(p) free(p); p=0;  }while(0)
#define XSTRDUP(s) ic_xstrdup(s)


struct ichnd
{
	char *host;
	unsigned short port;

	char *phost;
	unsigned short pport;
	int ptype;

	int sck;
	int state;

	char *linebuf;
	char *overbuf;
	char *mehptr;
	bool cancel;
	bool colon_trail;

	pthread_mutex_t cancelmtx;
};

static bool pxlogon_http(ichnd_t hnd, unsigned long to_us);
static bool pxlogon_socks4(ichnd_t hnd, unsigned long to_us);
static bool pxlogon_socks5(ichnd_t hnd, unsigned long to_us);
static int guess_hosttype(const char *host);

ichnd_t
irccon_init(void)
{
	ichnd_t r = XMALLOC(sizeof (*(ichnd_t)0)); // XXX

	if (!r) {
		WX("couldn't allocate memory");
		return NULL;
	}

	if (pthread_mutex_init(&r->cancelmtx, NULL) != 0) {
		WX("couldn't init mutex");
		return NULL;
	}

	r->host = XSTRDUP(DEF_HOST);
	r->port = DEF_PORT;
	r->phost = NULL;
	r->pport = 0;
	r->ptype = -1;
	r->sck = -1;
	r->state = OFF;
	r->linebuf = XCALLOC(LINEBUF_SZ);
	r->overbuf = XCALLOC(OVERBUF_SZ);
	r->mehptr = NULL;
	r->cancel = false;
	r->colon_trail = false;

	WVX("(%p) irc_con initialized", r);

	return r;
}

bool
irccon_canceled(ichnd_t hnd)
{
	pthread_mutex_lock(&hnd->cancelmtx);
	bool b = hnd->cancel;
	pthread_mutex_unlock(&hnd->cancelmtx);
	return b;
}

void
irccon_cancel(ichnd_t hnd)
{
	WX("(%p) async cancel requested", hnd);
	pthread_mutex_lock(&hnd->cancelmtx);
	hnd->cancel = true;
	pthread_mutex_unlock(&hnd->cancelmtx);
}

bool
irccon_reset(ichnd_t hnd)
{
	if (!hnd || hnd->state == INV)
		return false;

	WVX("(%p) resetting", hnd);

	if (hnd->sck != -1) {
		WVX("(%p) closing socket %d", hnd, hnd->sck);
		close(hnd->sck);
	}

	hnd->sck = -1;
	hnd->state = OFF;
	hnd->linebuf[0] = '\0';
	memset(hnd->overbuf, 0, OVERBUF_SZ);
	hnd->mehptr = NULL;

	return true;
}

bool
irccon_dispose(ichnd_t hnd)
{
	if (!hnd || hnd->state == INV)
		return false;

	irccon_reset(hnd);

	XFREE(hnd->host);
	XFREE(hnd->phost);
	XFREE(hnd->linebuf);
	XFREE(hnd->overbuf);
	hnd->state = INV;

	pthread_mutex_destroy(&hnd->cancelmtx);
	WVX("(%p) disposed", hnd);
	XFREE(hnd);

	return true;
}

bool
irccon_connect(ichnd_t hnd, unsigned long to_us)
{
	if (!hnd || hnd->state != OFF)
		return false;
	pthread_mutex_lock(&hnd->cancelmtx);
	hnd->cancel = false;
	pthread_mutex_unlock(&hnd->cancelmtx);
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	char *host = hnd->ptype != -1 ? hnd->phost : hnd->host;
	unsigned short port = hnd->ptype != -1 ? hnd->pport : hnd->port;

	{
		char pxspec[64];
		pxspec[0] = '\0';
		if (hnd->ptype != -1) {
			snprintf(pxspec, sizeof pxspec, " via %s:%s:%hu",
		                 pxtypestr(hnd->ptype), hnd->phost, hnd->pport);
		}
		unsigned long to = to_us;
		char unit[3] = "us";
		if (to > 1000000*60) { unit[0] = 'm'; unit[1] = '\0'; to /= 1000000*60; }
		else if (to > 1000000) { unit[0] = 's'; unit[1] = '\0'; to /= 1000000; }
		else if (to > 1000) { unit[0] = 'm'; to /= 1000; }
		WVX("(%p) wanna connect to %s:%hu%s, timeout: %lu%s",
		           hnd, hnd->host, hnd->port, pxspec, to, unit);
	}

	struct sockaddr sa;
	size_t addrlen;
	int sck = ic_mksocket(host, port, &sa, &addrlen);

	if (sck < 0) {
		WX("(%p) failed to ic_mksocket for %s:%hu", hnd, host, port);
		return false;
	}
	WVX("(%p) created socket %d for %s:%hu", hnd, sck, host, port);

	int opt = 1;
	socklen_t optlen = sizeof opt;


/*#ifdef BSDCODEZ
	errno = 0;
	if (setsockopt(sck, SOL_SOCKET, SO_NOSIGPIPE, &opt, optlen) != 0) {
		W("(%p) setsockopt failed", hnd);
		close(sck);
		return false;
	}
	WVX("(%p) done setsockopt()", hnd);
#endif*/

	errno = 0;
	if (fcntl(sck, F_SETFL, O_NONBLOCK) == -1) {
		W("(%p) failed to enable nonblocking mode", hnd);
		close(sck);
		return false;
	}
	
	WVX("(%p) set to nonblocking mode, calling connect() now", hnd);
	errno = 0;
	int r = connect(sck, &sa, addrlen);

	if (r == -1 && (errno != EINPROGRESS)) {
		W("(%p) connect() failed", hnd);
		close(sck);
		return false;
	}

	struct timeval tout;
	tout.tv_sec = 0;
	tout.tv_usec = 0;
	int64_t trem = 0;

	for(;;) {
		pthread_mutex_lock(&hnd->cancelmtx);
		bool cnc = hnd->cancel;
		pthread_mutex_unlock(&hnd->cancelmtx);
		if (cnc) {
			WX("(%p) cancel requested", hnd);
			close(sck);
			return false;
		}
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sck, &fds);

		if (tsend)
		{
			trem = tsend - ic_timestamp_us();
			if (trem > 500000)
				trem = 500000; //limiting time spent in read so we can cancel w/o much delay
			if (trem <= 0) {
				WX("(%p) timeout reached while in 3WHS", hnd);
				close(sck);
				return false;
			}

			ic_tconv(&tout, &trem, false);
		}

		errno = 0;
		r = select(sck+1, NULL, &fds, NULL, tsend ? &tout : NULL);
		if (r < 0)
		{
			W("(%p) select() failed", hnd);
			close(sck);
			return false;
		}
		if (r == 1) {
			break;
		}
	}

	if (getsockopt(sck, SOL_SOCKET, SO_ERROR, &opt, &optlen) != 0) {
		WX("(%p) getsockopt failed", hnd);
		close(sck);
		return false;
	}

	if (opt == 0)
		WVX("(%p) socket connected!", hnd);
	else {
		WX("(%p) could not connect socket (%d)", hnd, opt);
		close(sck);
		return false;
	}

	hnd->sck = sck; //must be set here for pxlogon

	if (hnd->ptype != -1) {
		if (tsend) {
			trem = tsend - ic_timestamp_us();
			if (trem <= 0) {
				WX("(%p) timeout *after* select() was done, odd", hnd);
				close(sck);
				hnd->sck = -1;
				return false;
			}
		}
		bool ok = false;
		WVX("(%p) logging on to proxy", hnd);
		if (hnd->ptype == IRCPX_HTTP)
			ok = pxlogon_http(hnd, (unsigned long)trem);
		else if (hnd->ptype == IRCPX_SOCKS4)
			ok = pxlogon_socks4(hnd, (unsigned long)trem);
		else if (hnd->ptype == IRCPX_SOCKS5)
			ok = pxlogon_socks5(hnd, (unsigned long)trem);
		
		if (!ok)
		{
			WX("(%p) proxy logon failed", hnd);
			close(sck);
			hnd->sck = -1;
			return false;
		}
		WVX("(%p) sent proxy logon sequence", hnd);
	}

	WVX("(%p) setting to blocking mode", hnd);
	errno = 0;
	if (fcntl(sck, F_SETFL, 0) == -1) {
		W("(%p) failed to clear nonblocking mode", hnd);
		close(sck);
		hnd->sck = -1;
		return false;
	}


	hnd->state = ON;

	WVX("(%p) %s connection to ircd established", hnd, hnd->ptype == -1?"TCP":"proxy");

	return true;
}

int
irccon_read(ichnd_t hnd, char **tok, size_t tok_len, unsigned long to_us)
{
	if (!hnd || hnd->state != ON)
		return -1;

	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	//WVX("(%p) wanna read (timeout: %lu)", hnd, to_us);

	int n;
	/* read a line, ignoring empty lines */
	do
	{
		int64_t trem;
		if (tsend)
		{
			trem = tsend - ic_timestamp_us();
			if (trem <= 0) {
				//WX("(%p) timeout hit", hnd);
				return 0;
			}
		}

		n = ircio_read(hnd->sck, hnd->linebuf, LINEBUF_SZ, hnd->overbuf, OVERBUF_SZ, &hnd->mehptr, tok, tok_len, tsend?(unsigned long)trem:0);
		if (n < 0) //read error
		{
			WX("(%p) read failed", hnd);
			irccon_reset(hnd);
			return -1;
		}
	} while(n == 0);

	size_t last = 2;
	for(; last < tok_len && tok[last]; last++);

	if (last > 2)
		hnd->colon_trail = tok[last-1][-1] == ':';

	//WVX("(%p) done reading", hnd);

	return 1;
}

bool
irccon_write(ichnd_t hnd, const char *line)
{
	if (!hnd || hnd->state != ON || !line)
		return false;


	if (ircio_write(hnd->sck, line) == -1)
	{
		WX("(%p) failed to write '%s'", hnd, line);
		irccon_reset(hnd);
		return false;
	}

	char buf[1024];
	strncpy(buf, line, sizeof buf);
	buf[sizeof buf - 1] = '\0';
	char *endptr = buf + strlen(buf) - 1;
	while(endptr >= buf && (*endptr == '\r' || *endptr == '\n'))
		*endptr-- = '\0';

	WVX("(%p) wrote a line: '%s'", hnd, buf);
	return true;

}

bool
irccon_online(ichnd_t hnd)
{
	if (!hnd || hnd->state == INV)
		return false;

	return hnd->state == ON;
}

bool
irccon_valid(ichnd_t hnd)
{
	if (!hnd || hnd->state == INV)
		return false;

	return true;
}

bool
irccon_colon_trail(ichnd_t hnd)
{
	if (!hnd || hnd->state != ON)
		return false;
	
	return hnd->colon_trail;
}

bool
irccon_set_proxy(ichnd_t hnd, const char *host, unsigned short port, int ptype)
{
	if (!hnd || hnd->state == INV)
		return false;

	XFREE(hnd->phost);
	hnd->phost = NULL;
	switch(ptype)
	{
		case IRCPX_HTTP:
		case IRCPX_SOCKS4:
		case IRCPX_SOCKS5:
			if (!host || !(1 <= port && port <= 65535))
				return false;

			hnd->phost = XSTRDUP(host);
			hnd->pport = port;
			hnd->ptype = ptype;
			break;
		default:
			hnd->ptype = -1;
	}

	return true;
}

bool
irccon_set_server(ichnd_t hnd, const char *host, unsigned short port)
{
	if (!hnd || hnd->state == INV)
		return false;

	XFREE(hnd->host);
	hnd->host = XSTRDUP(host?host:DEF_HOST);
	hnd->port = (1 <= port && port <= 65535)?port:DEF_PORT;
	return true;
}

const char*
irccon_get_proxy_host(ichnd_t hnd)
{
	if (!hnd || hnd->state == INV)
		return NULL;

	return hnd->phost;
}

unsigned short
irccon_get_proxy_port(ichnd_t hnd)
{
	if (!hnd || hnd->state == INV)
		return 0;

	return hnd->pport;
}

int
irccon_get_proxy_type(ichnd_t hnd)
{
	if (!hnd || hnd->state == INV)
		return 0;

	return hnd->ptype;
}

const char*
irccon_get_host(ichnd_t hnd)
{
	if (!hnd || hnd->state == INV)
		return NULL;

	return hnd->host;
}

unsigned short
irccon_get_port(ichnd_t hnd)
{
	if (!hnd || hnd->state == INV)
		return 0;

	return hnd->port;
}

static bool pxlogon_http(ichnd_t hnd, unsigned long to_us)
{
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	char buf[256];
	snprintf(buf, sizeof buf, "CONNECT %s:%d HTTP/1.0\r\nHost: %s:%d\r\n\r\n", hnd->host, hnd->port, hnd->host, hnd->port);

	errno = 0;
	ssize_t n = write(hnd->sck, buf, strlen(buf));
	if (n == -1) {
		W("(%p) write() failed", hnd);
		return false;
	} else if (n < (ssize_t)strlen(buf)) {
		WX("(%p) didn't send everything (%zd/%zu)", hnd, n, strlen(buf));
		return false;
	}

	memset(buf, 0, sizeof buf);

	WVX("(%p) wrote HTTP CONNECT, reading response", hnd);
	size_t c = 0;
	while(c < sizeof buf && (c < 4 || buf[c - 4] != '\r' || buf[c - 3] != '\n' || buf[c - 2] != '\r' || buf[c - 1] != '\n')) {
		errno = 0;
		n = read(hnd->sck, &buf[c], 1);
		if (n <= 0) {
			if (n == 0)
				WX("(%p) unexpected EOF", hnd);
			else if (errno == EAGAIN || errno == EWOULDBLOCK) {
				pthread_mutex_lock(&hnd->cancelmtx);
				bool cnc = hnd->cancel;
				pthread_mutex_unlock(&hnd->cancelmtx);
				if (cnc) {
					WX("(%p) pxlogon canceled", hnd);
					return false;
				}
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				WX("(%p) timeout hit", hnd);
			} else 
				W("(%p) read failed", hnd);
			return false;
		}
		c++;
	}
	char *ctx; //context ptr for strtok_r
	char *tok = strtok_r(buf, " ", &ctx);
	if (!tok) {
		WX("(%p) parse error 1 (buf: '%s')", hnd, buf);
		return false;
	}

	tok = strtok_r(NULL, " ", &ctx);
	if (!tok) {
		WX("(%p) parse error 2 (buf: '%s')", hnd, buf);
		return false;
	}

	WVX("(%p) http response: '%.3s' (should be '200')", hnd, tok);
	return strncmp(tok, "200", 3) == 0;
}

static bool pxlogon_socks4(ichnd_t hnd, unsigned long to_us) //SOCKS doesntsupport ipv6
{
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	unsigned char logon[14];
	uint16_t port = htons(hnd->port);
	uint32_t ip = inet_addr(hnd->host); //XXX FIXME this doesntwork if hnd->host is not an ipv4 addr but dns
	char name[6];
	for(size_t i = 0; i < sizeof name - 1; i++)
		name[i] = (char)(rand() % 26 + 'a');
	name[sizeof name - 1] = '\0';

	if (ip == INADDR_NONE || !(1 <= port && port <= 65535)) {
		WX("(%p) srsly what?", hnd);
		return false;
	}
	

	size_t c = 0;
	logon[c++] = 4;
	logon[c++] = 1;

	memcpy(logon+c, &port, 2); c += 2;
	memcpy(logon+c, &ip, 4); c += 4;

	memcpy(logon+c, (unsigned char*)name, strlen(name) + 1);
	c += strlen(name) + 1;
	
	errno = 0;
	ssize_t n = write(hnd->sck, logon, c);
	if (n == -1) {
		W("(%p) write() failed", hnd);
		return false;
	} else if (n < (ssize_t)c) {
		WX("(%p) didn't send everything (%zd/%zu)", hnd, n, c);
		return false;
	}
	
	WVX("(%p) wrote SOCKS4 logon sequence, reading response", hnd);
	char resp[8];
	c = 0;
	while(c < 8)
	{
		errno = 0;
		n = read(hnd->sck, &resp+c, 8-c);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				pthread_mutex_lock(&hnd->cancelmtx);
				bool cnc = hnd->cancel;
				pthread_mutex_unlock(&hnd->cancelmtx);
				if (cnc) {
					WX("(%p) pxlogon canceled", hnd);
					return false;
				}
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				WX("(%p) timeout hit", hnd);
			} else if (n == 0) {
				WX("(%p) unexpected EOF", hnd);
			} else 
				W("(%p) read failed", hnd);
			return false;
		}
		c += n;
	}
	WVX("(%p) socks4 response: %hhx %hhx (should be: 0x00 0x5a)", hnd, resp[0], resp[1]);
	return resp[0] == 0 && resp[1] == 0x5a;
}

static bool pxlogon_socks5(ichnd_t hnd, unsigned long to_us)
{
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	unsigned char logon[14];

	if(!(1 <= hnd->port && hnd->port <= 65535)) {
		WX("(%p) srsly what?!", hnd);
		return false;
	}

	uint16_t nport = htons(hnd->port);
	size_t c = 0;
	logon[c++] = 5;

	logon[c++] = 1;
	logon[c++] = 0;

	errno = 0;
	ssize_t n = write(hnd->sck, logon, c);
	if (n == -1) {
		W("(%p) write() failed", hnd);
		return false;
	} else if (n < (ssize_t)c) {
		WX("(%p) didn't send everything (%zd/%zu)", hnd, n, c);
		return false;
	}
	
	WVX("(%p) wrote SOCKS5 logon sequence 1, reading response", hnd);
	char resp[128];
	c = 0;
	while(c < 2)
	{
		errno = 0;
		n = read(hnd->sck, &resp+c, 2-c);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				pthread_mutex_lock(&hnd->cancelmtx);
				bool cnc = hnd->cancel;
				pthread_mutex_unlock(&hnd->cancelmtx);
				if (cnc) {
					WX("(%p) pxlogon canceled", hnd);
					return false;
				}
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				WX("(%p) timeout 1 hit", hnd);
			} else if (n == 0) {
				WX("(%p) unexpected EOF", hnd);
			} else 
				W("(%p) read failed", hnd);
			return false;
		}
		c += n;
	}

	if(resp[0] != 5) {
		WX("(%p) unexpected response %hhx %hhx (no socks5?)", hnd, resp[0], resp[1]);
		return false;
	}
	if (resp[1] != 0) {
		WX("(%p) socks5 denied (%hhx %hhx)", hnd, resp[0], resp[1]);
		return false;
	}
	WVX("(%p) socks5 let us in", hnd);
	
	c = 0;
	char connect[128];
	connect[c++] = 5;
	connect[c++] = 1;
	connect[c++] = 0;
	int type = guess_hosttype(hnd->host);
	struct in_addr ia4;
	struct in6_addr ia6;
	switch(type) {
	case HOST_IPV4:
		connect[c++] = 1;
		n = inet_pton(AF_INET, hnd->host, &ia4);
		if (n == -1) {
			W("(%p) inet_pton failed", hnd);
			return false;
		}
		if (n == 0) {
			WX("(%p) illegal ipv4 address: '%s'", hnd, hnd->host);
			return false;
		}
		memcpy(connect+c, &ia4.s_addr, 4); c +=4;
		break;
	case HOST_IPV6:
		connect[c++] = 4;
		n = inet_pton(AF_INET6, hnd->host, &ia6);
		if (n == -1) {
			W("(%p) inet_pton failed", hnd);
			return false;
		}
		if (n == 0) {
			WX("(%p) illegal ipv6 address: '%s'", hnd, hnd->host);
			return false;
		}
		memcpy(connect+c, &ia6.s6_addr, 16); c +=16;
		break;
	case HOST_DNS:
		connect[c++] = 3;
		connect[c++] = strlen(hnd->host);
		memcpy(connect+c, hnd->host, strlen(hnd->host)); c += strlen(hnd->host);
	}
	memcpy(connect+c, &nport, 2); c += 2;

	errno = 0;
	n = write(hnd->sck, connect, c);
	if (n == -1) {
		W("(%p) write() failed", hnd);
		return false;
	} else if (n < (ssize_t)c) {
		WX("(%p) didn't send everything (%zd/%zu)", hnd, n, c);
		return false;
	}
	WVX("(%p) wrote SOCKS5 logon sequence 2, reading response", hnd);
	
	size_t l = 4;
	c = 0;
	while(c < l)
	{
		errno = 0;
		n = read(hnd->sck, resp + c, l - c);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				pthread_mutex_lock(&hnd->cancelmtx);
				bool cnc = hnd->cancel;
				pthread_mutex_unlock(&hnd->cancelmtx);
				if (cnc) {
					WX("(%p) pxlogon canceled", hnd);
					return false;
				}
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				WX("(%p) timeout 2 hit", hnd);
			} else if (n == 0) {
				WX("(%p) unexpected EOF", hnd);
			} else 
				W("(%p) read failed", hnd);
			return false;
		}
		c += n;
	}

	if (resp[0] != 5 || resp[1] != 0) {
		WX("(%p) socks5 denied/failed (%hhx %hhx %hhx %hhx)", hnd, resp[0], resp[1],
		                                            resp[2], resp[3]);
		return false;
	}

	bool dns = false;
	switch(resp[3]) {
	case 1: //ipv4
		l = 4;
		break;
	case 4: //ipv6
		l = 16;
		break;
	case 3: //dns
		dns = true;
		l = 1; //length
		break;
	default:
		WX("(%p) socks returned illegal addresstype %d", hnd, resp[3]);
		return false;
	}

	c = 0;
	l += 2; //port
	while(c < l)
	{
		errno = 0;
		n = read(hnd->sck, resp + c, c ? l - c : 1); //read the veryfirst byte seperately
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				pthread_mutex_lock(&hnd->cancelmtx);
				bool cnc = hnd->cancel;
				pthread_mutex_unlock(&hnd->cancelmtx);
				if (cnc) {
					WX("(%p) pxlogon canceled", hnd);
					return false;
				}
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				WX("(%p) timeout 2 hit", hnd);
			} else if (n == 0) {
				WX("(%p) unexpected EOF", hnd);
			} else 
				W("(%p) read failed", hnd);
			return false;
		}
		if (!c && dns)
			l += resp[c];
		c += n;
	}

	/* not that we'd care about what we just have read but we want to make sure to
	 * read the correct amount of characters */
	
	WVX("(%p) socks5 success (apparently)", hnd);
	return true;
}

static int
guess_hosttype(const char *host) //dumb heuristic to tell apart domain name/ip4/ip6 addr XXX FIXME
{
	if (strchr(host, '['))
		return HOST_IPV6;
	int dc = 0;
	while(*host) {
		if (*host == '.')
			dc++;
		else if (!isdigit(*host))
			return HOST_DNS;
		host++;
	}
	return dc == 3 ? HOST_IPV4 : HOST_DNS;
}
