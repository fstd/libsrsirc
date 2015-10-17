/* irc.h - very basic front-end interface (irc_ext.h has the rest)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_H
#define LIBSRSIRC_IRC_H 1


#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>


/* Overview:
 *
 * irc *irc_init(void);
 * void irc_reset(irc *hnd);
 * void irc_dispose(irc *hnd);
 * bool irc_connect(irc *hnd);
 * bool irc_online(irc *hnd);
 * int irc_read(irc *hnd, tokarr *tok, uint64_t to_us);
 * bool irc_write(irc *hnd, const char *line);
 * bool irc_printf(irc *hnd, const char *fmt, ...);
 * const char *irc_mynick(irc *hnd);
 * bool irc_set_server(irc *hnd, const char *host, uint16_t port);
 * bool irc_set_pass(irc *hnd, const char *srvpass);
 * bool irc_set_uname(irc *hnd, const char *uname);
 * bool irc_set_fname(irc *hnd, const char *fname);
 * bool irc_set_nick(irc *hnd, const char *nick);
 * void irc_dump(irc *hnd);
*/

/** @file
 * \defgroup basicif Basic interface provided by irc.h
 * \addtogroup basicif
 *  @{
 */

/** \brief Produce a handle to a newly allocated and initialized IRC object.
 *
 * This function is used to create a new IRC context; the handle returned
 * by it must be supplied to most other library calls in order to distinguish
 * this session from others.  Most programs will probably need only one handle,
 * but it enables to open an arbitrary number of independent IRC connections
 * should the need arise.
 *
 * \return a handle to the new instance, or NULL if out of memory.
 */
irc *irc_init(void);

/** \brief Force a disconnect from the IRC server.
 *
 * Contrary to what the name suggests, this function does not reset any other
 * state apart from the TCP connection, so e.g. irc_lasterror() will still
 * return the ERROR message of the previous connection.  This kind of
 * information will be reset not before the next call to irc_connect()
 *
 * \param hnd   IRC handle as obtained by irc_init()
 */
void irc_reset(irc *hnd);

/** \brief Destroy an IRC object, invalidating the provided handle
 *
 * This is the counterpart to irc_init().  Calling this function will, after an
 * implicit call to irc_reset(), deallocate all resources associated with the
 * provided handle, invalidating it in the process.
 *
 * \param hnd   IRC handle as obtained by irc_init()
 */
void irc_dispose(irc *hnd);

/** \brief Connect and log on to IRC
 *
 * Attempts to connect/log on to the IRC server specified by irc_set_server().
 * Connecting will involve, if applicable, resolving a DNS name and attempting
 *     to connect to each of the resulting A- or AAAA records.
 *
 * Behavior is affected by two timeouts (a "hard" and a "soft" timeout), which
 *     can be set via irc_set_connect_timeout().  The "hard" timeout limits the
 *     overall time trying to get an IRC connection going, while the "soft"
 *     timeout applies each time anew for each attempt to establish a TCP
 *     connection (so that we can be sure all A- or AAAA records are tried).
 *
 * We consider ourselves logged on once we see the "004" message
 *     (or 383 if we're a service).  If of interest, the "log on conversation"
 *     i.e. 001, 002, 003 and 004 can be retrieved by irc_logonconv().
 *
 * \param hnd   IRC handle as obtained by irc_init()
 *
 * \return true if successfully logged on, false on failure
 */
bool irc_connect(irc *hnd);

/** \brief Tell whether or not we (think we) are connected
 *
 * Of course, we can't /really/ know unless we attempt to do I/O with the
 * server by calling irc_read() (or perhaps irc_write())
 *
 * \param hnd   IRC handle as obtained by irc_init()
 *
 * \return true if we still appear to be connected
 */
bool irc_online(irc *hnd);

/** \brief Read and process the next protocol message from the IRC server
 *
 * Reads a protocol message from the server, field-splits it and populates
 * populates the structure pointed to by`tok' with the results.
 *
 * Usage example:
 * \code
 *   tokarr tok;
 *
 *   int r = irc_read(hnd, &tok, 1000000); // 1 second timeout
 *
 *   if (r == -1) {
 *       // ...read failure, we're now disconnected...
 *   } else if (r == 0) {
 *       // ...timeout, nothing to read...
 *   } else {
 *       // Okay, we have read a message.
 *       if (strcmp(tok[1], "PING") == 0)
 *           irc_printf(hnd, "PONG %s", tok[2]);
 *   }
 * \endcode
 *
 * \param hnd   IRC handle as obtained by irc_init()
 * \param tok   Pointer to the result array, where pointers to the identified
 *              fields tokens are stored in.
 *                  (*tok)[0] will point to the "prefix" (not including the
 *                      leading colon), or NULL if the message did not
 *                      contain a prefix.
 *                  (*tok)[1] will point to the (mandatory) "command"
 *                  (*tok)[2+n] will point to the n-th "argument", if it
 *                      exists; NULL otherwise
 * \param to_us   Read timeout in microseconds (0 = no timeout)
 *
 * \return 1 on success; 0 on timeout; -1 on failure
 *
 * In the case of failure, an implicit call to irc_reset() is performed.
 */
int irc_read(irc *hnd, tokarr *tok, uint64_t to_us);

/** \brief Send a message to the IRC server
 *
 * \param hnd   IRC handle as obtained by irc_init()
 * \param line   Data to send, typically a single IRC protocol line (but may
 *               be multiple if properly separated by \\r\\n).
 *               If the line does not end in \\r\\n, it will be appended.
 *
 * \return true on success, false on failure
 *
 * In the case of failure, an implicit call to irc_reset() is performed.
 */
bool irc_write(irc *hnd, const char *line);

/** \brief Send a formatted message to the IRC server, printf-style
 *
 * \param hnd   IRC handle as obtained by irc_init()
 * \param fmt   A printf-style format string.
 *
 * This function uses irc_write() behind the scenes.
 *
 * \return true on success, false on failure
 *
 * In the case of failure, an implicit call to irc_reset() is performed.
 */
bool irc_printf(irc *hnd, const char *fmt, ...);

/** \brief Tell what our current nickname is
 *
 * We keep track of changes to our own nickname, this function can be used
 * to tell what our current nick is.
 *
 * \param hnd   IRC handle as obtained by irc_init()
 *
 * \return A pointer to our current nickname
 */
const char *irc_mynick(irc *hnd);

/** \brief Set server and port to connect to
 *
 * This setting will take effect not before the next call to irc_connect()
 *
 * \param hnd   IRC handle as obtained by irc_init()
 * \param host   Server host, may be an IPv4 or IPv6 address or a DNS name
 * \param port   Server port (0 uses default (6667 or 6697 (with SSL)))
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_server(irc *hnd, const char *host, uint16_t port);

/** \brief Set server password (NULL means no password)
 *
 * This setting will take effect not before the next call to irc_connect()
 *
 * \param hnd   IRC handle as obtained by irc_init()
 * \param srvpass   Server password for the next IRC connection
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_pass(irc *hnd, const char *srvpass);

/** \brief set IRC 'user name' (not the same as nickname)
 *
 * The 'username' is the middle part of an IRC identity (nickname!username\@host)
 *
 * This setting will take effect not before the next call to irc_connect()
 *
 * \param hnd   IRC handle as obtained by irc_init()
 * \param uname   Desired username for the next IRC connection
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_uname(irc *hnd, const char *uname);

/** \brief set IRC 'full name' (which appears e.g. in WHOIS)
 *
 * The 'full name' is typically shown in responses to the WHOIS IRC command.
 *
 * This setting will take effect not before the next call to irc_connect()
 *
 * \param hnd   IRC handle as obtained by irc_init()
 * \param fname   Desired full name for the next IRC connection
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_fname(irc *hnd, const char *fname);

/** \brief set IRC 'nickname'
 *
 * This setting will take effect not before the next call to irc_connect(),
 * i.e. it does *not* cause an on-line nick change.
 * (To do that, send a NICK message)
 *
 * \param hnd   IRC handle as obtained by irc_init()
 * \param nick   Desired nickname for the next IRC connection
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_nick(irc *hnd, const char *nick);

/** \brief Dump state for debugging purposes
 *
 * This function is intended for debugging and will puke out a dump of all
 * state associated with the given handle
 *
 * \param hnd   IRC handle as obtained by irc_init()
 */
void irc_dump(irc *hnd);

/** @} */

#endif /* LIBSRSIRC_IRC_H */
