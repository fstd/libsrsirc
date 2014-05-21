/* irc_ext.c - Front-end implementation
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "iconn.h"

#include <intlog.h>

#include "common.h"
#include <libsrsirc/irc_ext.h>

const char*
irc_myhost(irc hnd)
{
	return hnd->myhost;
}

int
irc_casemap(irc hnd)
{
	return hnd->casemapping;
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
	return icon_colon_trail(hnd->con);
}

int
irc_sockfd(irc hnd)
{
	return icon_sockfd(hnd->con);
}

const char *const *const*
irc_logonconv(irc hnd)
{
	return (const char *const *const*)hnd->logonconv;
}

const char *const*
irc_005chanmodes(irc hnd)
{
	return (const char *const*)hnd->m005chanmodes;
}

const char *const*
irc_005modepfx(irc hnd)
{
	return (const char *const*)hnd->m005modepfx;
}

bool
irc_regcb_conread(irc hnd, fp_con_read cb, void *tag)
{
	hnd->cb_con_read = cb;
	hnd->tag_con_read = tag;
	return true;
}

bool
irc_regcb_mutnick(irc hnd, fp_mut_nick cb)
{
	hnd->cb_mut_nick = cb;
	return true;
}

bool
irc_set_proxy(irc hnd, const char *host, uint16_t port, int ptype)
{
	return icon_set_proxy(hnd->con, host, port, ptype);
}

bool
irc_set_conflags(irc hnd, unsigned flags)
{
	hnd->conflags = flags;
	return true;
}

bool
irc_set_service_connect(irc hnd, bool enabled)
{
	hnd->serv_con = enabled;
	return true;
}

bool
irc_set_service_dist(irc hnd, const char *dist)
{
	return update_strprop(&hnd->serv_dist, dist);
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
	return update_strprop(&hnd->serv_info, info);
}

bool
irc_set_connect_timeout(irc hnd, uint64_t soft, uint64_t hard)
{
	hnd->conto_hard_us = hard;
	hnd->conto_soft_us = soft;
	return true;
}

bool
irc_set_ssl(irc hnd, bool on)
{
	return icon_set_ssl(hnd->con, on);
}

const char*
irc_get_host(irc hnd)
{
	return icon_get_host(hnd->con);
}

uint16_t
irc_get_port(irc hnd)
{
	return icon_get_port(hnd->con);
}

const char*
irc_get_proxy_host(irc hnd)
{
	return icon_get_proxy_host(hnd->con);
}

uint16_t
irc_get_proxy_port(irc hnd)
{
	return icon_get_proxy_port(hnd->con);
}

int
irc_get_proxy_type(irc hnd)
{
	return icon_get_proxy_type(hnd->con);
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

unsigned
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
	return icon_get_ssl(hnd->con);
}
