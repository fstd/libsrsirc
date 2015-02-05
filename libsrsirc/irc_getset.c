/* irc_getset.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_IRC

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <stdbool.h>

#include <logger/intlog.h>

#include "common.h"
#include "conn.h"
#include "msg.h"


/* Determiners - read-only access to information we keep track of */

bool
irc_online(irc hnd)
{
	return conn_online(hnd->con);
}

const char*
irc_mynick(irc hnd)
{
	return hnd->mynick;
}

const char*
irc_myhost(irc hnd)
{
	return hnd->myhost;
}

int
irc_casemap(irc hnd)
{
	return hnd->casemap;
}

bool
irc_service(irc hnd)
{
	return hnd->service;
}

const char*
irc_umodes(irc hnd)
{
	return hnd->umodes;
}

const char*
irc_cmodes(irc hnd)
{
	return hnd->cmodes;
}

const char*
irc_version(irc hnd)
{
	return hnd->ver;
}

const char*
irc_lasterror(irc hnd)
{
	return hnd->lasterr;
}

const char*
irc_banmsg(irc hnd)
{
	return hnd->banmsg;
}

bool
irc_banned(irc hnd)
{
	return hnd->banned;
}

bool
irc_colon_trail(irc hnd)
{
	return conn_colon_trail(hnd->con);
}

int
irc_sockfd(irc hnd)
{
	return conn_sockfd(hnd->con);
}

tokarr *(* /* ew.  returns pointer to array of 4 pointers to tokarr */
irc_logonconv(irc hnd))[4]
{
	return &hnd->logonconv;
}

const char*
irc_005chanmodes(irc hnd, size_t class) /* suck it, $(CXX) */
{
	if (class >= COUNTOF(hnd->m005chanmodes))
		return NULL;
	return hnd->m005chanmodes[class];
}

const char*
irc_005modepfx(irc hnd, bool symbols)
{
	return hnd->m005modepfx[symbols];
}


bool
irc_tracking_enab(irc hnd)
{
	return hnd->tracking && hnd->tracking_enab;
}

/* Getters - retrieve values previously set by the Setters */

const char*
irc_get_host(irc hnd)
{
	return conn_get_host(hnd->con);
}

uint16_t
irc_get_port(irc hnd)
{
	return conn_get_port(hnd->con);
}

const char*
irc_get_px_host(irc hnd)
{
	return conn_get_px_host(hnd->con);
}

uint16_t
irc_get_px_port(irc hnd)
{
	return conn_get_px_port(hnd->con);
}

int
irc_get_px_type(irc hnd)
{
	return conn_get_px_type(hnd->con);
}

const char*
irc_get_pass(irc hnd)
{
	return hnd->pass;
}

const char*
irc_get_uname(irc hnd)
{
	return hnd->uname;
}

const char*
irc_get_fname(irc hnd)
{
	return hnd->fname;
}

uint8_t
irc_get_conflags(irc hnd)
{
	return hnd->conflags;
}

const char*
irc_get_nick(irc hnd)
{
	return hnd->nick;
}

bool
irc_get_service_connect(irc hnd)
{
	return hnd->serv_con;
}

const char*
irc_get_service_dist(irc hnd)
{
	return hnd->serv_dist;
}

long
irc_get_service_type(irc hnd)
{
	return hnd->serv_type;
}

const char*
irc_get_service_info(irc hnd)
{
	return hnd->serv_info;
}

bool
irc_get_ssl(irc hnd)
{
	return conn_get_ssl(hnd->con);
}


/* Setters - set library parameters (none of these takes effect before the
 * next call to irc_connect() is done */


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

bool
irc_set_px(irc hnd, const char *host, uint16_t port, int ptype)
{
	return conn_set_px(hnd->con, host, port, ptype);
}

void
irc_set_conflags(irc hnd, uint8_t flags)
{
	hnd->conflags = flags;
}

void
irc_set_service_connect(irc hnd, bool enabled)
{
	hnd->serv_con = enabled;
}

bool
irc_set_service_dist(irc hnd, const char *dist)
{
	return com_update_strprop(&hnd->serv_dist, dist);
}

bool
irc_set_service_type(irc hnd, long type)
{
	hnd->serv_type = type;
	return true;
}

bool
irc_set_service_info(irc hnd, const char *info)
{
	return com_update_strprop(&hnd->serv_info, info);
}

void
irc_set_track(irc hnd, bool on)
{
	hnd->tracking = on;
}

void
irc_set_connect_timeout(irc hnd, uint64_t soft, uint64_t hard)
{
	hnd->hcto_us = hard;
	hnd->scto_us = soft;
}

bool
irc_set_ssl(irc hnd, bool on)
{
	return conn_set_ssl(hnd->con, on);
}

bool
irc_get_dumb(irc hnd)
{
	return hnd->dumb;
}

void
irc_set_dumb(irc hnd, bool dumbmode)
{
	hnd->dumb = dumbmode;
}

bool
irc_reg_msghnd(irc hnd, const char *cmd, uhnd_fn hndfn, bool pre)
{
	return msg_reguhnd(hnd, cmd, hndfn, pre);
}
