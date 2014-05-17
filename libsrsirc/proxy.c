/* proxy.c - Proxy subroutines implementation (socks4/socks5/http)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <arpa/inet.h>
#include <unistd.h>

#include <common.h>
#include <intlog.h>

#include "proxy.h"

#define HOST_IPV4 0
#define HOST_IPV6 1
#define HOST_DNS 2


static int guess_hosttype(const char *host);


bool
proxy_logon_http(int sck, const char *host, unsigned short port,
    unsigned long to_us)
{
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	char buf[256];
	snprintf(buf, sizeof buf, "CONNECT %s:%d HTTP/1.0\r\nHost: %s:%d"
	    "\r\n\r\n", host, port, host, port);

	errno = 0;
	ssize_t n = write(sck, buf, strlen(buf));
	if (n == -1) {
		WE("(%d,%s,%hu) write() failed", sck, host, port);
		return false;
	} else if (n < (ssize_t)strlen(buf)) {
		W("(%d,%s,%hu) didn't send everything (%zd/%zu)",
		    sck, host, port, n, strlen(buf));
		return false;
	}

	memset(buf, 0, sizeof buf);

	D("(%d,%s,%hu) wrote HTTP CONNECT, reading response",
	    sck, host, port);
	size_t c = 0;
	while(c < sizeof buf && (c < 4 || buf[c-4] != '\r' ||
	    buf[c-3] != '\n' || buf[c-2] != '\r' || buf[c-1] != '\n')) {
		errno = 0;
		n = read(sck, &buf[c], 1);
		if (n <= 0) {
			if (n == 0)
				W("(%d,%s,%hu) unexpected EOF",
				    sck, host, port);
			else if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				W("(%d,%s,%hu) timeout hit",
				    sck, host, port);
			} else
				WE("(%d,%s,%hu) read failed",
				    sck, host, port);
			return false;
		}
		c++;
	}
	char *ctx; //context ptr for strtok_r
	char *tok = strtok_r(buf, " ", &ctx);
	if (!tok) {
		W("(%d,%s,%hu) parse error 1 (buf: '%s')",
		    sck, host, port, buf);
		return false;
	}

	tok = strtok_r(NULL, " ", &ctx);
	if (!tok) {
		W("(%d,%s,%hu) parse error 2 (buf: '%s')",
		    sck, host, port, buf);
		return false;
	}

	D("(%d,%s,%hu) http response: '%.3s' (should be '200')",
	    sck, host, port, tok);
	return strncmp(tok, "200", 3) == 0;
}

/* SOCKS4 doesntsupport ipv6 */
bool
proxy_logon_socks4(int sck, const char *host, unsigned short port,
    unsigned long to_us)
{
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	unsigned char logon[14];
	uint16_t nport = htons(port);

	/*FIXME this doesntwork if host is not an ipv4 addr but dns*/
	uint32_t ip = inet_addr(host);
	char name[6];
	for(size_t i = 0; i < sizeof name - 1; i++)
		name[i] = (char)(rand() % 26 + 'a');
	name[sizeof name - 1] = '\0';

	if (ip == INADDR_NONE || !(1 <= nport && nport <= 65535)) {
		W("(%d,%s,%hu) srsly what?", sck, host, port);
		return false;
	}


	size_t c = 0;
	logon[c++] = 4;
	logon[c++] = 1;

	memcpy(logon+c, &nport, 2); c += 2;
	memcpy(logon+c, &ip, 4); c += 4;

	memcpy(logon+c, (unsigned char*)name, strlen(name) + 1);
	c += strlen(name) + 1;

	errno = 0;
	ssize_t n = write(sck, logon, c);
	if (n == -1) {
		WE("(%d,%s,%hu) write() failed", sck, host, port);
		return false;
	} else if (n < (ssize_t)c) {
		W("(%d,%s,%hu) didn't send everything (%zd/%zu)",
		    sck, host, port, n, c);
		return false;
	}

	D("(%d,%s,%hu) wrote SOCKS4 logon sequence, reading response",
	    sck, host, port);
	char resp[8];
	c = 0;
	while(c < 8) {
		errno = 0;
		n = read(sck, &resp+c, 8-c);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				W("(%d,%s,%hu) timeout hit",
				    sck, host, port);
			} else if (n == 0) {
				W("(%d,%s,%hu) unexpected EOF",
				    sck, host, port);
			} else
				WE("(%d,%s,%hu) read failed",
				    sck, host, port);
			return false;
		}
		c += n;
	}
	D("(%d,%s,%hu) socks4 response: %hhx %hhx (should be: 0x00 0x5a)",
	    sck, host, port, resp[0], resp[1]);
	return resp[0] == 0 && resp[1] == 0x5a;
}

bool
proxy_logon_socks5(int sck, const char *host, unsigned short port,
    unsigned long to_us)
{
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	unsigned char logon[14];

	if(!(1 <= port && port <= 65535)) {
		W("(%d,%s,%hu) srsly what?!", sck, host, port);
		return false;
	}

	uint16_t nport = htons(port);
	size_t c = 0;
	logon[c++] = 5;

	logon[c++] = 1;
	logon[c++] = 0;

	errno = 0;
	ssize_t n = write(sck, logon, c);
	if (n == -1) {
		WE("(%d,%s,%hu) write() failed", sck, host, port);
		return false;
	} else if (n < (ssize_t)c) {
		W("(%d,%s,%hu) didn't send everything (%zd/%zu)",
		    sck, host, port, n, c);
		return false;
	}

	D("(%d,%s,%hu) wrote SOCKS5 logon sequence 1, reading response",
	    sck, host, port);
	char resp[128];
	c = 0;
	while(c < 2) {
		errno = 0;
		n = read(sck, &resp+c, 2-c);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				W("(%d,%s,%hu) timeout 1 hit",
				    sck, host, port);
			} else if (n == 0) {
				W("(%d,%s,%hu) unexpected EOF",
				    sck, host, port);
			} else
				WE("(%d,%s,%hu) read failed",
				    sck, host, port);
			return false;
		}
		c += n;
	}

	if(resp[0] != 5) {
		W("(%d,%s,%hu) unexpected response %hhx %hhx (no socks5?)",
		    sck, host, port, resp[0], resp[1]);
		return false;
	}
	if (resp[1] != 0) {
		W("(%d,%s,%hu) socks5 denied (%hhx %hhx)",
		    sck, host, port, resp[0], resp[1]);
		return false;
	}
	D("(%d,%s,%hu) socks5 let us in", sck, host, port);

	c = 0;
	char connect[128];
	connect[c++] = 5;
	connect[c++] = 1;
	connect[c++] = 0;
	int type = guess_hosttype(host);
	struct in_addr ia4;
	struct in6_addr ia6;
	switch(type) {
	case HOST_IPV4:
		connect[c++] = 1;
		n = inet_pton(AF_INET, host, &ia4);
		if (n == -1) {
			WE("(%d,%s,%hu) inet_pton failed",
			    sck, host, port);
			return false;
		}
		if (n == 0) {
			W("(%d,%s,%hu) illegal ipv4 addr: '%s'",
			    sck, host, port, host);
			return false;
		}
		memcpy(connect+c, &ia4.s_addr, 4); c +=4;
		break;
	case HOST_IPV6:
		connect[c++] = 4;
		n = inet_pton(AF_INET6, host, &ia6);
		if (n == -1) {
			WE("(%d,%s,%hu) inet_pton failed",
			    sck, host, port);
			return false;
		}
		if (n == 0) {
			W("(%d,%s,%hu) illegal ipv6 addr: '%s'",
			    sck, host, port, host);
			return false;
		}
		memcpy(connect+c, &ia6.s6_addr, 16); c +=16;
		break;
	case HOST_DNS:
		connect[c++] = 3;
		connect[c++] = strlen(host);
		memcpy(connect+c, host, strlen(host));
		c += strlen(host);;
	}
	memcpy(connect+c, &nport, 2); c += 2;

	errno = 0;
	n = write(sck, connect, c);
	if (n == -1) {
		WE("(%d,%s,%hu) write() failed",
		    sck, host, port);
		return false;
	} else if (n < (ssize_t)c) {
		W("(%d,%s,%hu) didn't send everything (%zd/%zu)",
		    sck, host, port, n, c);
		return false;
	}
	D("(%d,%s,%hu) wrote SOCKS5 logon sequence 2, reading response",
	    sck, host, port);

	size_t l = 4;
	c = 0;
	while(c < l) {
		errno = 0;
		n = read(sck, resp + c, l - c);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				W("(%d,%s,%hu) timeout 2 hit",
				    sck, host, port);
			} else if (n == 0) {
				W("(%d,%s,%hu) unexpected EOF",
				    sck, host, port);
			} else
				WE("(%d,%s,%hu) read failed",
				    sck, host, port);
			return false;
		}
		c += n;
	}

	if (resp[0] != 5 || resp[1] != 0) {
		W("(%d,%s,%hu) socks5 denied/failed (%hhx %hhx %hhx %hhx)",
		    sck, host, port, resp[0], resp[1], resp[2], resp[3]);
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
		W("(%d,%s,%hu) socks returned illegal addrtype %d",
		    sck, host, port, resp[3]);
		return false;
	}

	c = 0;
	l += 2; //port
	while(c < l) {
		errno = 0;
		/* read the very first byte seperately */
		n = read(sck, resp + c, c ? l - c : 1);
		if (n <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (tsend && ic_timestamp_us() < tsend) {
					usleep(10000);
					continue;
				}
				W("(%d,%s,%hu) timeout 2 hit",
				    sck, host, port);
			} else if (n == 0) {
				W("(%d,%s,%hu) unexpected EOF",
				    sck, host, port);
			} else
				WE("(%d,%s,%hu) read failed",
				    sck, host, port);
			return false;
		}
		if (!c && dns)
			l += resp[c];
		c += n;
	}

	/* not that we'd care about what we just have read but we want to
	 * make sure to read the correct amount of characters */

	D("(%d,%s,%hu) socks5 success (apparently)", sck, host, port);
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
