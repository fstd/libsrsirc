/* icat_misc.c - Miscellaneous routines used in icat
 * icat - IRC netcat on top of libsrsirc - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_ICATMISC

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "icat_misc.h"

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <platform/base_misc.h>

#include <logger/intlog.h>

#include "icat_common.h"


uint8_t
icat_strtou8(const char *nptr, char **endptr, int base)
{
	return (uint8_t)strtoul(nptr, endptr, base);
}

uint64_t
icat_strtou64(const char *nptr, char **endptr, int base)
{
	return (uint64_t)strtoull(nptr, endptr, base);
}

bool
icat_ismychan(const char *chan)
{
	char chans[512];
	char search[512];
	snprintf(chans, sizeof chans, ",%s,", g_sett.chanlist);
	snprintf(search, sizeof search, ",%s,", chan);

	return strstr(chans, search);
}

bool
icat_isdigitstr(const char *str)
{
	while (*str)
		if (!isdigit((unsigned char)*str++))
			return false;

	return true;
}
