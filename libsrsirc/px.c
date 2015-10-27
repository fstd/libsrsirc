/* px.c - Proxy subroutines implementation (socks4/socks5/http)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_PROXY

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "px.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <platform/base_misc.h>
#include <platform/base_net.h>
#include <platform/base_string.h>
#include <platform/base_time.h>

#include <logger/intlog.h>

#include "common.h"

#include <libsrsirc/defs.h>


#define HOST_IPV4 0
#define HOST_IPV6 1
#define HOST_DNS 2


#define DBGSPEC "(%d,%s,%"PRIu16")"


bool
lsi_px_logon_http(int sck, const char *host, uint16_t port, uint64_t to_us)
{
	uint64_t tend = to_us ? lsi_b_tstamp_us() + to_us : 0;
	uint64_t tnow, trem = 0;
	char buf[256];
	snprintf(buf, sizeof buf, "CONNECT %s:%d HTTP/1.0\r\nHost: %s:%d"
	    "\r\n\r\n", host, port, host, port);

	errno = 0;
	long n = lsi_b_write(sck, buf, strlen(buf));
	if (n <= -1) {
		WE(DBGSPEC" write() failed", sck, host, port);
		return false;
	} else if ((size_t)n < strlen(buf)) {
		W(DBGSPEC" didn't send everything (%ld/%zu)",
		    sck, host, port, n, strlen(buf));
		return false;
	}

	memset(buf, 0, sizeof buf);

	D(DBGSPEC" wrote HTTP CONNECT, reading response",
	    sck, host, port);
	size_t c = 0;
	while (c < sizeof buf && (c < 4 || buf[c-4] != '\r' ||
	    buf[c-3] != '\n' || buf[c-2] != '\r' || buf[c-1] != '\n')) {
		errno = 0;
		if (tend) {
			tnow = lsi_b_tstamp_us();
			trem = tnow >= tend ? 1 : tend - tnow;
		}
		n = lsi_b_read(sck, &buf[c], 1, trem);
		if (n <= 0) {
			if (n == 0)
				W(DBGSPEC" timeout hit", sck, host, port);
			else if (n == -2)
				W(DBGSPEC" unexpected EOF", sck, host, port);
			else WE(DBGSPEC" read failed",
				    sck, host, port);
			return false;
		}
		c++;
	}

	char *sp = strchr(buf, ' ');
	if (!sp) {
		W(DBGSPEC" parse error 1 (buf: '%s')", sck, host, port, buf);
		return false;
	}

	D(DBGSPEC" http response: '%.3s' (should be '200')",
	    sck, host, port, sp+1);
	return strncmp(sp+1, "200", 3) == 0;
}

/* SOCKS4 doesntsupport ipv6 */
bool
lsi_px_logon_socks4(int sck, const char *host, uint16_t port, uint64_t to_us)
{
	uint64_t tend = to_us ? lsi_b_tstamp_us() + to_us : 0;
	uint64_t tnow, trem = 0;
	unsigned char logon[14];
	uint16_t nport = lsi_b_htons(port);

	/*FIXME this doesntwork if host is not an ipv4 addr but dns*/
	uint32_t ip = lsi_b_inet_addr(host);
	char name[6];
	for (size_t i = 0; i < sizeof name - 1; i++)
		name[i] = rand() % 26 + 'a';
	name[sizeof name - 1] = '\0';

	size_t c = 0;
	logon[c++] = 4;
	logon[c++] = 1;

	memcpy(logon+c, &nport, 2); c += 2;
	memcpy(logon+c, &ip, 4); c += 4;

	memcpy(logon+c, name, strlen(name) + 1);
	c += strlen(name) + 1;

	errno = 0;
	long n = lsi_b_write(sck, logon, c);
	if (n <= -1) {
		WE(DBGSPEC" write() failed", sck, host, port);
		return false;
	} else if ((size_t)n < c) {
		W(DBGSPEC" didn't send everything (%ld/%zu)",
		    sck, host, port, n, c);
		return false;
	}

	D(DBGSPEC" wrote SOCKS4 logon sequence, reading response",
	    sck, host, port);
	char resp[8];
	c = 0;
	while (c < 8) {
		if (tend) {
			tnow = lsi_b_tstamp_us();
			trem = tnow >= tend ? 1 : tend - tnow;
		}
		errno = 0;
		n = lsi_b_read(sck, &resp+c, 8-c, trem);
		if (n <= 0) {
			if (n == 0)
				W(DBGSPEC" timeout hit", sck, host, port);
			else if (n == -2)
			      W(DBGSPEC" unexpected EOF", sck, host, port);
			else
				WE(DBGSPEC" read failed", sck, host, port);
			return false;
		}
		c += n;
	}
	D(DBGSPEC" socks4 response: %"PRIu8" %"PRIu8" (should be: 0x00 0x5a)",
	    sck, host, port, resp[0], resp[1]);
	return resp[0] == 0 && resp[1] == 0x5a;
}

bool
lsi_px_logon_socks5(int sck, const char *host, uint16_t port, uint64_t to_us)
{
	// This is horrible.  sanitize!
	uint64_t tend = to_us ? lsi_b_tstamp_us() + to_us : 0;
	uint64_t tnow, trem = 0;
	unsigned char logon[14];

	if (!port) {
		W(DBGSPEC" srsly what?!", sck, host, port);
		return false;
	}

	uint16_t nport = lsi_b_htons(port);
	size_t c = 0;
	logon[c++] = 5;

	logon[c++] = 1;
	logon[c++] = 0;

	errno = 0;
	long n = lsi_b_write(sck, logon, c);
	if (n <= -1) {
		WE(DBGSPEC" write() failed", sck, host, port);
		return false;
	} else if ((size_t)n < c) {
		W(DBGSPEC" didn't send everything (%ld/%zu)",
		    sck, host, port, n, c);
		return false;
	}

	D(DBGSPEC" wrote SOCKS5 logon sequence 1, reading response",
	    sck, host, port);
	char resp[128];
	c = 0;
	while (c < 2) {
		errno = 0;
		if (tend) {
			tnow = lsi_b_tstamp_us();
			trem = tnow >= tend ? 1 : tend - tnow;
		}
		n = lsi_b_read(sck, &resp+c, 2-c, trem);
		if (n <= 0) {
			if (n == 0)
				W(DBGSPEC" timeout 1 hit", sck, host, port);
			else if (n == -2)
				W(DBGSPEC" unexpected EOF", sck, host, port);
			else
				WE(DBGSPEC" read failed", sck, host, port);
			return false;
		}
		c += n;
	}

	if (resp[0] != 5) {
		W(DBGSPEC" unexpected response %"PRIu8" %"PRIu8" (no socks5?)",
		    sck, host, port, resp[0], resp[1]);
		return false;
	}
	if (resp[1] != 0) {
		W(DBGSPEC" socks5 denied (%"PRIu8" %"PRIu8")",
		    sck, host, port, resp[0], resp[1]);
		return false;
	}
	D(DBGSPEC" socks5 let us in", sck, host, port);

	c = 0;
	unsigned char conbuf[128];
	conbuf[c++] = 5;
	conbuf[c++] = 1;
	conbuf[c++] = 0;
	int type = lsi_guess_hosttype(host);
	switch (type) {
	case HOST_IPV4:
		conbuf[c++] = 1;
		if (!lsi_b_inet4_addr(&conbuf[c], 4, host))
			return false;
		c += 16;
		break;
	case HOST_IPV6:
		conbuf[c++] = 4;
		if (!lsi_b_inet6_addr(&conbuf[c], 16, host))
			return false;
		c += 16;
		break;
	case HOST_DNS:
		conbuf[c++] = 3;
		conbuf[c++] = (uint8_t)strlen(host);
		memcpy(conbuf+c, host, strlen(host));
		c += strlen(host);
	}
	memcpy(conbuf+c, &nport, 2); c += 2;

	errno = 0;
	n = lsi_b_write(sck, conbuf, c);
	if (n <= -1) {
		WE(DBGSPEC" write() failed",
		    sck, host, port);
		return false;
	} else if ((size_t)n < c) {
		W(DBGSPEC" didn't send everything (%ld/%zu)",
		    sck, host, port, n, c);
		return false;
	}
	D(DBGSPEC" wrote SOCKS5 logon sequence 2, reading response",
	    sck, host, port);

	size_t l = 4;
	c = 0;
	while (c < l) {
		errno = 0;
		if (tend) {
			tnow = lsi_b_tstamp_us();
			trem = tnow >= tend ? 1 : tend - tnow;
		}
		n = lsi_b_read(sck, resp + c, l - c, trem);
		if (n <= 0) {
			if (n == 0)
				W(DBGSPEC" timeout 2 hit", sck, host, port);
			else if (n == -2)
				W(DBGSPEC" unexpected EOF", sck, host, port);
			else
				WE(DBGSPEC" read failed", sck, host, port);
			return false;
		}
		c += n;
	}

	if (resp[0] != 5 || resp[1] != 0) {
		W(DBGSPEC" socks5 deny/err %"PRIu8" %"PRIu8" %"PRIu8" %"PRIu8"",
		    sck, host, port, resp[0], resp[1], resp[2], resp[3]);
		return false;
	}

	bool dns = false;
	switch (resp[3]) {
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
		W(DBGSPEC" socks returned illegal addrtype %d",
		    sck, host, port, resp[3]);
		return false;
	}

	c = 0;
	l += 2; //port
	while (c < l) {
		errno = 0;
		/* read the very first byte seperately */
		if (tend) {
			tnow = lsi_b_tstamp_us();
			trem = tnow >= tend ? 1 : tend - tnow;
		}
		n = lsi_b_read(sck, resp + c, c ? l - c : 1, trem);
		if (n <= 0) {
			if (n == 0)
				W(DBGSPEC" timeout 2 hit", sck, host, port);
			else if (n == 0)
				W(DBGSPEC" unexpected EOF", sck, host, port);
			else
				WE(DBGSPEC" read failed", sck, host, port);
			return false;
		}
		if (!c && dns)
			l += (uint8_t)resp[c];
		c += n;
	}

	/* not that we'd care about what we just have read but we want to
	 * make sure to read the correct amount of characters */

	D(DBGSPEC" socks5 success (apparently)", sck, host, port);
	return true;
}

int
lsi_px_typenum(const char *typestr)
{
	return (lsi_b_strcasecmp(typestr, "socks4") == 0) ? IRCPX_SOCKS4 :
	       (lsi_b_strcasecmp(typestr, "socks5") == 0) ? IRCPX_SOCKS5 :
	       (lsi_b_strcasecmp(typestr, "http") == 0) ? IRCPX_HTTP : -1;
}

const char *
lsi_px_typestr(int typenum)
{
	return (typenum == IRCPX_HTTP) ? "HTTP" :
	       (typenum == IRCPX_SOCKS4) ? "SOCKS4" :
	       (typenum == IRCPX_SOCKS5) ? "SOCKS5" : "unknown";
}
