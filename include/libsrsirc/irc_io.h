/* irc_io.h - Back-end interface.
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#ifndef SRS_IRC_IO_H
#define SRS_IRC_IO_H 1

#define _GNU_SOURCE 1

#include <stdbool.h>
#include <stddef.h>

/* overview
int ircio_read(int sck, char *buf, size_t bsz, char **tok, size_t tlen);
int ircio_write(int sck, const char *line);
*/

/* ircio_read - attempt to read an IRC protocol message from socket
 * 
 * !! new parameter to_us not documented yet !!
 *
 * Blocks until there is some actual data available. However, said data *can*,
 *   for broken ircds, or unit tests, consist of only linear whitespace. In
 *   that case, no further attempt is made to read data, and 0 is returned.
 *
 * params: 
 *   int    ``sck''     - socket to read from
 *   char*  ``buf''     - buffer to read the data into and tokenize it
 *   size_t ``buf_sz''  - size of ``buf'', in chars
 *   char** ``tok''     - pointer to array of char* to hold the tokenized result
 *   size_t ``tok_len'' - no of elems of array pointed to by ``tok''.must be >=2
 *
 * return: 
 *    1 - success, a message has been read and tokenized
 *    0 - empty, the socket was readable, but all we got was an empty line
 *   -1 - an I/O- or parse error has occured, or given arguments are invalid
 *
 * example:
 *   A line read from an ircd consists of an optional 'prefix' (itself prefixed
 *     by ':'), a mandatory 'command', and a number (>= 0) of 'arguments', from
 *     which only the last one may contain whitespace (it is separated by ' :').
 *     Lines are always terminated by \r\n, according to RFC1459 (IRC)
 *   Examples for such lines are:
 *     :irc.example.com PRIVMSG #chan :hi there
 *     :somenick!someident@some.host INVITE #chan
 *     ERROR :too much faggotry from your site
 *     PING
 *
 *   The Examples show some possible combinations of prefix, command and a
 *     varying number of arguments.
 *   Such a line is first being read into ''buf``, possibly being truncated if
 *     there is not enough space available in order to hold the entire line.
 *   After a full line was read, it is tokenized according to the IRC protocol,
 *     by zeroing out all whitespace which acts as a delimiter.
 *   Pointers to the extracted tokens are finally stored in ''tok``, according
 *     to the following table (suppose a message with or without prefix, with
 *     n args (n >= 0): (furthermore we assume ``tok_len'' > n for this example)
 *     (*tok)[0] := <prefix without leading colon> or NULL if there was no pfx
 *     (*tok)[1] := <cmd>
 *     (*tok)[2] := <arg_0>
 *     (*tok)[3] := <arg_1>
 *     ...
 *     (*tok)[n-1] := <arg_n-1> (may contain whitespace)
 *     (*tok)[n] := NULL;
 *     ...
 *     (*tok)[``tok_len'' - 1] := NULL;
 *
 *  As hopefully becomes visible, the number of arguments can be determined
 *    by looking at the index of the first element to contain a NULL pointer
 *
 *  Note protocol parsing is not as strict as it could be, i.e. instead of
 *    requiring a single space to delimit parameters, any sequence of whitespace
 *    will do, empty lines are ignored, leading and trailing whitespace is
 *    stripped away (except for trail argument), etc.
 */
//int ircio_read(int sck, char *buf, char *overbuf, size_t bufoverbuf_sz, char **tok, size_t tok_len, unsigned long to_us);

int ircio_read(int sck, char *tokbuf, size_t tokbuf_sz, char *workbuf, size_t workbuf_sz, char **mehptr, char **tok, size_t tok_len, unsigned long to_us);
/* ircio_write - write one (or perhaps multiple) lines to socket
 * 
 * If multiple lines are to be written, they must be separated by "\r\n".
 * The last (or only) line may or may not be terminated by "\r\n", if not,
 * it is automatically added.
 *
 * This function will make sure everything is sent.
 *
 * params: 
 *   int         ``sck''    - socket to write to
 *   const char  ``line''   - line to send, may or may not be terminated by \r\n
 *
 * return: on success, the number of chars which were sent (i.e. strlen(line)
 *           (+2, if \r\n was added by this routine.)) is returned.
 *         if an error occurs, or invalid arguments were given, -1 is returned.
 */
int ircio_write(int sck, const char *line);

#endif /* SRS_IRC_IO_H */
