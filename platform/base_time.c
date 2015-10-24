/* base_time.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_BASETIME

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "base_time.h"

#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <logger/intlog.h>

#if HAVE_GETTIMEOFDAY
static void com_tconv(struct timeval *tv, uint64_t *ts, bool tv_to_ts);
#endif

uint64_t
lsi_b_tstamp_us(void)
{
#if HAVE_GETTIMEOFDAY
	struct timeval t;
	uint64_t ts = 0;
	if (gettimeofday(&t, NULL) != 0)
		EE("gettimeofday");
	else
		com_tconv(&t, &ts, true);

	return ts;
#else
# error "We need something like gettimeofday()"
#endif
}

#if HAVE_GETTIMEOFDAY
static void
com_tconv(struct timeval *tv, uint64_t *ts, bool tv_to_ts)
{
	if (tv_to_ts)
		*ts = (uint64_t)tv->tv_sec * 1000000 + tv->tv_usec;
	else {
		tv->tv_sec = *ts / 1000000;
		tv->tv_usec = *ts % 1000000;
	}
}
#endif
