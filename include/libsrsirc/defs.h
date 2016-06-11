/* defs.h - definitions, data types, etc
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_DEFS_H
#define LIBSRSIRC_IRC_DEFS_H 1


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @file
 * \defgroup defs Macro and type definitions provided by defs.h
 * \addtogroup defs
 *  @{
 */

/** \brief Default server to connect to (cf. irc_set_server()) */
#define DEF_HOST "localhost"

/** \brief Default TCP port for plaintext IRC (cf. irc_set_server()) */
#define DEF_PORT_PLAIN ((uint16_t)6667)

/** \brief Default TCP port for SSL IRC (cf. irc_set_server(), irc_set_ssl()) */
#define DEF_PORT_SSL ((uint16_t)6697)

/** \brief Default nickname (cf. irc_set_nick()) */
#define DEF_NICK "srsirc"

/** \brief Default IRC user name (cf. irc_set_uname()) */
#define DEF_UNAME "bsnsirc"

/** \brief Default IRC full name (cf. irc_set_fname()) */
#define DEF_FNAME "serious business irc"

/** \brief Default USER message flags (cf. irc_set_conflags()) */
#define DEF_CONFLAGS 0

/** \brief Default distribution for service logon (cf. irc_set_service_dist())*/
#define DEF_SERV_DIST "*"

/** \brief Default type string for service logon (cf. irc_set_service_type()) */
#define DEF_SERV_TYPE 0

/** \brief Default info string for service logon (cf. irc_set_service_info()) */
#define DEF_SERV_INFO "srsbsns srvc"

/** \brief Default hard connect timeout in microsecs
 *  (cf. irc_set_connect_timeout()) */
#define DEF_HCTO_US 120000000ul

/** \brief Default soft connect timeout in microsecs
 *  (cf. irc_set_connect_timeout()) */
#define DEF_SCTO_US 15000000ul

/** \brief RFC1459 case mapping as per the 005 ISUPPORT spec.
 *
 * In the RFC1459 case mapping, which is the default, the characters
 * }, { and | are the uppercase versions of ], [ and \,
 * respectively.
 *
 * \sa irc_casemap().
 */
#define CMAP_RFC1459 0

/** \brief Strict RFC1459 case mapping as per the 005 ISUPPORT spec.
 *
 * In the STRICT_RFC1459 case mapping, which is the default, the characters
 * }, {, | and ^ are the uppercase versions of ], [, \ and ~,
 * respectively.
 *
 * \sa irc_casemap().
 */
#define CMAP_STRICT_RFC1459 1

/** \brief ASCII case mapping as per the 005 ISUPPORT spec.
 *
 * In the ASCII case mapping, no special uppercase/lowercase rules exist.
 * A-Z are uppercase of a-z and that's it.
 *
 * \sa irc_casemap().
 */
#define CMAP_ASCII 2

/** \brief HTTP proxy type (cf. irc_set_proxy()) */
#define IRCPX_HTTP 0

/** \brief Socks4 proxy type (cf. irc_set_proxy()) */
#define IRCPX_SOCKS4 1 /* NOT socks4a */

/** \brief Socks5 proxy type (cf. irc_set_proxy()) */
#define IRCPX_SOCKS5 2

/** \brief Channel mode classes as per the 005 ISUPPORT spec. (A)
 *
 * Channel modes of class A are those that add or remove an entry from a list.
 * They take an argument when set and also when unset. Example: +b
 * (but not the mode prefix chars like +o, see irc_005modepfx())
 *
 * This is relevant for dealing with MODE.
 *
 * \sa lsi_ut_classify_chanmode() and irc_005chanmodes()
 */
#define CHANMODE_CLASS_A 1

/** \brief Channel mode classes as per the 005 ISUPPORT spec. (B)
 *
 * Channel modes of class B don't deal with lists, but will also take an
 * argument when set and when unset. Example: +k
 *
 * This is relevant for dealing with MODE.
 *
 * \sa lsi_ut_classify_chanmode() and irc_005chanmodes()
 */
#define CHANMODE_CLASS_B 2

/** \brief Channel mode classes as per the 005 ISUPPORT spec. (C)
 *
 * Channel modes of class C will only take an argument when set.
 * Example: +l (letter L)
 *
 * This is relevant for dealing with MODE.
 *
 * \sa lsi_ut_classify_chanmode() and irc_005chanmodes()
 */
#define CHANMODE_CLASS_C 3

/** \brief Channel mode classes as per the 005 ISUPPORT spec. (D)
 *
 * Channel modes of class D will never need an argument. Examples: +n, +t
 *
 * This is relevant for dealing with MODE.
 *
 * \sa lsi_ut_classify_chanmode() and irc_005chanmodes()
 */
#define CHANMODE_CLASS_D 4

/** \brief IRC context; pointers to this are our IRC context handle type.
 *
 * Pointers to this type are what irc_init() returns. It is necessary to
 * supply the pointer to most libsrsirc calls.  Behind the scenes, struct irc_s
 * holds the complete context and state associated with an IRC connection. */
typedef struct irc_s irc;

/** \brief Field array for the parts of incoming IRC protocol messages
 *
 * This is the array that holds the result of field-splitting an incoming
 * IRC protocol message, as happens when using irc_read().
 *
 * Consider:
 * \code
 *   tokarr tok;
 *   irc_read([...], &tok, [...]);
 * \endcode
 *
 * Suppose the message that was read in the above fragment was
 * \code
 *   :irc.srv JOIN #mychannel
 * \endcode
 * tok[0] would be "irc.srv", tok[1] "JOIN", tok[2] "#mychannel" and tok[3] NULL
 *
 * Suppose the message that was read in the above fragment was
 * \code
 *   ERROR :Nick collision
 * \endcode
 * tok[0] would be NULL, tok[1] "ERROR", tok[2] "Nick collision" and tok[3] NULL
 *
 * In other words, tok[1] will *always* be the "command" part of the message.
 *
 * RFC1459/2812 specify that a protcol message may consist of up to 17 fields;
 * we have room for one more to guarantee a NULL sentinel after the last one
 */
typedef char *tokarr[18];

/** \brief Logon-time callback for incoming protocol messages
 *
 * libsrsirc handles the logon conversation with the IRC server, which consists
 * of sending PASS/NICK/USER, potentially responding to a PING, handling a
 * number of exceptional situations (e.g. nickname already in use) and finally
 * waiting for the (required) messages 001-004, after which we consider
 * ourselves logged on.
 *
 * Thus, the first message that the user can actually irc_read() will be
 * whatever the IRC server sends after its 004.
 *
 * To access the messages that come before 004, a callback function can be
 * provided; whenever libsrsirc receives a logon-time message from the server,
 * it will callback to inform the user and let them decide whether to continue
 * or abort the logon.  This is the type of such a callback.
 *
 * \param   msg Pointer to a tokarr that contains the message we read
 * \param   tag An arbitrary "user data" pointer that can be provided when
 *            registering the callback (see irc_regcb_conread()).
 *
 * \return If the callback returns false, the attempt to logon is aborted.
 *
 * \sa irc_regcb_conread()
 */ //XXX why no ctx param?
typedef bool (*fp_con_read)(tokarr *msg, void *tag);

/** \brief Logon-time callback type for nickname problems
 *
 * This is the type of a callback function that can be registered to come up
 * with an alternative nickname, if while logging on it turns out that our
 * desired nickname isn't available.
 *
 * \param   nick   Buffer that contains the nickname that was not available.
 *                 The callback function's job is to modify the nickname stored
 *                 in this buffer, which will then be tried as an alternative
 * \param   nick_sz   Size of the buffer pointed to by `nick`.
 *
 * \sa irc_regcb_mutnick()
 */ //XXX return true or false to proceed/abort?
typedef void (*fp_mut_nick)(char *nick, size_t nick_sz);

/** \brief User-registered protocol command callback type
 *
 * As an alternative to interpreting messages right after irc_read(), the user
 * can register callback functions that are automatically invoked for matching
 * incoming protocol messages.  This is the type of such callbacks.
 *
 * \param ctx   The IRC context of the instance that read the protocol message
 * \param msg   Pointer to a tokarr that contains the message that was read
 * \param nargs    Number of non-NULL elements in `*msg`
 * \param pre   True if this callback was invoked BEFORE any changes to the
 *              internal library state that result from interpreting the
 *              message, false if it was invoked AFTERwards (message handlers
 *              can be registered both ways).  For example, if we have
 *              registered a PRE-NICK handler and change our nickname, then
 *              inside the handler, irc_mynick() will give us our old nickname.
 *              If it was a POST-NICK handler, we'd instead see our new nick.
 *              For most message types, PRE and POST handlers are equivalent
 *              because libsrsirc doesn't need to do anything with most
 *              messages.
 * \return   If the callback returns `false`, the connection is reset.
 *
 * \sa irc_reg_msghnd()
 */
typedef bool (*uhnd_fn)(irc *ctx, tokarr *msg, size_t nargs, bool pre);

/** @} */

#endif /* LIBSRSIRC_IRC_DEFS_H */
