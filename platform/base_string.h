/* base_string.h -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-18, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_BASE_STRING_H
#define LIBSRSIRC_BASE_STRING_H 1


#include <stddef.h>


#define STRACAT(DSTARR, STR) lsi_b_strNcat((DSTARR), (STR), sizeof (DSTARR))
#define STRACPY(DSTARR, STR) lsi_b_strNcpy((DSTARR), (STR), sizeof (DSTARR))

#define STRDUP(STR) lsi_b_strdup((STR), __FILE__, __LINE__, __func__)


void lsi_b_strNcat(char *dest, const char *src, size_t destsz);
void lsi_b_strNcpy(char *dest, const char *src, size_t destsz);

int lsi_b_strcasecmp(const char *a, const char *b);
int lsi_b_strncasecmp(const char *a, const char *b, size_t n);
char *lsi_b_strdup(const char *s, const char *file, int line, const char *func);


#endif /* LIBSRSIRC_BASE_STRING_H */
