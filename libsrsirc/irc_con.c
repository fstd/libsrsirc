/* irc_io.c - Semi back-end (btw irc_io and irc_basic) implementation
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#ifdef WITH_SSL
/* ssl */
# include <openssl/ssl.h>
# include <openssl/err.h>
#endif

#include <intlog.h>

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


#ifdef WITH_SSL
static bool s_sslinit;
#endif

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
	bool colon_trail;
#ifdef WITH_SSL
	bool ssl;
	SSL_CTX *sctx;
	SSL *shnd;
#endif
};

static bool pxlogon_http(ichnd_t hnd, unsigned long to_us);
static bool pxlogon_socks4(ichnd_t hnd, unsigned long to_us);
static bool pxlogon_socks5(ichnd_t hnd, unsigned long to_us);
static int guess_hosttype(const char *host);

ichnd_t
irccon_init(void)
{
	ichnd_t r = NULL;
	int preverrno = errno;
	errno = 0;

	/* no XMALLOC here because at this point we might as well
	 * check and error instead of terminating */
	if (!(r = malloc(sizeof *r)))
		goto irccon_init_fail;

	r->linebuf = NULL;
	r->overbuf = NULL;
	r->host = NULL;

	if ((!(r->linebuf = malloc(LINEBUF_SZ)))
	    || (!(r->overbuf = malloc(OVERBUF_SZ)))
	    || (!(r->host = strdup(DEF_HOST))))
		goto irccon_init_fail;

	/* these 2 neccessary? */
	memset(r->linebuf, 0, LINEBUF_SZ);
	memset(r->overbuf, 0, OVERBUF_SZ);

	errno = preverrno;
	r->port = DEF_PORT;
	r->phost = NULL;
	r->pport = 0;
	r->ptype = -1;
	r->sck = -1;
	r->state = OFF;
	r->mehptr = NULL;
	r->colon_trail = false;
#ifdef WITH_SSL
	r->ssl = false;
	r->shnd = NULL;
	r->sctx = NULL;
#endif

	D("(%p) irc_con initialized", r);

	return r;

irccon_init_fail:
	EE("failed to initialize irc_con handle");
	if (r) {
		free(r->host);
		free(r->overbuf);
		free(r->linebuf);
		free(r);
	}

	return NULL;
}

bool
irccon_reset(ichnd_t hnd)
{
	if (!hnd || hnd->state == INV)
		return false;

	D("(%p) resetting", hnd);

#ifdef WITH_SSL
	if (hnd->shnd) {
		D("(%p) shutting down ssl", hnd);
		SSL_shutdown(hnd->shnd);
		SSL_free(hnd->shnd);
		hnd->shnd = NULL;
	}
#endif

	if (hnd->sck != -1) {
		D("(%p) closing socket %d", hnd, hnd->sck);
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

#ifdef WITH_SSL
	irccon_set_ssl(hnd, false); //dispose ssl context if existing
#endif

	XFREE(hnd->host);
	XFREE(hnd->phost);
	XFREE(hnd->linebuf);
	XFREE(hnd->overbuf);
	hnd->state = INV;

	D("(%p) disposed", hnd);
	XFREE(hnd);

	return true;
}

bool
irccon_connect(ichnd_t hnd,
    unsigned long softto_us, unsigned long hardto_us)
{
	if (!hnd || hnd->state != OFF)
		return false;

	int64_t tsend = hardto_us ? ic_timestamp_us() + hardto_us : 0;

	char *host = hnd->ptype != -1 ? hnd->phost : hnd->host;
	unsigned short port = hnd->ptype != -1 ? hnd->pport : hnd->port;

	{
		char pxspec[64];
		pxspec[0] = '\0';
		if (hnd->ptype != -1)
			snprintf(pxspec, sizeof pxspec, " via %s:%s:%hu",
			    pxtypestr(hnd->ptype), hnd->phost, hnd->pport);

		D("(%p) wanna connect to %s:%hu%s, sto: %luus, hto: %luus",
		    hnd, hnd->host, hnd->port, pxspec,
		    softto_us, hardto_us);
	}

	struct sockaddr sa;
	size_t addrlen;

	int sck = ic_consocket(host, port, &sa, &addrlen,
	    softto_us, hardto_us);

	if (sck < 0) {
		W("(%p) ic_consocket failed for %s:%hu", hnd, host, port);
		return false;
	}

	D("(%p) connected socket %d for %s:%hu", hnd, sck, host, port);

	hnd->sck = sck; //must be set here for pxlogon

	int64_t trem = 0;
	if (hnd->ptype != -1) {
		if (tsend) {
			trem = tsend - ic_timestamp_us();
			if (trem <= 0) {
				W("(%p) timeout", hnd);
				close(sck);
				hnd->sck = -1;
				return false;
			}
		}

		bool ok = false;
		D("(%p) logging on to proxy", hnd);
		if (hnd->ptype == IRCPX_HTTP)
			ok = pxlogon_http(hnd, (unsigned long)trem);
		else if (hnd->ptype == IRCPX_SOCKS4)
			ok = pxlogon_socks4(hnd, (unsigned long)trem);
		else if (hnd->ptype == IRCPX_SOCKS5)
			ok = pxlogon_socks5(hnd, (unsigned long)trem);

		if (!ok) {
			W("(%p) proxy logon failed", hnd);
			close(sck);
			hnd->sck = -1;
			return false;
		}
		D("(%p) sent proxy logon sequence", hnd);
	}

	D("(%p) setting to blocking mode", hnd);
	errno = 0;
	if (fcntl(sck, F_SETFL, 0) == -1) {
		WE("(%p) failed to clear nonblocking mode", hnd);
		close(sck);
		hnd->sck = -1;
		return false;
	}
#ifdef WITH_SSL
	if (hnd->ssl) {
		bool fail = false;
		fail = !(hnd->shnd = SSL_new(hnd->sctx));
		fail = fail || !SSL_set_fd(hnd->shnd, sck);
		int r = SSL_connect(hnd->shnd);
		D("ssl_connect: %d", r);
		if (r != 1) {
			int rr = SSL_get_error(hnd->shnd, r);
			D("rr: %d", rr);
		}
		fail = fail || (r != 1);
		if (fail) {
			ERR_print_errors_fp(stderr);
			if (hnd->shnd) {
				SSL_free(hnd->shnd);
				hnd->shnd = NULL;
			}

			close(sck);
			W("connect bailing out; couldn't initiate ssl");
			return false;
		}
	}
#endif

	hnd->state = ON;

	D("(%p) %s connection to ircd established",
	    hnd, hnd->ptype == -1?"TCP":"proxy");

	return true;
}

int
irccon_read(ichnd_t hnd, char **tok, size_t tok_len, unsigned long to_us)
{
	if (!hnd || hnd->state != ON)
		return -1;

	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	D("(%p) wanna read (timeout: %lu)", hnd, to_us);

	int n;
	/* read a line, ignoring empty lines */
	do {
		int64_t trem;
		if (tsend) {
			trem = tsend - ic_timestamp_us();
			if (trem <= 0) {
				V("(%p) timeout hit", hnd);
				return 0;
			}
		}
		n = ircio_read_ex(hnd->sck,
#ifdef WITH_SSL
		    hnd->shnd,
#endif
		    hnd->linebuf, LINEBUF_SZ, hnd->overbuf, OVERBUF_SZ,
		    &hnd->mehptr, tok, tok_len,
		    tsend ? (unsigned long)trem : 0ul);

		if (n < 0) { //read error
			W("(%p) read failed", hnd);
			irccon_reset(hnd);
			return -1;
		}
	} while(n == 0);

	size_t last = 2;
	for(; last < tok_len && tok[last]; last++);

	if (last > 2)
		hnd->colon_trail = tok[last-1][-1] == ':';

	D("(%p) done reading", hnd);

	return 1;
}

bool
irccon_write(ichnd_t hnd, const char *line)
{
	if (!hnd || hnd->state != ON || !line)
		return false;


	if (ircio_write_ex(hnd->sck,
#ifdef WITH_SSL
	    hnd->shnd,
#endif
	    line) == -1) {
		W("(%p) failed to write '%s'", hnd, line);
		irccon_reset(hnd);
		return false;
	}

	char buf[1024];
	strncpy(buf, line, sizeof buf);
	buf[sizeof buf - 1] = '\0';
	char *endptr = buf + strlen(buf) - 1;
	while(endptr >= buf && (*endptr == '\r' || *endptr == '\n'))
		*endptr-- = '\0';

	D("(%p) wrote a line: '%s'", hnd, buf);
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
irccon_set_proxy(ichnd_t hnd, const char *host, unsigned short port,
    int ptype)
{
	if (!hnd || hnd->state == INV)
		return false;

	char *n = NULL;
	switch(ptype) {
	case IRCPX_HTTP:
	case IRCPX_SOCKS4:
	case IRCPX_SOCKS5:
		if (!host || !(1 <= port && port <= 65535))
			return false;

		if (!(n = strdup(host))) {
			EE("strdup failed");
			return false;
		}
		hnd->pport = port;
		hnd->ptype = ptype;
		XFREE(hnd->phost);
		hnd->phost = n;
		break;
	default:
		E("illegal proxy type %d", ptype);
		return false;
	}

	return true;
}

bool
irccon_set_server(ichnd_t hnd, const char *host, unsigned short port)
{
	if (!hnd || hnd->state == INV)
		return false;

	char *n;
	if (!(n = strdup(host?host:DEF_HOST))) {
		EE("strdup failed");
		return false;
	}
	XFREE(hnd->host);
	hnd->host = n;
	hnd->port = (1 <= port && port <= 65535)?port:DEF_PORT;
	return true;
}

#ifdef WITH_SSL
bool
irccon_set_ssl(ichnd_t hnd, bool on)
{
	if (!hnd || hnd->state == INV)
		return false;

	if (!s_sslinit && on) {
		SSL_load_error_strings();
		SSL_library_init();
		s_sslinit = true;
	}

	if (on && !hnd->sctx) {
		if (!(hnd->sctx = SSL_CTX_new(SSLv23_client_method()))) {
			W("SSL_CTX_new failed, ssl not enabled!");
			return false;
		}
	} else if (!on && hnd->sctx) {
		SSL_CTX_free(hnd->sctx);
	}

	hnd->ssl = on;

	return true;
}
#endif

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

#ifdef WITH_SSL
bool
irccon_get_ssl(ichnd_t hnd)
{
	if (!hnd || hnd->state == INV)
		return false;

	return hnd->ssl;
}
#endif

int
irccon_sockfd(ichnd_t hnd)
{
	if (!hnd || hnd->state != ON)
		return -1;

	return hnd->sck;
}

static bool pxlogon_http(ichnd_t hnd, unsigned long to_us)
{
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	char buf[256];
	snprintf(buf, sizeof buf, "CONNECT %s:%d HTTP/1.0\r\nHost: %s:%d"
	    "\r\n\r\n", hnd->host, hnd->port, hnd->host, hnd->port);

	errno = 0;
	ssize_t n = write(hnd->sck, buf, strlen(buf));
	if (n == -1) {
		WE("(%p) write() failed", hnd);
		return false;
	} else if (n < (ssize_t)strlen(buf)) {
		W("(%p) didn't send everything (%zd/%zu)",
		    hnd, n, strlen(buf));
		return false;
	}

	memset(buf, 0, sizeof buf);

	D("(%p) wrote HTTP CONNECT, reading response", hnd);
	size_t c = 0;
	while(c < sizeof buf && (c < 4 || buf[c-4] != '\r' ||
	    buf[c-3] != '\n' || buf[c-2] != '\r' || buf[c-1] != '\n')) {
		errno = 0;
		n = read(hnd->sck, &buf[c], 1);
		if (n <= 0) {
			if (n == 0)
				W("(%p) unexpected EOF", hnd);
			else if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				W("(%p) timeout hit", hnd);
			} else
				WE("(%p) read failed", hnd);
			return false;
		}
		c++;
	}
	char *ctx; //context ptr for strtok_r
	char *tok = strtok_r(buf, " ", &ctx);
	if (!tok) {
		W("(%p) parse error 1 (buf: '%s')", hnd, buf);
		return false;
	}

	tok = strtok_r(NULL, " ", &ctx);
	if (!tok) {
		W("(%p) parse error 2 (buf: '%s')", hnd, buf);
		return false;
	}

	D("(%p) http response: '%.3s' (should be '200')", hnd, tok);
	return strncmp(tok, "200", 3) == 0;
}

/* SOCKS4 doesntsupport ipv6 */
static bool pxlogon_socks4(ichnd_t hnd, unsigned long to_us)
{
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	unsigned char logon[14];
	uint16_t port = htons(hnd->port);

	/*FIXME this doesntwork if hnd->host is not an ipv4 addr but dns*/
	uint32_t ip = inet_addr(hnd->host);
	char name[6];
	for(size_t i = 0; i < sizeof name - 1; i++)
		name[i] = (char)(rand() % 26 + 'a');
	name[sizeof name - 1] = '\0';

	if (ip == INADDR_NONE || !(1 <= port && port <= 65535)) {
		W("(%p) srsly what?", hnd);
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
		WE("(%p) write() failed", hnd);
		return false;
	} else if (n < (ssize_t)c) {
		W("(%p) didn't send everything (%zd/%zu)", hnd, n, c);
		return false;
	}

	D("(%p) wrote SOCKS4 logon sequence, reading response", hnd);
	char resp[8];
	c = 0;
	while(c < 8) {
		errno = 0;
		n = read(hnd->sck, &resp+c, 8-c);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				W("(%p) timeout hit", hnd);
			} else if (n == 0) {
				W("(%p) unexpected EOF", hnd);
			} else
				WE("(%p) read failed", hnd);
			return false;
		}
		c += n;
	}
	D("(%p) socks4 response: %hhx %hhx (should be: 0x00 0x5a)",
	    hnd, resp[0], resp[1]);
	return resp[0] == 0 && resp[1] == 0x5a;
}

static bool pxlogon_socks5(ichnd_t hnd, unsigned long to_us)
{
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	unsigned char logon[14];

	if(!(1 <= hnd->port && hnd->port <= 65535)) {
		W("(%p) srsly what?!", hnd);
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
		WE("(%p) write() failed", hnd);
		return false;
	} else if (n < (ssize_t)c) {
		W("(%p) didn't send everything (%zd/%zu)", hnd, n, c);
		return false;
	}

	D("(%p) wrote SOCKS5 logon sequence 1, reading response", hnd);
	char resp[128];
	c = 0;
	while(c < 2) {
		errno = 0;
		n = read(hnd->sck, &resp+c, 2-c);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				W("(%p) timeout 1 hit", hnd);
			} else if (n == 0) {
				W("(%p) unexpected EOF", hnd);
			} else
				WE("(%p) read failed", hnd);
			return false;
		}
		c += n;
	}

	if(resp[0] != 5) {
		W("(%p) unexpected response %hhx %hhx (no socks5?)",
		    hnd, resp[0], resp[1]);
		return false;
	}
	if (resp[1] != 0) {
		W("(%p) socks5 denied (%hhx %hhx)", hnd, resp[0], resp[1]);
		return false;
	}
	D("(%p) socks5 let us in", hnd);

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
			WE("(%p) inet_pton failed", hnd);
			return false;
		}
		if (n == 0) {
			W("(%p) illegal ipv4 addr: '%s'", hnd, hnd->host);
			return false;
		}
		memcpy(connect+c, &ia4.s_addr, 4); c +=4;
		break;
	case HOST_IPV6:
		connect[c++] = 4;
		n = inet_pton(AF_INET6, hnd->host, &ia6);
		if (n == -1) {
			WE("(%p) inet_pton failed", hnd);
			return false;
		}
		if (n == 0) {
			W("(%p) illegal ipv6 addr: '%s'", hnd, hnd->host);
			return false;
		}
		memcpy(connect+c, &ia6.s6_addr, 16); c +=16;
		break;
	case HOST_DNS:
		connect[c++] = 3;
		connect[c++] = strlen(hnd->host);
		memcpy(connect+c, hnd->host, strlen(hnd->host));
		c += strlen(hnd->host);
	}
	memcpy(connect+c, &nport, 2); c += 2;

	errno = 0;
	n = write(hnd->sck, connect, c);
	if (n == -1) {
		WE("(%p) write() failed", hnd);
		return false;
	} else if (n < (ssize_t)c) {
		W("(%p) didn't send everything (%zd/%zu)", hnd, n, c);
		return false;
	}
	D("(%p) wrote SOCKS5 logon sequence 2, reading response", hnd);

	size_t l = 4;
	c = 0;
	while(c < l) {
		errno = 0;
		n = read(hnd->sck, resp + c, l - c);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				W("(%p) timeout 2 hit", hnd);
			} else if (n == 0) {
				W("(%p) unexpected EOF", hnd);
			} else
				WE("(%p) read failed", hnd);
			return false;
		}
		c += n;
	}

	if (resp[0] != 5 || resp[1] != 0) {
		W("(%p) socks5 denied/failed (%hhx %hhx %hhx %hhx)",
		    hnd, resp[0], resp[1], resp[2], resp[3]);
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
		W("(%p) socks returned illegal addrtype %d", hnd, resp[3]);
		return false;
	}

	c = 0;
	l += 2; //port
	while(c < l) {
		errno = 0;
		/* read the very first byte seperately */
		n = read(hnd->sck, resp + c, c ? l - c : 1);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				W("(%p) timeout 2 hit", hnd);
			} else if (n == 0) {
				W("(%p) unexpected EOF", hnd);
			} else
				WE("(%p) read failed", hnd);
			return false;
		}
		if (!c && dns)
			l += resp[c];
		c += n;
	}

	/* not that we'd care about what we just have read but we want to
	 * make sure to read the correct amount of characters */

	D("(%p) socks5 success (apparently)", hnd);
	return true;
}

/*dumb heuristic to tell apart domain name/ip4/ip6 addr XXX FIXME */
static int
guess_hosttype(const char *host)
{
	if (strchr(host, '['))
		return HOST_IPV6;
	int dc = 0;
	while(*host) {
		if (*host == '.')
			dc++;
		else if (!isdigit((unsigned char)*host))
			return HOST_DNS;
		host++;
	}
	return dc == 3 ? HOST_IPV4 : HOST_DNS;
}
