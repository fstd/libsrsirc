/* iio.h - Back-end interface.
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef SRS_IRC_IO_H
#define SRS_IRC_IO_H 1

#include <stdbool.h>
#include <stddef.h>

#ifdef WITH_SSL
/* ssl */
# include <openssl/ssl.h>
#endif

#include <libsrsirc/irc_defs.h>

/* ircio_read - attempt to read an IRC protocol message from socket
 *
 * Blocks until there is some actual data available, or a timeout is
 *   reached. However, said data *can*, for broken ircds, or unit tests,
 *   consist of only linear whitespace. In that case, no further attempt
 *   is made to read data, and 0 is returned.
 *
 * params:
 *   sckhld ``sh'' - socket "holder" structure
 *   struct readctx ``ctx'' - read context structure
 *   tokarr* ``tok'' - pointer to array of char* pointing to tokens
 *   unsigned long ``to_us'' - timeout in microseconds
 *
 * return:
 *    1 - success, a message has been read and tokenized
 *    0 - empty, the socket was readable, but all we got was an empty line
 *          also returned when a timeout is hit
 *   -1 - an I/O- or parse error has occured, or given args are invalid
 *
 * example:
 *   A line read from an ircd consists of an optional 'prefix' (itself
 *     prefixed by ':'), a mandatory 'command', and a number (>= 0) of
 *     'arguments', from which only the last one may contain whitespace
 *     (it is separated by ' :').
 *     Lines are always terminated by \r\n, according to RFC1459 (IRC)
 *   Examples for such lines are:
 *     :irc.example.com PRIVMSG #chan :hi there
 *     :somenick!someident@some.host INVITE #chan
 *     ERROR :too much faggotry from your site
 *     PING
 *
 *   The Examples show some possible combinations of prefix, command and a
 *     varying number of arguments.
 *   Pointers to the extracted tokens are stored in ''tok``,
 *     according to the following table (suppose a message with or without
 *     prefix, with n args (n >= 0):
 *     (*tok)[0] := <prefix w/o leading colon> or NULL if there was no pfx
 *     (*tok)[1] := <cmd>
 *     (*tok)[2] := <arg_0>
 *     (*tok)[3] := <arg_1>
 *     ...
 *     (*tok)[2+n-1] := <arg_n-1> (may contain whitespace)
 *     (*tok)[2+n] := NULL;
 *
 *  As hopefully becomes visible, the number of arguments can be determined
 *    by looking at the index of the first element to hold a NULL pointer,
 *    starting with index 1.
 *
 *  Note protocol parsing is not as strict as it could be, i.e. instead of
 *    requiring a single space to delimit parameters, any sequence of
 *    whitespace will do, empty lines are ignored, leading and trailing
 *    whitespace may be stripped away (except for trail argument), etc.
 */
int ircio_read(sckhld sh, struct readctx *ctx, tokarr *tok, uint64_t to_us);

/* ircio_write - write one (or perhaps multiple) lines to socket
 *
 * If multiple lines are to be written, they must be separated by "\r\n".
 * The last (or only) line may or may not be terminated by "\r\n", if not,
 * it is automatically added.
 *
 * This function will make sure everything is sent.
 *
 * params:
 *   sckhld      ``sh''    - socket "holder" structure
 *   const char  ``line''   - line to send, may be terminated by \r\n
 *
 * return: on success, the number of chars sent (i.e. strlen(line)
 *           (+2, if \r\n was added by this routine.)) is returned.
 *         if an error occurs, or invalid args were given, -1 is returned.
 */
bool ircio_write(sckhld sck, const char *line);

#endif /* SRS_IRC_IO_H */
