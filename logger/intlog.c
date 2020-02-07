/* intlog.c - library debugging impl
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-20, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "intlog.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define DEF_LVL LOG_CRIT

#define COL_REDINV "\033[07;31;01m"
#define COL_RED "\033[31;01m"
#define COL_YELLOW "\033[33;01m"
#define COL_GREEN "\033[32;01m"
#define COL_WHITE "\033[37;01m"
#define COL_WHITEINV "\033[07;37;01m"
#define COL_GRAY "\033[30;01m"
#define COL_GRAYINV "\033[07;30;01m"
#define COL_LBLUE "\033[34;01m"
#define COL_RST "\033[0m"

#define COUNTOF(ARR) (sizeof (ARR) / sizeof (ARR)[0])


const char *modnames[NUM_MODS] = {
	[MOD_IRC] = "libsrsirc/irc",
	[MOD_COMMON] = "libsrsirc/common",
	[MOD_IRC_UTIL] = "libsrsirc/util",
	[MOD_ICONN] = "libsrsirc/conn",
	[MOD_IIO] = "libsrsirc/io",
	[MOD_PROXY] = "libsrsirc/px",
	[MOD_IMSG] = "libsrsirc/msg",
	[MOD_SKMAP] = "libsrsirc/skmap",
	[MOD_PLST] = "libsrsirc/plst",
	[MOD_TRACK] = "libsrsirc/track",
	[MOD_UCBASE] = "libsrsirc/ucbase",
	[MOD_V3] = "libsrsirc/v3",
	[MOD_BASEIO] = "libsrsirc/base-io",
	[MOD_BASENET] = "libsrsirc/base-net",
	[MOD_BASETIME] = "libsrsirc/base-time",
	[MOD_BASESTR] = "libsrsirc/base-str",
	[MOD_BASEMISC] = "libsrsirc/base-misc",
	[MOD_ICATINIT] = "icat/init",
	[MOD_ICATCORE] = "icat/core",
	[MOD_ICATSERV] = "icat/serv",
	[MOD_ICATUSER] = "icat/user",
	[MOD_ICATMISC] = "icat/misc",
	[MOD_IWAT] = "iwat",
	[MOD_UNKNOWN] = "(??" "?)"
};


static bool s_open;
static bool s_stderr = true;
static bool s_fancy;
static bool s_init;
static int s_lvlarr[NUM_MODS];

static int s_w_modnam = 20;
static int s_w_file = 0;
static int s_w_line = 0;
static int s_w_func = 0;

static int s_calldepth = 0;

static const char *lvlnam(int lvl);
static const char *lvlcol(int lvl);
static int getenv_m(const char *nam, char *dest, size_t destsz);
static bool isdigitstr(const char *p);


// ----- public interface implementation -----


void
lsi_log_syslog(const char *ident)
{
	if (s_open)
		lsi_b_closelog();

	/* currently redundant */
	s_open = false;
	s_stderr = true;

	if (!lsi_b_openlog(ident))
		return;

	s_open = true;
	s_fancy = false;
	s_stderr = false;
}


void
lsi_log_stderr(void)
{
	if (s_open)
		lsi_b_closelog();

	s_stderr = true;
}


void
lsi_log_setfancy(bool fancy)
{
	if (!s_stderr)
		return; //don't send color sequences to syslog
	s_fancy = fancy;
}


bool
lsi_log_getfancy(void)
{
	return s_stderr && s_fancy;
}


void
lsi_log_setlvl(int mod, int lvl)
{
	s_lvlarr[mod] = lvl;
}

int
lsi_log_getlvl(int mod)
{
	return s_lvlarr[mod];
}

void
lsi_log_log(int mod, int lvl, int errn, const char *file, int line,
    const char *func, const char *fmt, ...)
{
	if (!s_init)
		lsi_log_init();

	bool always = lvl == INT_MIN;

	if (lvl > s_lvlarr[mod])
		return;

	char resmsg[4096];

	char payload[4096];
	va_list vl;
	va_start(vl, fmt);

	vsnprintf(payload, sizeof payload, fmt, vl);
	char *c = payload;
	while (*c) {
		if (*c == '\n' || *c == '\r')
			*c = '$';
		c++;
	}

	char errmsg[256];
	errmsg[0] = '\0';
	if (errn >= 0) {
		errmsg[0] = ':';
		errmsg[1] = ' ';
		lsi_b_strerror(errn, errmsg + 2, sizeof errmsg - 2);
	}

	if (s_stderr) {
		if (always) {
			fputs(payload, stderr);
			fputs("\n", stderr);
		} else {
			char pad[256];
			if (lvl == LOG_TRACE) {
				size_t d = s_calldepth * 2;
				if (d > sizeof pad)
					d = sizeof pad - 1;
				memset(pad, ' ', d);
				pad[d] = '\0';
			} else
				pad[0] = '\0';

			char timebuf[27];
			time_t t = time(NULL);
			if (!lsi_b_ctime(&t, timebuf))
				strcpy(timebuf, "(lsi_b_ctime() failed)");
			char *ptr = strchr(timebuf, '\n');
			if (ptr)
				*ptr = '\0';

			snprintf(resmsg, sizeof resmsg, "%s%s: %*s: "
			    "%s: %s%*s:%*d:%*s(): %s%s%s\n",
			    s_fancy ? lvlcol(lvl) : "",
			    timebuf,
			    s_w_modnam, modnames[mod],
			    lvlnam(lvl),
			    pad,
			    s_w_file, file,
			    s_w_line, line,
			    s_w_func, func,
			    payload,
			    errmsg,
			    s_fancy ? COL_RST : "");

			fputs(resmsg, stderr);
		}
	} else {
		if (always)
			lsi_b_syslog(LOG_NOTICE, "%s", payload);
		else {
			snprintf(resmsg, sizeof resmsg, "%s: %s:%d:%s(): %s%s",
			    modnames[mod], file, line, func, payload, errmsg);
			lsi_b_syslog(lvl, "%s", resmsg);
		}
	}

	va_end(vl);
}

void
lsi_log_init(void)
{
	int deflvl = DEF_LVL;
	for (size_t i = 0; i < COUNTOF(s_lvlarr); i++)
		s_lvlarr[i] = INT_MIN;

	char v[128];
	if (getenv_m("LIBSRSIRC_DEBUG", v, sizeof v) == 0 && v[0]) {
		for (char *tok = strtok(v, " "); tok; tok = strtok(NULL, " ")) {
			char *eq = strchr(tok, '=');
			if (eq) {
				if (tok[0] == '=' || !isdigitstr(eq+1))
					continue;

				*eq = '\0';
				size_t mod = 0;
				for (; mod < COUNTOF(modnames); mod++)
					if (strcmp(modnames[mod], tok) == 0)
						break;

				if (mod < COUNTOF(s_lvlarr))
					s_lvlarr[mod] =
					    (int)strtol(eq+1, NULL, 10);

				*eq = '=';
				continue;
			}
			eq = strchr(tok, ':');
			if (eq) {
				if (tok[0] == ':' || !isdigitstr(eq+1))
					continue;

				*eq = '\0';
				int val = (int)strtol(eq+1, NULL, 10);
				if (strcmp(tok, "modnam") == 0) {
					s_w_modnam = val;
				} else if (strcmp(tok, "file") == 0) {
					s_w_file = val;
				} else if (strcmp(tok, "line") == 0) {
					s_w_line = val;
				} else if (strcmp(tok, "func") == 0) {
					s_w_func = val;
				}

				*eq = ':';
				continue;
			}
			if (isdigitstr(tok)) {
				/* special case: a stray number
				 * means set default loglevel */
				deflvl = (int)strtol(tok, NULL, 10);
			}
		}
	}

	for (size_t i = 0; i < COUNTOF(s_lvlarr); i++)
		if (s_lvlarr[i] == INT_MIN)
			s_lvlarr[i] = deflvl;

	const char *vv = getenv("LIBSRSIRC_DEBUG_TARGET");
	if (vv && strcmp(vv, "syslog") == 0)
		lsi_log_syslog("libsrsirc");
	else
		lsi_log_stderr();

	vv = getenv("LIBSRSIRC_DEBUG_FANCY");
	if (vv && vv[0] != '0')
		lsi_log_setfancy(true);
	else
		lsi_log_setfancy(false);

	s_init = true;
}

void
lsi_log_tret(void)
{
	if (s_calldepth == 0) {
		fprintf(stderr, "call depth would be <0\n");
		return;
	}
	s_calldepth--;
}

void
lsi_log_tcall(void) {
	s_calldepth++;
}

// ---- local helpers ----


static const char *
lvlnam(int lvl)
{
	return lvl == LOG_DEBUG ? "DBG" :
	       lvl == LOG_TRACE ? "TRC" :
	       lvl == LOG_VIVI ? "VIV" :
	       lvl == LOG_INFO ? "INF" :
	       lvl == LOG_NOTICE ? "NOT" :
	       lvl == LOG_WARNING ? "WRN" :
	       lvl == LOG_CRIT ? "CRT" :
	       lvl == LOG_ERR ? "ERR" : "???";
}

static const char *
lvlcol(int lvl)
{
	return lvl == LOG_DEBUG ? COL_GRAY :
	       lvl == LOG_TRACE ? COL_LBLUE :
	       lvl == LOG_VIVI ? COL_GRAYINV :
	       lvl == LOG_INFO ? COL_WHITE :
	       lvl == LOG_NOTICE ? COL_GREEN :
	       lvl == LOG_WARNING ? COL_YELLOW :
	       lvl == LOG_CRIT ? COL_REDINV :
	       lvl == LOG_ERR ? COL_RED : COL_WHITEINV;
}

static bool
isdigitstr(const char *p)
{
	while (*p)
		if (!isdigit((unsigned char)*p++))
			return false;
	return true;
}

static int
getenv_m(const char *nam, char *dest, size_t destsz)
{
	const char *v = getenv(nam);
	if (!v)
		return -1;

	snprintf(dest, destsz, "%s", v);
	return 0;
}
