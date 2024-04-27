/* common.c - lib-internal IRC-unrelated common routines
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-2024, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_COMMON

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "common.h"


#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <platform/base_net.h>
#include <platform/base_string.h>
#include <platform/base_time.h>

#include <logger/intlog.h>

#include <libsrsirc/defs.h>


static int tryhost(struct addrlist *ai, const char *laddr, uint16_t lport,
    char *remaddr, size_t remaddr_sz, uint16_t *peerport, uint64_t to_us);

size_t
lsi_com_strCchr(const char *str, char c)
{
	size_t r = 0;
	while (*str)
		if (*str++ == c)
			r++;
	return r;
}


int
lsi_com_consocket(const char *host, uint16_t port, const char *laddr,
    uint16_t lport, char *remaddr, size_t remaddr_sz, uint16_t *peerport,
    uint64_t softto, uint64_t hardto)
{
	uint64_t hardtend = hardto ? lsi_b_tstamp_us() + hardto : 0;

	struct addrlist *alist;
	int count = lsi_b_mkaddrlist(host, port, &alist);
	if (count <= 0)
		return -1;

	if (softto && hardto && softto * count < hardto)
		softto = hardto / count;

	int sck = -1;
	for (struct addrlist *ai = alist; ai; ai = ai->next) {
		uint64_t trem = 0;

		if (lsi_com_check_timeout(hardtend, &trem)) {
			W("hard timeout");
			return false;
		}

		if (trem > softto)
			trem = softto;

		sck = tryhost(ai, laddr, lport, remaddr, remaddr_sz, peerport,
		              trem);

		if (sck != -1)
			break;
	}

	lsi_b_freeaddrlist(alist);

	return sck;
}

static int
tryhost(struct addrlist *ai, const char *laddr, uint16_t lport, char *remaddr,
    size_t remaddr_sz, uint16_t *peerport, uint64_t to_us)
{
	D("trying host '%s' ('%s')", ai->reqname, ai->addrstr);
	int sck = lsi_b_socket(ai->ipv6);

	if (sck == -1)
		return -1;

	if ((laddr || lport) && !lsi_b_bind(sck, laddr, lport, ai->ipv6))
		C("can't bind to %s:%"PRIu16, laddr?laddr:"(default)", lport);

	if (!lsi_b_blocking(sck, false))
		W("failed to set socket non-blocking, timeout will not work");

	int r = lsi_b_connect(sck, ai);
	if (r == 1)
		return sck;
	else if (r == -1)
		goto fail;

	r = lsi_b_select(&sck, 1, true, false, to_us);

	if (r == 1) {
		//if (!lsi_b_blocking(sck, true))
		//	W("failed to clear socket non-blocking mode");

		if (lsi_b_sock_ok(sck)) {
			if (remaddr && remaddr_sz)
				lsi_b_strNcpy(remaddr, ai->addrstr, remaddr_sz);
			if (peerport)
				*peerport = ai->port;

			return sck;
		} else
			W("could not connect socket");

	}

	/* fall-thru */
fail:
	if (sck != -1)
		lsi_b_close(sck);
	return -1;
}


bool
lsi_com_update_strprop(char **field, const char *val)
{
	char *n = NULL;
	if (val && !(n = STRDUP(val)))
		return false;

	free(*field);
	*field = n;

	return true;
}

bool
lsi_com_check_timeout(uint64_t tend, uint64_t *trem)
{
	if (!tend) {
		if (trem)
			*trem = 0;
		return false;
	}

	uint64_t now = lsi_b_tstamp_us();
	if (now >= tend) {
		if (trem)
			*trem = 0;
		return true;
	}

	if (trem)
		*trem = tend - now;

	return false;
}

/*dumb heuristic to tell apart domain name/ip4/ip6 addr XXX FIXME */
enum hosttypes
lsi_com_guess_hosttype(const char *host)
{
	if (strchr(host, '['))
		return HOSTTYPE_IPV6;
	int dc = 0;
	while (*host) {
		if (*host == '.')
			dc++;
		else if (!isdigit((unsigned char)*host))
			return HOSTTYPE_DNS;
		host++;
	}
	return dc == 3 ? HOSTTYPE_IPV4 : HOSTTYPE_DNS;
}

static const char *b64_alpha =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void
b64_core(char *outquad, const uint8_t *intrip, size_t ntrip)
{
	uint8_t t[3] = {0};
	size_t i;
	for (i = 0; i < ntrip; i++)
		t[i] = intrip[i];

	uint8_t q[4];
	size_t nquad = ntrip + 1;  /* dangerously relies on 1 <= ntrip <= 3 */

	q[0] = t[0] >> 2;
	q[1] = (t[0]&0x03) << 4 | (t[1]&0xf0) >> 4;
	q[2] = (t[1]&0x0f) << 2 | (t[2]&0xc0) >> 6;
	q[3] = t[2]&0x3f;

	for (i = 0; i < nquad; i++)
		outquad[i] = b64_alpha[q[i]];

	for (; i < 4; i++)
		outquad[i] = '=';
	return;
}

size_t
lsi_com_base64(char *dest, size_t destsz, const uint8_t *input, size_t len)
{
	size_t reslen = ((len + 2) / 3) * 4;
	if (reslen >= destsz)
		return 0; /* too big */

	const uint8_t *iptr = input;
	char *optr = dest;

	for (size_t i = 0; i < len; i += 3, iptr += 3, optr += 4)
		b64_core(optr, iptr, MIN(len - i, 3));

	return reslen;
}

/* \0-terminate the (to-be)-token `buf' points to, then locate the next token,
 * if any, and return pointer to it (or NULL) */
char *
lsi_com_next_tok(char *buf, char delim)
{
	while (*buf && *buf != delim) /* walk until end of (former) token */
		buf++;

	if (!*buf)
		return NULL; /* there's no next token */

	while (*buf == delim) /* walk over token delimiter, zero it out */
		*buf++ = '\0';

	if (!*buf)
		return NULL; /* trailing whitespace, but no next token */

	return buf; /* return pointer to beginning of the next token */
}

