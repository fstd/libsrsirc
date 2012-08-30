/* log.c - dirty pretty logging, usually to stderr
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _BSD_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#define _POSIX_C_SOURCE 199506L
#include <pthread.h>

#include <common.h> // :s
#include <log.h>

#define LOGBUFSZ 2048

#define COLSTR_RED "\033[31;01m"
#define COLSTR_YELLOW "\033[33;01m"
#define COLSTR_GREEN "\033[32;01m"
#define COLSTR_GRAY "\033[30;01m"

static const char* levtag(int level);
static const char* colstr(int level);
static void vlogf(logctx_t ctx, const char *file, int line, const char *func, int level, const char *fmt, va_list l);

struct thrlist_s {
	pthread_t thr;
	char *name;
	struct thrlist_s *next;
};

struct thrlist_s *thrlist = NULL;

struct logctx {
	FILE *str;
	int loglevel;
	char *mod;
	bool colors;
};


logctx_t
log_register(const char *mod, int loglevel, FILE *str, bool colors)
{
	logctx_t ctx = malloc(sizeof *ctx);
	ctx->str = str;
	ctx->loglevel = loglevel;
	ctx->mod = strdup(mod);
	ctx->colors = colors;
	return ctx;
}

void
log_set_color(logctx_t ctx, bool colors)
{
	ctx->colors = colors;
}

void
log_set_mod(logctx_t ctx, const char *mod)
{
	free(ctx->mod);
	ctx->mod = strdup(mod);
}

void
log_set_loglvl(logctx_t ctx, int lvl)
{
	ctx->loglevel = lvl;
}

void
log_set_str(logctx_t ctx, FILE *str)
{
	ctx->str = str;
}

void
log_set_thrname(pthread_t thr, const char *name)
{
	struct thrlist_s *newnode = malloc(sizeof *newnode);
	newnode->name = strdup(name);
	newnode->thr = thr;
	newnode->next = NULL;

	if (!thrlist)
		thrlist = newnode;
	else {
		struct thrlist_s *node = thrlist;

		while(node->next) {
			if (node->thr == thr) {
				free(node->name);
				node->name = strdup(name);
				free(newnode);
				return;
			}
			node = node->next;
		}
		
		node->next = newnode;
	}
}

bool
log_get_color(logctx_t ctx)
{
	return ctx->colors;
}

const char*
log_get_mod(logctx_t ctx)
{
	return ctx->mod;
}

int
log_get_loglvl(logctx_t ctx)
{
	return ctx->loglevel;
}

FILE*
log_get_str(logctx_t ctx)
{
	return ctx->str;
}

const char*
log_get_thrname(pthread_t thr)
{
	struct thrlist_s *node = thrlist;
	while(node) {
		if (node->thr == thr)
			return node->name;
		node = node->next;
	}
	return NULL;
}

static void
vlogf(logctx_t ctx, const char *file, int line, const char *func, int level, const char *fmt, va_list l)
{
	static int tid = 0;
	if (level > ctx->loglevel) //should maybe ditch it earlier
		return;
	char buf[LOGBUFSZ];
	const char *tname = log_get_thrname(pthread_self());
	if (!tname) {
		snprintf(buf, sizeof buf, "t-%d", tid++);
		log_set_thrname(pthread_self(), buf);
		tname = log_get_thrname(pthread_self());
	}
	snprintf(buf, sizeof buf, "%s(%lu) [%6.6s] %-7.7s: %16.16s():%-4d: ",
		(ctx->colors ? colstr(level):levtag(level)), time(NULL), tname, ctx->mod, func, line);

	size_t len = strlen(buf);
	vsnprintf(buf + len, sizeof buf - len, fmt, l);
	if (ctx->colors)
		strNcat(buf, "\033[0m", sizeof buf);
	strNcat(buf, "\n", sizeof buf);

	fputs(buf, ctx->str);
}

void
xerr(logctx_t ctx, const char *file, int line, const char *func, int eval, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xverr(ctx, file, line, func, eval, fmt, l);
	va_end(l);
}

void
xerrc(logctx_t ctx, const char *file, int line, const char *func, int eval, int code, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xverrc(ctx, file, line, func, eval, code, fmt, l);
	va_end(l);
}

void
xerrx(logctx_t ctx, const char *file, int line, const char *func, int eval, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xverrx(ctx, file, line, func, eval, fmt, l);
	va_end(l);
}

void
xverr (logctx_t ctx, const char *file, int line, const char *func, int eval, const char *fmt, va_list args)
{
	xverrc(ctx, file, line, func, eval, errno, fmt, args);
}

void
xverrc(logctx_t ctx, const char *file, int line, const char *func, int eval, int code, const char *fmt, va_list args)
{
	char buf[LOGBUFSZ];
	strncpy(buf, fmt, sizeof buf);
	strNcat(buf, ": ", sizeof buf);
	size_t len = strlen(buf);
	strerror_r(code, buf + len, sizeof buf - len);
	xverrx(ctx, file, line, func, eval, buf, args);
}

void
xverrx(logctx_t ctx, const char *file, int line, const char *func, int eval, const char *fmt, va_list args)
{
	vlogf(ctx, file, line, func, LOGLVL_ERR, fmt, args);
	exit(eval);
}

void
xwarn(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xvwarn(ctx, file, line, func, fmt, l);
	va_end(l);
}

void
xwarnc(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xvwarnc(ctx, file, line, func, code, fmt, l);
	va_end(l);
}

void
xwarnx(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xvwarnx(ctx, file, line, func, fmt, l);
	va_end(l);
}

void
xvwarn (logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	xvwarnc(ctx, file, line, func, errno, fmt, args);
}

void
xvwarnc(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, va_list args)
{
	char buf[LOGBUFSZ];
	strncpy(buf, fmt, sizeof buf);
	strNcat(buf, ": ", sizeof buf);
	size_t len = strlen(buf);
	strerror_r(code, buf + len, sizeof buf - len);
	xvwarnx(ctx, file, line, func, buf, args);
}

void
xvwarnx(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	vlogf(ctx, file, line, func, LOGLVL_WARN, fmt, args);
}

void
xnote(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xvnote(ctx, file, line, func, fmt, l);
	va_end(l);
}

void
xnotec(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xvnotec(ctx, file, line, func, code, fmt, l);
	va_end(l);
}

void
xnotex(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xvnotex(ctx, file, line, func, fmt, l);
	va_end(l);
}

void
xvnote (logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	xvnotec(ctx, file, line, func, errno, fmt, args);
}

void
xvnotec(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, va_list args)
{
	char buf[LOGBUFSZ];
	strncpy(buf, fmt, sizeof buf);
	strNcat(buf, ": ", sizeof buf);
	size_t len = strlen(buf);
	strerror_r(code, buf + len, sizeof buf - len);
	xvnotex(ctx, file, line, func, buf, args);
}

void
xvnotex(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	vlogf(ctx, file, line, func, LOGLVL_NOTE, fmt, args);
}

void
xdbg(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xvdbg(ctx, file, line, func, fmt, l);
	va_end(l);
}

void
xdbgc(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xvdbgc(ctx, file, line, func, code, fmt, l);
	va_end(l);
}

void
xdbgx(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
		xvdbgx(ctx, file, line, func, fmt, l);
	va_end(l);
}

void
xvdbg (logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	xvdbgc(ctx, file, line, func, errno, fmt, args);
}

void
xvdbgc(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, va_list args)
{
	char buf[LOGBUFSZ];
	strncpy(buf, fmt, sizeof buf);
	strNcat(buf, ": ", sizeof buf);
	size_t len = strlen(buf);
	strerror_r(code, buf + len, sizeof buf - len);
	xvdbgx(ctx, file, line, func, buf, args);
}

void
xvdbgx(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args)
{
	vlogf(ctx, file, line, func, LOGLVL_DBG, fmt, args);
}


static const char*
levtag(int level)
{
	switch(level) {
		case LOGLVL_ERR: return "[ERR] ";
		case LOGLVL_WARN: return "[WRN] ";
		case LOGLVL_NOTE: return "[NOT] ";
		case LOGLVL_DBG: return "[DBG] ";
		default:
			assert(false);
	}
	return "";
}

static const char*
colstr(int level)
{
	switch(level) {
		case LOGLVL_ERR: return COLSTR_RED;
		case LOGLVL_WARN: return COLSTR_YELLOW;
		case LOGLVL_NOTE: return COLSTR_GREEN;
		case LOGLVL_DBG: return COLSTR_GRAY;
		default:
			assert(false);
	}
	return "";
}


