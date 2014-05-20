/* irc_basic.c - Front-end implementation
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <common.h>
#include <libsrsirc/irc_util.h>
#include "irc_con.h"
#include "msghandle.h"

#include <intlog.h>

#include <libsrsirc/irc_basic.h>

static bool send_logon(ibhnd_t hnd);

static bool onread(ibhnd_t hnd, char **tok, size_t tok_len);

ibhnd_t
ircbas_init(void)
{
	ichnd_t con;
	ibhnd_t r = NULL;
	int preverrno = errno;
	errno = 0;
	if (!(con = irccon_init()))
		goto ircbas_init_fail;


	if (!(r = malloc(sizeof (*(ibhnd_t)0))))
		goto ircbas_init_fail;

	r->pass = r->nick = r->uname = r->fname = r->serv_dist
	    = r->serv_info = r->lasterr = r->banmsg = NULL;

	ic_strNcpy(r->m005chanmodes[0], "b", sizeof r->m005chanmodes[0]);
	ic_strNcpy(r->m005chanmodes[1], "k", sizeof r->m005chanmodes[1]);
	ic_strNcpy(r->m005chanmodes[2], "l", sizeof r->m005chanmodes[2]);
	ic_strNcpy(r->m005chanmodes[3], "psitnm", sizeof r->m005chanmodes[3]);
	ic_strNcpy(r->m005modepfx[0], "ov", sizeof r->m005modepfx[0]);
	ic_strNcpy(r->m005modepfx[1], "@+", sizeof r->m005modepfx[1]);

	if (!(r->pass = strdup(DEF_PASS)))
		goto ircbas_init_fail;

	size_t len = strlen(DEF_NICK);
	if (!(r->nick = malloc((len > 9 ? len : 9) + 1)))
		goto ircbas_init_fail;
	strcpy(r->nick, DEF_NICK);

	if ((!(r->uname = strdup(DEF_UNAME)))
	    || (!(r->fname = strdup(DEF_FNAME)))
	    || (!(r->serv_dist = strdup(DEF_SERV_DIST)))
	    || (!(r->serv_info = strdup(DEF_SERV_INFO))))
		goto ircbas_init_fail;

	errno = preverrno;

	r->con = con;
	/* persistent after reset */
	r->mynick[0] = r->myhost[0] = r->umodes[0] = r->cmodes[0]
	    = r->ver[0] = '\0';

	r->service = r->restricted = r->banned = r->serv_con = false;
	r->cb_con_read = NULL;
	r->cb_mut_nick = mutilate_nick;
	r->casemapping = CMAP_RFC1459;
	r->conflags = DEF_CONFLAGS;
	r->serv_type = DEF_SERV_TYPE;
	r->conto_soft_us = DEF_CONTO_SOFT;
	r->conto_hard_us = DEF_CONTO_HARD;

	for(int i = 0; i < 4; i++)
		r->logonconv[i] = NULL;


	D("(%p) irc_bas initialized (backend: %p)", r, r->con);
	return r;

ircbas_init_fail:
	EE("failed to initialize irc_basic handle");
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
		irccon_dispose(con);

	return NULL;
}

bool
ircbas_reset(ibhnd_t hnd)
{
	return irccon_reset(hnd->con);
}

bool
ircbas_dispose(ibhnd_t hnd)
{
	if (!irccon_dispose(hnd->con))
		return false;

	free(hnd->lasterr);
	free(hnd->banmsg);
	free(hnd->pass);
	free(hnd->nick);
	free(hnd->uname);
	free(hnd->fname);
	free(hnd->serv_dist);
	free(hnd->serv_info);

	D("(%p) disposed", hnd);
	free(hnd);

	return true;
}

bool
ircbas_connect(ibhnd_t hnd)
{
	int64_t tsend = hnd->conto_hard_us ?
	    ic_timestamp_us() + hnd->conto_hard_us : 0;

	free(hnd->lasterr);
	hnd->lasterr = NULL;
	free(hnd->banmsg);
	hnd->banmsg = NULL;
	hnd->banned = false;

	for(int i = 0; i < 4; i++) {
		ic_freearr(hnd->logonconv[i], MAX_IRCARGS);
		hnd->logonconv[i] = NULL;
	}

	D("(%p) connecting backend (timeout: %luus (soft), %luus (hard))",
	    hnd, hnd->conto_soft_us, hnd->conto_hard_us);

	if (!irccon_connect(hnd->con, hnd->conto_soft_us,
	    hnd->conto_hard_us)) {
		W("(%p) backend failed to establish connection", hnd);
		return false;
	}

	D("(%p) sending IRC logon sequence", hnd);
	if (!send_logon(hnd)) {
		W("(%p) failed writing IRC logon sequence", hnd);
		ircbas_reset(hnd);
		return false;
	}

	D("(%p) connection established, IRC logon sequence sent", hnd);
	char *msg[MAX_IRCARGS];
	ic_strNcpy(hnd->mynick, hnd->nick, sizeof hnd->mynick);

	bool success = false;
	int64_t trem = 0;
	do {
		if(tsend) {
			trem = tsend - ic_timestamp_us();
			if (trem <= 0) {
				W("(%p) timeout waiting for 004", hnd);
				ircbas_reset(hnd);
				return false;
			}
		}

		int r = irccon_read(hnd->con, msg, MAX_IRCARGS,
		    (unsigned long)trem);

		if (r < 0) {
			W("(%p) irccon_read() failed", hnd);
			ircbas_reset(hnd);
			return false;
		}

		if (r == 0)
			continue;

		if (hnd->cb_con_read && !hnd->cb_con_read(msg, MAX_IRCARGS,
		    hnd->tag_con_read)) {
			W("(%p) further logon prohibited by conread", hnd);
			ircbas_reset(hnd);
			return false;
		}

		size_t ac = 2;
		while (ac < MAX_IRCARGS && msg[ac])
			ac++;

		bool failure = false;

		/* these are the protocol messages we deal with.
		 * seeing 004 or 383 makes us consider ourselves logged on
		 * note that we do not wait for 005, but we will later
		 * parse it as we ran across it. */
		if (strcmp(msg[1], "001") == 0) {
			if (!(handle_001(hnd, msg, ac)))
				failure = true;
		} else if (strcmp(msg[1], "002") == 0) {
			if (!(handle_002(hnd, msg, ac)))
				failure = true;
		} else if (strcmp(msg[1], "003") == 0) {
			if (!(handle_003(hnd, msg, ac)))
				failure = true;
		} else if (strcmp(msg[1], "004") == 0) {
			if (!(handle_004(hnd, msg, ac)))
				failure = true;
			success = true;
		} else if (strcmp(msg[1], "PING") == 0) {
			if (!(handle_PING(hnd, msg, ac)))
				failure = true;
		} else if (strcmp(msg[1], "432") == 0) { //errorneous nick
			if (!(handle_432(hnd, msg, ac)))
				failure = true;
		} else if (strcmp(msg[1], "433") == 0) { //nick in use
			if (!(handle_433(hnd, msg, ac)))
				failure = true;
		} else if (strcmp(msg[1], "436") == 0) { //nick collision
			if (!(handle_436(hnd, msg, ac)))
				failure = true;
		} else if (strcmp(msg[1], "437") == 0) { //unavail resource
			if (!(handle_437(hnd, msg, ac)))
				failure = true;
		} else if (strcmp(msg[1], "464") == 0) { //passwd missmatch
			handle_464(hnd, msg, ac);
			failure = true;
		} else if (strcmp(msg[1], "383") == 0) { //we're service
			if (!(handle_383(hnd, msg, ac)))
				failure = true;
			success = true;
		} else if (strcmp(msg[1], "484") == 0) { //restricted
			if (!(handle_484(hnd, msg, ac)))
				failure = true;
		} else if (strcmp(msg[1], "465") == 0) { //banned
			if (!(handle_465(hnd, msg, ac)))
				failure = true;
		} else if (strcmp(msg[1], "466") == 0) { //will be banned
			if (!(handle_466(hnd, msg, ac)))
				failure = true;
		} else if (strcmp(msg[1], "ERROR") == 0) {
			handle_ERROR(hnd, msg, ac);
			W("(%p) received error while logging on: %s",
			    hnd, msg[2]);
			failure = true;
		}

		if (failure) {
			char line[1024];
			sndumpmsg(line, sizeof line, NULL, msg, MAX_IRCARGS);
			E("choked on '%s' (colontrail: %d",
			    line, irccon_colon_trail(hnd->con));
			ircbas_reset(hnd);
			return false;
		}

	} while (!success);
	D("(%p) irc logon finished, U R online", hnd);
	return true;
}

bool
ircbas_online(ibhnd_t hnd)
{
	return irccon_online(hnd->con);
}

int
ircbas_read(ibhnd_t hnd, char **tok, size_t tok_len, unsigned long to_us)
{
	//D("(%p) wanna read (timeout: %lu)", hnd, to_us);
	int r = irccon_read(hnd->con, tok, tok_len, to_us);

	if (r == -1 || (r != 0 && !onread(hnd, tok, tok_len))) {
		W("(%p) irccon_read() failed or onread() denied (r:%d)",
		    hnd, r);
		ircbas_reset(hnd);
		return -1;
	}
	//D("(%p) done reading", hnd);

	return r;
}

bool
ircbas_write(ibhnd_t hnd, const char *line)
{
	bool r = irccon_write(hnd->con, line);

	if (!r) {
		W("(%p) irccon_write() failed", hnd);
		ircbas_reset(hnd);
	}

	return r;
}

const char*
ircbas_mynick(ibhnd_t hnd)
{
	return hnd->mynick;
}

const char*
ircbas_myhost(ibhnd_t hnd)
{
	return hnd->myhost;
}

int
ircbas_casemap(ibhnd_t hnd)
{
	return hnd->casemapping;
}

bool
ircbas_service(ibhnd_t hnd)
{
	return hnd->service;
}

const char*
ircbas_umodes(ibhnd_t hnd)
{
	return hnd->umodes;
}

const char*
ircbas_cmodes(ibhnd_t hnd)
{
	return hnd->cmodes;
}

const char*
ircbas_version(ibhnd_t hnd)
{
	return hnd->ver;
}

const char*
ircbas_lasterror(ibhnd_t hnd)
{
	return hnd->lasterr;
}

const char*
ircbas_banmsg(ibhnd_t hnd)
{
	return hnd->banmsg;
}

bool
ircbas_banned(ibhnd_t hnd)
{
	return hnd->banned;
}

bool
ircbas_colon_trail(ibhnd_t hnd)
{
	return irccon_colon_trail(hnd->con);
}

int
ircbas_sockfd(ibhnd_t hnd)
{
	return irccon_sockfd(hnd->con);
}

const char *const *const*
ircbas_logonconv(ibhnd_t hnd)
{
	return (const char *const *const*)hnd->logonconv;
}

const char *const*
ircbas_005chanmodes(ibhnd_t hnd)
{
	return (const char *const*)hnd->m005chanmodes;
}

const char *const*
ircbas_005modepfx(ibhnd_t hnd)
{
	return (const char *const*)hnd->m005modepfx;
}

bool
ircbas_regcb_conread(ibhnd_t hnd, fp_con_read cb, void *tag)
{
	hnd->cb_con_read = cb;
	hnd->tag_con_read = tag;
	return true;
}

bool
ircbas_regcb_mutnick(ibhnd_t hnd, fp_mut_nick cb)
{
	hnd->cb_mut_nick = cb;
	return true;
}

bool
ircbas_set_server(ibhnd_t hnd, const char *host, unsigned short port)
{
	return irccon_set_server(hnd->con, host, port);
}

bool
ircbas_set_proxy(ibhnd_t hnd, const char *host, unsigned short port,
    int ptype)
{
	return irccon_set_proxy(hnd->con, host, port, ptype);
}

bool
ircbas_set_pass(ibhnd_t hnd, const char *srvpass)
{
	char *n = strdup(srvpass);
	if (!n)
		EE("strdup");
	else {
		free(hnd->pass);
		hnd->pass = n;
	}
	return n;
}

bool
ircbas_set_uname(ibhnd_t hnd, const char *uname)
{
	char *n = strdup(uname);
	if (!n)
		EE("strdup");
	else {
		free(hnd->uname);
		hnd->uname = n;
	}
	return n;
}

bool
ircbas_set_fname(ibhnd_t hnd, const char *fname)
{
	char *n = strdup(fname);
	if (!n)
		EE("strdup");
	else {
		free(hnd->fname);
		hnd->fname = n;
	}
	return n;
}

bool
ircbas_set_conflags(ibhnd_t hnd, unsigned flags)
{
	hnd->conflags = flags;
	return true;
}

bool
ircbas_set_nick(ibhnd_t hnd, const char *nick)
{
	char *n = strdup(nick);
	if (!n)
		EE("strdup");
	else {
		free(hnd->nick);
		hnd->nick = n;
	}
	return n;
}

bool
ircbas_set_service_connect(ibhnd_t hnd, bool enabled)
{
	hnd->serv_con = enabled;
	return true;
}

bool
ircbas_set_service_dist(ibhnd_t hnd, const char *dist)
{
	char *n = strdup(dist);
	if (!n)
		EE("strdup");
	else {
		free(hnd->serv_dist);
		hnd->serv_dist = n;
	}
	return n;
}

bool
ircbas_set_service_type(ibhnd_t hnd, long type)
{
	hnd->serv_type = type;
	return true;
}

bool
ircbas_set_service_info(ibhnd_t hnd, const char *info)
{
	char *n = strdup(info);
	if (!n)
		EE("strdup");
	else {
		free(hnd->serv_info);
		hnd->serv_info = n;
	}
	return n;
}

bool
ircbas_set_connect_timeout(ibhnd_t hnd,
    unsigned long soft, unsigned long hard)
{
	hnd->conto_hard_us = hard;
	hnd->conto_soft_us = soft;
	return true;
}

bool
ircbas_set_ssl(ibhnd_t hnd, bool on)
{
	return irccon_set_ssl(hnd->con, on);
}

const char*
ircbas_get_host(ibhnd_t hnd)
{
	return irccon_get_host(hnd->con);
}

unsigned short
ircbas_get_port(ibhnd_t hnd)
{
	return irccon_get_port(hnd->con);
}

const char*
ircbas_get_proxy_host(ibhnd_t hnd)
{
	return irccon_get_proxy_host(hnd->con);
}

unsigned short
ircbas_get_proxy_port(ibhnd_t hnd)
{
	return irccon_get_proxy_port(hnd->con);
}

int
ircbas_get_proxy_type(ibhnd_t hnd)
{
	return irccon_get_proxy_type(hnd->con);
}

const char*
ircbas_get_pass(ibhnd_t hnd)
{
	return hnd->pass;
}

const char*
ircbas_get_uname(ibhnd_t hnd)
{
	return hnd->uname;
}

const char*
ircbas_get_fname(ibhnd_t hnd)
{
	return hnd->fname;
}

unsigned
ircbas_get_conflags(ibhnd_t hnd)
{
	return hnd->conflags;
}

const char*
ircbas_get_nick(ibhnd_t hnd)
{
	return hnd->nick;
}

bool
ircbas_get_service_connect(ibhnd_t hnd)
{
	return hnd->serv_con;
}

const char*
ircbas_get_service_dist(ibhnd_t hnd)
{
	return hnd->serv_dist;
}

long
ircbas_get_service_type(ibhnd_t hnd)
{
	return hnd->serv_type;
}

const char*
ircbas_get_service_info(ibhnd_t hnd)
{
	return hnd->serv_info;
}

bool
ircbas_get_ssl(ibhnd_t hnd)
{
	return irccon_get_ssl(hnd->con);
}

static bool
send_logon(ibhnd_t hnd)
{
	if (!irccon_online(hnd->con))
		return false;
	char aBuf[256];
	char *pBuf = aBuf;
	aBuf[0] = '\0';
	int rem = (int)sizeof aBuf;
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

	return ircbas_write(hnd, aBuf);
}

static bool
onread(ibhnd_t hnd, char **tok, size_t tok_len)
{
	bool failure = false;

	if (strcmp(tok[1], "NICK") == 0) {
		if (!handle_NICK(hnd, tok, countargs(tok, tok_len)))
			failure = true;
	} else if (strcmp(tok[1], "ERROR") == 0) {
		if (!handle_ERROR(hnd, tok, countargs(tok, tok_len)))
			failure = true;
	} else if (strcmp(tok[1], "005") == 0) {
		if (!handle_005(hnd, tok, countargs(tok, tok_len)))
			failure = true;
	}

	if (failure) {
		char line[1024];
		sndumpmsg(line, sizeof line, NULL, tok, tok_len);
		E("choked on '%s' (colontrail: %d",
		    line, irccon_colon_trail(hnd->con));
	}

	return !failure;
}
