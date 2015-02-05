/* base_misc.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "base_log.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


#define MAX_LOGMSG 1024


int
b_strerror(int errnum, char *buf, size_t sz)
{
	int r = 0;
#if HAVE_STRERROR_R
	r = strerror_r(errnum, buf, sz)
# if STRERROR_R_CHAR_P
	    ? 0 : -1
# endif
	    ;
#else
	snprintf(buf, sz, "Errno %d", errnum);
#endif
	return r;
}


char *
b_ctime(const time_t *t, char *buf)
{
#if HAVE_CTIME_R
	return ctime_r(t, buf);
#else
	sprintf(buf, "%lu", !t ? 0lu : (unsigned long)*t);
	return buf;
#endif
}


bool
b_openlog(const char *ident)
{
#if HAVE_SYSLOG_H
	openlog(ident, LOG_PID, LOG_USER);
	return true;
#endif
	return false;
}


void
b_closelog(void)
{
#if HAVE_SYSLOG_H
	closelog();
#endif
}


void
b_syslog(int prio, const char *fmt, ...)
{
#if HAVE_SYSLOG_H
	va_list l;
	va_start(l, fmt);
// this is a bitch to get a prototype in scope without having 
// to define _BSD_SOURCE or _GNU_SOURCE, in glibc
//# if HAVE_VSYSLOG
//	vsyslog(prio, fmt, l);
//# else
	char buf[2048];
	vsnprintf(buf, sizeof buf, fmt, l);
	syslog(prio, "%s", buf);
//# endif
	va_end(l);
#endif
}
