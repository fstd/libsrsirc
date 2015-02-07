/* icat_misc.c - Miscellaneous routines used in icat
 * icat - IRC netcat on top of libsrsirc - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_ICATMISC

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "icat_misc.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <inttypes.h>

#include <platform/base_misc.h>

#include <logger/intlog.h>

#include "icat_common.h"


uint8_t
strtou8(const char *nptr, char **endptr, int base)
{ T("trace");
	return (uint8_t)strtoul(nptr, endptr, base);
}

uint64_t
strtou64(const char *nptr, char **endptr, int base)
{ T("trace");
	return (uint64_t)strtoull(nptr, endptr, base);
}

bool
ismychan(const char *chan)
{ T("trace");
	char chans[512];
	char search[512];
	snprintf(chans, sizeof chans, ",%s,", g_sett.chanlist);
	snprintf(search, sizeof search, ",%s,", chan);

	return strstr(chans, search);
}

bool
isdigitstr(const char *str)
{ T("trace");
	while (*str)
		if (!isdigit((unsigned char)*str++))
			return false;

	return true;
}
