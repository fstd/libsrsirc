/* v3.c - IRCv3 protocol support
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-16, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_V3

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "v3.h"

#include <stdio.h>
#include <string.h>

#include <logger/intlog.h>
#include <platform/base_string.h>
#include <platform/base_misc.h>

#include "conn.h"
#include "msg.h"
#include "common.h"
#include "irc_msghnd.h"

static uint16_t handle_CAP(irc *ctx, tokarr *msg, size_t nargs, bool logon);
static uint16_t handle_CAP_ACK(irc *ctx, tokarr *msg, size_t nargs, bool logon);
static uint16_t handle_CAP_NAK(irc *ctx, tokarr *msg, size_t nargs, bool logon);
static uint16_t handle_CAP_LS(irc *ctx, tokarr *msg, size_t nargs, bool logon);

static uint16_t handle_AUTHENTICATE(irc *ctx, tokarr *msg, size_t nargs,
    bool logon);
static uint16_t handle_903(irc *ctx, tokarr *msg, size_t nargs, bool logon);
static uint16_t handle_670(irc *ctx, tokarr *msg, size_t nargs, bool logon);
static uint16_t handle_691(irc *ctx, tokarr *msg, size_t nargs, bool logon);
static uint16_t handle_saslerr(irc *ctx, tokarr *msg, size_t nargs, bool logon);
static struct v3cap *find_cap(irc *ctx, const char *cap);
static bool conclude_sasl_cap(irc *ctx);


bool
lsi_v3_check_caps(irc *ctx, bool offered)
{
	for (size_t i = 0; i < COUNTOF(ctx->v3caps); i++) {
		if (!ctx->v3caps[i])
			break;
		struct v3cap *cap = ctx->v3caps[i];
		if (cap->musthave &&
		    ((offered && !cap->offered) ||
		    (!offered && !cap->enabled))) {
			E("Must-have CAP '%s' not %s by server",
			    cap->name, offered?"offered":"enabled");
			return false;
		}
	}

	return true;
}

bool
lsi_v3_mk_capreq(irc *ctx, char *dest, size_t destsz)
{
	dest[0] = '\0';
	bool first = true;
	for (size_t i = 0; i < COUNTOF(ctx->v3caps); i++) {
		if (!ctx->v3caps[i])
			break;
		struct v3cap *cap = ctx->v3caps[i];
		if (cap->offered) {
			if (!first)
				lsi_b_strNcat(dest, " ", destsz);
			lsi_b_strNcat(dest, cap->name, destsz);
			first = false;
		}
	}

	return !first; // true if we have at least one cap to request
}

void
lsi_v3_update_cap(irc *ctx, const char *cap, const char *adddata,
    int offered, int enabled) //-1: don't upd
{
	for (size_t i = 0; i < COUNTOF(ctx->v3caps); i++) {
		if (!ctx->v3caps)
			break;

		if (strcmp(ctx->v3caps[i]->name, cap) == 0) {
			if (offered != -1)
				ctx->v3caps[i]->offered = offered;
			if (enabled != -1)
				ctx->v3caps[i]->enabled = enabled;
		}

		if (adddata)
			STRACPY(ctx->v3caps[i]->adddata, adddata);
		else
			ctx->v3caps[i]->adddata[0] = '\0';
	}
}

static uint16_t
handle_CAP_ACK(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (nargs < 5)
		return PROTO_ERR;

	/* yes, servers are this retarded */
	char *arg = (*msg)[4];
	int alen = strlen(arg) - 1;
	while (alen >= 0 && arg[alen] == ' ')
		arg[alen--] = '\0';
	
	if (strcmp(arg, ctx->v3capreq) != 0) {
		E("Wanted caps '%s' but server ACKed '%s'",
		    ctx->v3capreq, (*msg)[4]);
		/* this is an ircv3 protocol violation */
		return CAP_ERR;
	}

	lsi_v3_update_caps(ctx, arg, false);
	return 0;
}

static uint16_t
handle_CAP_NAK(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (nargs < 5)
		return PROTO_ERR;
	
	E("Server NAKed our caps '%s' ('%s')", ctx->v3capreq, (*msg)[4]);
	return CAP_ERR;
}

static uint16_t
handle_CAP_LS(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (!logon)
		return 0;
	
	if (nargs < 5)
		return PROTO_ERR;
	
	size_t arg = 4;
	if ((*msg)[arg][0] == '*')
		arg++;

	lsi_v3_update_caps(ctx, (*msg)[arg], true);

	return arg == 5 ? MORE_CAPS : 0;
}


static uint16_t
handle_CAP(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (nargs < 4)
		return PROTO_ERR;

	V("Handling a CAP");
	uint16_t r = 0;
	const char *subcmd = (*msg)[3];
	if (strcmp(subcmd, "LS") == 0) {
		r = handle_CAP_LS(ctx, msg, nargs, logon);
		if (r & CANT_PROCEED)
			return r;
		if (r & MORE_CAPS)
			return 0;
		if (!lsi_v3_check_caps(ctx, true))
			return CAP_ERR;
		char capreq[MAX_V3CAPLEN] = "CAP REQ :";
		size_t crlen = strlen(capreq);

		if (!lsi_v3_mk_capreq(ctx, capreq + crlen,
		    sizeof capreq - crlen))
			snprintf(capreq, sizeof capreq, "CAP END\r\n");

		/* XXX this does not really handle the case when more caps
		 * are requested than fit in a single message.  should
		 * perhaps send multiple CAP REQs. */
		STRACPY(ctx->v3capreq, capreq + crlen);
		if (!lsi_conn_write(ctx->con, capreq))
			return IO_ERR;
	} else if (strcmp(subcmd, "ACK") == 0) {
		r = handle_CAP_ACK(ctx, msg, nargs, logon);
		if (r & CANT_PROCEED)
			return r;
		/* if we want to STARTTLS, do that first */
		struct v3cap *c = find_cap(ctx, "tls");
		if (c && c->enabled) {
			if (!lsi_conn_write(ctx->con, "STARTTLS\r\n"))
				return IO_ERR;
			return 0;
			/* the sasl code is duplicated elsewhere... */
		}

		/* SASL is special in that it delays the CAP END until
		 * authentication is through, so no CAP END when we SASL... */
		return conclude_sasl_cap(ctx) ? 0 : IO_ERR;
	} else if (strcmp(subcmd, "NAK") == 0) {
		r = handle_CAP_NAK(ctx, msg, nargs, logon);
		if (r & CANT_PROCEED)
			return r;
	} else
		W("unrecognized CAP subcmd '%s'", subcmd);

	return 0;
}

bool
lsi_v3_want_caps(irc *ctx)
{
	for (size_t i = 0; i < COUNTOF(ctx->v3caps); i++)
		if (ctx->v3caps[i])
			return true;
	return false;
}

bool
lsi_v3_want_cap(irc *ctx, const char *cap, bool musthave)
{
	size_t i = 0;
	for (; i < COUNTOF(ctx->v3caps); i++)
		if (!ctx->v3caps[i] || strcmp(ctx->v3caps[i]->name, cap) == 0)
			break;

	if (i >= COUNTOF(ctx->v3caps)) {
		E("too many caps requested, raise MAX_V3CAPS");
		return false;
	}

	if (!ctx->v3caps[i]
	    && !(ctx->v3caps[i] = MALLOC(sizeof *ctx->v3caps[i])))
		return false;
	
	ctx->v3caps[i]->musthave = musthave;
	ctx->v3caps[i]->offered = false;
	ctx->v3caps[i]->enabled = false;
	ctx->v3caps[i]->adddata[0] = '\0';
	STRACPY(ctx->v3caps[i]->name, cap);

	return true;
}

void
lsi_v3_reset_caps(irc *ctx)
{
	for (size_t i = 0; i < COUNTOF(ctx->v3caps); i++)
		free(ctx->v3caps[i]), ctx->v3caps[i] = NULL;
}

void
lsi_v3_clear_cap(irc *ctx, const char *cap)
{
	size_t i = 0;
	for (; i < COUNTOF(ctx->v3caps); i++)
		if (!ctx->v3caps[i])
			break;
	size_t cnt = i;

	for (i = 0; i < COUNTOF(ctx->v3caps); i++) {
		if (!ctx->v3caps[i])
			return;

		if (strcmp(ctx->v3caps[i]->name, cap) == 0) {
			free(ctx->v3caps[i]);
			ctx->v3caps[i] = ctx->v3caps[cnt-1];
			ctx->v3caps[cnt-1] = NULL;
			break;
		}
	}
}

static uint16_t
handle_AUTHENTICATE(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (nargs < 3)
		return PROTO_ERR;

	if (ctx->dumb || !(ctx->sasl_mech && ctx->sasl_msg))
		return 0; // ignore

	if (strcmp((*msg)[2], "+") != 0)
		W("Unexpected arg in AUTHENTICATE '%s'", (*msg)[2]);

	char buf[512];
	snprintf(buf, sizeof buf, "AUTHENTICATE %s\r\n", ctx->sasl_msg);

	return lsi_conn_write(ctx->con, buf) ? 0 : IO_ERR;
}
static struct v3cap *
find_cap(irc *ctx, const char *cap)
{
	for (size_t i = 0; i < COUNTOF(ctx->v3caps); i++)
		if (ctx->v3caps[i] && strcmp(ctx->v3caps[i]->name, cap) == 0)
			return ctx->v3caps[i];
	return NULL;
}

/* capsline should contain at least one cap */
void
lsi_v3_update_caps(irc *ctx, const char *capsline, bool offered)
{
	char buf[MAX_V3CAPLINE];
	STRACPY(buf, capsline);

	char *cap = buf;
	char *next;
	do {
		next = lsi_com_next_tok(cap, ' ');
		char *eq = strchr(cap, '=');
		char *adddata = NULL;
		if (eq) {
			*eq = '\0';
			adddata = eq+1;
		}

		struct v3cap *c = find_cap(ctx, cap);
		if (c) {
			if (offered)
				c->offered = true;
			else
				c->enabled = true;

			if (adddata)
				STRACPY(c->adddata, adddata);
		}
		cap = next;
	} while (cap);
}

static bool
conclude_sasl_cap(irc *ctx)
{
	char buf[512];
	struct v3cap *c = find_cap(ctx, "sasl");
	if (c && c->enabled)
		snprintf(buf, sizeof buf, "AUTHENTICATE %s\r\n",
		    ctx->sasl_mech);
	else
		snprintf(buf, sizeof buf, "CAP END\r\n");

	return lsi_conn_write(ctx->con, buf);
}

bool
lsi_v3_regall(irc *ctx, bool dumb)
{
	bool fail = false;
	if (dumb)
		return true;

	fail = fail || !lsi_msg_reghnd(ctx, "670", handle_670, "v3");
	fail = fail || !lsi_msg_reghnd(ctx, "691", handle_691, "v3");
	fail = fail || !lsi_msg_reghnd(ctx, "903", handle_903, "v3");
	fail = fail || !lsi_msg_reghnd(ctx, "902", handle_saslerr, "v3");
	fail = fail || !lsi_msg_reghnd(ctx, "904", handle_saslerr, "v3");
	fail = fail || !lsi_msg_reghnd(ctx, "905", handle_saslerr, "v3");
	fail = fail || !lsi_msg_reghnd(ctx, "908", handle_saslerr, "v3");
	fail = fail || !lsi_msg_reghnd(ctx, "CAP", handle_CAP, "v3");
	fail = fail || !lsi_msg_reghnd(ctx, "AUTHENTICATE",
	    handle_AUTHENTICATE, "v3");

	return !fail;
}

void
lsi_v3_unregall(irc *ctx)
{
	lsi_msg_unregall(ctx, "v3");
	return;
}

/* SASL success */
static uint16_t
handle_903(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (!(ctx->sasl_mech && ctx->sasl_msg))
		return 0; //ignore

	V("Handling a 903");

	if (logon && !lsi_conn_write(ctx->con, "CAP END\r\n"))
		return IO_ERR;

	return SASL_COMPLETE;
}

/* 902 (locked), 904 (auth fail), 905 (too long), 908 (bad mech) */
static uint16_t
handle_saslerr(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (nargs < 2)
		return PROTO_ERR;

	if (!(ctx->sasl_mech && ctx->sasl_msg))
		return 0; //ignore

	V("Handling a %s", (*msg)[1]);
	W("SASL auth failed due to '%s'", (*msg)[1]);

	return SASL_ERR;
}

/* STARTTLS error */
static uint16_t
handle_691(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (nargs < 4)
		return PROTO_ERR;

	V("Handling a 691");
	E("STARTTLS failed: '%s'", (*msg)[3]);
	struct v3cap *c = find_cap(ctx, "tls");
	if (c && c->musthave)
		return IO_ERR;
	W("Continuing anyway...");

	if (ctx->starttls_first)
		return STARTTLS_OVER;

	return conclude_sasl_cap(ctx) ? 0 : IO_ERR;
}

/* go aheadst with thy STARTTLS */
static uint16_t
handle_670(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	D("setting to blocking mode for ssl connect");

	V("Handling a 670");

	struct sckhld *sh = &ctx->con->sh;

	if (!lsi_b_blocking(sh->sck, true)) {
		EE("failed to set blocking mode");
		return IO_ERR;
	}

	if (!(ctx->con->sctx = lsi_b_mksslctx())) {
		E("could not create ssl context, ssl not enabled!");
		return IO_ERR;
	}

	if (!(sh->shnd = lsi_b_sslize(sh->sck, ctx->con->sctx))) {
		E("connect bailing out; couldn't initiate ssl");
		return IO_ERR;
	}

	D("setting to nonblocking mode after ssl connect");

	if (!lsi_b_blocking(sh->sck, false)) {
		/* This would actually suck */
		EE("failed to clear blocking mode");
		return IO_ERR;
	}

	ctx->con->ssl = true;

	if (ctx->starttls_first)
		return STARTTLS_OVER;

	return conclude_sasl_cap(ctx) ? 0 : IO_ERR;
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
	W("decoding");
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

