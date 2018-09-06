/* base_misc.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-18, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_BASEMISC

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "base_misc.h"

#include <inttypes.h>
#include <signal.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <platform/base_misc.h>

#include <logger/intlog.h>

void
lsi_b_usleep(uint64_t us)
{
	V("Sleeping %"PRIu64" us", us);
	uint64_t secs = us / 1000000u;
	if (secs > INT_MAX)
		secs = INT_MAX; //eh.. yeah.

#if HAVE_NANOSLEEP
	struct timespec tv = {(time_t)secs, (long)(us % 1000000u)*1000};
	if (tv.tv_sec || tv.tv_nsec)
		nanosleep(&tv, NULL);
#else
# error "We need something like nanosleep() (or usleep()-ish)"
#endif
	return;
}

int
lsi_b_getopt(int argc, char * const argv[], const char *optstring)
{
#if HAVE_GETOPT
	return getopt(argc, argv, optstring);
#else
# error "We need something like getopt()"
#endif
}

const char *
lsi_b_optarg(void)
{
#if HAVE_GETOPT
	return optarg;
#else
# error "We need something like getopt()"
#endif
}

int
lsi_b_optind(void)
{
#if HAVE_GETOPT
	return optind;
#else
# error "We need something like getopt()'s optind"
#endif
}

void
lsi_b_regsig(int sig, void (*sigfn)(int))
{
#if HAVE_SIGACTION
	struct sigaction act;
	act.sa_handler = sigfn;
	act.sa_flags = 0;
	if (sigemptyset(&act.sa_mask) != 0) {
		WE("sigemptyset");
	} else if (sigaction(sig, &act, NULL) != 0) {
		WE("sigaction");
	}
#else
	W("No signal support; use a real OS for that");
#endif
	return;
}

void *
lsi_b_malloc(size_t sz, const char *file, int line, const char *func)
{
	void *r = malloc(sz);
	if (!r)
		/* NOTE: This does NOT call exit() or anything */
		EE("malloc in %s() at %s:%d", func, file, line);
	return r;
}
