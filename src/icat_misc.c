/* icat_misc.c - Miscellaneous routines used in icat
 * icat - IRC netcat on top of libsrsirc - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_MISC

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "icat_misc.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>
#include <sys/time.h>
#include <unistd.h>

#include "icat_common.h"
#include "icat_debug.h"


static void tconv(struct timeval *tv, uint64_t *ts, bool tv_to_ts);

uint8_t
strtou8(const char *nptr, char **endptr, int base)
{
	return (uint8_t)strtoul(nptr, endptr, base);
}

uint64_t
strtou64(const char *nptr, char **endptr, int base)
{
	return (uint64_t)strtoull(nptr, endptr, base);
}

void
strNcpy(char *dest, const char *src, size_t destsz)
{
	if (!destsz)
		return;

	while (destsz-- && (*dest++ = *src++))
		;

	dest[-!destsz] = '\0';
}

void
strNcat(char *dest, const char *src, size_t destsz)
{
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

uint64_t
timestamp_us(void)
{
	struct timeval t;
	uint64_t ts = 0;
	if (gettimeofday(&t, NULL) != 0)
		CE("gettimeofday");
	else
		tconv(&t, &ts, true);

	return ts;
}

bool
isdigitstr(const char *str)
{
	while (*str)
		if (!isdigit((unsigned char)*str++))
			return false;

	return true;
}

void
sleep_us(uint64_t us)
{
	D("sleeping %"PRIu64" us", us);
	uint64_t secs = us / 1000000u;
	if (secs > INT_MAX)
		secs = INT_MAX; //eh.. yeah.

	sleep((int)secs);

	useconds_t u = us % 1000000u;
	if (u)
		usleep(u);
}

bool
ismychan(const char *chan)
{
	char chans[512];
	char search[512];
	snprintf(chans, sizeof chans, ",%s,", g_sett.chanlist);
	snprintf(search, sizeof search, ",%s,", chan);

	return strstr(chans, search);
}

static void
tconv(struct timeval *tv, uint64_t *ts, bool tv_to_ts)
{
	if (tv_to_ts)
		*ts = (uint64_t)tv->tv_sec * 1000000u + tv->tv_usec;
	else {
		tv->tv_sec = *ts / 1000000u;
		tv->tv_usec = *ts % 1000000u;
	}
}
