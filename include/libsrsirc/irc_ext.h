/* irc_ext.h - less commonly used part of the main interface
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_EXT_H
#define LIBSRSIRC_IRC_EXT_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/irc.h>
#include <libsrsirc/defs.h>
/* Overview:
 *
 * const char *irc_myhost(irc hnd);
 * int irc_casemap(irc hnd);
 * bool irc_service(irc hnd);
 * const char *irc_umodes(irc hnd);
 * const char *irc_cmodes(irc hnd);
 * const char *irc_version(irc hnd);
 * const char *irc_lasterror(irc hnd);
 * const char *irc_banmsg(irc hnd);
 * bool irc_banned(irc hnd);
 * bool irc_colon_trail(irc hnd);
 * int irc_sockfd(irc hnd);
 * tokarr *(*irc_logonconv(irc hnd))[4];
 * const char* irc_005chanmodes(irc hnd, size_t class);
 * const char* irc_005modepfx(irc hnd, bool symbols);
 *
 * void irc_regcb_conread(irc hnd, fp_con_read cb, void *tag);
 * void irc_regcb_mutnick(irc hnd, fp_mut_nick mn);
 *
 * bool irc_set_ssl(irc hnd, bool on);
 * void irc_set_connect_timeout(irc hnd, uint64_t soft, uint64_t hard);
 * bool irc_set_proxy(irc hnd, const char *host, uint16_t port, int ptype);
 * void irc_set_conflags(irc hnd, uint8_t flags);
 * void irc_set_service_connect(irc hnd, bool enabled);
 * bool irc_set_service_dist(irc hnd, const char *dist);
 * bool irc_set_service_type(irc hnd, long type);
 * bool irc_set_service_info(irc hnd, const char *info);
 *
 * const char* irc_get_host(irc hnd);
 * uint16_t irc_get_port(irc hnd);
 * const char* irc_get_px_host(irc hnd);
 * uint16_t irc_get_px_port(irc hnd);
 * int irc_get_px_type(irc hnd);
 * const char* irc_get_pass(irc hnd);
 * const char* irc_get_uname(irc hnd);
 * const char* irc_get_fname(irc hnd);
 * uint8_t irc_get_conflags(irc hnd);
 * const char* irc_get_nick(irc hnd);
 * bool irc_get_ssl(irc hnd);
 * bool irc_get_service_connect(irc hnd);
 * const char* irc_get_service_dist(irc hnd);
 * long irc_get_service_type(irc hnd);
 * const char* irc_get_service_info(irc hnd);
 */

/* irc_myhost - tell who ircd claimed to be in 004.  Don't rely upon. */
const char *irc_myhost(irc hnd);

/* irc_casemap - tell what "case mapping" the server advertised in 005
 * Returns one of CMAP_* constants; CMAP_RFC1459 if there was no 005 */
int irc_casemap(irc hnd);

/* irc_service - tell whether or not we're a service */
bool irc_service(irc hnd);

/* irc_umodes - tell the (as per 004) advertised supported user modes
 * Returns a string of supported mode chars, e.g. "iswo". */
const char *irc_umodes(irc hnd);

/* irc_cmodes - tell the (as per 004) advertised supported channel modes
 * Returns a string similar to irc_umodes() (but see also irc_005chanmodes()) */
const char *irc_cmodes(irc hnd);

/* irc_version - tell what ircd version the ircd advertised in 004 */
const char *irc_version(irc hnd);

/* irc_lasterror - tell the last received ERROR message, or NULL if none */
const char *irc_lasterror(irc hnd);

/* irc_banmsg - tell the 'ban reason' the server MIGHT have told us w/ 465 */
const char *irc_banmsg(irc hnd);

/* irc_banned - tell whether the server told us w/ a 465 that we're banned */
bool irc_banned(irc hnd);

/* irc_colon_trail - tell whether the last argument of the most recently
 *     read line was a "trailing" argument, i.e. was prefixed by a colon.
 *     This is as useless as it sounds, don't use. */
bool irc_colon_trail(irc hnd);

/* irc_sockfd - return the underlying socket file descriptor. This is absolutely
 *     insane, do not use, it will be removed soon, with a better solution
 *     to what this tries to accomplish (i/o multiplexing) */
int irc_sockfd(irc hnd);

/* irc_logonconv - give access to the "log on conversation" (messages 001, 002
       003 and 004, which are consumed in the log on process.
 * Returns a pointer to an array 4 of pointer of (the pertinent) tokarrs.  */
tokarr *(*irc_logonconv(irc hnd))[4];

/* irc_005chanmodes - tell what channel modes the ircd claimed to support in
 *     the (non-standard but almost universally implemented) 005-message.
 * Params: `class': "class" of channel modes to return (there are four, 0 - 3)
 * Returns a pointer to a string consisting of the supported channel modes
 *     for the given class (see 005 ISUPPORT "spec") */
const char *irc_005chanmodes(irc hnd, size_t class);

/* irc_005modepfx - tell what channel mode prefixes the ircd claimed to support
 *     in the (non-standard) but almost universally implemented) 005-message.
 * Params: `symbols': Determine whether (true) we return the mode prefix
 *             symbols (e.g. "@+"), or (when false) the associated mode
 *             letters (e.g. "ov").
 * Returns a string of mode symbols or letters, in descending order of power. */
const char* irc_005modepfx(irc hnd, bool symbols);

/* irc_regcb_conread - register callback for msgs which are read at logon time
 * Params: `cb': a function pointer of appropriate type (see defs.h)
 *         `tag`: user data, passed to `cb' as-is on every call
 * If the registered function returns false, the attempt to log on is aborted */
void irc_regcb_conread(irc hnd, fp_con_read cb, void *tag);

/* irc_regcb_mutnick - register a function which is called when, while logging
 *     on, the nickname we want is not available.  the supplied function is
 *     supposed to come up with a new nickname.  there is a default which is
 *     probably okay for most cases.
 * Params: `mn': a function pointer of appropriate type, or NULL, which means
 *     logging on fails if the nick we want is unavailable */
void irc_regcb_mutnick(irc hnd, fp_mut_nick mn);

/* irc_set_ssl - enable or disable SSL (lib must be compiled with SSL support)
 * This setting will take effect on the next call to irc_connect().
 * Returns true if the request could be fulfilled */
bool irc_set_ssl(irc hnd, bool on);

/* irc_set_connect_timeout - set "hard" and "soft" timeout for irc_connect()
 *     The "hard" timeout is the maximum overall time spent in irc_connect().
 *     The "soft" timeout is the maximum time spent in connect(2), for each
 *         address which is tried (potentially many due to DNS resolving) anew.
 *     If in conflict, the hard timeout will overrule.  0 means no timeout. */
void irc_set_connect_timeout(irc hnd, uint64_t soft, uint64_t hard);

/* irc_set_px - set HTTP/SOCKS4/SOCKS5 proxy server to use
 * Params: `host':  Proxy host, may be an IPv4 or IPv6 address or a DNS name
 *         `port':  Proxy port (1 <= `port' <= 65535)
 *         `ptype`: Proxy type, one of IRCPX_[HTTP|SOCKS4|SOCKS5].
 * Returns true on success, false on failure (out of memory, illegal args) */
bool irc_set_px(irc hnd, const char *host, uint16_t port, int ptype);

/* irc_set_conflags - set flags for the USER message at log on time.  This is
 *     a bit mask for which RFC2812 defines two bits: bit 3 (i.e. 8) leads to
 *     user mode +i being set, bit 2 (i.e. 4) to user mode +w.  RFC1459 doesn't
 *     define this, so it should probably not be relied upon. */
void irc_set_conflags(irc hnd, uint8_t flags);

/* irc_set_service_connect - log on as service? if not, we're a normal client */
void irc_set_service_connect(irc hnd, bool enabled);

/* irc_set_service_dist - set the "distribution" string for service log on */
bool irc_set_service_dist(irc hnd, const char *dist);

/* irc_set_service_type - set the "service type" for service log on */
bool irc_set_service_type(irc hnd, long type);

/* irc_set_service_info - set the "service info" string for service log on */
bool irc_set_service_info(irc hnd, const char *info);

/* irc_get_host - tell the host of the irc server we use or intend to use */
const char* irc_get_host(irc hnd);

/* irc_get_port - tell the port of the irc server we use or intend to use
 * A return value of zero means we use the default port */
uint16_t irc_get_port(irc hnd);

/* irc_get_px_host - tell the host of the proxy we use or intend to use */
const char* irc_get_px_host(irc hnd);

/* irc_get_px_port - tell the port of the proxy we use or intend to use */
uint16_t irc_get_px_port(irc hnd);

/* irc_get_px_type - tell the type of the proxy we use or intend to use
 * Returns one of IRCPX_[HTTP|SOCKS4|SOCKS5] or -1 for no proxy */
int irc_get_px_type(irc hnd);

/* irc_get_pass - tell the password we may have set earlier by irc_set_pass() */
const char* irc_get_pass(irc hnd);

/* irc_get_uname - tell the 'user name' we will use on the next log on */
const char* irc_get_uname(irc hnd);

/* irc_get_fname - tell the 'full name' we will use on the next log on */
const char* irc_get_fname(irc hnd);

/* irc_get_conflags - tell the USER flags we will use on the next log on */
uint8_t irc_get_conflags(irc hnd);

/* irc_get_nick - tell the nick we will use on the next log on */
const char* irc_get_nick(irc hnd);

/* irc_get_ssl - tell whether or not we will use SSL for the next connection */
bool irc_get_ssl(irc hnd);

/* well you get the idea... */
bool irc_get_service_connect(irc hnd);
const char* irc_get_service_dist(irc hnd);
long irc_get_service_type(irc hnd);
const char* irc_get_service_info(irc hnd);

#endif /* LIBSRSIRC_IRC_EXT_H */
