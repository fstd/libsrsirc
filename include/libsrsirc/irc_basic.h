/* irc_basic.h - Front-end (main) interface.
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef SRS_IRC_BASIC_H
#define SRS_IRC_BASIC_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/ihnd.h>

ibhnd_t ircbas_init(void);
bool ircbas_reset(ibhnd_t hnd);
bool ircbas_dispose(ibhnd_t hnd);
bool ircbas_connect(ibhnd_t hnd);
int ircbas_read(ibhnd_t hnd, char **tok, size_t tok_len,
    unsigned long to_us);
bool ircbas_write(ibhnd_t hnd, const char *line);
bool ircbas_online(ibhnd_t hnd);

const char *ircbas_mynick(ibhnd_t hnd);
const char *ircbas_myhost(ibhnd_t hnd);
int ircbas_casemap(ibhnd_t hnd);
bool ircbas_service(ibhnd_t hnd);
const char *ircbas_umodes(ibhnd_t hnd);
const char *ircbas_cmodes(ibhnd_t hnd);
const char *ircbas_version(ibhnd_t hnd);
const char *ircbas_lasterror(ibhnd_t hnd);
const char *ircbas_banmsg(ibhnd_t hnd);
bool ircbas_banned(ibhnd_t hnd);
bool ircbas_colon_trail(ibhnd_t hnd);

bool ircbas_regcb_conread(ibhnd_t hnd, fp_con_read cb, void *tag);
bool ircbas_regcb_mutnick(ibhnd_t hnd, fp_mut_nick);
//getter/setter:
bool ircbas_set_server(ibhnd_t hnd, const char *host, unsigned short port);
bool ircbas_set_proxy(ibhnd_t hnd, const char *host, unsigned short port,
    int ptype);
bool ircbas_set_pass(ibhnd_t hnd, const char *srvpass);
bool ircbas_set_uname(ibhnd_t hnd, const char *uname);
bool ircbas_set_fname(ibhnd_t hnd, const char *fname);
bool ircbas_set_conflags(ibhnd_t hnd, unsigned flags);
bool ircbas_set_nick(ibhnd_t hnd, const char *nick);
bool ircbas_set_service_connect(ibhnd_t hnd, bool enabled);
bool ircbas_set_service_dist(ibhnd_t hnd, const char *dist);
bool ircbas_set_service_type(ibhnd_t hnd, long type);
bool ircbas_set_service_info(ibhnd_t hnd, const char *info);
bool ircbas_set_connect_timeout(ibhnd_t hnd,
    unsigned long soft, unsigned long hard);

const char *ircbas_get_host(ibhnd_t hnd);
unsigned short ircbas_get_port(ibhnd_t hnd);
const char *ircbas_get_pass(ibhnd_t hnd);
const char *ircbas_get_uname(ibhnd_t hnd);
const char *ircbas_get_fname(ibhnd_t hnd);
unsigned ircbas_get_conflags(ibhnd_t hnd);
const char *ircbas_get_nick(ibhnd_t hnd);
bool ircbas_get_service_connect(ibhnd_t hnd);
const char *ircbas_get_service_dist(ibhnd_t hnd);
long ircbas_get_service_type(ibhnd_t hnd);
const char *ircbas_get_service_info(ibhnd_t hnd);
const char *ircbas_get_proxy_host(ibhnd_t hnd);
unsigned short ircbas_get_proxy_port(ibhnd_t hnd);
int ircbas_get_proxy_type(ibhnd_t hnd);
bool ircbas_set_ssl(ibhnd_t hnd, bool on);
bool ircbas_get_ssl(ibhnd_t hnd);

int ircbas_sockfd(ibhnd_t hnd);

const char* const* const* ircbas_logonconv(ibhnd_t hnd);
const char* const* ircbas_005chanmodes(ibhnd_t hnd);
const char* const* ircbas_005modepfx(ibhnd_t hnd);

#endif /* SRS_IRC_BASIC_H */
