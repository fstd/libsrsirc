/* irc_ext.c - core message handlers (uses msg.[ch])
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_IRC

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "irc_msghnd.h"


#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <platform/base_string.h>

#include <logger/intlog.h>

#include "common.h"
#include "conn.h"
#include "irc_track_int.h"
#include "msg.h"

#include <libsrsirc/defs.h>
#include <libsrsirc/util.h>


static uint16_t
handle_001(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (nargs < 3 || (!ctx->dumb && !logon))
		return PROTO_ERR;

	V("Handling a 001 (%s, '%s')", (*msg)[2], (*msg)[3]);

	lsi_ut_freearr(ctx->logonconv[0]);
	ctx->logonconv[0] = lsi_ut_clonearr(msg);

	/* 001 is the first (guaranteed) numeric we'll see, and numerics have
	 * our nickname as the first argument.  We use this to determine what
	 * our nickname ended up as. */
	STRACPY(ctx->mynick, (*msg)[2]);
	D("Our nickname is '%s', it's official!", ctx->mynick);

	return 0;
}

static uint16_t
handle_002(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (!ctx->dumb && !logon)
		return PROTO_ERR;

	V("Handling a 002");

	lsi_ut_freearr(ctx->logonconv[1]);
	ctx->logonconv[1] = lsi_ut_clonearr(msg);

	return 0;
}

static uint16_t
handle_003(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (!ctx->dumb && !logon)
		return PROTO_ERR;

	V("Handling a 003");

	lsi_ut_freearr(ctx->logonconv[2]);
	ctx->logonconv[2] = lsi_ut_clonearr(msg);

	return 0;
}

static uint16_t
handle_004(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (nargs < 7 || (!ctx->dumb && !logon))
		return PROTO_ERR;

	V("Handling a 004");

	lsi_ut_freearr(ctx->logonconv[3]);
	ctx->logonconv[3] = lsi_ut_clonearr(msg);

	STRACPY(ctx->myhost, (*msg)[3]);
	STRACPY(ctx->umodes, (*msg)[5]);
	STRACPY(ctx->cmodes, (*msg)[6]);
	STRACPY(ctx->ver, (*msg)[4]);
	D("004 seen! myhost: '%s', umodes: '%s', cmodes '%s', ver: '%s'",
	    ctx->myhost, ctx->umodes, ctx->cmodes, ctx->ver);

	return LOGON_COMPLETE;
}

static uint16_t
handle_PING(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	/* We only handle PINGs at logon. It's the user's job afterwards. */
	if (!logon)
		return 0;

	if (nargs < 3)
		return PROTO_ERR;

	char buf[256];
	snprintf(buf, sizeof buf, "PONG :%s\r\n", (*msg)[2]);
	D("Replying to a logon-time PING (%s)", (*msg)[2]);

	return lsi_conn_write(ctx->con, buf) ? 0 : IO_ERR;
}

/* This handles 432, 433, 436 and 437 all of which signal us that
 * we can't have the nickname we wanted */
static uint16_t
handle_bad_nick(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (!logon)
		return 0;

	W("Cannot take nickname '%s'", ctx->mynick);

	if (!ctx->cb_mut_nick)
		return OUT_OF_NICKS;

	ctx->cb_mut_nick(ctx->mynick, sizeof ctx->mynick);

	if (!ctx->mynick[0])
		return OUT_OF_NICKS;

	N("Mutilated nick to '%s'", ctx->mynick);

	char buf[MAX_NICK_LEN];
	snprintf(buf, sizeof buf, "NICK %s\r\n", ctx->mynick);
	return lsi_conn_write(ctx->con, buf) ? 0 : IO_ERR;
}

static uint16_t
handle_464(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (!logon)
		return PROTO_ERR;

	W("wrong server password");
	return AUTH_ERR;
}

/* Successful service logon.  I guess we don't get to see a 004, but haven't
 * really tried this yet */
static uint16_t
handle_383(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (nargs < 3 || (!ctx->dumb && !logon))
		return PROTO_ERR;

	STRACPY(ctx->mynick, (*msg)[2]);
	D("Our nickname is '%s', it's official!", ctx->mynick);

	STRACPY(ctx->myhost, (*msg)[0] ? (*msg)[0] : ctx->con->host);
	W("Obtained myhost from prefix '%s'", ctx->myhost);

	STRACPY(ctx->umodes, DEF_UMODES);
	STRACPY(ctx->cmodes, DEF_CMODES);
	ctx->ver[0] = '\0';
	ctx->service = true;
	D("got beloved 383");

	return LOGON_COMPLETE;
}

static uint16_t
handle_484(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	ctx->restricted = true;
	N("we're 'restricted'");
	return 0;
}

static uint16_t
handle_465(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	ctx->banned = true;
	free(ctx->banmsg);
	ctx->banmsg = STRDUP((*msg)[3] ? (*msg)[3] : "");

	W("we're banned (%s)", ctx->banmsg);

	return 0; /* well if we are, the server will sure disconnect us */
}

static uint16_t
handle_466(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	W("we will be banned");

	return 0; /* so what, bitch? */
}

static uint16_t
handle_ERROR(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	free(ctx->lasterr);
	ctx->lasterr = STRDUP((*msg)[2] ? (*msg)[2] : "");
	W("sever said ERROR: '%s'", ctx->lasterr);
	/* not strictly a case for CANT_PROCEED.  We certainly could
	 * proceed, it's the server that doesn't seem willing to */
	return 0;
}

static uint16_t
handle_NICK(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (nargs < 3 || !(*msg)[0])
		return PROTO_ERR;

	char nick[128];
	lsi_ut_ident2nick(nick, sizeof nick, (*msg)[0]);

	if (!lsi_ut_istrcmp(nick, ctx->mynick, ctx->casemap)) {
		STRACPY(ctx->mynick, (*msg)[2]);
		N("Our nickname is now '%s'", ctx->mynick);
	}

	return 0;
}

/* Deals only wih user modes */
static uint16_t
handle_MODE(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	if (nargs < 4)
		return PROTO_ERR;

	if (lsi_ut_istrcmp((*msg)[2], ctx->mynick, ctx->casemap) != 0)
		return 0; // We track channel modes elsewhere

	const char *p = (*msg)[3];
	bool enab = true;
	char c;
	while ((c = *p++)) {
		if (c == '+' || c == '-') {
			enab = c == '+';
			continue;
		}
		char *m = strchr(ctx->myumodes, c);
		if (enab == !!m) {
			W("user mode '%c' %s set ('%s')", c,
			    enab?"already":"not", ctx->myumodes);
			continue;
		}

		if (!enab) {
			*m++ = '\0';
			while (*m) {
				m[-1] = *m;
				*m++ = '\0';
			}
		} else {
			char mch[] = {c, '\0'};
			STRACAT(ctx->myumodes, mch);
		}
	}

	return 0;
}


static uint16_t
handle_005_CASEMAPPING(irc *ctx, const char *val)
{
	if (lsi_b_strcasecmp(val, "ascii") == 0)
		ctx->casemap = CMAP_ASCII;
	else if (lsi_b_strcasecmp(val, "strict-rfc1459") == 0)
		ctx->casemap = CMAP_STRICT_RFC1459;
	else {
		if (lsi_b_strcasecmp(val, "rfc1459") != 0)
			W("unknown 005 casemapping: '%s'", val);
		ctx->casemap = CMAP_RFC1459;
	}

	return 0;
}

static uint16_t
handle_005_PREFIX(irc *ctx, const char *val)
{
	char str[32];
	if (!val[0])
		return PROTO_ERR;

	STRACPY(str, val + 1);
	char *p = strchr(str, ')');
	if (!p)
		return PROTO_ERR;

	*p++ = '\0';

	size_t slen = strlen(str);
	if (slen == 0 || slen != strlen(p))
		return PROTO_ERR;

	lsi_b_strNcpy(ctx->m005modepfx[0], str, MAX_005_MDPFX);
	lsi_b_strNcpy(ctx->m005modepfx[1], p, MAX_005_MDPFX);

	return 0;
}

static uint16_t
handle_005_CHANMODES(irc *ctx, const char *val)
{
	for (int z = 0; z < 4; ++z)
		ctx->m005chanmodes[z][0] = '\0';

	int c = 0;
	char argbuf[64];
	STRACPY(argbuf, val);
	char *ptr = strtok(argbuf, ",");

	while (ptr) {
		if (c < 4)
			lsi_b_strNcpy(ctx->m005chanmodes[c++], ptr,
			    MAX_005_CHMD);
		ptr = strtok(NULL, ",");
	}

	if (c != 4)
		W("005 chanmodes: want 4 params, got %d. arg: \"%s\"", c, val);

	return 0;
}

static uint16_t
handle_005_CHANTYPES(irc *ctx, const char *val)
{
	lsi_b_strNcpy(ctx->m005chantypes, val, MAX_005_CHTYP);
	return 0;
}

static uint16_t
handle_005(irc *ctx, tokarr *msg, size_t nargs, bool logon)
{
	uint16_t ret = 0;
	bool have_casemap = false;
	D("handing a 005 with %zu args", nargs);

	/* last arg is "are supported by this server" or equivalent */
	for (size_t z = 3; z < nargs - 1; ++z) {
		char *nam = (*msg)[z];
		char *val;
		char *tmp = strchr(nam, '=');
		if (tmp) {
			*tmp = '\0';
			val = STRDUP(tmp + 1);
		} else
			val = STRDUP("");

		D("005 nam: '%s', val: '%s'", nam, val);

		free(lsi_skmap_get(ctx->m005attrs, nam));
		lsi_skmap_del(ctx->m005attrs, nam);

		if (!val || !lsi_skmap_put(ctx->m005attrs, nam, val))
			E("Out of memory, m005attrs will be incomplete");

		if (lsi_b_strcasecmp(nam, "CASEMAPPING") == 0) {
			ret |= handle_005_CASEMAPPING(ctx, val);
			have_casemap = true;
		} else if (lsi_b_strcasecmp(nam, "PREFIX") == 0)
			ret |= handle_005_PREFIX(ctx, val);
		else if (lsi_b_strcasecmp(nam, "CHANMODES") == 0)
			ret |= handle_005_CHANMODES(ctx, val);
		else if (lsi_b_strcasecmp(nam, "CHANTYPES") == 0)
			ret |= handle_005_CHANTYPES(ctx, val);

		if (tmp)
			*tmp = '='; // Others might want to handle this tokarr

		if (ret & CANT_PROCEED)
			return ret;
	}

	if (have_casemap && ctx->tracking && !ctx->tracking_enab) {
		/* Now that we know the casemapping used by the server, we
		 * can enable tracking if it was originally (before connecting)
		 * asked for */
		if (!lsi_trk_init(ctx))
			E("failed to enable tracking");
		else {
			ctx->tracking_enab = true;
			I("tracking enabled");
		}
	}

	return ret;
}


bool
lsi_imh_regall(irc *ctx, bool dumb)
{
	bool fail = false;
	if (!dumb) {
		/* In dumb mode, we don't care about the numerics telling us
		 * that we can't have the nick we wanted; we don't reply to
		 * PING and we also don't pay attention to 464 (Wrong server
		 * password).  This is all left to the user */
		fail = fail || !lsi_msg_reghnd(ctx, "PING", handle_PING, "core");
		fail = fail || !lsi_msg_reghnd(ctx, "432", handle_bad_nick, "core");
		fail = fail || !lsi_msg_reghnd(ctx, "433", handle_bad_nick, "core");
		fail = fail || !lsi_msg_reghnd(ctx, "436", handle_bad_nick, "core");
		fail = fail || !lsi_msg_reghnd(ctx, "437", handle_bad_nick, "core");
		fail = fail || !lsi_msg_reghnd(ctx, "464", handle_464, "core");
	}

	fail = fail || !lsi_msg_reghnd(ctx, "NICK", handle_NICK, "core");
	fail = fail || !lsi_msg_reghnd(ctx, "ERROR", handle_ERROR, "core");
	fail = fail || !lsi_msg_reghnd(ctx, "MODE", handle_MODE, "core");
	fail = fail || !lsi_msg_reghnd(ctx, "001", handle_001, "core");
	fail = fail || !lsi_msg_reghnd(ctx, "002", handle_002, "core");
	fail = fail || !lsi_msg_reghnd(ctx, "003", handle_003, "core");
	fail = fail || !lsi_msg_reghnd(ctx, "004", handle_004, "core");
	fail = fail || !lsi_msg_reghnd(ctx, "383", handle_383, "core");
	fail = fail || !lsi_msg_reghnd(ctx, "484", handle_484, "core");
	fail = fail || !lsi_msg_reghnd(ctx, "465", handle_465, "core");
	fail = fail || !lsi_msg_reghnd(ctx, "466", handle_466, "core");
	fail = fail || !lsi_msg_reghnd(ctx, "005", handle_005, "core");

	return !fail;
}

void
lsi_imh_unregall(irc *ctx)
{
	lsi_msg_unregall(ctx, "core");
	return;
}
