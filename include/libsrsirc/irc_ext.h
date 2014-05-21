/* irc_ext.h - Front-end (main) interface.
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef SRS_IRC_EXT_H
#define SRS_IRC_EXT_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/irc.h>
#include <libsrsirc/irc_defs.h>

const char *irc_myhost(irc hnd);
int irc_casemap(irc hnd);
bool irc_service(irc hnd);
const char *irc_umodes(irc hnd);
const char *irc_cmodes(irc hnd);
const char *irc_version(irc hnd);
const char *irc_lasterror(irc hnd);
const char *irc_banmsg(irc hnd);
bool irc_banned(irc hnd);
bool irc_colon_trail(irc hnd);
int irc_sockfd(irc hnd);
const char *const *const *irc_logonconv(irc hnd);
const char *const *irc_005chanmodes(irc hnd);
const char *const *irc_005modepfx(irc hnd);

bool irc_regcb_conread(irc hnd, fp_con_read cb, void *tag);
bool irc_regcb_mutnick(irc hnd, fp_mut_nick);

bool irc_set_proxy(irc hnd, const char *host, uint16_t port, int ptype);
bool irc_set_conflags(irc hnd, unsigned flags);
bool irc_set_service_connect(irc hnd, bool enabled);
bool irc_set_service_dist(irc hnd, const char *dist);
bool irc_set_service_type(irc hnd, long type);
bool irc_set_service_info(irc hnd, const char *info);
bool irc_set_connect_timeout(irc hnd, uint64_t soft, uint64_t hard);
bool irc_set_ssl(irc hnd, bool on);

const char* irc_get_host(irc hnd);
uint16_t irc_get_port(irc hnd);
const char* irc_get_proxy_host(irc hnd);
uint16_t irc_get_proxy_port(irc hnd);
int irc_get_proxy_type(irc hnd);
const char* irc_get_pass(irc hnd);
const char* irc_get_uname(irc hnd);
const char* irc_get_fname(irc hnd);
unsigned irc_get_conflags(irc hnd);
const char* irc_get_nick(irc hnd);
bool irc_get_service_connect(irc hnd);
const char* irc_get_service_dist(irc hnd);
long irc_get_service_type(irc hnd);
const char* irc_get_service_info(irc hnd);
bool irc_get_ssl(irc hnd);

#endif /* SRS_IRC_EXT_H */
