/* irc_ext.h - less commonly used part of the main interface
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_EXT_H
#define LIBSRSIRC_IRC_EXT_H 1


#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include <libsrsirc/irc.h>


/* Overview:
 *
 * const char *irc_myhost(irc *hnd);
 * int irc_casemap(irc *hnd);
 * bool irc_service(irc *hnd);
 * const char *irc_umodes(irc *hnd);
 * const char *irc_cmodes(irc *hnd);
 * const char *irc_version(irc *hnd);
 * const char *irc_lasterror(irc *hnd);
 * const char *irc_banmsg(irc *hnd);
 * bool irc_banned(irc *hnd);
 * bool irc_colon_trail(irc *hnd);
 * int irc_sockfd(irc *hnd);
 * tokarr *(*irc_logonconv(irc *hnd))[4];
 * const char *irc_005chanmodes(irc *hnd, size_t mclass);
 * const char *irc_005modepfx(irc *hnd, bool symbols);
 *
 * void irc_regcb_conread(irc *hnd, fp_con_read cb, void *tag);
 * void irc_regcb_mutnick(irc *hnd, fp_mut_nick mn);
 *
 * bool irc_set_ssl(irc *hnd, bool on);
 * void irc_set_connect_timeout(irc *hnd, uint64_t soft, uint64_t hard);
 * bool irc_set_px(irc *hnd, const char *host, uint16_t port, int ptype);
 * void irc_set_conflags(irc *hnd, uint8_t flags);
 * void irc_set_service_connect(irc *hnd, bool enabled);
 * bool irc_set_service_dist(irc *hnd, const char *dist);
 * bool irc_set_service_type(irc *hnd, long type);
 * bool irc_set_service_info(irc *hnd, const char *info);
 * bool irc_set_track(irc *hnd, bool on);
 *
 * const char *irc_get_host(irc *hnd);
 * uint16_t irc_get_port(irc *hnd);
 * const char *irc_get_px_host(irc *hnd);
 * uint16_t irc_get_px_port(irc *hnd);
 * int irc_get_px_type(irc *hnd);
 * const char *irc_get_pass(irc *hnd);
 * const char *irc_get_uname(irc *hnd);
 * const char *irc_get_fname(irc *hnd);
 * uint8_t irc_get_conflags(irc *hnd);
 * const char *irc_get_nick(irc *hnd);
 * bool irc_get_ssl(irc *hnd);
 * bool irc_get_service_connect(irc *hnd);
 * const char *irc_get_service_dist(irc *hnd);
 * long irc_get_service_type(irc *hnd);
 * const char *irc_get_service_info(irc *hnd);
 * bool irc_tracking_enab(irc *hnd); //tell if tracking is (actually) enabled
 * bool irc_get_dumb(irc *hnd);
 * void irc_set_dumb(irc *hnd, bool dumbmode);
 * bool irc_can_read(irc *hnd);
 * bool irc_reg_msghnd(irc *hnd, const char *cmd, uhnd_fn hndfn, bool pre);
 * bool irc_eof(irc *hnd);
 */


/** @file
 * \defgroup extif Extended interface provided by irc_ext.h
 * \addtogroup extif
 *  @{
 */


/** \brief Tell who the ircd claimed to be in the 004 message.
 * \return Whoever the server pretends to be in 004.  Typically, a FQDN. */
const char *irc_myhost(irc *hnd);

/** \brief Tell what "case mapping" the server advertised in 005
 * \return One of CMAP_* constants; CMAP_RFC1459 if there was no 005
 *
 * See CMAP_ASCII, CMAP_RFC1459, CMAP_STRICT_RFC1459
 */
int irc_casemap(irc *hnd);

/** \brief Tell whether or not we're a service
 * \return True if we are a service, false if we are a normal client */
bool irc_service(irc *hnd);

/** \brief Tell the (as per 004) supported user modes
 * \return A string of supported mode chars, e.g. "iswo". */
const char *irc_umodes(irc *hnd);

/** \brief Tell the (as per 004) supported channel modes
 *
 * As of widespread adoption of the 005 message, the information returned
 * by this function is likely *wrong*.  Use irc_005chanmodes().
 *
 * \return A string similar to irc_umodes() (but see also irc_005chanmodes()) */
const char *irc_cmodes(irc *hnd);

/** \brief Tell what ircd version the ircd advertised in 004
 * \return A ircd-specific version string from the 004 message */
const char *irc_version(irc *hnd);

/** \brief Tell the last received ERROR message
 * \return The argument of the last received ERROR message, or NULL if there
 *         was none so far. */
const char *irc_lasterror(irc *hnd);

/** \brief Tell the 'ban reason' the server MIGHT have told us w/ 465
 *
 * Sometimes, ircds tell us why/that we are banned using the 465 message.
 *
 * \return The argument of the last received 465 message, or NULL if there was
 *         none so far. */
const char *irc_banmsg(irc *hnd);

/** \brief Tell whether the server told us w/ a 465 that we're banned
 * \return true if a 465 has been seen. */
bool irc_banned(irc *hnd);

/** \brief Deprecated.  Please disregard.
 *
 * Tell whether the last argument of the most recently read line was a
 * "trailing" argument, i.e. was prefixed by a colon.
 *
 * \return true if so.
 *
 * This is as useless as it sounds, don't use.  It was a hack, and it will
 * be removed in the near future. */
bool irc_colon_trail(irc *hnd);

/** \brief Deprecated.  Please disregard.
 *
 * This gives us access to the underlying socket. This is absolutely insane,
 * do not use, it will be removed soon, with a better solution to what this
 * tries to accomplish (i/o multiplexing)
 *
 * \returns The socket.
 */
int irc_sockfd(irc *hnd);

/** \brief Give access to the "log on conversation" (messages 001-004)
 *
 * The messages 001-004 are consumed in the process of logging on to IRC;
 * this function provides access to them after logon is complete, should the
 * need arise.
 *
 * The return type is a bit twisted.
 *
 * \return A pointer to an array of 4 elements, each pointing to a tokarr.
 *         (i.e. one for each of 001, 002, 003, 004). */
tokarr *(*irc_logonconv(irc *hnd))[4];

/** \brief Tell what channel modes the ircd claims to support as per 005
 *
 * 005 is a non-standard but almost universally implemented message through
 * which and ircd advertises features it supports or characteristics it has.
 *
 * 005 specifies four classes of channel modes (see description of
 * CHANMODE_CLASS_A, CHANMODE_CLASS_B, CHANMODE_CLASS_C, CHANMODE_CLASS_D).
 *
 * This function can be used to query the supported channel modes of a given
 * class
 *
 * \param mclass: class of channel modes to return (see above)
 * \return A pointer to a string consisting of the supported channel modes
 *     for the given class */
const char *irc_005chanmodes(irc *hnd, size_t mclass);

/** \brief Tell what channel mode prefixes the ircd claims to support as per 005
 *
 * 005 is a non-standard but almost universally implemented message through
 * which and ircd advertises features it supports or characteristics it has.
 *
 * Channel mode prefixes come in two flavors, letters (o, v) and symbols (@, +)
 * (for the default ops and voice, respectively.)
 *
 * This function can be used to determine what channel mode prefixes the
 * ircd supports.
 *
 * \param symbols   If true, use symbols, otherwise use letters
 * \return A string of available mode symbols or letters, in descending order
 *         of power.  For example, "@+" or "ov". */
const char *irc_005modepfx(irc *hnd, bool symbols);

/** \brief Register callback for protocol messages that are read at logon time
 *
 * \param cb   Function pointer to the callback function to be registered
 * \param tag   Arbitrary userdata that is passed back to the callback as-is
 *
 * See uhnd_fn for semantics of the callback. */ // XXX xref
void irc_regcb_conread(irc *hnd, fp_con_read cb, void *tag);

/** \brief Register a function to come up with an alternative nickname at logon
 *         time
 *
 * \param mn   Function pointer to the callback function to be registered.
 *             If NULL, the attempt to logon will be aborted if we can't have
 *             the nickname we want.
 * \param tag   Arbitrary userdata that is passed back to the callback as-is
 *
 * A default callback is provided which is probably okay for most cases. */
void irc_regcb_mutnick(irc *hnd, fp_mut_nick mn);

/** \brief Enable or disable SSL
 *
 * In order to use this, libsrsirc must be compiled with SSL support
 * (`./configure --with-ssl`)
 *
 * \param on   True to enable, false to disable SSL
 *
 * This setting will take effect not before the next call to irc_connect().
 *
 * \return true if the request could be fulfilled. *Be sure to check this*,
 *         to avoid accidentally doing plaintext when you meant to use SSL */
bool irc_set_ssl(irc *hnd, bool on);

/** \brief Set timeout(s) for irc_connect()
 *
 * Connecting to an IRC server might involve trying a bunch of addresses, since
 * the main entry points to IRC networks typically resolve to many different
 * servers (e.g. irc.freenode.org currently (2015) resolves to 24 different
 * addresses.)  However, we cannot know in advance how many it will be and
 * which one will end up working for us, so two timeouts are introduced.
 *
 * The *hard timeout* limits the overall time we can possibly spend in
 * irc_connect().
 *
 * The *soft timeout* limits the time we will spend trying to connect to each
 * individual address that our server hostname resolved to (only until one works
 * for us, of course).  This timeout will be stretched if found to be smaller
 * than necessary.
 *
 * Generally, no matter what you set the soft timeout to, you can expect the
 * irc_connect() function to give up after the /hard/ timeout is exceeded.
 * This is because a too-long soft timeout can never overrule the hard timeout,
 * and because a too-small soft timeout will be stretched once the number of
 * addresses to be tried is known.
 *
 * For example, suppose the hard timeout is *10* seconds, the soft timeout is
 * *1* second, and our irc server resolved to *4* addresses.
 *
 * This will cause the soft timeout to be raised to 2.5 seconds, so that
 * each of the 4 addresses is given the maximum possible time to react while
 * staying within the bounds of the hard limit.
 *
 * \param soft   The soft limit, in microseconds (see above)
 * \param hard   The hard limit, in microseconds (see above)
 *
 * A timeout of 0 means no timeout (but still, a hard timeout >0 overrules
 * a soft timeout of 0) */
void irc_set_connect_timeout(irc *hnd, uint64_t soft, uint64_t hard);

/** \brief Set proxy server to use
 *
 * libsrsirc supports redirecting the IRC connection through a proxy server
 *
 * Supported proxy types are HTTP CONNECT, SOCKS4 and SOCKS5.
 *
 * \param host   Proxy host, may be an IPv4 or IPv6 address or a DNS name
 * \param port   Proxy port (1 <= `port' <= 65535)
 * \param ptype   Proxy type, one of IRCPX_HTTP, IRCPX_SOCKS4, IRCPX_SOCKS5.
 * \return true on success, false on failure (out of memory, illegal args) */
bool irc_set_px(irc *hnd, const char *host, uint16_t port, int ptype);

/** \brief Set flags for the USER message at log on time.
 *
 * This is a bit mask for which RFC2812 defines two bits: bit 3 (i.e. 8) leads
 * to user mode +i being set, bit 2 (i.e. 4) to user mode +w.  RFC1459 doesn't
 *     define this, so it should probably not be relied upon.
 *
 * \param flags   Connect flags for the USER message
 */
void irc_set_conflags(irc *hnd, uint8_t flags);

/** \brief Log on as service? if not, we're a normal client
 *
 * This function can be used to tell libsrsirc that our next attempt to
 * irc_connect() should do a service logon, rather than a user logon.
 * This is pretty much untested.
 *
 * \param enabled   If true, we're trying to be a service.  If not, we're a user.
 */
void irc_set_service_connect(irc *hnd, bool enabled);

/** \brief Set the "distribution" string for service log on
 * \param dist   The dist string provided with a service logon
 * \return True if successful, false otherwise (which means we're out of memory)
 */
bool irc_set_service_dist(irc *hnd, const char *dist);

/** \brief Set the "service type" for service log on
 * \param type   The service type identifier provided with a service logon
 * \return True.
 */
bool irc_set_service_type(irc *hnd, long type);

/** \brief Set the "service info" string for service log on
 * \param info   The info string provided with a service logon
 * \return True if successful, false otherwise (which means we're out of memory)
 */
bool irc_set_service_info(irc *hnd, const char *info);

/** \brief Enable or disable channel- and user tracking
 *
 * libsrsirc has a sizeable portion of code concerned with keeping track of
 * channels we're in and users we can see.  This is useful for implementing
 * clients on top of libsrsirc, but simpler applications won't require it.
 *
 * Therefore, tracking is disabled by default.  This function can be used to
 * enable it.
 *
 * \param on   True to enable tracking, false to disable
 *
 * This setting will take effect not before the next call to irc_connect().
 */
bool irc_set_track(irc *hnd, bool on);

/** \brief Tell the name or address of the IRC server we use or intend to use
 * \return The hostname-part of what was set using irc_set_server() */
const char *irc_get_host(irc *hnd);

/** \brief Tell the port of the IRC server we use or intend to use
 * \return The port-part of what was set using irc_set_server() */
uint16_t irc_get_port(irc *hnd);

/** \brief Tell the host of the proxy we use or intend to use
 * \return The hostname-part of what was set using irc_set_px() */
const char *irc_get_px_host(irc *hnd);

/** \brief Tell the port of the proxy we use or intend to use
 * \return The port-part of what was set using irc_set_px() */
uint16_t irc_get_px_port(irc *hnd);

/** \brief Tell the type of the proxy we use or intend to use
 *
 * \return The type-part of what was set using irc_set_px(),
 *         i.e. one of IRCPX_HTTP, IRCPX_SOCKS4, IRCPX_SOCKS5 or -1 for no
 *         proxy */
int irc_get_px_type(irc *hnd);

/** \brief Tell the password we may have set earlier by irc_set_pass()
 * \return The server password set by irc_set_pass(), or NULL if none */
const char *irc_get_pass(irc *hnd);

/** \brief Tell the 'user name' we will use on the next log on
 * \return The username set by irc_set_uname() */
const char *irc_get_uname(irc *hnd);

/** \brief Tell the 'full name' we will use on the next log on
 * \return The full name set by irc_set_fname() */
const char *irc_get_fname(irc *hnd);

/** \brief Tell the USER flags we will use on the next log on
 * \return The flags set by irc_set_conflags() */
uint8_t irc_get_conflags(irc *hnd);

/** \brief Tell the nick we will use on the next log on
 * \return The nickname set by irc_set_nick().  *Not* necessarily the last
 *         nick we had before disconnecting, use irc_mynick() for that. */
const char *irc_get_nick(irc *hnd);

/** \brief Tell whether or not we will use SSL for the next connection
 * \return True if we are going to use SSL on the next connection */
bool irc_get_ssl(irc *hnd);

/** \brief Tell whether we'll next connect as a service
 * \return The value set by irc_set_service_connect() */
bool irc_get_service_connect(irc *hnd);

/** \brief Tell the 'dist' string we're going to use for the next service logon
 * \return The value set by irc_set_service_dist() */
const char *irc_get_service_dist(irc *hnd);

/** \brief Tell the type we're going to use for the next service logon
 * \return The value set by irc_set_service_type() */
long irc_get_service_type(irc *hnd);

/** \brief Tell the 'info' string we're going to use for the next service logon
 * \return The value set by irc_set_service_info() */
const char *irc_get_service_info(irc *hnd);


/** \brief Tell if channel- and user tracking is active.
 *
 * Tracking currently depends on 005 CASEMAPPING and will not be enabled until
 * such a message is seen (which fortunately tends to be one of the first few
 * messages received).  As soon as 005 CASEMAPPING is seen, this function will
 * start returning true to reflect that, provided that tracking was set to be
 * used by irc_set_track() *before* calling irc_connect().
 * \return True if tracking is enabled and active */
bool irc_tracking_enab(irc *hnd); //tell if tracking is (actually) enabled

/** brief Set or clear "dumb mode"
 *
 * In dumb mode, we pretty much leave everything apart from the protocol
 * parsing to the user.  That is, irc_connect() will return once the TCP
 * connection is established, leaving the logon sequence to the user.
 *
 * This setting will take effect not before the next call to irc_connect().
 *
 */ //XXX is at least the nickname tracked?
void irc_set_dumb(irc *hnd, bool dumbmode);

/** brief Tell whether we're operating in "dumb mode"
 *
 * \return true if we're in dumb mode (see irc_set_dumb())
 */ //XXX is at least the nickname tracked?
bool irc_get_dumb(irc *hnd);

/** brief Tell whether our next attempt to irc_read() a message would block
 *
 * This relies on the fact that ircds only ever send full lines, because the
 * assumption this function makes is that /if/ there is *some* data available
 * to read from the socket (as determined by, say, select()), then a there's
 * a whole line available t be read.  Needless to point out, a broken/malicious
 * ircd could trick us into blocking in irc_read() anyway, at least until the
 * timeout expires.
 *
 * \return true if the next call to irc_read() can give us a message without
 *         waiting.
 */
bool irc_can_read(irc *hnd);

/** \brief Register a protocol message handler
 *
 * This function can be used to register user-defined protocol message handlers
 * for arbitrary protocol commands (PRIVMSG, NICK, 005, etc)
 *
 * \param cmd   Protocol command to register the handler for.  E.g. "PRIVMSG"
 *              or "005".
 * \param hndfn   Function pointer to the handler function
 * \param pre   True if the handler is to be called BEFORE any effect that the
 *              message might have on libsrsirc itself, false if it should be
 *              called afterwards (see uhnd_fn).  Registering both a pre- and
 *              a post- handler for the same command is okay.
 *
 * \return true if the handler was registered, false if the maximum number of
 *              handlers is exceeded, in which case you should probably email
 *              me to disprove my assumption about how many would be enough for
 *              everybody(TM).  A compile-time constant will need to be bumped
 *              then.
 */
bool irc_reg_msghnd(irc *hnd, const char *cmd, uhnd_fn hndfn, bool pre);

/** \brief Tell whether the connection was closed gracefully
 *
 * If we were disconnected, this function can be used to tell whether the
 * connection was closed cleanly or not.
 *
 * \return true if the connection was closed gracefully.
 */
bool irc_eof(irc *hnd);

/** @} */

#endif /* LIBSRSIRC_IRC_EXT_H */
