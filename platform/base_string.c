/* base_string.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-18, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_BASESTR

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <inttypes.h>
#include <string.h>

#if HAVE_STRINGS_H
# include <strings.h>
#endif

#include "base_string.h"

#include <platform/base_misc.h>

#include <logger/intlog.h>


void
lsi_b_strNcat(char *dest, const char *src, size_t destsz)
{
	const char *osrc = src;
	size_t len = strlen(dest);
	if (len + 1 >= destsz) {
		W("Buffer already full, cannot concatenate '%s'", osrc);
		return;
	}
	size_t rem = destsz - (len + 1);

	char *ptr = dest + len;
	while (rem-- && *src) {
		*ptr++ = *src++;
	}

	if (rem + 1 == 0 && *src)
		W("Buffer was too small for '%s', result truncated", osrc);

	*ptr = '\0';
	return;
}

void
lsi_b_strNcpy(char *dest, const char *src, size_t destsz)
{
	if (!destsz)
		return;

	do {
		destsz--;
		if (!(*dest++ = *src++))
			break;
	} while (destsz);

	if (!destsz)
		dest[-1] = '\0';

	return;
}

int
lsi_b_strcasecmp(const char *a, const char *b)
{
#if HAVE_STRCASECMP
	return strcasecmp(a, b);
#else
	return lsi_b_strncasecmp(a, b, strlen(a) + 1);
#endif
}

int
lsi_b_strncasecmp(const char *a, const char *b, size_t n)
{
#if HAVE_STRNCASECMP
	return strncasecmp(a, b, n);
#else
	unsigned char ua, ub;
	do {
		if (!n--)
			break;

		ua = (unsigned char)*a++;
		ub = (unsigned char)*b++;

	} while (ua && ub && (ua == ub || tolower(ua) == tolower(ub)));

	if (!ua && !ub)
		return 0;

	if (!ua)
		return -1;

	if (!ub)
		return 1;

	return (int)ua - ub;
#endif
}

char *
lsi_b_strdup(const char *s, const char *file, int line, const char *func)
{
	char *r = NULL;
	r = MALLOC(strlen(s)+1);
	if (r)
		strcpy(r, s);

	return r;
}
