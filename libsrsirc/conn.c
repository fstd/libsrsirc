/* conn.c - irc connection handling
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_ICONN

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "conn.h"


#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <platform/base_string.h>
#include <platform/base_time.h>

#include <logger/intlog.h>

#include "common.h"
#include "io.h"
#include "px.h"

#include <libsrsirc/util.h>


#define INV -1
#define OFF 0
#define ON 1


iconn
lsi_conn_init(void)
{ T("(no args)");
	iconn r = NULL;
	int preverrno = errno;
	errno = 0;

	if (!(r = lsi_com_malloc(sizeof *r)))
		goto conn_init_fail;

	r->host = NULL;

	if (!(r->host = lsi_b_strdup(DEF_HOST)))
		goto conn_init_fail;

	errno = preverrno;
	r->rctx.wptr = r->rctx.eptr = r->rctx.workbuf;
	r->port = 0;
	r->phost = NULL;
	r->pport = 0;
	r->ptype = -1;
	r->state = OFF;
	r->eof = false;
	r->colon_trail = false;
	r->ssl = false;
	r->sh.shnd = NULL;
	r->sh.sck = -1;
	r->sctx = NULL;

	D("(%p) iconn initialized", (void *)r);

	return r;

conn_init_fail:
	EE("failed to initialize iconn handle");
	if (r) {
		free(r->host);
		free(r);
	}

	return NULL;
}

void
lsi_conn_reset(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	D("(%p) resetting", (void *)hnd);

	if (hnd->ssl && hnd->sh.shnd) {
		D("(%p) shutting down ssl", (void *)hnd);
		lsi_b_sslfin(hnd->sh.shnd);
		hnd->sh.shnd = NULL;
	}

	if (hnd->sh.sck != -1) {
		D("(%p) closing socket %d", (void *)hnd, hnd->sh.sck);
		lsi_b_close(hnd->sh.sck);
	}

	hnd->sh.sck = -1;
	hnd->state = OFF;
	hnd->rctx.wptr = hnd->rctx.eptr = hnd->rctx.workbuf;
}

void
lsi_conn_dispose(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	lsi_conn_reset(hnd);

	lsi_conn_set_ssl(hnd, false); //dispose ssl context if existing

	free(hnd->host);
	free(hnd->phost);
	hnd->state = INV;

	D("(%p) disposed", (void *)hnd);
	free(hnd);
}

bool
lsi_conn_connect(iconn hnd, uint64_t softto_us, uint64_t hardto_us)
{ T("hnd=%p, softto_us=%"PRIu64", hardto_us=%"PRIu64, (void *)hnd, softto_us, hardto_us);
	if (!hnd || hnd->state != OFF)
		return false;

	uint64_t tsend = hardto_us ? lsi_b_tstamp_us() + hardto_us : 0;

	uint16_t realport = hnd->port;
	if (!realport)
		realport = hnd->ssl ? DEF_PORT_SSL : DEF_PORT_PLAIN;

	char *host = hnd->ptype != -1 ? hnd->phost : hnd->host;
	uint16_t port = hnd->ptype != -1 ? hnd->pport : realport;

	{
		char ps[64];
		ps[0] = '\0';
		if (hnd->ptype != -1)
			snprintf(ps, sizeof ps, " via %s:%s:%" PRIu16,
			    lsi_px_typestr(hnd->ptype), hnd->phost, hnd->pport);

		I("(%p) wanna connect to %s:%"PRIu16"%s, "
		    "sto: %"PRIu64"us, hto: %"PRIu64"us",
		    (void *)hnd, hnd->host, realport, ps, softto_us, hardto_us);
	}

	char peerhost[256];
	uint16_t peerport;

	sckhld sh;
	sh.sck = lsi_com_consocket(host, port, peerhost, sizeof peerhost,
	    &peerport, softto_us, hardto_us);
	sh.shnd = NULL;

	if (sh.sck < 0) {
		W("(%p) lsi_com_consocket failed for %s:%"PRIu16"",
		    (void *)hnd, host, port);
		return false;
	}

	D("(%p) connected socket %d for %s:%"PRIu16"",
	    (void *)hnd, sh.sck, host, port);

	hnd->sh = sh; //must be set here for px_logon

	uint64_t trem = 0;
	if (hnd->ptype != -1) {
		if (lsi_com_check_timeout(tsend, &trem)) {
			W("(%p) timeout", (void *)hnd);
			lsi_b_close(sh.sck);
			hnd->sh.sck = -1;
			return false;
		}

		if (!lsi_b_blocking(sh.sck, false)) {
			WE("(%p) failed to set nonblocking mode", (void *)hnd);
			lsi_b_close(sh.sck);
			hnd->sh.sck = -1;
			return false;
		}

		bool ok = false;
		D("(%p) logging on to proxy", (void *)hnd);
		if (hnd->ptype == IRCPX_HTTP)
			ok = lsi_px_logon_http(hnd->sh.sck, hnd->host,
			    realport, trem);
		else if (hnd->ptype == IRCPX_SOCKS4)
			ok = lsi_px_logon_socks4(hnd->sh.sck, hnd->host,
			    realport, trem);
		else if (hnd->ptype == IRCPX_SOCKS5)
			ok = lsi_px_logon_socks5(hnd->sh.sck, hnd->host,
			    realport, trem);

		if (!ok) {
			W("(%p) proxy logon failed", (void *)hnd);
			lsi_b_close(sh.sck);
			hnd->sh.sck = -1;
			return false;
		}
		D("(%p) sent proxy logon sequence", (void *)hnd);

		D("(%p) setting to blocking mode", (void *)hnd);

		if (!lsi_b_blocking(sh.sck, true)) {
			WE("(%p) failed to set blocking mode", (void *)hnd);
			lsi_b_close(sh.sck);
			hnd->sh.sck = -1;
			return false;
		}
	}

	if (hnd->ssl && !(hnd->sh.shnd = lsi_b_sslize(sh.sck, hnd->sctx))) {
		lsi_b_close(sh.sck);
		hnd->sh.sck = -1;
		W("connect bailing out; couldn't initiate ssl");
		return false;
	}

	hnd->state = ON;

	D("(%p) %s connection to ircd established",
	    (void *)hnd, hnd->ptype == -1?"TCP":"proxy");

	return true;
}

int
lsi_conn_read(iconn hnd, tokarr *tok, uint64_t to_us)
{ T("hnd=%p, tokarr=%p, to_us=%"PRIu64, (void *)hnd, (void *)tok, to_us);
	if (!hnd || hnd->state != ON)
		return -1;

	int n;
	if (!(n = lsi_io_read(hnd->sh, &hnd->rctx, tok, to_us)))
		return 0; /* timeout */

	if (n < 0) {
		W("(%p) lsi_io_read %s", (void *)hnd, n == -1 ? "failed":"EOF");
		lsi_conn_reset(hnd);
		hnd->eof = n == -2;
		return -1;
	}

	size_t last = 2;
	for (; last < COUNTOF(*tok) && (*tok)[last]; last++);

	if (last > 2)
		hnd->colon_trail = (*tok)[last-1][-1] == ':';

	D("(%p) got a msg ('%s', %zu args)", (void *)hnd, (*tok)[1], last);

	return 1;
}

bool
lsi_conn_write(iconn hnd, const char *line)
{ T("hnd=%p, line='%s'", (void *)hnd, line);
	if (!hnd || hnd->state != ON || !line)
		return false;


	if (!lsi_io_write(hnd->sh, line)) {
		W("(%p) failed to write '%s'", (void *)hnd, line);
		lsi_conn_reset(hnd);
		hnd->eof = false;
		return false;
	}

	D("(%p) wrote: '%s'", (void *)hnd, line);
	return true;

}

bool
lsi_conn_online(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	return hnd->state == ON;
}

bool
lsi_conn_eof(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	return hnd->eof;
}

bool
lsi_conn_colon_trail(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	if (!hnd || hnd->state != ON)
		return false;

	return hnd->colon_trail;
}

bool
lsi_conn_set_px(iconn hnd, const char *host, uint16_t port, int ptype)
{ T("hnd=%p, host='%s', port=%"PRIu16", ptype=%d", (void *)hnd, host, port, ptype);
	char *n = NULL;
	switch (ptype) {
	case IRCPX_HTTP:
	case IRCPX_SOCKS4:
	case IRCPX_SOCKS5:
		if (!host || !port) //XXX `most' default port per type?
			return false;

		if (!(n = lsi_b_strdup(host)))
			return false;

		hnd->pport = port;
		hnd->ptype = ptype;
		free(hnd->phost);
		hnd->phost = n;
		I("set proxy to %s:%s:%"PRIu16,
		    lsi_px_typestr(hnd->ptype), n, port);
		break;
	default:
		E("illegal proxy type %d", ptype);
		return false;
	}

	return true;
}

bool
lsi_conn_set_server(iconn hnd, const char *host, uint16_t port)
{ T("hnd=%p, host='%s', port=%"PRIu16, (void *)hnd, host, port);
	char *n;
	if (!(n = lsi_b_strdup(host?host:DEF_HOST)))
		return false;

	free(hnd->host);
	hnd->host = n;
	hnd->port = port;
	I("set server to %s:%"PRIu16, n, port);
	return true;
}

bool
lsi_conn_set_ssl(iconn hnd, bool on)
{ T("hnd=%p, on=%d", (void *)hnd, on);
	if (on && !hnd->sctx) {
		if (!(hnd->sctx = lsi_b_mksslctx())) {
			E("could not create ssl context, ssl not enabled!");
			return false;
		}
	} else if (!on && hnd->sctx) {
		lsi_b_freesslctx(hnd->sctx);
		hnd->sctx = NULL;
	}

	if (hnd->ssl != on)
		N("ssl %sabled", on ? "en" : "dis");

	hnd->ssl = on;

	return true;
}

const char *
lsi_conn_get_px_host(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	return hnd->phost;
}

uint16_t
lsi_conn_get_px_port(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	return hnd->pport;
}

int
lsi_conn_get_px_type(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	return hnd->ptype;
}

const char *
lsi_conn_get_host(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	return hnd->host;
}

uint16_t
lsi_conn_get_port(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	return hnd->port;
}

bool
lsi_conn_get_ssl(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	return hnd->ssl;
}

int
lsi_conn_sockfd(iconn hnd)
{ T("hnd=%p", (void *)hnd);
	if (!hnd || hnd->state != ON)
		return -1;

	return hnd->sh.sck;
}
