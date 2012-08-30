/* log.h - Interface to the dirty pretty logger
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>
/* ugly hackish pretty logger */

/* must never be included from another header file */

/* In order to use the logger, the user has to directly include this
 *   header, and invoke LOG_INIT() or LOG_INITX() somewhere, usually at
 *   the beginning of main, or in a module's initialization routine.
 * Afterwards, behaviour/appearance can be adjusted by use of the `LOG_COLORS`,
 *   `LOG_TARGET`, `LOG_MOD` and `LOG_LEVEL` macros
 * By default, "unnamed" logs only errors to stderr, w/o colors.
 */


#define LOGLVL_ERR 0
#define LOGLVL_WARN 1
#define LOGLVL_NOTE 2
#define LOGLVL_DBG 3

#define LOG_INIT()      do{s_logctx = log_register("unnamed", LOGLVL_ERR, stderr, false);}while(0)
#define LOG_ISINIT()    (s_logctx != NULL)
#define LOG_INITX(MOD,LVL,STR,COL) do{s_logctx = log_register((MOD), (LVL), (STR), (COL));}while(0)
#define LOG_COLORS(ON)  do{log_set_color(s_logctx, (ON));}while(0)
#define LOG_TARGET(STR) do{log_set_str(s_logctx, (STR));}while(0)
#define LOG_MOD(STR)    do{log_set_mod(s_logctx, (STR));}while(0)
#define LOG_LEVEL(LVL)  do{log_set_loglvl(s_logctx, (LVL));}while(0)
#define LOG_TNAME(STR)  do{log_set_thrname(pthread_self(), (STR));}while(0)
#define LOG_GETCOLORS   log_get_color(s_logctx)
#define LOG_GETTARGET   log_get_str(s_logctx)
#define LOG_GETMOD      log_get_mod(s_logctx)
#define LOG_GETLEVEL    log_get_loglvl(s_logctx)
#define LOG_GETTNAME    log_get_thrname(pthread_self())

/*explanation:
macro naming convention is V?[EWND][CE]? (regex)
the V* class of macros is meant to be called with a va_list argument.
E(rror), W(arning), N(otice) and D(ebug) denote the log level
the *C-flavour of these macros is supplied with an integer representing an errno-value whose string rep. is printed out
the *E-flavour will instead use the errno-value of the actual errno
the plain group with neither E nor C disregards errno
the E*-functions are special in that they eventually call exit()
*/

#define E(FMT,ARGS...)     xerrx (s_logctx, __FILE__, __LINE__, __func__, EXIT_FAILURE,       (FMT), ##ARGS)
#define EC(EC,FMT,ARGS...) xerrc (s_logctx, __FILE__, __LINE__, __func__, EXIT_FAILURE, (EC), (FMT), ##ARGS)
#define EE(FMT,ARGS...)    xerr  (s_logctx, __FILE__, __LINE__, __func__, EXIT_FAILURE,       (FMT), ##ARGS)
#define W(FMT,ARGS...)     xwarnx(s_logctx, __FILE__, __LINE__, __func__,                     (FMT), ##ARGS)
#define WC(EC,FMT,ARGS...) xwarnc(s_logctx, __FILE__, __LINE__, __func__,               (EC), (FMT), ##ARGS)
#define WE(FMT,ARGS...)    xwarn (s_logctx, __FILE__, __LINE__, __func__,                     (FMT), ##ARGS)
#define N(FMT,ARGS...)     xnotex(s_logctx, __FILE__, __LINE__, __func__,                     (FMT), ##ARGS)
#define NC(EC,FMT,ARGS...) xnotec(s_logctx, __FILE__, __LINE__, __func__,               (EC), (FMT), ##ARGS)
#define NE(FMT,ARGS...)    xnote (s_logctx, __FILE__, __LINE__, __func__,                     (FMT), ##ARGS)
#define D(FMT,ARGS...)     xdbgx (s_logctx, __FILE__, __LINE__, __func__,                     (FMT), ##ARGS)
#define DC(EC,FMT,ARGS...) xdbgc (s_logctx, __FILE__, __LINE__, __func__,               (EC), (FMT), ##ARGS)
#define DE(FMT,ARGS...)    xdbg  (s_logctx, __FILE__, __LINE__, __func__,                     (FMT), ##ARGS)
#define VE(FMT,ARGS)      xverrx (s_logctx, __FILE__, __LINE__, __func__,                     (FMT), (ARGS))
#define VEC(EC,FMT,ARGS)  xverrc (s_logctx, __FILE__, __LINE__, __func__,               (EC), (FMT), (ARGS))
#define VEE(FMT,ARGS)     xverr  (s_logctx, __FILE__, __LINE__, __func__,                     (FMT), (ARGS))
#define VW(FMT,ARGS)      xvwarnx(s_logctx, __FILE__, __LINE__, __func__,                     (FMT), (ARGS))
#define VWC(EC,FMT,ARGS)  xvwarnc(s_logctx, __FILE__, __LINE__, __func__,               (EC), (FMT), (ARGS))
#define VWE(FMT,ARGS)     xvwarn (s_logctx, __FILE__, __LINE__, __func__,                     (FMT), (ARGS))
#define VN(FMT,ARGS)      xvnotex(s_logctx, __FILE__, __LINE__, __func__,                     (FMT), (ARGS))
#define VNC(EC,FMT,ARGS)  xvnotec(s_logctx, __FILE__, __LINE__, __func__,               (EC), (FMT), (ARGS))
#define VNE(FMT,ARGS)     xvnote (s_logctx, __FILE__, __LINE__, __func__,                     (FMT), (ARGS))
#define VD(FMT,ARGS)      xvdbgx (s_logctx, __FILE__, __LINE__, __func__,                     (FMT), (ARGS))
#define VDC(EC,FMT,ARGS)  xvdbgc (s_logctx, __FILE__, __LINE__, __func__,               (EC), (FMT), (ARGS))
#define VDE(FMT,ARGS)     xvdbg  (s_logctx, __FILE__, __LINE__, __func__,                     (FMT), (ARGS))

/* ------ end of interface ------ */

typedef struct logctx *logctx_t;

logctx_t log_register(const char *mod, int loglevel, FILE *str, bool colors);
void log_set_color(logctx_t ctx, bool colors);
void log_set_mod(logctx_t ctx, const char *mod);
void log_set_loglvl(logctx_t ctx, int lvl);
void log_set_str(logctx_t ctx, FILE *str);
void log_set_thrname(pthread_t thr, const char *name);
bool log_get_color(logctx_t ctx);
const char* log_get_mod(logctx_t ctx);
int log_get_loglvl(logctx_t ctx);
FILE* log_get_str(logctx_t ctx);
const char* log_get_thrname(pthread_t thr);

void xerr(logctx_t ctx, const char *file, int line, const char *func, int eval, const char *fmt, ...);
void xerrc(logctx_t ctx, const char *file, int line, const char *func, int eval, int code, const char *fmt, ...);
void xerrx(logctx_t ctx, const char *file, int line, const char *func, int eval, const char *fmt, ...);
void xverr (logctx_t ctx, const char *file, int line, const char *func, int eval, const char *fmt, va_list args);
void xverrc(logctx_t ctx, const char *file, int line, const char *func, int eval, int code, const char *fmt, va_list args);
void xverrx(logctx_t ctx, const char *file, int line, const char *func, int eval, const char *fmt, va_list args);

void xwarn(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...);
void xwarnc(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, ...);
void xwarnx(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...);
void xvwarn (logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args);
void xvwarnc(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, va_list args);
void xvwarnx(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args);

void xnote(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...);
void xnotec(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, ...);
void xnotex(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...);
void xvnote (logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args);
void xvnotec(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, va_list args);
void xvnotex(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args);

void xdbg(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...);
void xdbgc(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, ...);
void xdbgx(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, ...);
void xvdbg (logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args);
void xvdbgc(logctx_t ctx, const char *file, int line, const char *func, int code, const char *fmt, va_list args);
void xvdbgx(logctx_t ctx, const char *file, int line, const char *func, const char *fmt, va_list args);

#endif /* LOG_H */

static logctx_t s_logctx; //boilerplate
