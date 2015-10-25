/* irc.h - very basic front-end interface (irc_ext.h has the rest)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_H
#define LIBSRSIRC_IRC_H 1


#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>


/** @file
 * \defgroup basicif Basic interface provided by irc.h
 *
 * \brief This is the basic interface to the core facilities of libsrsirc.
 *
 * Everything needed to get an IRC connection going and do simple IRC operations
 * is provided by this header.
 *
 * Usage example for a simple ECHO-like IRC bot (receives messages and sends
 * them back to the channel it saw them on):
 * \code
 * #include <stdio.h>
 * #include <stdlib.h>
 * #include <string.h>
 *
 * #include <libsrsirc/irc.h>
 *
 * #define SERVER "irc.example.org"
 * #define PORT 6667
 * #define CHANNEL "#mychannel"
 * #define NICKNAME "mynickname"
 *
 * int main(void) {
 *     irc *ictx = irc_init();
 *
 *     irc_set_server(ictx, SERVER, PORT);
 *     irc_set_nick(ictx, NICKNAME);
 *
 *     if (!irc_connect(ictx)) {
 *         fprintf(stderr, "irc_connect() failed\n");
 *         exit(EXIT_FAILURE);
 *     }
 *
 *     irc_printf(ictx, "JOIN %s", CHANNEL);
 *
 *     while (irc_online(ictx)) {
 *         tokarr msg;
 *         int r = irc_read(ictx, &msg, 1000000); // 1 second timeout
 *
 *         if (r == 0)
 *             continue; // Timeout, nothing was read yet.
 *
 *         if (r == -1) {
 *             fprintf(stderr, "irc_read() failed\n");
 *             break; // We're offline, irc_read() calls irc_reset() on error.
 *         }
 *
 *         // If we have reached this point, r must be 1, which means a message
 *         // was read.  Let's see what it is.  We care about PRIVMSG to
 *         // implement our ECHO behavior, and about PING to avoid timing out.
 *
 *         if (strcmp(msg[1], "PING") == 0)         // msg[1] is always the cmd
 *             irc_printf(ictx, "PONG %s", msg[2]); // msg[2] is the first arg
 *         else if (strcmp(msg[1], "PRIVMSG") == 0 && msg[2][0] == '#') // (*)
 *             irc_printf(ictx, "PRIVMSG %s :%s", msg[2], msg[3]);
 *
 *         // (*) Note that the line marked with (*) is NOT the right way to
 *         //     check whether the target is a channel (as opposed to a
 *         //     nickname - ours - in which case this would be a PM.
 *         //     However, it should suffice for the purposes of this example.
 *         //     The correct way would be to use irc_005chanmodes() to figure
 *         //     out whether tok[2][0] is a known channel type.
 *     }
 *
 *     fprintf(stderr, "Our connection is rekt.\n");
 *
 *     irc_dispose(ictx);
 *
 *     return 0;
 * }
 * \endcode
 * Assuming that code is saved as ircecho.c and libsrsirc was installed under
 * the /usr/local prefix (`./configure --prefix=/usr/local`), you'd build it
 * as follows:
 * \code
 * cc -I /usr/local/include -L /usr/local/lib -o ircecho ircecho.c -lsrsirc
 * \endcode
 * And run as:
 * \code
 * ./ircecho
 * \endcode
 * If that doesn't seem to work, run it with debugging messages as follows:
 * \code
 * export LIBSRSIRC_DEBUG=7       # 7 enables loglevels up to DEBUG.
 * export LIBSRSIRC_DEBUG_FANCY=1 # Only if your terminal supports ANSI colors
 * ./ircecho # Should now spit out information about what went wrong.
 *           # Forgot to change those #define-s, eh, master paster? :-)
 * \endcode
 * Have fun!
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
 * \return A pointer to the new IRC context, or NULL if we are out of memory. \n
 *         This pointer will remain valid until it is given to irc_dispose().
 * \sa irc, irc_dispose()
 */
irc *irc_init(void);

/** \brief Destroy an IRC context, invalidating the provided pointer.
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

/** \brief Connect and log on to IRC.
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
 * the soft timeout).
 *
 * We consider ourselves logged on once the IRC server sends us a "004" message
 * (or 383 if we're a service).  If of interest, the "log on conversation"
 * i.e. 001, 002, 003 and 004 can be retrieved by irc_logonconv() if there is
 * a need to see them (there usually isn't).
 *
 * \param ctx   IRC context as obtained by irc_init()
 *
 * \return true if successfully logged on, false on failure.
 *
 * \sa irc_set_server(), irc_set_pass(), irc_set_connect_timeout(),
 *     irc_logonconv()
 */
bool irc_connect(irc *ctx);

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

/** \brief Tell whether or not we (think we) are connected.
 *
 * Of course, we can't /really/ know unless we attempt to do I/O with the
 * server by calling irc_read() (or perhaps irc_write()).
 *
 * The following functions have the ability to change our idea of whether or
 * not we're connected: irc_connect(), irc_read(), irc_write(), irc_printf(),
 * irc_reset().
 *
 * \param ctx   IRC context as obtained by irc_init()
 *
 * \return true if we still appear to be connected.
 * \sa irc_connect(), irc_read(), irc_write(), irc_printf(), irc_reset()
 */
bool irc_online(irc *ctx);

/** \brief Read and process the next protocol message from the IRC server.
 *
 * Reads a protocol message from the server, field-splits it and populates
 * the tokarr structure pointed to by `tok` with the results.
 *
 * Usage example:
 * \code
 *   tokarr msg;
 *
 *   int r = irc_read(ctx, &msg, 1000000); // 1 second timeout
 *
 *   if (r == -1) {
 *       // ...read failure, we're now disconnected...
 *   } else if (r == 0) {
 *       // ...timeout, nothing to read...
 *   } else {
 *       // Okay, we have read a message.  If it is a PING, respond with a PONG.
 *       if (strcmp(msg[1], "PING") == 0)        // msg[1] is always the command
 *           irc_printf(ctx, "PONG %s", msg[2]); // msg[2] is the first argument
 *   }
 * \endcode
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param tok   Pointer to the result array, where pointers to the identified
 *              fields are stored in. \n
 *                  (*tok)[0] will point to the "prefix" (not including the
 *                      leading colon), or NULL if the message did not
 *                      contain a prefix (see also doc/terminology.txt). \n
 *                  (*tok)[1] will point to the (mandatory) "command" \n
 *                  (*tok)[2+n] will point to the n-th "argument", if it
 *                      exists; NULL otherwise \n
 * \param to_us   Read timeout in microseconds.\n
 *                0 means no timeout,\n
 *                1 is special in that it will effectively case a poll (return
 *                immediately when there's nothing to read yet).\n
 *                Note that *very* small timeouts >1 will likely NOT cause
 *                satisfactory results, depending on system speed, because
 *                the timeout might expire before the first actual call to
 *                select(2) (or equivalent).
 *
 * \return 1 on success; 0 on timeout; -1 on failure.
 *
 * The data pointed to by the elements of `tok` is valid until the next call
 * to this function is made.
 *
 * In the case of failure, an implicit call to irc_reset() is performed.
 *
 * \sa tokarr, irc_write(), irc_printf(), irc_reset()
 */
int irc_read(irc *ctx, tokarr *tok, uint64_t to_us);

/** \brief Send a protocol message to the IRC server.
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param line   Data to send, typically a single IRC protocol line (but may
 *               be multiple if properly separated by \\r\\n).
 *               If the (last or only) line does not end in \\r\\n, it will be
 *               appended.
 *
 * \return true on success, false on failure.
 *
 * In the case of failure, an implicit call to irc_reset() is performed.
 * \sa irc_printf(), irc_read(), irc_reset()
 */
bool irc_write(irc *ctx, const char *line);

/** \brief Send a formatted message to the IRC server, printf-style.
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param fmt   A printf-style format string
 *
 * After evaluating the format string and its respective arguments, irc_write()
 * is used to actually send the message.
 *
 * \return true on success, false on failure.
 *
 * In the case of failure, an implicit call to irc_reset() is performed.
 * \sa irc_write(), irc_read(), irc_reset()
 */
bool irc_printf(irc *ctx, const char *fmt, ...);

/** \brief Tell what our nickname is or was.
 *
 * We keep track of changes to our own nickname, this function can be used
 * to tell what our current nick is, or what our last nickname was before we
 * (got) disconnected.
 *
 * \param ctx   IRC context as obtained by irc_init()
 *
 * \return A pointer to our nickname.  The pointer is guaranteed to be
 *         valid only until the next call to irc_read() or irc_connect(). \n
 *         It does not matter whether we are connected or not; if we aren't,
 *         the returned nickname is simply the one we had before disconnecting.
 *
 * When used before the *first* call to irc_connect(), the returned pointer
 * will point to an empty string.
 *
 * \sa irc_set_nick()
 */
const char *irc_mynick(irc *ctx);

/** \brief Set IRC server and port to connect to.
 *
 * This setting will take effect not before the next call to irc_connect()
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param host   Server host, may be an IPv4 or IPv6 address or a DNS name.
 *               It is an error to pass NULL. \n
 *               We do *not* depend on this pointer to remain valid after
 *               we return, i.e. we do make a copy.
 * \param port   Server port (0 uses default (6667 or 6697 (with SSL)))
 *
 * \return true on success, false on failure (which means we're out of memory).
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_server(irc *ctx, const char *host, uint16_t port);

/** \brief Set server password (NULL means no password).
 *
 * This setting will take effect not before the next call to irc_connect().
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param srvpass   Server password for the next IRC connection.  Passing
 *                  NULL or a pointer to an empty string means not to use
 *                  any server password whatsoever. \n
 *                  We do *not* depend on this pointer to remain valid after
 *                  we return, i.e. we do make a copy.
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 */
bool irc_set_pass(irc *ctx, const char *srvpass);

/** \brief set IRC 'user name' (not the same as nickname).
 *
 * The 'username' is the middle part of an IRC identity
 * (`nickname!username@host`) (see also doc/terminology.txt).
 *
 * This setting will take effect not before the next call to irc_connect().
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param uname   Desired username for the next IRC connection. It is an error
 *                to pass NULL, but there is a default username in case this
 *                function is not used at all. \n
 *                We do *not* depend on this pointer to remain valid after
 *                we return, i.e. we do make a copy.
 *
 * \return true on success, false on failure (which means we're out of memory).
 *
 * In case of failure, the old value is left unchanged.
 * \sa DEF_UNAME
 */
bool irc_set_uname(irc *ctx, const char *uname);

/** \brief set IRC 'full name' for the next connection.
 *
 * The 'full name' is typically shown in responses to the WHOIS IRC command.
 *
 * This setting will take effect not before the next call to irc_connect()
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param fname   Desired full name for the next IRC connection. It is an error
 *                to pass NULL, but there is a default full name in case this
 *                function is not used at all. \n
 *                We do *not* depend on this pointer to remain valid after
 *                we return, i.e. we do make a copy.
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 * \sa DEF_FNAME
 */
bool irc_set_fname(irc *ctx, const char *fname);

/** \brief set IRC 'nickname' for the next connection.
 *
 * The nickname is the primary means to identify users on IRC.
 *
 * This setting will take effect not before the next call to irc_connect(),
 * i.e. it does *not* cause an on-line nick change.
 * (To do that, send a NICK message using irc_write() or irc_printf())
 *
 * \param ctx   IRC context as obtained by irc_init()
 * \param nick   Desired nickname for the next IRC connection.  It is an error
 *               to pass NULL, but there is a default nickname in case this
 *               function is not used at all. \n
 *               We do *not* depend on this pointer to remain valid after
 *               we return, i.e. we do make a copy.
 *
 * \return true on success, false on failure (which means we're out of memory)
 *
 * In case of failure, the old value is left unchanged.
 * \sa DEF_NICK
 */
bool irc_set_nick(irc *ctx, const char *nick);

/** \brief Dump state for debugging purposes.
 *
 * This function is intended for debugging and will puke out a dump of all
 * state associated with the given context to stderr.
 *
 * \param ctx   IRC context as obtained by irc_init()
 */
void irc_dump(irc *ctx);

/** @} */

#endif /* LIBSRSIRC_IRC_H */
