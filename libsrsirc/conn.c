/* conn.c - IRC connection handling
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


iconn *
lsi_conn_init(void)
{
	iconn *r = NULL;
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
lsi_conn_reset(iconn *ctx)
{
	D("(%p) resetting", (void *)ctx);

	if (ctx->ssl && ctx->sh.shnd) {
		D("(%p) shutting down ssl", (void *)ctx);
		lsi_b_sslfin(ctx->sh.shnd);
		ctx->sh.shnd = NULL;
	}

	if (ctx->sh.sck != -1) {
		D("(%p) closing socket %d", (void *)ctx, ctx->sh.sck);
		lsi_b_close(ctx->sh.sck);
	}

	ctx->sh.sck = -1;
	ctx->state = OFF;
	ctx->rctx.wptr = ctx->rctx.eptr = ctx->rctx.workbuf;
}

void
lsi_conn_dispose(iconn *ctx)
{
	lsi_conn_reset(ctx);

	lsi_conn_set_ssl(ctx, false); //dispose ssl context if existing

	free(ctx->host);
	free(ctx->phost);
	ctx->state = INV;

	D("(%p) disposed", (void *)ctx);
	free(ctx);
}

bool
lsi_conn_connect(iconn *ctx, uint64_t softto_us, uint64_t hardto_us)
{
	if (!ctx || ctx->state != OFF)
		return false;

	uint64_t tsend = hardto_us ? lsi_b_tstamp_us() + hardto_us : 0;

	uint16_t realport = ctx->port;
	if (!realport)
		realport = ctx->ssl ? DEF_PORT_SSL : DEF_PORT_PLAIN;

	char *host = ctx->ptype != -1 ? ctx->phost : ctx->host;
	uint16_t port = ctx->ptype != -1 ? ctx->pport : realport;

	{
		char ps[64];
		ps[0] = '\0';
		if (ctx->ptype != -1)
			snprintf(ps, sizeof ps, " via %s:%s:%" PRIu16,
			    lsi_px_typestr(ctx->ptype), ctx->phost, ctx->pport);

		I("(%p) wanna connect to %s:%"PRIu16"%s, "
		    "sto: %"PRIu64"us, hto: %"PRIu64"us",
		    (void *)ctx, ctx->host, realport, ps, softto_us, hardto_us);
	}

	char peerhost[256];
	uint16_t peerport;

	sckhld sh;
	sh.sck = lsi_com_consocket(host, port, peerhost, sizeof peerhost,
	    &peerport, softto_us, hardto_us);
	sh.shnd = NULL;

	if (sh.sck < 0) {
		W("(%p) lsi_com_consocket failed for %s:%"PRIu16"",
		    (void *)ctx, host, port);
		return false;
	}

	D("(%p) connected socket %d for %s:%"PRIu16"",
	    (void *)ctx, sh.sck, host, port);

	ctx->sh = sh; //must be set here for px_logon

	uint64_t trem = 0;
	if (ctx->ptype != -1) {
		if (lsi_com_check_timeout(tsend, &trem)) {
			W("(%p) timeout", (void *)ctx);
			lsi_b_close(sh.sck);
			ctx->sh.sck = -1;
			return false;
		}

		if (!lsi_b_blocking(sh.sck, false)) {
			WE("(%p) failed to set nonblocking mode", (void *)ctx);
			lsi_b_close(sh.sck);
			ctx->sh.sck = -1;
			return false;
		}

		bool ok = false;
		D("(%p) logging on to proxy", (void *)ctx);
		if (ctx->ptype == IRCPX_HTTP)
			ok = lsi_px_logon_http(ctx->sh.sck, ctx->host,
			    realport, trem);
		else if (ctx->ptype == IRCPX_SOCKS4)
			ok = lsi_px_logon_socks4(ctx->sh.sck, ctx->host,
			    realport, trem);
		else if (ctx->ptype == IRCPX_SOCKS5)
			ok = lsi_px_logon_socks5(ctx->sh.sck, ctx->host,
			    realport, trem);

		if (!ok) {
			W("(%p) proxy logon failed", (void *)ctx);
			lsi_b_close(sh.sck);
			ctx->sh.sck = -1;
			return false;
		}
		D("(%p) sent proxy logon sequence", (void *)ctx);

		D("(%p) setting to blocking mode", (void *)ctx);

		if (!lsi_b_blocking(sh.sck, true)) {
			WE("(%p) failed to set blocking mode", (void *)ctx);
			lsi_b_close(sh.sck);
			ctx->sh.sck = -1;
			return false;
		}
	}

	if (ctx->ssl && !(ctx->sh.shnd = lsi_b_sslize(sh.sck, ctx->sctx))) {
		lsi_b_close(sh.sck);
		ctx->sh.sck = -1;
		W("connect bailing out; couldn't initiate ssl");
		return false;
	}

	ctx->state = ON;

	D("(%p) %s connection to ircd established",
	    (void *)ctx, ctx->ptype == -1?"TCP":"proxy");

	return true;
}

int
lsi_conn_read(iconn *ctx, tokarr *tok, uint64_t to_us)
{
	if (!ctx || ctx->state != ON)
		return -1;

	int n;
	if (!(n = lsi_io_read(ctx->sh, &ctx->rctx, tok, to_us)))
		return 0; /* timeout */

	if (n < 0) {
		W("(%p) lsi_io_read %s", (void *)ctx, n == -1 ? "failed":"EOF");
		lsi_conn_reset(ctx);
		ctx->eof = n == -2;
		return -1;
	}

	size_t last = 2;
	for (; last < COUNTOF(*tok) && (*tok)[last]; last++);

	if (last > 2)
		ctx->colon_trail = (*tok)[last-1][-1] == ':';

	D("(%p) got a msg ('%s', %zu args)", (void *)ctx, (*tok)[1], last);

	return 1;
}

bool
lsi_conn_write(iconn *ctx, const char *line)
{
	if (!ctx || ctx->state != ON || !line)
		return false;


	if (!lsi_io_write(ctx->sh, line)) {
		W("(%p) failed to write '%s'", (void *)ctx, line);
		lsi_conn_reset(ctx);
		ctx->eof = false;
		return false;
	}

	D("(%p) wrote: '%s'", (void *)ctx, line);
	return true;

}

bool
lsi_conn_online(iconn *ctx)
{
	return ctx->state == ON;
}

bool
lsi_conn_eof(iconn *ctx)
{
	return ctx->eof;
}

bool
lsi_conn_colon_trail(iconn *ctx)
{
	if (!ctx || ctx->state != ON)
		return false;

	return ctx->colon_trail;
}

bool
lsi_conn_set_px(iconn *ctx, const char *host, uint16_t port, int ptype)
{
	char *n = NULL;
	switch (ptype) {
	case IRCPX_HTTP:
	case IRCPX_SOCKS4:
	case IRCPX_SOCKS5:
		if (!host || !port) //XXX `most' default port per type?
			return false;

		if (!(n = lsi_b_strdup(host)))
			return false;

		ctx->pport = port;
		ctx->ptype = ptype;
		free(ctx->phost);
		ctx->phost = n;
		I("set proxy to %s:%s:%"PRIu16,
		    lsi_px_typestr(ctx->ptype), n, port);
		break;
	default:
		E("illegal proxy type %d", ptype);
		return false;
	}

	return true;
}

bool
lsi_conn_set_server(iconn *ctx, const char *host, uint16_t port)
{
	char *n;
	if (!(n = lsi_b_strdup(host?host:DEF_HOST)))
		return false;

	free(ctx->host);
	ctx->host = n;
	ctx->port = port;
	I("set server to %s:%"PRIu16, n, port);
	return true;
}

bool
lsi_conn_set_ssl(iconn *ctx, bool on)
{
	if (on && !ctx->sctx) {
		if (!(ctx->sctx = lsi_b_mksslctx())) {
			E("could not create ssl context, ssl not enabled!");
			return false;
		}
	} else if (!on && ctx->sctx) {
		lsi_b_freesslctx(ctx->sctx);
		ctx->sctx = NULL;
	}

	if (ctx->ssl != on)
		N("ssl %sabled", on ? "en" : "dis");

	ctx->ssl = on;

	return true;
}

const char *
lsi_conn_get_px_host(iconn *ctx)
{
	return ctx->phost;
}

uint16_t
lsi_conn_get_px_port(iconn *ctx)
{
	return ctx->pport;
}

int
lsi_conn_get_px_type(iconn *ctx)
{
	return ctx->ptype;
}

const char *
lsi_conn_get_host(iconn *ctx)
{
	return ctx->host;
}

uint16_t
lsi_conn_get_port(iconn *ctx)
{
	return ctx->port;
}

bool
lsi_conn_get_ssl(iconn *ctx)
{
	return ctx->ssl;
}

int
lsi_conn_sockfd(iconn *ctx)
{
	if (!ctx || ctx->state != ON)
		return -1;

	return ctx->sh.sck;
}
