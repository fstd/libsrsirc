/* base_misc.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_BASEMISC

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "base_misc.h"

#include <inttypes.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <logger/intlog.h>

void
b_usleep(uint64_t us)
{ T("trace");
	V("Sleeping %"PRIu64" us", us);
	uint64_t secs = us / 1000000u;
	if (secs > INT_MAX)
		secs = INT_MAX; //eh.. yeah.

#if HAVE_NANOSLEEP
	struct timespec tv = {(time_t)secs, (long)(us % 1000000u)*1000};
	if (tv.tv_sec || tv.tv_nsec)
		nanosleep(&tv, NULL);
#else
	E("we need something like nanosleep() (or usleep()-ish)")
#endif
}

int
b_getopt(int argc, char * const argv[], const char *optstring)
{ T("trace");
#if HAVE_GETOPT
	return getopt(argc, argv, optstring);
#else
	E("we need something like getopt()");
#endif
}

const char *
b_optarg(void)
{ T("trace");
#if HAVE_GETOPT
	return optarg;
#else
	E("we need something like getopt()");
#endif
}

int
b_optind(void)
{ T("trace");
#if HAVE_GETOPT
	return optind;
#else
	E("we need something like getopt()'s optind");
#endif
}
