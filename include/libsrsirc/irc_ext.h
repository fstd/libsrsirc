/* irc_ext.h - less commonly used part of the main interface
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-20, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_EXT_H
#define LIBSRSIRC_IRC_EXT_H 1


#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include <libsrsirc/irc.h>


/** @file
 * \defgroup extif Extended interface provided by irc_ext.h
 *
 * \brief This is the extended interface complementing the basic one
 *        to make accessible the rest of libsrsirc's core functionality.
 * \addtogroup extif
 *  @{
 */


/** \brief Tell who the IRC server claims to be.
 *
 * The IRC server advertises its own hostname (or is supposed to, anyway)
 * as part of the 004 message, which is a part of the logon conversation. \n
 * This function can be used to retrieve that information.  Note that servers
 * might lie about it; it need not be the hostname set with irc_set_server(),
 * so don't rely on this information.
 *
 * \param ctx   IRC context as obtained by irc_init()
 *
 * \return A pointer to the server's self-advertised hostname. \n
 *         The pointer is guaranteed to be valid until the next time
 *         irc_connect() is called.
 *
 * This function can be called even after we disconnected; it will then return
 * whatever it would have returned while the connection was still alive.
 *
 * When used before the *first* call to irc_connect(), the returned pointer
 * will point to an empty string.
 *
 * \sa irc_set_server(), irc_logonconv()
 */
const char *irc_myhost(irc *ctx);

/** \brief Tell how the IRC server maps lowercase to uppercase characters and
 *         vice-versa.
 *
 * Different IRC servers may have different ideas about what characters are
 * considered alternative-case variants of other characters.
 *
 * In particular, there are three different conventions, all of which have
 * in common that `a-z` are considered the lowercase equivalents of `A-Z`. \n
 * -- The convention called `RFC1459` additionally specifies that
 * the characters `]` and `[` and `\` are the lowercase equivalents of `}` and
 * `{` and `|`, respectively. \n
 * -- The convention called `STRICT_RFC1459` additionally specifies that
 * the characters `]` and `[` and `\` and `~` are the lowercase equivalents of
 * `}` and `{` and `|` and `^`, respectively. \n
 * -- The convention called ASCII, does not specify any additional nonsense.
 *
 * This information is supplied by the server as part of the (non-standard but
 * almost universally implemented) 005 ISUPPORT message, which libsrsirc does
 * NOT consider part of the logon conversation.  As long as no 005 message was
 * seen, this function will return CMAP_RFC1459.  As soon as the 005 is
 * encountered, the return value of this function will change to reflect the
 * information provided therein.
 *
 * Casemapping information is required to reliably ascertain equivalence of
 * non-identical nicknames.  A number of utility functions will depend on
 * being told the correct case mapping.
 *
 * \return One of CMAP_* constants; CMAP_RFC1459 if there was no 005 (yet)
 *
 * \sa CMAP_ASCII, CMAP_RFC1459, CMAP_STRICT_RFC1459, lsi_ut_istrcmp(),
 * lsi_ut_tolower(), lsi_ut_casemap_nam()
 */
int irc_casemap(irc *ctx);

/** \brief Tell whether or not we're a service (as opposed to a user).
 *
 * IRC connections come in two flavors -- services and users.
 * If irc_set_service_connect() was used to express the wish to connect as a
 * service rather than as a user, a successful logon will (behind the scenes)
 * involve a 383 message that tells us that we are, in fact, a service.
 *
 * This function determines whether or not that happened.
 *
 * \return true if we are a service, false if we are a user.
 */
bool irc_service(irc *ctx);

/** \brief Determine the available user modes.
 *
 * Different IRC servers may support different user modes. The 004 message,
 * which is part of the logon conversation, includes a field that enumerates the
 * available user modes. \n
 * This function can be used to retrieve that information.
 *
 * \return A string of supported mode chars, e.g. "iswo".
 */
const char *irc_umodes(irc *ctx);

/** \brief Lie about the available channel modes
 *
 * Different IRC servers may support different channel modes modes. \n
 * Historically, the 004 message was used to specify the channel modes
 * available, however, this feature has been de-facto obsoleted by the
 * `CHANMODE`-attribute of the 005 ISUPPORT message. \n
 *
 * For completeness, and to deal with the handfull of servers that still don't
 * use 005 ISUPPORT, this function is provided to retrieve the per-004 available
 * channel modes. \n
 *
 * On servers that do support 005, the string returned by this function ranges
 * from "reasonably accurate" to "complete bullshit".
 *
 * Tl;DR: Do not use this function. Use irc_005chanmodes() instead.
 *
 * \return A string of channel modes that may or may not exist (e.g. "bnlkt")
 * \sa irc_005chanmodes()
 */
const char *irc_cmodes(irc *ctx);

/** \brief Tell what ircd version the IRC server claims to run.
 *
 * The 004 message, which is part of the logon conversation, includes a field
 * that describes the ircd software running the server.
 *
 * This function can be used to retrieve that information.
 * \return A ircd-specific version string (e.g. "u2.10.12.10+snircd(1.3.4a)")
 */
const char *irc_version(irc *ctx);

/** \brief Tell what the last received ERROR message said.
 *
 * When an IRC server has a problem with us, it will typically send an ERROR
 * message describing what's going on, before disconnecting us.
 *
 * Whenever we receive an ERROR message, we store its argument and make it
 * available through this function for the user to inspect.
 *
 * \return The argument of the last received ERROR message, or NULL if we
 *         haven't received any since the last call to irc_connect().
 */
const char *irc_lasterror(irc *ctx);

/** \brief Tell why we are banned, if the server was polite enough to let us
 *         know.
 *
 * Sometimes, ircds tell us why/that we are banned using the 465 message, before
 * getting rid of us.  If we see such a message, we store its argument and make
 * it available through this function for the user to inspect.
 *
 * \return The argument of the last received 465 message, or NULL if we haven't
 *         received any since the last call to irc_connect().
 * \sa irc_banned()
 */
const char *irc_banmsg(irc *ctx);

/* XXX document */
size_t irc_v3tags_cnt(irc *ctx);
bool irc_v3tag(irc *ctx, size_t ind, const char **key, const char **value);
bool irc_v3tag_bykey(irc *ctx, const char *key, const char **value);

/** \brief set SASL mechanism and authentication string for the next connection.
 *
 * This setting will take effect not before the next call to irc_connect().
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param mech   The mechanism to use for SASL authentication (e.g., 'PLAIN'). \n
 *                We do *not* depend on this pointer to remain valid after
 *                we return, i.e. we do make a copy.
 * \param msg   The message that will serve as the SASL authentication string
 *                (e.g., '+' if 'mech' is 'EXTERNAL') \n
 *                We do *not* depend on this pointer to remain valid after
 *                we return, i.e. we do make a copy.
 * \param n   The size of the message (in bytes).
 *
 * \return true on success, false on failure (which means we're out of memory).
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_sasl(irc *ctx, const char *mech, const void *msg, size_t n,
    bool musthave);

/* XXX document */
bool irc_set_starttls(irc *ctx, int mode, bool musthave);


/** \brief Determine whether we are banned, if the server was polite enough to
 *         let us know.
 *
 * Sometimes, ircds tell us why/that we are banned using the 465 message, before
 * getting rid of us.  Seeing such a message will cause this function to start
 * returning true.
 *
 * \return true if we have seen a 465 message telling us that we're banned,
 *         false otherwise (which need not mean that we are *not* banned; many
 *         servers don't bother sending 465.
 * \sa irc_banmsg()
 */
bool irc_banned(irc *ctx);

/** \brief Deprecated.  Please disregard.
 *
 * Tell whether the last argument of the most recently read line was a
 * "trailing" argument, i.e. was prefixed by a colon.
 *
 * \return true if so.
 *
 * This is as useless as it sounds, don't use.  It was a hack, and it will
 * be removed in the near future.
 */
bool irc_colon_trail(irc *ctx);

/** \brief Deprecated.  Please disregard.
 *
 * This gives us access to the underlying socket. This is absolutely insane,
 * do not use, it will be removed soon, with a better solution to what this
 * tries to accomplish (i/o multiplexing)
 *
 * \returns The socket.
 */
int irc_sockfd(irc *ctx);

/** \brief Give access to the "logon conversation" (see doc/terminology.txt).
 *
 * On a successful logon to IRC, the server is required to send the 001, 002,
 * 003 and 004 message.  Since libsrsirc handles the logon itself, those
 * messages are consumed before irc_connect() even returns, and hence will never
 * be returned by irc_read().
 *
 * This function provides access to those messages after logon is complete,
 * should the need arise.
 *
 * Note that a typical logon conversation might include other non-standard
 * messages in addition to 001-004, these are *not* accessible through this
 * function. If you need those, use irc_regcb_conread() to register a `conread`
 * handler.
 *
 * The return type `tokarr *(*)[4]` is a bit twisted (see below).
 *
 * \return A pointer to an array of 4 pointers, each pointing to a tokarr.
 *         (i.e. one for each of 001, 002, 003, 004).
 * \sa tokarr, irc_regcb_conread(), fp_con_read
 */
tokarr *(*irc_logonconv(irc *ctx))[4];

/** \brief Tell what channel modes the ircd claims to support.
 *
 * 005 ISUPPORT is a non-standard but almost universally implemented message
 * through which an ircd advertises features it supports or characteristics it
 * has.
 *
 * 005 specifies four classes of channel modes (see description of
 * CHANMODE_CLASS_A, CHANMODE_CLASS_B, CHANMODE_CLASS_C, CHANMODE_CLASS_D).
 *
 * This function can be used to query the supported channel modes of a given
 * class
 *
 * For the rare case that an IRC server does *not* implement the 005 ISUPPORT,
 * use irc_cmodes() instead.
 *
 * Before the 005 message is seen (not part of the logon conversation, but
 * typically sent as the first message after a successful logon), this function
 * will return the default channel modes for a given class.
 *
 * \param mclass: class of channel modes to return (see above)
 * \return A pointer to a string consisting of the supported channel modes
 *     for the given class
 * \sa CHANMODE_CLASS_A, CHANMODE_CLASS_B, CHANMODE_CLASS_C, CHANMODE_CLASS_D,
 *     irc_cmodes()
 */
const char *irc_005chanmodes(irc *ctx, size_t mclass);

/** \brief Tell what channel mode prefixes the ircd claims to support.
 *
 * 005 is a non-standard but almost universally implemented message through
 * which and ircd advertises features it supports or characteristics it has.
 *
 * Channel mode prefixes come in two flavors, letters (o, v) and symbols (@, +)
 * (for the default ops and voice, respectively).
 *
 * This function can be used to determine what channel mode prefixes the
 * ircd supports.
 *
 * For the rare case that an IRC server does *not* implement the 005 ISUPPORT,
 * as well as before the 005 message is seen firstly, the default mode prefixes
 * are assumed to be "ov" (letters) and "@+" (symbols), respectively.
 *
 * \param symbols   If true, return symbols, otherwise return letters
 * \return A string of available mode symbols or letters, in descending order
 *         of power.  For example, "@+" or "ov".
 */
const char *irc_005modepfx(irc *ctx, bool symbols);

/** \brief Tell the value of a given 005 attribute.
 *
 * 005 is a non-standard but almost universally implemented message through
 * which and ircd advertises features it supports or characteristics it has.
 *
 * It consists of a list of `key[=value]` pairs, where `key` is generally an
 * uppercase ascii string.  We record this information when 005 is encountered;
 * this function can be used to obtain the `value` of a given `key`.
 *
 * \param name The key.
 * \return A string corresponding to the value of the 005 attribute `name`. If
 *         the attribute `name` was never seen, NULL is returned.  If `name` was
 *         present, but did not have a value, an empty string is returned.
 */
const char *irc_005attr(irc *ctx, const char *name);

/** \brief Register callback for protocol messages that are read at logon time.
 *
 * If for some reason the messages received at logon time (while irc_connect()
 * is executing) are of interest to the user, a callback can be registered using
 * this function. It will be called for every incoming protocol message received
 * while logging on.
 *
 * \param cb   Function pointer to the callback function to be registered
 * \param tag   Arbitrary userdata that is passed back to the callback as-is
 *
 * \sa fp_con_read for semantics of the callback.
 */
void irc_regcb_conread(irc *ctx, fp_con_read cb, void *tag);

/** \brief Register a function to come up with an alternative nickname at logon
 *         time.
 *
 * If while logging on we realize that we cannot have the nickname we wanted,
 * the callback registered through this function is called to come up with
 * an alternative nickname.
 *
 * A default callback is provided which is probably okay for most cases.
 *
 * \param mn   Function pointer to the callback function to be registered.
 *             If NULL, the attempt to logon will be aborted if we can't have
 *             the nickname we want.
 *
 * \sa fp_mut_nick for semantics of the callback.
 */
void irc_regcb_mutnick(irc *ctx, fp_mut_nick mn);

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
 *         to avoid accidentally doing plaintext when you meant to use SSL
 */
bool irc_set_ssl(irc *ctx, bool on);

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
 * *1* second, and our irc server resolved to *4* addresses: \n
 * This would cause the soft timeout to be raised to 2.5 seconds, so that
 * each of the 4 addresses is given the maximum possible time to react while
 * staying within the bounds of the hard limit.
 *
 * \param soft   The soft limit, in microseconds (see above)
 * \param hard   The hard limit, in microseconds (see above)
 *
 * A timeout of 0 means no timeout (but still, a hard timeout >0 overrules
 * a soft timeout of 0)
 */
void irc_set_connect_timeout(irc *ctx, uint64_t soft, uint64_t hard);

/** \brief Set proxy server to use
 *
 * libsrsirc supports redirecting the IRC connection through a proxy server.
 *
 * Supported proxy types are HTTP CONNECT, SOCKS4 and SOCKS5.
 *
 * \param host   Proxy host, may be an IPv4 or IPv6 address or a DNS name. \n
 *               Passing NULL will cause no proxy to be used.
 *
 * \param port   Proxy port (1 <= `port' <= 65535)
 * \param ptype   Proxy type, one of IRCPX_HTTP, IRCPX_SOCKS4, IRCPX_SOCKS5.
 * \return true on success, false on failure (out of memory, illegal args)
 * \sa IRCPX_HTTP, IRCPX_SOCKS4, IRCPX_SOCKS5
 */
bool irc_set_px(irc *ctx, const char *host, uint16_t port, int ptype);

/** \brief Set flags for the USER message at log on time.
 *
 * This is a bit mask for which RFC2812 defines two bits: bit 3 (i.e. 8) leads
 * to user mode +i being set, bit 2 (i.e. 4) to user mode +w.  RFC1459 doesn't
 *     define this, so it should probably not be relied upon.
 *
 * \param flags   Connect flags for the USER message
 */
void irc_set_conflags(irc *ctx, uint8_t flags);

/** \brief Log on as service? if not, we're a normal client.
 *
 * This function can be used to tell libsrsirc that our next attempt to
 * irc_connect() should perform a service logon, rather than a user logon.
 * This is pretty much untested.
 *
 * \param enabled   If true, we're trying to be a service.  If not, we're a user.
 */
void irc_set_service_connect(irc *ctx, bool enabled);

/** \brief Set the "distribution" string for service log on.
 *
 * The distribution is a wildcarded host mask that determines on which servers
 * the service will be available.
 *
 * \param dist   The dist string provided with a service logon
 * \return True if successful, false otherwise (which means we're out of memory)
 */
bool irc_set_service_dist(irc *ctx, const char *dist);

/** \brief Set the "service type" for service log on.
 *
 * According to RFC2812, this is currently reserved for future usage.
 * \param type   The service type identifier provided with a service logon
 * \return True.
 */
bool irc_set_service_type(irc *ctx, long type);

/** \brief Set the "service info" string for service log on.
 *
 * This is a freetext string that describes the service.
 *
 * \param info   The info string provided with a service logon
 * \return True if successful, false otherwise (which means we're out of memory)
 */
bool irc_set_service_info(irc *ctx, const char *info);

/** \brief Enable or disable channel- and user tracking
 *
 * libsrsirc has a sizeable portion of code concerned with keeping track of
 * channels we're in and users we can see.  This is useful for implementing
 * clients on top of libsrsirc, but simpler applications won't require it.
 *
 * Therefore, tracking is disabled by default.  This function can be used to
 * enable it.
 *
 * When tracking is enabled, the functions in the tracking interface
 * (irc_track.h) are available to query information about channels and users.
 *
 * Note that tracking currently depends on the presence of a 005 ISUPPORT
 * message. If irc_set_track() was used *before* connecting, tracking will
 * be activated as soon as an 005 message (with a CASEMAPPING attribute) is
 * encountered. irc_tracking_enab() can be used to tell whether tracking
 * is actually active.
 *
 * \param on   True to enable tracking, false to disable
 *
 * This setting will take effect not before the next call to irc_connect().
 * \sa irc_tracking_enab(), irc_casemap(), irc_track.h
 */
bool irc_set_track(irc *ctx, bool on);

/** \brief Tell the name or address of the IRC server we use or intend to use
 * \return The hostname-part of what was set using irc_set_server()
 * \sa irc_set_server()
 */
const char *irc_get_host(irc *ctx);

/** \brief Tell the port of the IRC server we use or intend to use
 * \return The port-part of what was set using irc_set_server()
 */
uint16_t irc_get_port(irc *ctx);

/** \brief Tell the host of the proxy we use or intend to use
 * \return The hostname-part of what was set using irc_set_px()
 */
const char *irc_get_px_host(irc *ctx);

/** \brief Tell the port of the proxy we use or intend to use
 * \return The port-part of what was set using irc_set_px()
 */
uint16_t irc_get_px_port(irc *ctx);

/** \brief Tell the type of the proxy we use or intend to use
 *
 * \return The type-part of what was set using irc_set_px(),
 *         i.e. one of IRCPX_HTTP, IRCPX_SOCKS4, IRCPX_SOCKS5 or -1 for no
 *         proxy
 */
int irc_get_px_type(irc *ctx);

/** \brief Tell the password we may have set earlier by irc_set_pass()
 * \return The server password set by irc_set_pass(), or NULL if none
 */
const char *irc_get_pass(irc *ctx);

/** \brief Tell the 'user name' we will use on the next log on
 * \return The username set by irc_set_uname()
 */
const char *irc_get_uname(irc *ctx);

/** \brief Tell the 'full name' we will use on the next log on
 * \return The full name set by irc_set_fname()
 */
const char *irc_get_fname(irc *ctx);

/** \brief Tell the USER flags we will use on the next log on
 * \return The flags set by irc_set_conflags()
 */
uint8_t irc_get_conflags(irc *ctx);

/** \brief Tell the nick we will use on the next log on
 * \return The nickname set by irc_set_nick().  *Not* necessarily the last
 *         nick we had before disconnecting, use irc_mynick() for that.
 */
const char *irc_get_nick(irc *ctx);

/** \brief Tell whether or not we will use SSL for the next connection
 * \return True if we are going to use SSL on the next connection
 */
bool irc_get_ssl(irc *ctx);

/** \brief Tell whether we'll next connect as a service
 * \return The value set by irc_set_service_connect()
 */
bool irc_get_service_connect(irc *ctx);

/** \brief Tell the 'dist' string we're going to use for the next service logon
 * \return The value set by irc_set_service_dist()
 */
const char *irc_get_service_dist(irc *ctx);

/** \brief Tell the type we're going to use for the next service logon
 * \return The value set by irc_set_service_type()
 */
long irc_get_service_type(irc *ctx);

/** \brief Tell the 'info' string we're going to use for the next service logon
 * \return The value set by irc_set_service_info()
 */
const char *irc_get_service_info(irc *ctx);


/** \brief Tell if channel- and user tracking is active.
 *
 * Tracking currently depends on 005 CASEMAPPING and will not be enabled until
 * such a message is seen (which fortunately tends to be one of the first few
 * messages received).  As soon as 005 CASEMAPPING is seen, this function will
 * start returning true to reflect that, provided that tracking was set to be
 * used by irc_set_track() *before* calling irc_connect().
 * \return True if tracking is enabled and active
 */
bool irc_tracking_enab(irc *ctx); //tell if tracking is (actually) enabled

/** \brief Set or clear "dumb mode"
 *
 * In dumb mode, we pretty much leave everything apart from the protocol
 * parsing to the user.  That is, irc_connect() will return once the TCP
 * connection is established, leaving the logon sequence to the user.
 *
 * This setting will take effect not before the next call to irc_connect().
 *
 */ //XXX is at least the nickname tracked?
void irc_set_dumb(irc *ctx, bool dumbmode);

/** \brief Tell whether we're operating in "dumb mode"
 *
 * \return true if we're in dumb mode (see irc_set_dumb())
 */ //XXX is at least the nickname tracked?
bool irc_get_dumb(irc *ctx);

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
 * \sa uhnd_fn
 */
bool irc_reg_msghnd(irc *ctx, const char *cmd, uhnd_fn hndfn, bool pre);

/** \brief Tell whether the connection was closed gracefully
 *
 * If we were disconnected, this function can be used to tell whether the
 * connection was closed cleanly or not.
 *
 * \return true if the connection was closed gracefully.
 */
bool irc_eof(irc *ctx);

/** @} */

#endif /* LIBSRSIRC_IRC_EXT_H */
