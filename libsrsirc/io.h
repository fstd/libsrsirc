/* io.h - i/o processing, protocol tokenization
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IIO_H
#define LIBSRSIRC_IIO_H 1


#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include "intdefs.h"

/* io_read
 * Read one message from the ircd, tokenize and populate `tok' with the results.
 *
 * Params: `sh':    Structure holding socket and, if enabled, SSL handle
 *         `ctx':   Read context structure primarily holding the read buffer
 *         `tok':   Pointer to result array, where pointers to the identified
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
 */
int io_read(sckhld sh, struct readctx *ctx, tokarr *tok, uint64_t to_us);

/* io_write
 * Send a message to the ircd
 *
 * Params: `sh':   Structure holding socket and, if enabled, SSL handle
 *         `line': Data to send, typically a single IRC protocol line (but may
 *                     be multiple if properly separated by \r\n).
 *                     If the line does not end in \r\n, it will be appended.
 *
 * Returns true on success, false on failure
 */
bool io_write(sckhld sh, const char *line);


#endif /* LIBSRSIRC_IIO_H */
