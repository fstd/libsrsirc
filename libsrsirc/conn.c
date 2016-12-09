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


#include <platform/base_misc.h>
#include <platform/base_string.h>
#include <platform/base_time.h>

#include <logger/intlog.h>

#include "common.h"
#include "io.h"
#include "px.h"

#include <libsrsirc/util.h>


#define OFF 0
#define ON 1


iconn *
lsi_conn_init(void)
{
	iconn *r = NULL;
	int preverrno = errno;
	errno = 0;

	if (!(r = MALLOC(sizeof *r)))
		goto fail;

	r->host = NULL;

	if (!(r->host = STRDUP(DEF_HOST)))
		goto fail;

	errno = preverrno;
	r->rctx.wptr = r->rctx.eptr = r->rctx.workbuf;
	r->port = 0;
	r->phost = NULL;
	r->pport = 0;
	r->ptype = -1;
	r->online = false;
	r->eof = false;
	r->colon_trail = false;
	r->ssl = false;
	r->sh.shnd = NULL;
	r->sh.sck = -1;
	r->sctx = NULL;

	D("Connection context initialized (%p)", (void *)r);

	return r;

fail:
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
	D("resetting");

	if (ctx->ssl && ctx->sh.shnd) {
		D("shutting down ssl");
		lsi_b_sslfin(ctx->sh.shnd);
		ctx->sh.shnd = NULL;
	}

	if (ctx->sh.sck != -1) {
		D("closing socket %d", ctx->sh.sck);
		lsi_b_close(ctx->sh.sck);
	}

	ctx->sh.sck = -1;
	ctx->online = false;
	ctx->rctx.wptr = ctx->rctx.eptr = ctx->rctx.workbuf;
	return;
}

void
lsi_conn_dispose(iconn *ctx)
{
	lsi_conn_reset(ctx);

	lsi_conn_set_ssl(ctx, false); //dispose ssl context if existing

	free(ctx->host);
	free(ctx->phost);

	D("disposed");
	free(ctx);
	return;
}

bool
lsi_conn_connect(iconn *ctx, uint64_t softto_us, uint64_t hardto_us)
{
	if (ctx->online) {
		E("Can't connect when already online");
		return false;
	}

	uint64_t tend = hardto_us ? lsi_b_tstamp_us() + hardto_us : 0;

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

		I("wanna connect to %s:%"PRIu16"%s, "
		    "sto: %"PRIu64"us, hto: %"PRIu64"us",
		    ctx->host, realport, ps, softto_us, hardto_us);
	}

	char peerhost[256];
	uint16_t peerport;

	sckhld sh;
	sh.sck = lsi_com_consocket(host, port, peerhost, sizeof peerhost,
	    &peerport, softto_us, hardto_us);
	sh.shnd = NULL;

	if (sh.sck < 0) {
		W("lsi_com_consocket failed for %s:%"PRIu16"", host, port);
		return false;
	}

	D("connected socket %d for %s:%"PRIu16"", sh.sck, host, port);

	ctx->sh = sh; //must be set here for px_logon

	uint64_t trem = 0;
	if (ctx->ptype != -1) {
		if (lsi_com_check_timeout(tend, &trem)) {
			W("timeout");
			lsi_b_close(sh.sck);
			ctx->sh.sck = -1;
			return false;
		}

		if (!lsi_b_blocking(sh.sck, false)) {
			WE("failed to set nonblocking mode");
			lsi_b_close(sh.sck);
			ctx->sh.sck = -1;
			return false;
		}

		bool ok = false;
		D("logging on to proxy");
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
			W("proxy logon failed");
			lsi_b_close(sh.sck);
			ctx->sh.sck = -1;
			return false;
		}
		D("sent proxy logon sequence");

	}

	if (ctx->ssl) {
		D("setting to blocking mode for ssl connect");

		if (!lsi_b_blocking(sh.sck, true)) {
			WE("failed to set blocking mode");
			lsi_b_close(sh.sck);
			ctx->sh.sck = -1;
			return false;
		}

		if (!(ctx->sh.shnd = lsi_b_sslize(sh.sck, ctx->sctx))) {
			lsi_b_close(sh.sck);
			ctx->sh.sck = -1;
			W("connect bailing out; couldn't initiate ssl");
			return false;
		}

		D("setting to nonblocking mode after ssl connect");

		if (!lsi_b_blocking(sh.sck, false)) {
			WE("failed to clear blocking mode");
			lsi_b_close(sh.sck);
			ctx->sh.sck = -1;
			return false;
		}
	}

	ctx->online = true;

	D("%s connection to ircd established", ctx->ptype == -1?"TCP":"proxy");

	return true;
}

int
lsi_conn_read(iconn *ctx, tokarr *tok, char **tags, size_t *ntags,
    uint64_t to_us)
{
	if (!ctx->online) {
		E("Can't read while offline");
		return -1;
	}

	int n;
	if (!(n = lsi_io_read(ctx->sh, &ctx->rctx, tok,
	    tags, ntags, to_us)))
		return 0; /* timeout */

	if (n < 0) {
		W("lsi_io_read %s", n == -1 ? "failed":"EOF");
		lsi_conn_reset(ctx);
		ctx->eof = n == -2;
		return -1;
	}

	size_t last = 2;
	for (; last < COUNTOF(*tok) && (*tok)[last]; last++);

	if (last > 2)
		ctx->colon_trail = (*tok)[last-1][-1] == ':';

	D("got a msg ('%s', %zu args)", (*tok)[1], last);

	return 1;
}

bool
lsi_conn_write(iconn *ctx, const char *line)
{
	if (!ctx->online) {
		E("Can't write while offline");
		return false;
	}

	if (!lsi_io_write(ctx->sh, line)) {
		W("failed to write '%s'", line);
		lsi_conn_reset(ctx);
		ctx->eof = false;
		return false;
	}

	D("wrote: '%s'", line);
	return true;
}

bool
lsi_conn_online(iconn *ctx)
{
	return ctx->online;
}

bool
lsi_conn_eof(iconn *ctx)
{
	return ctx->eof;
}

bool
lsi_conn_colon_trail(iconn *ctx)
{
	return ctx->colon_trail;
}

bool
lsi_conn_set_px(iconn *ctx, const char *host, uint16_t port, int ptype)
{
	char *n = NULL;
	if (!host) {
		free(ctx->phost);
		ctx->phost = NULL;
		ctx->ptype = ptype;
		ctx->pport = 0;
		I("proxy disabled");
		return true;
	}

	switch (ptype) {
	case IRCPX_HTTP:
	case IRCPX_SOCKS4:
	case IRCPX_SOCKS5:
		if (!host || !port) { //XXX `most' default port per type?
			E("Missing %s for proxy", host ? "port" : "hostname");
			return false;
		}

		if (!(n = STRDUP(host)))
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
	if (!(n = STRDUP(host?host:DEF_HOST)))
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
	return ctx->sh.sck;
}

void
irc_conn_dump(iconn *ctx)
{
	N("--- connection context %p dump---", (void *)ctx);
	N("host: '%s'", ctx->host);
	N("port: %"PRIu16, ctx->port);
	N("phost: '%s'", ctx->phost);
	N("pport: %"PRIu16, ctx->pport);
	N("ptype: %d (%s)", ctx->ptype, ctx->ptype == -1 ? "NONE" : lsi_px_typestr(ctx->ptype));
	N("sh.sck: %d", ctx->sh.sck);
	N("sh.shnd: %p", (void *)ctx->sh.shnd);
	N("online: %d", ctx->online);
	N("eof: %d", ctx->eof);
	N("colon_trail: %d", ctx->colon_trail);
	N("ssl: %d", ctx->ssl);
	N("read buffer: %zu bytes in use", (size_t)(ctx->rctx.eptr - ctx->rctx.wptr));
	N("--- end of connection context dump ---");
	return;
}
