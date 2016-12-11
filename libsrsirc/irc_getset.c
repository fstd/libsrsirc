/* irc_getset.c - getters, setters, determiners
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_IRC

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <platform/base_misc.h>
#include <platform/base_string.h>

#include <logger/intlog.h>

#include "common.h"
#include "conn.h"
#include "msg.h"
#include "skmap.h"
#include "v3.h"


/* Determiners - read-only access to information we keep track of */

bool
irc_online(irc *ctx)
{
	return lsi_conn_online(ctx->con);
}

const char *
irc_mynick(irc *ctx)
{
	return ctx->mynick;
}

const char *
irc_myhost(irc *ctx)
{
	return ctx->myhost;
}

int
irc_casemap(irc *ctx)
{
	return ctx->casemap;
}

bool
irc_service(irc *ctx)
{
	return ctx->service;
}

const char *
irc_umodes(irc *ctx)
{
	return ctx->umodes;
}

const char *
irc_cmodes(irc *ctx)
{
	return ctx->cmodes;
}

const char *
irc_version(irc *ctx)
{
	return ctx->ver;
}

const char *
irc_lasterror(irc *ctx)
{
	return ctx->lasterr;
}

const char *
irc_banmsg(irc *ctx)
{
	return ctx->banmsg;
}

bool
irc_banned(irc *ctx)
{
	return ctx->banned;
}

size_t
irc_v3tags_cnt(irc *ctx)
{
	return ctx->v3ntags;
}

static size_t
decode_v3tag(char *dest, size_t destsz, const char *v3tag)
{
	size_t bc = 0;
	bool escnext = false;
	while (bc < destsz) {
		char c = *v3tag++;
		switch (c) {
		case '\\':// \\ -> backslash
			if (escnext)
				dest[bc++] = '\\';
			escnext = !escnext;
			continue;
		case 's': dest[bc++] = escnext?' ' :c; break;// \s -> ' '
		case 'r': dest[bc++] = escnext?'\r':c; break;// \r -> CR
		case 'n': dest[bc++] = escnext?'\n':c; break;// \n -> LF
		case ':': dest[bc++] = escnext?';' :c; break;// \: -> ; (!)
		default:  dest[bc++] = c;
		}
		escnext = false;
		if (!c)
			break;
	}

	if (bc >= destsz) {
		E("Tag buffer too small, result truncated");
		dest[destsz-1] = '\0';
	}

	return bc;
}

static void
mkv3tag(irc *ctx, size_t ind)
{
	decode_v3tag(ctx->v3tags_dec[ind], MAX_V3TAGLEN, ctx->v3tags_raw[ind]);
	ctx->v3tags[ind].key = ctx->v3tags_dec[ind];
	char *p = strchr(ctx->v3tags_dec[ind], '=');
	if (p) {
		*p = '\0';
		ctx->v3tags[ind].value = p+1;
	} else
		ctx->v3tags[ind].value = NULL;
}


bool
irc_v3tag_bykey(irc *ctx, const char *key, const char **value)
{
	for (size_t i = 0; i < ctx->v3ntags; i++) {
		if (!ctx->v3tags_dec[i][0])
			mkv3tag(ctx, i);

		if (lsi_b_strcasecmp(key, ctx->v3tags[i].key) == 0) {
			if (value)
				*value = ctx->v3tags[i].value;
			return true;
		}
	}
	return false;
}

bool
irc_v3tag(irc *ctx, size_t ind, const char **key, const char **value)
{
	if (ind >= ctx->v3ntags || !ctx->v3tags_raw[ind])
		return false;

	if (!ctx->v3tags_dec[ind][0])
		mkv3tag(ctx, ind);

	if (key)
		*key = ctx->v3tags[ind].key;
	if (value)
		*value = ctx->v3tags[ind].value;

	return true;
}

bool
irc_colon_trail(irc *ctx)
{
	return lsi_conn_colon_trail(ctx->con);
}

int
irc_sockfd(irc *ctx)
{
	return lsi_conn_sockfd(ctx->con);
}

/* ew.  returns pointer to array of 4 pointers to tokarr */
tokarr *(*
irc_logonconv(irc *ctx))[4]
{
	return &ctx->logonconv;
}

const char *
irc_005chanmodes(irc *ctx, size_t class) /* suck it, $(CXX) */
{
	if (class >= COUNTOF(ctx->m005chanmodes))
		return NULL;
	return ctx->m005chanmodes[class];
}

const char *
irc_005modepfx(irc *ctx, bool symbols)
{
	return ctx->m005modepfx[symbols];
}

const char *
irc_005attr(irc *ctx, const char *name)
{
	return lsi_skmap_get(ctx->m005attrs, name);
}

bool
irc_tracking_enab(irc *ctx)
{
	return ctx->tracking && ctx->tracking_enab;
}

/* Getters - retrieve values previously set by the Setters */

const char *
irc_get_host(irc *ctx)
{
	return lsi_conn_get_host(ctx->con);
}

uint16_t
irc_get_port(irc *ctx)
{
	return lsi_conn_get_port(ctx->con);
}

const char *
irc_get_px_host(irc *ctx)
{
	return lsi_conn_get_px_host(ctx->con);
}

uint16_t
irc_get_px_port(irc *ctx)
{
	return lsi_conn_get_px_port(ctx->con);
}

int
irc_get_px_type(irc *ctx)
{
	return lsi_conn_get_px_type(ctx->con);
}

const char *
irc_get_pass(irc *ctx)
{
	return ctx->pass;
}

const char *
irc_get_uname(irc *ctx)
{
	return ctx->uname;
}

const char *
irc_get_fname(irc *ctx)
{
	return ctx->fname;
}

uint8_t
irc_get_conflags(irc *ctx)
{
	return ctx->conflags;
}

const char *
irc_get_nick(irc *ctx)
{
	return ctx->nick;
}

bool
irc_get_service_connect(irc *ctx)
{
	return ctx->serv_con;
}

const char *
irc_get_service_dist(irc *ctx)
{
	return ctx->serv_dist;
}

long
irc_get_service_type(irc *ctx)
{
	return ctx->serv_type;
}

const char *
irc_get_service_info(irc *ctx)
{
	return ctx->serv_info;
}

bool
irc_get_ssl(irc *ctx)
{
	return lsi_conn_get_ssl(ctx->con);
}

bool
irc_get_dumb(irc *ctx)
{
	return ctx->dumb;
}


/* Setters - set library parameters (none of these takes effect before the
 * next call to irc_connect() is done */


bool
irc_set_server(irc *ctx, const char *host, uint16_t port)
{
	return lsi_conn_set_server(ctx->con, host, port);
}

bool
irc_set_pass(irc *ctx, const char *srvpass)
{
	return lsi_com_update_strprop(&ctx->pass, srvpass);
}

bool
irc_set_uname(irc *ctx, const char *uname)
{
	return lsi_com_update_strprop(&ctx->uname, uname);
}

bool
irc_set_fname(irc *ctx, const char *fname)
{
	return lsi_com_update_strprop(&ctx->fname, fname);
}

bool
irc_set_starttls(irc *ctx, bool on, bool musthave)
{
	lsi_v3_clear_cap(ctx, "tls");
	return on && lsi_v3_want_cap(ctx, "tls", musthave);
}

bool
irc_set_sasl(irc *ctx, const char *mech, const void *msg, size_t n,
    bool musthave)
{
	if (!lsi_com_update_strprop(&ctx->sasl_mech, mech))
		return false;

	lsi_v3_clear_cap(ctx, "sasl");
	free(ctx->sasl_msg);
	if (!(ctx->sasl_msg = MALLOC(n))) {
		free(ctx->sasl_mech);
		ctx->sasl_mech = NULL;
		return false;
	}

	memcpy(ctx->sasl_msg, msg, n);
	ctx->sasl_msg_len = n;
	lsi_v3_want_cap(ctx, "sasl", musthave);

	return true;
}

bool
irc_set_nick(irc *ctx, const char *nick)
{
	return lsi_com_update_strprop(&ctx->nick, nick);
}

bool
irc_set_px(irc *ctx, const char *host, uint16_t port, int ptype)
{
	return lsi_conn_set_px(ctx->con, host, port, ptype);
}

void
irc_set_conflags(irc *ctx, uint8_t flags)
{
	ctx->conflags = flags;
	return;
}

void
irc_set_service_connect(irc *ctx, bool enabled)
{
	ctx->serv_con = enabled;
	return;
}

bool
irc_set_service_dist(irc *ctx, const char *dist)
{
	return lsi_com_update_strprop(&ctx->serv_dist, dist);
}

bool
irc_set_service_type(irc *ctx, long type)
{
	ctx->serv_type = type;
	return true;
}

bool
irc_set_service_info(irc *ctx, const char *info)
{
	return lsi_com_update_strprop(&ctx->serv_info, info);
}

void
irc_set_track(irc *ctx, bool on)
{
	ctx->tracking = on;
	return;
}

void
irc_set_connect_timeout(irc *ctx, uint64_t soft, uint64_t hard)
{
	ctx->hcto_us = hard;
	ctx->scto_us = soft;
	return;
}

bool
irc_set_ssl(irc *ctx, bool on)
{
	return lsi_conn_set_ssl(ctx->con, on);
}

void
irc_set_dumb(irc *ctx, bool dumbmode)
{
	ctx->dumb = dumbmode;
	return;
}
