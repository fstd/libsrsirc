/* base_string.h -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_BASE_STRING_H
#define LIBSRSIRC_BASE_STRING_H 1


#include <stddef.h>


#define STRACAT(DSTARR, STR) b_strNcat((DSTARR), (STR), sizeof (DSTARR))
#define STRACPY(DSTARR, STR) b_strNcpy((DSTARR), (STR), sizeof (DSTARR))


void b_strNcat(char *dest, const char *src, size_t destsz);
void b_strNcpy(char *dest, const char *src, size_t destsz);

int b_strcasecmp(const char *a, const char *b);
int b_strncasecmp(const char *a, const char *b, size_t n);
char* b_strdup(const char *s);


#endif /* LIBSRSIRC_BASE_STRING_H */
