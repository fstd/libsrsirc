/* intlog.h - library debugging interface
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
* See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_INTLOG_H
#define LIBSRSIRC_INTLOG_H 1

#include <stdbool.h>
#include <errno.h>

#include <syslog.h>

#define MOD_IRC 0
#define MOD_COMMON 1
#define MOD_IRC_UTIL 2
#define MOD_ICONN 3
#define MOD_IIO 4
#define MOD_PROXY 5
#define MOD_IMSG 6
#define MOD_SMAP 7
#define MOD_PLST 8
#define MOD_UCBASE 9
#define MOD_UNKNOWN 10
#define NUM_MODS 11 /* when adding modules, don't forget intlog.c's `modnames' */

/* our two higher-than-debug custom loglevels */
#define LOG_TRACE (LOG_VIVI+1)
#define LOG_VIVI (LOG_DEBUG+1)

#ifndef LOG_MODULE
# define LOG_MODULE MOD_UNKNOWN
#endif

//[TVDINWE](): log with Trace, Vivi, Debug, Info, Notice, Warn, Error severity.
//[TVDINWE]E(): similar, but also append ``errno'' message
//C(), CE(): as above, but also call exit(EXIT_FAILURE)

// ----- logging interface -----

#define T(F,A...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_TRACE,-1,__FILE__,__LINE__,__func__,F,##A)

#define TE(F,A...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_TRACE,errno,__FILE__,__LINE__,__func__,F,##A)

#define V(F,A...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_VIVI,-1,__FILE__,__LINE__,__func__,F,##A)

#define VE(F,A...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_VIVI,errno,__FILE__,__LINE__,__func__,F,##A)

#define D(F,A...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_DEBUG,-1,__FILE__,__LINE__,__func__,F,##A)

#define DE(F,A...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_DEBUG,errno,__FILE__,__LINE__,__func__,F,##A)

#define I(F,A...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_INFO,-1,__FILE__,__LINE__,__func__,F,##A)

#define IE(F,A...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_INFO,errno,__FILE__,__LINE__,__func__,F,##A)

#define N(F,A...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_NOTICE,-1,__FILE__,__LINE__,__func__,F,##A)

#define NE(F,A...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_NOTICE,errno,__FILE__,__LINE__,__func__,F,##A)

#define W(F,A...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_WARNING,-1,__FILE__,__LINE__,__func__,F,##A)

#define WE(F,A...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_WARNING,errno,__FILE__,__LINE__,__func__,F,##A)

#define E(F,A...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_ERR,-1,__FILE__,__LINE__,__func__,F,##A)

#define EE(F,A...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_ERR,errno,__FILE__,__LINE__,__func__,F,##A)

#define C(F,A...) do {                                                       \
    ircdbg_log(LOG_MODULE,LOG_CRIT,-1,__FILE__,__LINE__,__func__,F,##A);     \
    exit(EXIT_FAILURE); } while (0)

#define CE(F,A...) do {                                                      \
    ircdbg_log(LOG_MODULE,LOG_CRIT,errno,__FILE__,__LINE__,__func__,F,##A);  \
    exit(EXIT_FAILURE); } while (0)

// ----- logger control interface -----

void ircdbg_stderr(void);
void ircdbg_syslog(const char *ident, int facility);

void ircdbg_setlvl(int mod, int lvl);
int ircdbg_getlvl(int mod);

void ircdbg_setfancy(bool fancy);
bool ircdbg_getfancy(void);


// ----- backend -----
void ircdbg_log(int mod, int lvl, int errn, const char *file, int line,
    const char *func, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 7, 8)))
#endif
    ;

#endif /* LIBSRSIRC_INTLOG_H */
