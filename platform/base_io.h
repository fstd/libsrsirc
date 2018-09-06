/* base_io.h -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-18, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_BASE_IO_H
#define LIBSRSIRC_BASE_IO_H 1


#include <stddef.h>


long lsi_b_stdin_read(void *buf, size_t nbytes);
int lsi_b_stdin_canread(void);
int lsi_b_stdin_fd(void);


#endif /* LIBSRSIRC_BASE_IO_H */
