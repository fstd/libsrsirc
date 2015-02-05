/* icat_misc.h - Miscellaneous routines used in icat
 * icat - IRC netcat on top of libsrsirc - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_ICAT_MISC_H
#define LIBSRSIRC_ICAT_MISC_H 1


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>



uint8_t strtou8(const char *nptr, char **endptr, int base);
uint64_t strtou64(const char *nptr, char **endptr, int base);

bool ismychan(const char *chan);
bool isdigitstr(const char *str);



#endif /* LIBSRSIRC_ICAT_MISC_H */
