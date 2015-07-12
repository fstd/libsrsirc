/* irc.h - very basic front-end interface (irc_ext.h has the rest)
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_H
#define LIBSRSIRC_IRC_H 1


#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>


/* Overview:
 *
 * irc irc_init(void);
 * bool irc_reset(irc hnd);
 * bool irc_dispose(irc hnd);
 * bool irc_connect(irc hnd);
 * bool irc_online(irc hnd);
 * int irc_read(irc hnd, tokarr *tok, uint64_t to_us);
 * bool irc_write(irc hnd, const char *line);
 * const char *irc_mynick(irc hnd);
 * bool irc_set_server(irc hnd, const char *host, uint16_t port);
 * bool irc_set_pass(irc hnd, const char *srvpass);
 * bool irc_set_uname(irc hnd, const char *uname);
 * bool irc_set_fname(irc hnd, const char *fname);
 * bool irc_set_nick(irc hnd, const char *nick);
*/

/* irc_init - Allocate and initialize a new irc object
 * Returns a handle to the instance, or NULL if out of memory.  */
irc irc_init(void);

/* irc_reset - Force a disconnect from the IRC server */
void irc_reset(irc hnd);

/* irc_dispose - Destroy irc object and relinquish associated resources */
void irc_dispose(irc hnd);

/* irc_connect - Connect and log on to IRC
 *
 * Attempts to connect/log on to the IRC server specified by irc_set_server().
 * Connecting will involve, if applicable, resolving a DNS name and attempting
 *     to connect to each of the resulting A- or AAAA records.
 * Behavior is affected by two timeouts (a "hard" and a "soft" timeout), which
 *     can be set via irc_set_connect_timeout().  The "hard" timeout limits the
 *     overall time trying to get an IRC connection going, while the "soft"
 *     timeout applies each time anew for each attempt to establish a TCP
 *     connection (so that we can be sure all A- or AAAA records are tried).
 * We consider ourselves logged on once we see the "004" message
 *     (or 383 if we're a service).  If of interest, the "log on conversation"
 *     i.e. 001, 002, 003 and 004 can be retrieved by irc_logonconv().
 *
 * Returns true on success, false on failure
 */
bool irc_connect(irc hnd);

/* irc_online - Tell whether or not we (think we) are connected
 * Of course, we can't /really/ know unless we attempt to read or write */
bool irc_online(irc hnd);

/* irc_read - Read and process the next protocol message from server
 *
 * Read a protocol message from the ircd, tokenize and populate `tok' with the
 *     results.
 *
 * Params: `tok':   Pointer to result array, where pointers to the identified
 *                      tokens are stored in.
 *                  (*tok)[0] will point to the "prefix" (not including the
 *                      leading colon), or NULL if the message did not
 *                      contain a prefix.
 *                  (*tok)[1] will point to the (mandatory) "command"
 *                  (*tok)[2+n] will point to the n-th "argument", if it
 *                      exists; NULL otherwise (for 0 <= n < sizeof *tok - 2)
 *         `to_us': Timeout in microseconds (0 = no timeout)
 *
 * Returns 1 on success; 0 on timeout; -1 on failure
 * In the case of failure, an implicit call to irc_reset() is made.
 */
int irc_read(irc hnd, tokarr *tok, uint64_t to_us);

/* irc_write - Send a message to the ircd
 *
 * Params: `line': Data to send, typically a single IRC protocol line (but may
 *                     be multiple if properly separated by \r\n).
 *                     If the line does not end in \r\n, it will be appended.
 *
 * Returns true on success, false on failure
 * In the case of failure, an implicit call to irc_reset() is made.
 */
bool irc_write(irc hnd, const char *line);

/* irc_mynick - Tell what we think is our current nick (we keep track of it) */
const char *irc_mynick(irc hnd);

/* irc_set_server - Set server and port to connect to
 * Params: `host': Server host, may be an IPv4 or IPv6 address or a DNS name
 *         `port': Server port (0 uses default (6667 or 6697 (SSL)))
 *
 * This setting will take effect on the next call to irc_connect()
 * Returns true on success, false on failure (which means we're out of memory)
 * In case of failure, the old value is left untouched.
 */
bool irc_set_server(irc hnd, const char *host, uint16_t port);

/* irc_set_pass - set server password (NULL means no password)
 * This setting will take effect on the next call to irc_connect()
 * Returns true on success, false on failure (which means we're out of memory)
 * In case of failure, the old value is left untouched.
 */
bool irc_set_pass(irc hnd, const char *srvpass);

/* irc_set_uname - Set IRC 'user name' (not the same as nickname)
 * This setting will take effect on the next call to irc_connect()
 * Returns true on success, false on failure (which means we're out of memory)
 * In case of failure, the old value is left untouched.
 */
bool irc_set_uname(irc hnd, const char *uname);

/* irc_set_fname - Set IRC 'full name' (which appears e.g. in WHOIS)
 * This setting will take effect on the next call to irc_connect()
 * Returns true on success, false on failure (which means we're out of memory)
 * In case of failure, the old value is left untouched.
 */
bool irc_set_fname(irc hnd, const char *fname);

/* irc_set_nick - Set IRC 'nickname'
 * This setting will take effect on the next call to irc_connect() (i.e. it does
 *     /not/ cause an on-line nick change.  To do that, send a NICK message)
 * Returns true on success, false on failure (which means we're out of memory)
 * In case of failure, the old value is left untouched.
 */
bool irc_set_nick(irc hnd, const char *nick);

/* for debugging */
void irc_dump(irc h);


#endif /* LIBSRSIRC_IRC_H */
