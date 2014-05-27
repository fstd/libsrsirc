/* irc.c - basic irc functionality (see also irc_ext.c)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_IRC

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "common.h"
#include <libsrsirc/util.h>
#include "conn.h"
#include "msg.h"

#include <intlog.h>

#include <libsrsirc/irc.h>

static bool send_logon(irc hnd);

irc
irc_init(void)
{
	iconn con;
	irc r = NULL;
	int preverrno = errno;
	errno = 0;
	if (!(con = conn_init()))
		goto irc_init_fail;


	if (!(r = malloc(sizeof (*(irc)0))))
		goto irc_init_fail;

	r->pass = r->nick = r->uname = r->fname = r->serv_dist
	    = r->serv_info = r->lasterr = r->banmsg = NULL;

	com_strNcpy(r->m005chanmodes[0], "b", sizeof r->m005chanmodes[0]);
	com_strNcpy(r->m005chanmodes[1], "k", sizeof r->m005chanmodes[1]);
	com_strNcpy(r->m005chanmodes[2], "l", sizeof r->m005chanmodes[2]);
	com_strNcpy(r->m005chanmodes[3], "psitnm", sizeof r->m005chanmodes[3]);
	com_strNcpy(r->m005modepfx[0], "ov", sizeof r->m005modepfx[0]);
	com_strNcpy(r->m005modepfx[1], "@+", sizeof r->m005modepfx[1]);

	if (!(r->pass = strdup(DEF_PASS)))
		goto irc_init_fail;

	size_t len = strlen(DEF_NICK);
	if (!(r->nick = malloc((len > 9 ? len : 9) + 1)))
		goto irc_init_fail;
	strcpy(r->nick, DEF_NICK);

	if ((!(r->uname = strdup(DEF_UNAME)))
	    || (!(r->fname = strdup(DEF_FNAME)))
	    || (!(r->serv_dist = strdup(DEF_SERV_DIST)))
	    || (!(r->serv_info = strdup(DEF_SERV_INFO))))
		goto irc_init_fail;

	errno = preverrno;

	r->con = con;
	/* persistent after reset */
	r->mynick[0] = r->myhost[0] = r->umodes[0] = r->cmodes[0]
	    = r->ver[0] = '\0';

	r->service = r->restricted = r->banned = r->serv_con = false;
	r->cb_con_read = NULL;
	r->cb_mut_nick = ut_mut_nick;
	r->casemapping = CMAP_RFC1459;
	r->conflags = DEF_CONFLAGS;
	r->serv_type = DEF_SERV_TYPE;
	r->scto_us = DEF_SCTO_US;
	r->hcto_us = DEF_HCTO_US;

	for (int i = 0; i < 4; i++)
		r->logonconv[i] = NULL;


	D("(%p) irc_bas initialized (backend: %p)", r, r->con);
	return r;

irc_init_fail:
	EE("failed to initialize irc handle");
	if (r) {
		free(r->pass);
		free(r->nick);
		free(r->uname);
		free(r->fname);
		free(r->serv_dist);
		free(r->serv_info);
		free(r);
	}

	if (con)
		conn_dispose(con);

	return NULL;
}

void
irc_reset(irc hnd)
{
	conn_reset(hnd->con);
}

void
irc_dispose(irc hnd)
{
	conn_dispose(hnd->con);
	free(hnd->lasterr);
	free(hnd->banmsg);
	free(hnd->pass);
	free(hnd->nick);
	free(hnd->uname);
	free(hnd->fname);
	free(hnd->serv_dist);
	free(hnd->serv_info);

	for (int i = 0; i < 4; i++)
		ut_freearr(hnd->logonconv[i]);

	D("(%p) disposed", hnd);
	free(hnd);
}

bool
irc_connect(irc hnd)
{
	uint64_t tsend = hnd->hcto_us ?
	    com_timestamp_us() + hnd->hcto_us : 0;

	com_update_strprop(&hnd->lasterr, NULL);
	com_update_strprop(&hnd->banmsg, NULL);
	hnd->banned = false;

	for (int i = 0; i < 4; i++) {
		ut_freearr(hnd->logonconv[i]);
		hnd->logonconv[i] = NULL;
	}

	if (!conn_connect(hnd->con, hnd->scto_us, hnd->hcto_us))
		return false;

	if (!send_logon(hnd))
		goto irc_connect_fail;

	I("(%p) connection established, IRC logon sequence sent", hnd);

	com_strNcpy(hnd->mynick, hnd->nick, sizeof hnd->mynick);
	tokarr msg;

	bool success = false;
	uint64_t trem = 0;
	int r;
	do {
		if (com_check_timeout(tsend, &trem)) {
			W("(%p) timeout waiting for 004", hnd);
			goto irc_connect_fail;
		}

		if ((r = conn_read(hnd->con, &msg, trem)) < 0)
			goto irc_connect_fail;

		if (r == 0)
			continue;

		if (hnd->cb_con_read &&
		    !hnd->cb_con_read(&msg, hnd->tag_con_read)) {
			W("(%p) further logon prohibited by conread", hnd);
			goto irc_connect_fail;
		}

		size_t ac = 2;
		while (ac < COUNTOF(msg) && msg[ac])
			ac++;

		/* these are the protocol messages we deal with.
		 * seeing 004 or 383 makes us consider ourselves logged on
		 * note that we do not wait for 005, but we will later
		 * parse it as we ran across it. */
		uint8_t flags = msg_handle(hnd, &msg, true);

		if (flags & CANT_PROCEED)
			goto irc_connect_fail;

		if (flags & LOGON_COMPLETE)
			success = true;

	} while (!success);

	N("(%p) logged on to IRC", hnd);
	return true;

irc_connect_fail:
	irc_reset(hnd);
	return false;
}

bool
irc_online(irc hnd)
{
	return conn_online(hnd->con);
}

int
irc_read(irc hnd, tokarr *tok, uint64_t to_us)
{
	int r = conn_read(hnd->con, tok, to_us);
	if (r == 0)
		return 0;

	if (r < 0 || msg_handle(hnd, tok, false) & CANT_PROCEED) {
		irc_reset(hnd);
		return -1;
	}

	return 1;
}

bool
irc_write(irc hnd, const char *line)
{
	bool r = conn_write(hnd->con, line);

	if (!r)
		irc_reset(hnd);

	return r;
}

const char*
irc_mynick(irc hnd)
{
	return hnd->mynick;
}

bool
irc_set_server(irc hnd, const char *host, uint16_t port)
{
	return conn_set_server(hnd->con, host, port);
}

bool
irc_set_pass(irc hnd, const char *srvpass)
{
	return com_update_strprop(&hnd->pass, srvpass);
}

bool
irc_set_uname(irc hnd, const char *uname)
{
	return com_update_strprop(&hnd->uname, uname);
}

bool
irc_set_fname(irc hnd, const char *fname)
{
	return com_update_strprop(&hnd->fname, fname);
}

bool
irc_set_nick(irc hnd, const char *nick)
{
	return com_update_strprop(&hnd->nick, nick);
}

static bool
send_logon(irc hnd)
{
	if (!conn_online(hnd->con))
		return false;
	char aBuf[256];
	char *pBuf = aBuf;
	aBuf[0] = '\0';
	ssize_t rem = sizeof aBuf;
	int r;

	if (hnd->pass && strlen(hnd->pass) > 0) {
		r = snprintf(pBuf, rem, "PASS :%s\r\n", hnd->pass);
		rem -= r;
		pBuf += r;
	}

	if (rem <= 0)
		return false;

	if (hnd->service) {
		r = snprintf(pBuf, rem, "SERVICE %s 0 %s %ld 0 :%s\r\n",
		    hnd->nick, hnd->serv_dist, hnd->serv_type,
		    hnd->serv_info);
	} else {
		r = snprintf(pBuf, rem, "NICK %s\r\nUSER %s %u * :%s\r\n",
		    hnd->nick, hnd->uname, hnd->conflags, hnd->fname);
	}

	rem -= r;
	pBuf += r;
	if (rem < 0)
		return false;

	return irc_write(hnd, aBuf);
}
