/* intlog.c - library debugging impl
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "intlog.h"

#define COL_REDINV "\033[07;31;01m"
#define COL_RED "\033[31;01m"
#define COL_YELLOW "\033[33;01m"
#define COL_GREEN "\033[32;01m"
#define COL_WHITE "\033[37;01m"
#define COL_WHITEINV "\033[07;37;01m"
#define COL_GRAY "\033[30;01m"
#define COL_RST "\033[0m"


static bool s_open;
static bool s_stderr = true;
static int s_lvl = -1;
static bool s_fancy;

static const char* lvlnam(int lvl);
static const char* lvlcol(int lvl);


// ----- public interface implementation -----


void
ircdbg_syslog(const char *ident, int facility)
{
	if (s_open)
		closelog();
	openlog(ident, LOG_PID, facility);
	s_open = true;
	s_fancy = false;
	s_stderr = false;
}


void
ircdbg_stderr(void)
{
	if (s_open)
		closelog();
	
	s_stderr = true;
}


void
ircdbg_setfancy(bool fancy)
{
	if (!s_stderr)
		return; //don't send color sequences to syslog
	s_fancy = fancy;
}


bool
ircdbg_getfancy(void)
{
	return s_stderr && s_fancy;
}


void
ircdbg_setlvl(int lvl)
{
	s_lvl = lvl;
}


int
ircdbg_getlvl(void)
{
	return s_lvl;
}


void
ircdbg_log(int lvl, int errn, const char *file, int line, const char *func,
    const char *fmt, ...)
{
	if (s_lvl == -1) {
		const char *v = getenv("LIBSRSIRC_DEBUG");
		s_lvl = v ? v[0] - '0' : LOG_ERR;
		v = getenv("LIBSRSIRC_DEBUG_TARGET");
		if (v && strcmp(v, "syslog") == 0)
			ircdbg_syslog("libsrsirc", LOG_USER);
		else
			ircdbg_stderr();
		v = getenv("LIBSRSIRC_DEBUG_FANCY");
		if (v && v[0] != '0')
			ircdbg_setfancy(true);
		else
			ircdbg_setfancy(false);
	}

	if (lvl > s_lvl)
		return;

	char buf[4096];

	va_list vl;
	va_start(vl, fmt);

	char errmsg[256];
	errmsg[0] = '\0';
	if (errn >= 0) {
		errmsg[0] = ':';
		errmsg[1] = ' ';
		strerror_r(errn, errmsg + 2, sizeof errmsg - 2);
	}

	if (s_stderr) {
		char timebuf[27];
		time_t t = time(NULL);
		if (!ctime_r(&t, timebuf))
			strcpy(timebuf, "(ctime_r() failed)");
		char *ptr = strchr(timebuf, '\n');
		if (ptr)
			*ptr = '\0';

		snprintf(buf, sizeof buf, "%s%s: libsrsirc: %s: %s:%d:%s(): %s%s%s\n",
		    s_fancy ? lvlcol(lvl) : "", timebuf, lvlnam(lvl), file, line,
		    func, fmt, errmsg, s_fancy ? COL_RST : "");
		vfprintf(stderr, buf, vl);

	} else {
		snprintf(buf, sizeof buf, "%s:%d:%s(): %s%s",
		    file, line, func, fmt, errmsg);
		vsyslog(lvl, buf, vl);
	}

	va_end(vl);
}


// ---- local helpers ---- 


static const char*
lvlnam(int lvl)
{
	return lvl == LOG_DEBUG ? "DBG" :
	       lvl == LOG_INFO ? "INF" :
	       lvl == LOG_NOTICE ? "NOT" :
	       lvl == LOG_WARNING ? "WRN" :
	       lvl == LOG_CRIT ? "CRT" :
	       lvl == LOG_ERR ? "ERR" : "???";
}

static const char*
lvlcol(int lvl)
{
	return lvl == LOG_DEBUG ? COL_GRAY :
	       lvl == LOG_INFO ? COL_WHITE :
	       lvl == LOG_NOTICE ? COL_GREEN :
	       lvl == LOG_WARNING ? COL_YELLOW :
	       lvl == LOG_CRIT ? COL_REDINV :
	       lvl == LOG_ERR ? COL_RED : COL_WHITEINV;
}
