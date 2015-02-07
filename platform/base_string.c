/* base_string.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_BASESTR

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <string.h>

#if HAVE_STRINGS_H
# include <strings.h>
#endif

#include "base_string.h"

#include <logger/intlog.h>


void
b_strNcat(char *dest, const char *src, size_t destsz)
{ T("trace");
	size_t len = strlen(dest);
	if (len + 1 >= destsz)
		return;
	size_t rem = destsz - (len + 1);

	char *ptr = dest + len;
	while (rem-- && *src) {
		*ptr++ = *src++;
	}
	*ptr = '\0';
}

void
b_strNcpy(char *dest, const char *src, size_t destsz)
{ T("trace");
	if (!destsz)
		return;

	while (destsz-- && (*dest++ = *src++))
		;

	dest[-!destsz] = '\0';
}

int
b_strcasecmp(const char *a, const char *b)
{ T("trace");
#if HAVE_STRCASECMP
	return strcasecmp(a, b);
#else
	return b_strncasecmp(a, b, strlen(a) + 1);
#endif
}

int
b_strncasecmp(const char *a, const char *b, size_t n)
{ T("trace");
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

char*
b_strdup(const char *s)
{ T("trace");
	char *r = NULL;
	r = malloc(strlen(s)+1);
	if (!r)
		EE("malloc");
	else
		strcpy(r, s);

	return r;
}
