/* intlog.h - library debugging interface
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
* See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_INTLOG_H
#define LIBSRSIRC_INTLOG_H 1


#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

#include <platform/base_log.h>


#define MOD_IRC 0
#define MOD_COMMON 1
#define MOD_IRC_UTIL 2
#define MOD_ICONN 3
#define MOD_IIO 4
#define MOD_PROXY 5
#define MOD_IMSG 6
#define MOD_SKMAP 7
#define MOD_PLST 8
#define MOD_TRACK 9
#define MOD_UCBASE 10
#define MOD_BASEIO 11
#define MOD_BASENET 12
#define MOD_BASETIME 13
#define MOD_BASESTR 14
#define MOD_BASEMISC 15
#define MOD_ICATINIT 16
#define MOD_ICATCORE 17
#define MOD_ICATSERV 18
#define MOD_ICATUSER 19
#define MOD_ICATMISC 20
#define MOD_UNKNOWN 21
#define NUM_MODS 22 /* when adding modules, don't forget intlog.c's `modnames' */

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

#define T(...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_TRACE,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define TE(...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_TRACE,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define V(...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_VIVI,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define VE(...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_VIVI,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define D( ...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_DEBUG,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define DE(...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_DEBUG,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define I(...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_INFO,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define IE(...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_INFO,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define N(...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_NOTICE,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define NE(...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_NOTICE,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define W(...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_WARNING,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define WE(...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_WARNING,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define E(...)                                                            \
    ircdbg_log(LOG_MODULE,LOG_ERR,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define EE(...)                                                           \
    ircdbg_log(LOG_MODULE,LOG_ERR,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define C(...) do {                                                       \
    ircdbg_log(LOG_MODULE,LOG_CRIT,-1,__FILE__,__LINE__,__func__,__VA_ARGS__);     \
    exit(EXIT_FAILURE); } while (0)

#define CE(...) do {                                                      \
    ircdbg_log(LOG_MODULE,LOG_CRIT,errno,__FILE__,__LINE__,__func__,__VA_ARGS__);  \
    exit(EXIT_FAILURE); } while (0)

/* special: always printed, never decorated */
#define A(...)                                                            \
    ircdbg_log(-1,INT_MIN,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

// ----- logger control interface -----

void ircdbg_stderr(void);
void ircdbg_syslog(const char *ident);

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

void ircdbg_init(void);


#endif /* LIBSRSIRC_INTLOG_H */
