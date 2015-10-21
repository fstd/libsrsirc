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

#include <platform/base_string.h>
#include <platform/base_time.h>

#include <logger/intlog.h>

#include "common.h"
#include "conn.h"
#include "irc_msghnd.h"
#include "irc_track_int.h"
#include "msg.h"
#include "skmap.h"

#include <libsrsirc/util.h>


static bool send_logon(irc *ctx);

irc *
irc_init(void)
{
	iconn *con;
	irc *r = NULL;
	int preverrno = errno;
	errno = 0;
	if (!(con = lsi_conn_init()))
		goto irc_init_fail;

	if (!(r = lsi_com_malloc(sizeof *r)))
		goto irc_init_fail;

	r->pass = r->nick = r->uname = r->fname = r->serv_dist
	    = r->serv_info = r->lasterr = r->banmsg = NULL;

	r->msghnds = NULL;
	r->uprehnds = r->uposthnds = NULL;
	r->chans = r->users = NULL;
	r->m005chantypes = NULL;

	for (size_t i = 0; i < COUNTOF(r->m005chanmodes); i++)
		r->m005chanmodes[i] = NULL;

	for (size_t i = 0; i < COUNTOF(r->m005modepfx); i++)
		r->m005modepfx[i] = NULL;

	if (!(r->m005chantypes = lsi_com_malloc(MAX_005_CHTYP)))
		goto irc_init_fail;

	for (size_t i = 0; i < COUNTOF(r->m005chanmodes); i++)
		if (!(r->m005chanmodes[i] = lsi_com_malloc(MAX_005_CHMD)))
			goto irc_init_fail;

	for (size_t i = 0; i < COUNTOF(r->m005modepfx); i++)
		if (!(r->m005modepfx[i] = lsi_com_malloc(MAX_005_MDPFX)))
			goto irc_init_fail;

	lsi_com_strNcpy(r->m005chantypes, "#&", MAX_005_CHTYP);
	lsi_com_strNcpy(r->m005chanmodes[0], "b", MAX_005_CHMD);
	lsi_com_strNcpy(r->m005chanmodes[1], "k", MAX_005_CHMD);
	lsi_com_strNcpy(r->m005chanmodes[2], "l", MAX_005_CHMD);
	lsi_com_strNcpy(r->m005chanmodes[3], "psitnm", MAX_005_CHMD);
	lsi_com_strNcpy(r->m005modepfx[0], "ov", MAX_005_MDPFX);
	lsi_com_strNcpy(r->m005modepfx[1], "@+", MAX_005_MDPFX);

	size_t len = strlen(DEF_NICK);
	if (!(r->nick = lsi_com_malloc((len > 9 ? len : 9) + 1)))
		goto irc_init_fail;
	strcpy(r->nick, DEF_NICK);

	if ((!(r->uname = lsi_b_strdup(DEF_UNAME)))
	    || (!(r->fname = lsi_b_strdup(DEF_FNAME)))
	    || (!(r->serv_dist = lsi_b_strdup(DEF_SERV_DIST)))
	    || (!(r->serv_info = lsi_b_strdup(DEF_SERV_INFO))))
		goto irc_init_fail;

	r->msghnds_cnt = 64;
	if (!(r->msghnds = malloc(r->msghnds_cnt * sizeof *r->msghnds)))
		goto irc_init_fail;

	for (size_t i = 0; i < r->msghnds_cnt; i++)
		r->msghnds[i].cmd[0] = '\0';

	r->uprehnds_cnt = 8;
	if (!(r->uprehnds = malloc(r->uprehnds_cnt * sizeof *r->uprehnds)))
		goto irc_init_fail;
	for (size_t i = 0; i < r->uprehnds_cnt; i++)
		r->uprehnds[i].cmd[0] = '\0';

	r->uposthnds_cnt = 8;
	if (!(r->uposthnds = malloc(r->uposthnds_cnt * sizeof *r->uposthnds)))
		goto irc_init_fail;
	for (size_t i = 0; i < r->uposthnds_cnt; i++)
		r->uposthnds[i].cmd[0] = '\0';

	errno = preverrno;

	r->con = con;
	/* persistent after reset */
	r->mynick[0] = r->myhost[0] = r->umodes[0] = r->myumodes[0]
	    = r->cmodes[0] = r->ver[0] = '\0';

	r->service = r->restricted = r->banned = r->serv_con = false;
	r->cb_con_read = NULL;
	r->cb_mut_nick = lsi_ut_mut_nick;
	r->casemap = CMAP_RFC1459;
	r->conflags = DEF_CONFLAGS;
	r->serv_type = DEF_SERV_TYPE;
	r->scto_us = DEF_SCTO_US;
	r->hcto_us = DEF_HCTO_US;
	r->dumb = false;
	r->tracking_enab = r->tracking = false;

	for (size_t i = 0; i < COUNTOF(r->logonconv); i++)
		r->logonconv[i] = NULL;


	D("(%p) irc_bas initialized (backend: %p)", (void *)r, (void *)r->con);
	return r;

irc_init_fail:
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
	}

	if (con)
		lsi_conn_dispose(con);

	return NULL;
}

void
irc_reset(irc *ctx)
{
	lsi_conn_reset(ctx->con);
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

	D("(%p) disposed", (void *)ctx);
	free(ctx);
}

bool
irc_connect(irc *ctx)
{
	uint64_t tsend = ctx->hcto_us ?
	    lsi_b_tstamp_us() + ctx->hcto_us : 0;

	lsi_trk_deinit(ctx);
	ctx->tracking_enab = false;

	lsi_imh_unregall(ctx);
	if (!lsi_imh_regall(ctx, ctx->dumb))
		return false;

	lsi_com_update_strprop(&ctx->lasterr, NULL);
	lsi_com_update_strprop(&ctx->banmsg, NULL);
	ctx->banned = false;

	for (size_t i = 0; i < COUNTOF(ctx->logonconv); i++) {
		lsi_ut_freearr(ctx->logonconv[i]);
		ctx->logonconv[i] = NULL;
	}

	if (!lsi_conn_connect(ctx->con, ctx->scto_us, ctx->hcto_us))
		return false;

	I("(%p) connection established", (void *)ctx);

	if (ctx->dumb)
		return true;

	if (!send_logon(ctx))
		goto irc_connect_fail;

	I("(%p) IRC logon sequence sent", (void *)ctx);

	lsi_com_strNcpy(ctx->mynick, ctx->nick, sizeof ctx->mynick);
	tokarr msg;

	bool success = false;
	uint64_t trem = 0;
	int r;
	do {
		if (lsi_com_check_timeout(tsend, &trem)) {
			W("(%p) timeout waiting for 004", (void *)ctx);
			goto irc_connect_fail;
		}

		if ((r = lsi_conn_read(ctx->con, &msg, trem)) < 0)
			goto irc_connect_fail;

		if (r == 0)
			continue;

		if (ctx->cb_con_read &&
		    !ctx->cb_con_read(&msg, ctx->tag_con_read)) {
			W("(%p) logon prohibited by conread", (void *)ctx);
			goto irc_connect_fail;
		}

		size_t ac = 2;
		while (ac < COUNTOF(msg) && msg[ac])
			ac++;

		/* these are the protocol messages we deal with.
		 * seeing 004 or 383 makes us consider ourselves logged on
		 * note that we do not wait for 005, but we will later
		 * parse it as we ran across it. */
		uint8_t flags = lsi_msg_handle(ctx, &msg, true);

		if (flags & CANT_PROCEED)
			goto irc_connect_fail;

		if (flags & LOGON_COMPLETE)
			success = true;

	} while (!success);

	N("(%p) logged on to IRC", (void *)ctx);
	return true;

irc_connect_fail:
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

	int r = lsi_conn_read(ctx->con, tok, to_us);
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

/* this is ugly and insane and BTW: it won't work */
bool
irc_can_read(irc *ctx)
{
	return ctx->con->rctx.eptr - ctx->con->rctx.wptr >= 3;
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
	char aBuf[256];
	char *pBuf = aBuf;
	aBuf[0] = '\0';
	size_t rem = sizeof aBuf;
	int r;

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
}

void
irc_regcb_mutnick(irc *ctx, fp_mut_nick cb)
{
	ctx->cb_mut_nick = cb;
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
	for (size_t i = 0; i < COUNTOF(ctx->logonconv); i++) {
		if (!ctx->logonconv[i])
			continue;
		char line[1024];
		lsi_ut_sndumpmsg(line, sizeof line, NULL, ctx->logonconv[i]),
		N("logonconv[%zu]: '%s'", i, line);
	}
	lsi_skmap_dump(ctx->m005attrs, printstr);
	//skmap *chans
	//skmap *users
	//fp_con_read cb_con_read
	//fp_mut_nick cb_mut_nick
	//struct msghnd msghnds[64]
	//struct iconn_s *con
	N("--- end of IRC context dump ---");
}
