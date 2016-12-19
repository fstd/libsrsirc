/* irc.c - basic IRC functionality (see also irc_ext.c)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_IRC

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <libsrsirc/irc.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <platform/base_misc.h>
#include <platform/base_string.h>
#include <platform/base_time.h>

#include <logger/intlog.h>

#include "common.h"
#include "conn.h"
#include "irc_msghnd.h"
#include "irc_track_int.h"
#include "msg.h"
#include "skmap.h"
#include "v3.h"

#include <libsrsirc/irc_track.h>
#include <libsrsirc/util.h>


static bool send_logon(irc *ctx);
static void reset_state(irc *ctx);

irc *
irc_init(void)
{
	iconn *con;
	irc *r = NULL;
	int preverrno = errno;
	errno = 0;
	if (!(con = lsi_conn_init()))
		goto fail;

	if (!(r = MALLOC(sizeof *r)))
		goto fail;

	r->pass = r->nick = r->uname = r->fname = r->sasl_mech = r->sasl_msg
	   = r->serv_dist = r->serv_info = r->lasterr = r->banmsg = NULL;

	r->sasl_msg_len = r->v3ntags = 0;
	r->starttls = r->starttls_first = false;

	r->msghnds = NULL;
	r->uprehnds = r->uposthnds = NULL;
	r->chans = r->users = NULL;
	r->m005chantypes = NULL;
	r->m005attrs = NULL;

	lsi_v3_init_caps(r);

	for (size_t i = 0; i < COUNTOF(r->m005chanmodes); i++)
		r->m005chanmodes[i] = NULL;

	for (size_t i = 0; i < COUNTOF(r->m005modepfx); i++)
		r->m005modepfx[i] = NULL;

	for (size_t i = 0; i < COUNTOF(r->v3tags_raw); i++)
		r->v3tags_raw[i] = NULL;

	for (size_t i = 0; i < COUNTOF(r->v3tags_dec); i++)
		r->v3tags_dec[i] = NULL;

	if (!(r->m005chantypes = MALLOC(MAX_005_CHTYP)))
		goto fail;

	for (size_t i = 0; i < COUNTOF(r->m005chanmodes); i++)
		if (!(r->m005chanmodes[i] = MALLOC(MAX_005_CHMD)))
			goto fail;

	for (size_t i = 0; i < COUNTOF(r->m005modepfx); i++)
		if (!(r->m005modepfx[i] = MALLOC(MAX_005_MDPFX)))
			goto fail;

	for (size_t i = 0; i < COUNTOF(r->v3tags_dec); i++) {
		if (!(r->v3tags_dec[i] = MALLOC(MAX_V3TAGLEN)))
			goto fail;
		r->v3tags_dec[i][0] = '\0';
	}

	if (!(r->m005attrs = lsi_skmap_init(256, CMAP_ASCII)))
		goto fail;

	lsi_b_strNcpy(r->m005chantypes, "#&", MAX_005_CHTYP);
	lsi_b_strNcpy(r->m005chanmodes[0], "b", MAX_005_CHMD);
	lsi_b_strNcpy(r->m005chanmodes[1], "k", MAX_005_CHMD);
	lsi_b_strNcpy(r->m005chanmodes[2], "l", MAX_005_CHMD);
	lsi_b_strNcpy(r->m005chanmodes[3], "psitnm", MAX_005_CHMD);
	lsi_b_strNcpy(r->m005modepfx[0], "ov", MAX_005_MDPFX);
	lsi_b_strNcpy(r->m005modepfx[1], "@+", MAX_005_MDPFX);

	size_t len = strlen(DEF_NICK);
	if (!(r->nick = MALLOC((len > 9 ? len : 9) + 1)))
		goto fail;
	strcpy(r->nick, DEF_NICK);

	if ((!(r->uname = STRDUP(DEF_UNAME)))
	    || (!(r->fname = STRDUP(DEF_FNAME)))
	    || (!(r->serv_dist = STRDUP(DEF_SERV_DIST)))
	    || (!(r->serv_info = STRDUP(DEF_SERV_INFO))))
		goto fail;

	r->msghnds_cnt = 64;
	if (!(r->msghnds = MALLOC(r->msghnds_cnt * sizeof *r->msghnds)))
		goto fail;

	for (size_t i = 0; i < r->msghnds_cnt; i++)
		r->msghnds[i].cmd[0] = '\0';

	r->uprehnds_cnt = 8;
	if (!(r->uprehnds = MALLOC(r->uprehnds_cnt * sizeof *r->uprehnds)))
		goto fail;
	for (size_t i = 0; i < r->uprehnds_cnt; i++)
		r->uprehnds[i].cmd[0] = '\0';

	r->uposthnds_cnt = 8;
	if (!(r->uposthnds = MALLOC(r->uposthnds_cnt * sizeof *r->uposthnds)))
		goto fail;
	for (size_t i = 0; i < r->uposthnds_cnt; i++)
		r->uposthnds[i].cmd[0] = '\0';

	errno = preverrno;

	r->con = con;

	for (size_t i = 0; i < COUNTOF(r->logonconv); i++)
		r->logonconv[i] = NULL;

	r->serv_con = false;
	r->cb_con_read = NULL;
	r->cb_mut_nick = lsi_ut_mut_nick;
	r->conflags = DEF_CONFLAGS;
	r->serv_type = DEF_SERV_TYPE;
	r->scto_us = DEF_SCTO_US;
	r->hcto_us = DEF_HCTO_US;
	r->dumb = false;
	r->tracking_enab = r->tracking = false;
	r->endofnames = false;

	reset_state(r);

	D("IRC context initialized (%p)", (void *)r->con);
	return r;

fail:
	EE("failed to initialize IRC context");
	if (r) {
		free(r->pass);
		free(r->nick);
		free(r->uname);
		free(r->fname);
		free(r->serv_dist);
		free(r->serv_info);
		free(r->msghnds);
		free(r->uprehnds);
		free(r->uposthnds);
		free(r->m005chantypes);
		for (size_t i = 0; i < COUNTOF(r->m005chanmodes); i++)
			free(r->m005chanmodes[i]);
		for (size_t i = 0; i < COUNTOF(r->m005modepfx); i++)
			free(r->m005modepfx[i]);
		for (size_t i = 0; i < COUNTOF(r->v3tags_dec); i++)
			free(r->v3tags_dec[i]);
		lsi_skmap_dispose(r->m005attrs);
	}

	if (con)
		lsi_conn_dispose(con);

	return NULL;
}

void
irc_reset(irc *ctx)
{
	lsi_conn_reset(ctx->con);
	return;
}

void
irc_dispose(irc *ctx)
{
	lsi_trk_deinit(ctx);
	lsi_conn_dispose(ctx->con);
	free(ctx->lasterr);
	free(ctx->banmsg);
	free(ctx->pass);
	free(ctx->nick);
	free(ctx->uname);
	free(ctx->fname);
	free(ctx->sasl_mech);
	free(ctx->sasl_msg);
	free(ctx->serv_dist);
	free(ctx->serv_info);
	free(ctx->msghnds);
	free(ctx->uprehnds);
	free(ctx->uposthnds);

	for (size_t i = 0; i < COUNTOF(ctx->logonconv); i++)
		lsi_ut_freearr(ctx->logonconv[i]);

	free(ctx->m005chantypes);

	for (size_t i = 0; i < COUNTOF(ctx->m005chanmodes); i++)
		free(ctx->m005chanmodes[i]);

	for (size_t i = 0; i < COUNTOF(ctx->m005modepfx); i++)
		free(ctx->m005modepfx[i]);

	for (size_t i = 0; i < COUNTOF(ctx->v3tags_dec); i++)
		free(ctx->v3tags_dec[i]);

	lsi_v3_reset_caps(ctx);

	void *v;
	if (lsi_skmap_first(ctx->m005attrs, NULL, &v))
		do free(v); while (lsi_skmap_next(ctx->m005attrs, NULL, &v));
	lsi_skmap_dispose(ctx->m005attrs);

	D("disposed");
	free(ctx);
	return;
}

bool
irc_connect(irc *ctx)
{
	uint64_t tend = ctx->hcto_us ?
	    lsi_b_tstamp_us() + ctx->hcto_us : 0;

	lsi_trk_deinit(ctx);
	ctx->tracking_enab = false;

	lsi_imh_unregall(ctx);
	if (!lsi_imh_regall(ctx, ctx->dumb))
		return false;

	lsi_v3_unregall(ctx);
	if (!lsi_v3_regall(ctx, ctx->dumb))
		return false;

	reset_state(ctx);

	for (size_t i = 0; i < COUNTOF(ctx->logonconv); i++) {
		lsi_ut_freearr(ctx->logonconv[i]);
		ctx->logonconv[i] = NULL;
	}

	void *v;
	if (lsi_skmap_first(ctx->m005attrs, NULL, &v))
		do free(v); while (lsi_skmap_next(ctx->m005attrs, NULL, &v));
	lsi_skmap_clear(ctx->m005attrs);

	if (!lsi_conn_connect(ctx->con, ctx->scto_us, ctx->hcto_us))
		return false;

	I("connection established");

	if (ctx->dumb)
		return true;

	bool logon_sent = false;
	if (ctx->starttls_first) {
		if (!lsi_conn_write(ctx->con, "STARTTLS\r\n"))
			goto fail;
	} else {
		logon_sent = true;
		if (!send_logon(ctx))
			goto fail;
		I("IRC logon sequence sent");
	}

	STRACPY(ctx->mynick, ctx->nick);
	tokarr msg;

	bool logged_on = false;
	bool using_sasl = ctx->sasl_mech && ctx->sasl_msg;
	bool sasl_authed = false;
	uint64_t trem = 0;
	int r;
	do {
		if (lsi_com_check_timeout(tend, &trem)) {
			W("timeout waiting for 004");
			goto fail;
		}

		if ((r = lsi_conn_read(ctx->con, &msg, NULL, NULL, trem)) < 0)
			goto fail;

		if (r == 0)
			continue;

		if (ctx->cb_con_read &&
		    !ctx->cb_con_read(&msg, ctx->tag_con_read)) {
			W("logon prohibited by conread");
			goto fail;
		}

		size_t ac = 2;
		while (ac < COUNTOF(msg) && msg[ac])
			ac++;

		/* these are the protocol messages we deal with.
		 * seeing 004 or 383 makes us consider ourselves logged on
		 * note that we do not wait for 005, but we will later
		 * parse it as we ran across it. */
		uint16_t flags = lsi_msg_handle(ctx, &msg, true);

		if (flags & CANT_PROCEED)
			goto fail;

		if (flags & LOGON_COMPLETE)
			logged_on = true;

		if (flags & SASL_COMPLETE)
			sasl_authed = true;

		if (flags & STARTTLS_OVER && !logon_sent) {
			if (!send_logon(ctx))
				goto fail;
			logon_sent = true;
		}

	} while (!logged_on || (using_sasl && !sasl_authed));

	N("logged on to IRC");
	return true;

fail:
	irc_reset(ctx);
	return false;
}

int
irc_read(irc *ctx, tokarr *tok, uint64_t to_us)
{
	/* Allow to be called with tok == NULL if we don't need the result */
	tokarr dummy;
	if (!tok)
		tok = &dummy;

	for (size_t i = 0; i < COUNTOF(ctx->v3tags_dec); i++)
		ctx->v3tags_dec[i][0] = '\0';
	ctx->v3ntags = COUNTOF(ctx->v3tags_raw);

	int r = lsi_conn_read(ctx->con, tok, ctx->v3tags_raw, &ctx->v3ntags, to_us);

	if (r == 0)
		return 0;

	if (r < 0 || lsi_msg_handle(ctx, tok, false) & CANT_PROCEED) {
		irc_reset(ctx);
		return -1;
	}

	return 1;
}

bool
irc_eof(irc *ctx)
{
	return lsi_conn_eof(ctx->con);
}

bool
irc_write(irc *ctx, const char *line)
{
	bool r = lsi_conn_write(ctx->con, line);

	if (!r)
		irc_reset(ctx);

	return r;
}

bool
irc_printf(irc *ctx, const char *fmt, ...)
{
	char buf[1024];

	va_list vl;
	va_start(vl, fmt);
	vsnprintf(buf, sizeof buf, fmt, vl);
	va_end(vl);

	return irc_write(ctx, buf);
}


static bool
send_logon(irc *ctx)
{
	if (!lsi_conn_online(ctx->con))
		return false;
	char aBuf[512];
	char *pBuf = aBuf;
	aBuf[0] = '\0';
	size_t rem = sizeof aBuf;
	int r;

	/* XXX use strNcat? STRACAT? invent STRAPRINTF?  this clearly sucks */
	if (lsi_v3_want_caps(ctx)) {
		r = snprintf(pBuf, rem, "CAP LS 302\r\n");
		if (r > (int)rem)
			r = rem;

		rem -= r;
		pBuf += r;
	}

	if (!rem)
		return false;

	if (ctx->pass && strlen(ctx->pass) > 0) {
		r = snprintf(pBuf, rem, "PASS :%s\r\n", ctx->pass);
		if (r > (int)rem)
			r = rem;

		rem -= r;
		pBuf += r;
	}

	if (!rem)
		return false;

	if (ctx->service) {
		r = snprintf(pBuf, rem, "SERVICE %s 0 %s %ld 0 :%s\r\n",
		    ctx->nick, ctx->serv_dist, ctx->serv_type,
		    ctx->serv_info);
	} else {
		r = snprintf(pBuf, rem, "NICK %s\r\nUSER %s %u * :%s\r\n",
		    ctx->nick, ctx->uname, ctx->conflags, ctx->fname);
	}

	if (r > (int)rem) {
		r = rem;
		return false;
	}

	rem -= r;
	pBuf += r;

	return irc_write(ctx, aBuf);
}

void
irc_regcb_conread(irc *ctx, fp_con_read cb, void *tag)
{
	ctx->cb_con_read = cb;
	ctx->tag_con_read = tag;
	return;
}

void
irc_regcb_mutnick(irc *ctx, fp_mut_nick cb)
{
	ctx->cb_mut_nick = cb;
	return;
}

bool
irc_reg_msghnd(irc *ctx, const char *cmd, uhnd_fn hndfn, bool pre)
{
	return lsi_msg_reguhnd(ctx, cmd, hndfn, pre);
}

void
irc_dump(irc *ctx)
{
	N("--- IRC context %p dump---", (void *)ctx);
	irc_conn_dump(ctx->con);
	N("mynick: '%s'", ctx->mynick);
	N("myhost: '%s'", ctx->myhost);
	N("service: %d", ctx->service);
	N("cmodes: '%s'", ctx->cmodes);
	N("umodes: '%s'", ctx->umodes);
	N("myumodes: '%s'", ctx->myumodes);
	N("ver: '%s'", ctx->ver);
	N("lasterr: '%s'", ctx->lasterr);
	N("hcto_us: %"PRIu64, ctx->hcto_us);
	N("scto_us: %"PRIu64, ctx->scto_us);
	N("restricted: %d", ctx->restricted);
	N("banned: %d", ctx->banned);
	N("banmsg: '%s'", ctx->banmsg);
	N("casemap: %d", ctx->casemap);
	N("pass: '%s'", ctx->pass);
	N("nick: '%s'", ctx->nick);
	N("uname: '%s'", ctx->uname);
	N("fname: '%s'", ctx->fname);
	N("conflags: %"PRIu8, ctx->conflags);
	N("serv_con: %d", ctx->serv_con);
	N("serv_dist: '%s'", ctx->serv_dist);
	N("serv_type: %ld", ctx->serv_type);
	N("serv_info: '%s'", ctx->serv_info);
	N("m005chanmodes[0]: '%s'", ctx->m005chanmodes[0]);
	N("m005chanmodes[1]: '%s'", ctx->m005chanmodes[1]);
	N("m005chanmodes[2]: '%s'", ctx->m005chanmodes[2]);
	N("m005chanmodes[3]: '%s'", ctx->m005chanmodes[3]);
	N("m005modepfx[0]: '%s'", ctx->m005modepfx[0]);
	N("m005modepfx[1]: '%s'", ctx->m005modepfx[1]);
	N("m005chantypes: '%s'", ctx->m005chantypes);
	N("tag_con_read: %p", (void *)ctx->tag_con_read);
	N("tracking: %d", ctx->tracking);
	N("tracking_enab: %d", ctx->tracking_enab);
	N("endofnames: %d", ctx->endofnames);
	N("v3ntags: %zu", ctx->v3ntags);
	for (size_t i = 0; i < COUNTOF(ctx->v3tags_raw); i++)
		N("v3tags_raw[%zu]: '%s'", i, ctx->v3tags_raw[i]);
	for (size_t i = 0; i < COUNTOF(ctx->v3tags_dec); i++)
		N("v3tags_dec[%zu]: '%s'", i, ctx->v3tags_dec[i]);
	for (size_t i = 0; i < COUNTOF(ctx->logonconv); i++) {
		if (!ctx->logonconv[i])
			continue;
		char line[1024];
		lsi_ut_sndumpmsg(line, sizeof line, NULL, ctx->logonconv[i]),
		N("logonconv[%zu]: '%s'", i, line);
	}
	//lsi_skmap_dump(ctx->m005attrs, printstr);
	//skmap *chans
	//skmap *users
	//fp_con_read cb_con_read
	//fp_mut_nick cb_mut_nick
	//struct msghnd msghnds[64]
	//struct iconn_s *con
	if (ctx->tracking_enab)
		lsi_trk_dump(ctx, true);
	N("--- end of IRC context dump ---");
	return;
}

static void
reset_state(irc *ctx)
{
	ctx->mynick[0] = ctx->myhost[0] = ctx->myumodes[0] = ctx->ver[0]
	    = ctx->v3capreq[0] = '\0';

	ctx->restricted = ctx->banned = ctx->service = false;
	ctx->casemap = CMAP_RFC1459;
	lsi_com_update_strprop(&ctx->lasterr, NULL);
	lsi_com_update_strprop(&ctx->banmsg, NULL);
	STRACPY(ctx->umodes, DEF_UMODES);
	STRACPY(ctx->cmodes, DEF_CMODES);
	lsi_b_strNcpy(ctx->m005chantypes, "#&", MAX_005_CHTYP);
	lsi_b_strNcpy(ctx->m005chanmodes[0], "b", MAX_005_CHMD);
	lsi_b_strNcpy(ctx->m005chanmodes[1], "k", MAX_005_CHMD);
	lsi_b_strNcpy(ctx->m005chanmodes[2], "l", MAX_005_CHMD);
	lsi_b_strNcpy(ctx->m005chanmodes[3], "psitnm", MAX_005_CHMD);
	lsi_b_strNcpy(ctx->m005modepfx[0], "ov", MAX_005_MDPFX);
	lsi_b_strNcpy(ctx->m005modepfx[1], "@+", MAX_005_MDPFX);
	for (size_t i = 0; i < COUNTOF(ctx->v3tags_raw); i++)
		ctx->v3tags_raw[i] = NULL;
	for (size_t i = 0; i < COUNTOF(ctx->v3tags_dec); i++)
		ctx->v3tags_dec[i][0] = '\0';
	ctx->v3ntags = 0;
	return;
}
