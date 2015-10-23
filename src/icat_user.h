/* icat_user.h - User I/O (i.e. stdin/stdout) handling
 * icat - IRC netcat on top of libsrsirc - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_ICAT_USER_H
#define LIBSRSIRC_ICAT_USER_H 1


#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>


bool lsi_user_canread(void);
size_t lsi_user_readline(char *dest, size_t destsz);
int lsi_user_printf(const char *fmt, ...);
int lsi_user_fd(void);

bool lsi_user_eof(void);


#endif /* LIBSRSIRC_ICAT_USER_H */
