/* iio.h - i/o processing, protocol tokenization
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IIO_H
#define LIBSRSIRC_IIO_H 1

#include <stdbool.h>
#include <stddef.h>

#ifdef WITH_SSL
/* ssl */
# include <openssl/ssl.h>
#endif

#include <libsrsirc/irc_defs.h>

int ircio_read(sckhld sh, struct readctx *ctx, tokarr *tok, uint64_t to_us);
bool ircio_write(sckhld sck, const char *line);

#endif /* LIBSRSIRC_IIO_H */
