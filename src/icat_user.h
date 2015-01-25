/* icat_user.h - User I/O (i.e. stdin/stdout) handling
 * icat - IRC netcat on top of libsrsirc - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_ICAT_USER_H
#define LIBSRSIRC_ICAT_USER_H 1


#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include <sys/types.h>


bool user_canread(void);
size_t user_readline(char *dest, size_t destsz);
int user_printf(const char *fmt, ...);

bool user_eof(void);


#endif /* LIBSRSIRC_ICAT_USER_H */
