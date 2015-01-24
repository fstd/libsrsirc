/* log.c - (C) 2012-14, Timo Buhrmester
 * libsrslog - A srs lib
 * See README for contact-, COPYING for license information.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "icat_debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <time.h>

#define DEF_LVL LOG_WARNING

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


static bool s_open;
static bool s_stderr = true;
static bool s_fancy;
static bool s_init;
static char *s_prgnam;
static int s_deflvl = DEF_LVL;
static char **s_modnam;
static int *s_lvlarr;
static size_t s_num_mods;


static const char* lvlnam(int lvl);
static const char* lvlcol(int lvl);


static void log_init(void);

// ----- public interface implementation -----


void
log_syslog(const char *ident, int facility)
{
	if (s_open)
		closelog();
	openlog(ident, LOG_PID, facility);
	s_open = true;
	s_fancy = false;
	s_stderr = false;
}


void
log_stderr(void)
{
	if (s_open)
		closelog();

	s_stderr = true;
}


void
log_setfancy(bool fancy)
{
	if (!s_stderr)
		return; //don't send color sequences to syslog
	s_fancy = fancy;
}


bool
log_getfancy(void)
{
	return s_stderr && s_fancy;
}


void
log_setlvl(int mod, int lvl)
{
	if (mod < 0)
		s_deflvl = lvl;
	else
		s_lvlarr[mod] = lvl;
}


int
log_getlvl(int mod)
{
	return mod < 0 ? s_deflvl : s_lvlarr[mod];
}


void
log_regmod(size_t modnumber, const char *modname)
{
	while (modnumber >= s_num_mods) {
		size_t nelem = s_num_mods * 2;
		if (!nelem)
			nelem = 8;

		char **na = malloc(nelem * sizeof *na);
		int *nl = malloc(nelem * sizeof *nl);
		memcpy(na, s_modnam, s_num_mods * sizeof *na);
		memcpy(nl, s_lvlarr, s_num_mods * sizeof *nl);
		for (size_t i = s_num_mods; i < nelem; i++)
			na[i] = NULL;
		for (size_t i = s_num_mods; i < nelem; i++)
			nl[i] = INT_MIN;
		free(s_modnam);
		free(s_lvlarr);
		s_modnam = na;
		s_lvlarr = nl;
		s_num_mods = nelem;
	}

	s_modnam[modnumber] = strdup(modname);
}

ssize_t
log_modnum(const char *modname)
{
	for (size_t i = 0; i < s_num_mods; i++) {
		if (s_modnam[i] && strcmp(s_modnam[i], modname))
			return i;
	}

	return -1;
}

ssize_t
log_maxmodnum(void)
{
	ssize_t i = s_num_mods - 1;
	for (; i >= 0 && !s_modnam[i]; i--);
	return i;
}

const char*
log_modname(size_t modnum)
{
	return s_modnam[modnum];
}

void
log_setprgnam(const char *prgnam)
{
	free(s_prgnam), s_prgnam = strdup(prgnam);
}

void
log_log(int mod, int lvl, int errn, const char *file, int line,
    const char *func, const char *fmt, ...)
{
	if (!s_init)
		log_init();

	bool always = lvl == INT_MIN;

	char *modnam;

	if (mod < 0 || (size_t)mod >= s_num_mods || !s_modnam[mod]) {
		if (lvl > s_deflvl)
			return;
		modnam = "unknown_module";
	} else {
		if (lvl > (s_lvlarr[mod] == INT_MIN ? s_deflvl : s_lvlarr[mod]))
			return;
		modnam = s_modnam[mod];
	}

	char payload[4096];
	char out[4096];

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
		strerror_r(errn, errmsg + 2, sizeof errmsg - 2);
	}

	if (s_stderr) {
		if (always) {
			fputs(payload, stderr);
			fputs("\n", stderr);
		} else {
			char timebuf[27];
			time_t t = time(NULL);
			if (!ctime_r(&t, timebuf))
				strcpy(timebuf, "(ctime_r() failed)");
			char *ptr = strchr(timebuf, '\n');
			if (ptr)
				*ptr = '\0';

			snprintf(out, sizeof out, "%s%s: %s/%s: %s: %s:%d:%s():"
			    " %s%s%s\n", s_fancy ? lvlcol(lvl) : "", timebuf,
			    s_prgnam, modnam, lvlnam(lvl), file, line, func,
			    payload, errmsg, s_fancy ? COL_RST : "");

			fputs(out, stderr);
		}
	} else {
		if (always)
			syslog(LOG_NOTICE, "%s", payload);
		else {
			snprintf(out, sizeof out, "%s: %s:%d:%s(): %s%s",
			    lvlnam(lvl), file, line, func, payload, errmsg);
			if (lvl > LOG_DEBUG)
				lvl = LOG_DEBUG; //syslog doesnt know >DEBUG
			/* not sure if should clamp the other end to LOG_ERR */
			syslog(lvl, "%s", out);
		}
	}

	va_end(vl);
}


// ---- local helpers ----


static const char*
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

static const char*
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

static void
log_init(void)
{
	char *pgnuc;
	if (!s_prgnam)
		s_prgnam = strdup("UNNAMED");

	pgnuc = strdup(s_prgnam);
	char *p = pgnuc;
	while (*p) {
		*p = toupper((unsigned char)*p);
		p++;
	}

	char evn[128];
	snprintf(evn, sizeof evn, "%s_DEBUG", pgnuc);
	char v[128];
	if (getenv_m(evn, v, sizeof v) == 0 && v[0]) {
		for (char *tok = strtok(v, " "); tok; tok = strtok(NULL, " ")) {
			char *eq = strchr(tok, '=');
			if (eq) {
				if (tok[0] == '=' || !isdigitstr(eq+1))
					continue;

				*eq = '\0';
				size_t mod = 0;
				for (; mod < s_num_mods; mod++)
					if (s_modnam[mod] &&
					    strcmp(s_modnam[mod], tok) == 0)
						break;

				if (mod < s_num_mods)
					s_lvlarr[mod] = (int)strtol(eq+1, NULL, 10);

				*eq = '=';
				continue;
			}
			if (isdigitstr(tok)) {
				/* special case: a stray number
				 * means set default loglevel */
				s_deflvl = (int)strtol(tok, NULL, 10);
			}
		}
	}

	snprintf(evn, sizeof evn, "%s_DEBUG_TARGET", pgnuc);
	const char *vv = getenv(evn);
	if (vv) {
		if (strcmp(vv, "syslog") == 0)
			log_syslog(s_prgnam, LOG_USER);
		else
			log_stderr();
	}

	snprintf(evn, sizeof evn, "%s_DEBUG_FANCY", pgnuc);
	vv = getenv(evn);
	if (vv)
		log_setfancy(vv[0] != '0');

	free(pgnuc);

	s_init = true;
}
