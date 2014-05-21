/* irc_ext.c - Front-end implementation
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "irc_con.h"

#include <intlog.h>

#include <common.h>
#include <libsrsirc/irc_ext.h>

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
ircbas_set_proxy(ibhnd_t hnd, const char *host, unsigned short port,
    int ptype)
{
	return irccon_set_proxy(hnd->con, host, port, ptype);
}

bool
ircbas_set_conflags(ibhnd_t hnd, unsigned flags)
{
	hnd->conflags = flags;
	return true;
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
	return update_strprop(&hnd->serv_dist, dist);
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
	return update_strprop(&hnd->serv_info, info);
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
