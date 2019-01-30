/* intlog.h - library debugging interface
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-19, Timo Buhrmester
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
#define MOD_V3 11
#define MOD_BASEIO 12
#define MOD_BASENET 13
#define MOD_BASETIME 14
#define MOD_BASESTR 15
#define MOD_BASEMISC 16
#define MOD_ICATINIT 17
#define MOD_ICATCORE 18
#define MOD_ICATSERV 19
#define MOD_ICATUSER 20
#define MOD_ICATMISC 21
#define MOD_IWAT 22
#define MOD_UNKNOWN 23
#define NUM_MODS 24 /* when adding modules, don't forget intlog.c's `modnames' */

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

#define V(...)                                                                 \
 lsi_log_log(LOG_MODULE,LOG_VIVI,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define VE(...)                                                                \
 lsi_log_log(LOG_MODULE,LOG_VIVI,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define D( ...)                                                                \
 lsi_log_log(LOG_MODULE,LOG_DEBUG,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define DE(...)                                                                \
 lsi_log_log(LOG_MODULE,LOG_DEBUG,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define I(...)                                                                 \
 lsi_log_log(LOG_MODULE,LOG_INFO,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define IE(...)                                                                \
 lsi_log_log(LOG_MODULE,LOG_INFO,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define N(...)                                                                 \
 lsi_log_log(LOG_MODULE,LOG_NOTICE,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define NE(...)                                                                \
 lsi_log_log(LOG_MODULE,LOG_NOTICE,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define W(...)                                                                 \
 lsi_log_log(LOG_MODULE,LOG_WARNING,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define WE(...)                                                                \
 lsi_log_log(LOG_MODULE,LOG_WARNING,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define E(...)                                                                 \
 lsi_log_log(LOG_MODULE,LOG_ERR,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define EE(...)                                                                \
 lsi_log_log(LOG_MODULE,LOG_ERR,errno,__FILE__,__LINE__,__func__,__VA_ARGS__)

#define C(...) do {                                                            \
 lsi_log_log(LOG_MODULE,LOG_CRIT,-1,__FILE__,__LINE__,__func__,__VA_ARGS__);   \
 exit(EXIT_FAILURE); } while (0)

#define CE(...) do {                                                           \
 lsi_log_log(LOG_MODULE,LOG_CRIT,errno,__FILE__,__LINE__,__func__,__VA_ARGS__);\
 exit(EXIT_FAILURE); } while (0)

/* special: always printed, never decorated */
#define A(...)                                                                 \
    lsi_log_log(-1,INT_MIN,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

/* tracing */
#if NOTRACE
# define T(...) do{}while(0)
# define TC(...) do{}while(0)
# define TR(...) do{}while(0)
#else
# define T(...)                                                                \
 lsi_log_log(LOG_MODULE,LOG_TRACE,-1,__FILE__,__LINE__,__func__,__VA_ARGS__)

# define TC(...)                                                               \
 do{                                                                           \
 lsi_log_log(LOG_MODULE,LOG_TRACE,-1,__FILE__,__LINE__,__func__,__VA_ARGS__);  \
 lsi_log_tcall();                                                              \
 } while (0)

# define TR(...)                                                               \
 do{                                                                           \
 lsi_log_tret();                                                               \
 lsi_log_log(LOG_MODULE,LOG_TRACE,-1,__FILE__,__LINE__,__func__,__VA_ARGS__);  \
 } while (0)
#endif

// ----- logger control interface -----

void lsi_log_stderr(void);
void lsi_log_syslog(const char *ident);

void lsi_log_setlvl(int mod, int lvl);
int  lsi_log_getlvl(int mod);

void lsi_log_setfancy(bool fancy);
bool lsi_log_getfancy(void);

void lsi_log_tret(void);
void lsi_log_tcall(void);

// ----- backend -----
void lsi_log_log(int mod, int lvl, int errn, const char *file, int line,
    const char *func, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 7, 8)))
#endif
    ;

void lsi_log_init(void);


#endif /* LIBSRSIRC_INTLOG_H */
