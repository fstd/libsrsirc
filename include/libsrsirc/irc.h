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
 * void irc_reset(irc *ctx);
 * void irc_dispose(irc *ctx);
 * bool irc_connect(irc *ctx);
 * bool irc_online(irc *ctx);
 * int irc_read(irc *ctx, tokarr *tok, uint64_t to_us);
 * bool irc_write(irc *ctx, const char *line);
 * bool irc_printf(irc *ctx, const char *fmt, ...);
 * const char *irc_mynick(irc *ctx);
 * bool irc_set_server(irc *ctx, const char *host, uint16_t port);
 * bool irc_set_pass(irc *ctx, const char *srvpass);
 * bool irc_set_uname(irc *ctx, const char *uname);
 * bool irc_set_fname(irc *ctx, const char *fname);
 * bool irc_set_nick(irc *ctx, const char *nick);
 * void irc_dump(irc *ctx);
*/

/** @file
 * \defgroup basicif Basic interface provided by irc.h
 * \addtogroup basicif
 *  @{
 */

/** \brief Allocate and initialize a new IRC context.
 *
 * This is the function you will typically use before calling any other
 * libsrsirc function.  It allocates and initializes a new IRC context that is
 * used internally to hold all the state associated with this IRC connection,
 * as well as to distinguish it from other, unrelated IRC connections made
 * using libsrsirc.
 *
 * An opaque pointer (i.e. a handle) to the new IRC context is returned to the
 * user and will remain valid until irc_dispose() is called for that pointer.
 * The user will generally need to supply this pointer as the first argument
 * to most libsrsirc functions, but will never dereference it themselves.
 * (This is analoguous to how FILE * works in standard C.)
 *
 * This function can be called an arbitrary number of times, if there is a need
 * for many independent IRC connections using libsrsirc.  However, most programs
 * will probably need only one IRC connection and therefore call this function
 * only once.
 *
 * If the IRC context is no longer required, call irc_dispose() on it to
 * release the associated state and resources.
 *
 * \return A pointer to the new IRC context, or NULL if we are out of memory.
 *         This pointer will remain valid until it is given to irc_dispose().
 * \sa irc, irc_dispose()
 */
irc *irc_init(void);

/** \brief Force a disconnect from the IRC server.
 *
 * Contrary to what the name suggests, this function does *not* reset any other
 * state apart from the TCP connection, so functions like irc_lasterror(),
 * irc_mynick() etc. will keep returning the information they would have
 * returned when the connection was still alive.
 *
 * The mentioned kind of information will be reset not before the next call to
 * irc_connect()
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \sa irc_connect()
 */
void irc_reset(irc *ctx);

/** \brief Destroy an IRC context, invalidating the provided pointer
 *
 * This is the counterpart to irc_init().  Calling this function will, after an
 * implicit call to irc_reset(), deallocate all resources associated with the
 * provided context, invalidating it in the process.
 *
 * Use this function when you don't need your IRC context anymore, maybe as
 * part of other cleanup your program does when terminating.
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \sa irc_reset(), irc_init()
 */
void irc_dispose(irc *ctx);

/** \brief Connect and log on to IRC
 *
 * Attempts to connect/log on to the IRC server specified earlier using
 * irc_set_server().
 *
 * Connecting will involve, if applicable, resolving a DNS name and attempting
 * to establish a TCP connection to each of the resulting A- or AAAA records
 * (until one works for us).
 *
 * Behavior is affected by two timeouts (a "hard" and a "soft" timeout)
 * The "hard" timeout limits the overall time we try to get an IRC connection
 * going, while the "soft" timeout applies each time anew for each attempt to
 * establish a TCP connection (so that we can be sure all A- or AAAA records
 * are tried) (see irc_set_connect_timeout() for further information regarding
 * the soft timeout)
 *
 * We consider ourselves logged on once the IRC server sends us a "004" message
 * (or 383 if we're a service).  If of interest, the "log on conversation"
 * i.e. 001, 002, 003 and 004 can be retrieved by irc_logonconv() if there is
 * a need to see them (there usually isn't).
 *
 * \param ctx   IRC context as obtained by irc_init()
 *
 * \return true if successfully logged on, false on failure
 *
 * \sa irc_set_server(), irc_set_pass(), irc_set_connect_timeout(),
 *     irc_logonconv()
 */
bool irc_connect(irc *ctx);

/** \brief Tell whether or not we (think we) are connected
 *
 * Of course, we can't /really/ know unless we attempt to do I/O with the
 * server by calling irc_read() (or perhaps irc_write())
 *
 * \param ctx   IRC context as obtained by irc_init()
 *
 * \return true if we still appear to be connected
 */
bool irc_online(irc *ctx);

/** \brief Read and process the next protocol message from the IRC server
 *
 * Reads a protocol message from the server, field-splits it and populates
 * populates the structure pointed to by`tok' with the results.
 *
 * Usage example:
 * \code
 *   tokarr tok;
 *
 *   int r = irc_read(ctx, &tok, 1000000); // 1 second timeout
 *
 *   if (r == -1) {
 *       // ...read failure, we're now disconnected...
 *   } else if (r == 0) {
 *       // ...timeout, nothing to read...
 *   } else {
 *       // Okay, we have read a message.
 *       if (strcmp(tok[1], "PING") == 0)
 *           irc_printf(ctx, "PONG %s", tok[2]);
 *   }
 * \endcode
 *
 * \param ctx   IRC context as obtained by irc_init()
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
int irc_read(irc *ctx, tokarr *tok, uint64_t to_us);

/** \brief Send a message to the IRC server
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param line   Data to send, typically a single IRC protocol line (but may
 *               be multiple if properly separated by \\r\\n).
 *               If the line does not end in \\r\\n, it will be appended.
 *
 * \return true on success, false on failure
 *
 * In the case of failure, an implicit call to irc_reset() is performed.
 */
bool irc_write(irc *ctx, const char *line);

/** \brief Send a formatted message to the IRC server, printf-style
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param fmt   A printf-style format string.
 *
 * This function uses irc_write() behind the scenes.
 *
 * \return true on success, false on failure
 *
 * In the case of failure, an implicit call to irc_reset() is performed.
 */
bool irc_printf(irc *ctx, const char *fmt, ...);

/** \brief Tell what our current nickname is
 *
 * We keep track of changes to our own nickname, this function can be used
 * to tell what our current nick is.
 *
 * \param ctx   IRC context as obtained by irc_init()
 *
 * \return A pointer to our current nickname
 */
const char *irc_mynick(irc *ctx);

/** \brief Set server and port to connect to
 *
 * This setting will take effect not before the next call to irc_connect()
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param host   Server host, may be an IPv4 or IPv6 address or a DNS name
 * \param port   Server port (0 uses default (6667 or 6697 (with SSL)))
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_server(irc *ctx, const char *host, uint16_t port);

/** \brief Set server password (NULL means no password)
 *
 * This setting will take effect not before the next call to irc_connect()
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param srvpass   Server password for the next IRC connection
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_pass(irc *ctx, const char *srvpass);

/** \brief set IRC 'user name' (not the same as nickname)
 *
 * The 'username' is the middle part of an IRC identity (nickname!username\@host)
 *
 * This setting will take effect not before the next call to irc_connect()
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param uname   Desired username for the next IRC connection
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_uname(irc *ctx, const char *uname);

/** \brief set IRC 'full name' (which appears e.g. in WHOIS)
 *
 * The 'full name' is typically shown in responses to the WHOIS IRC command.
 *
 * This setting will take effect not before the next call to irc_connect()
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param fname   Desired full name for the next IRC connection
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_fname(irc *ctx, const char *fname);

/** \brief set IRC 'nickname'
 *
 * This setting will take effect not before the next call to irc_connect(),
 * i.e. it does *not* cause an on-line nick change.
 * (To do that, send a NICK message)
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param nick   Desired nickname for the next IRC connection
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_nick(irc *ctx, const char *nick);

/** \brief Dump state for debugging purposes
 *
 * This function is intended for debugging and will puke out a dump of all
 * state associated with the given context
 *
 * \param ctx   IRC context as obtained by irc_init()
 */
void irc_dump(irc *ctx);

/** @} */

#endif /* LIBSRSIRC_IRC_H */
