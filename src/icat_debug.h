/* icat_debug.h - Logging interface from libsrslog
 * icat - IRC netcat on top of libsrsirc - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_ICAT_DEBUG_H
#define LIBSRSIRC_ICAT_DEBUG_H 1


#include <stdbool.h>
#include <limits.h>
#include <errno.h>

#include <sys/types.h>

#include <syslog.h>


#ifndef LOG_MODULE
# define LOG_MODULE -1
#endif

/* our two higher-than-debug custom loglevels */
#define LOG_TRACE (LOG_VIVI+1)
#define LOG_VIVI (LOG_DEBUG+1)

//[TVDINWE](): log with Trace, Vivi, Debug, Info, Notice, Warn, Error severity.
//[TVDINWE]E(): similar, but also append ``errno'' message
//C(), CE(): as above but critical severity; calls exit(EXIT_FAILURE)
//A(): always print (and do not decorate w/ time or colors or anything

/* logging interface */
#define A(F,A...)                                                            \
    log_log(-1,INT_MIN,-1,__FILE__,__LINE__,__func__,F,##A)

#define T(F,A...)                                                            \
    log_log(LOG_MODULE,LOG_TRACE,-1,__FILE__,__LINE__,__func__,F,##A)

#define TE(F,A...)                                                           \
    log_log(LOG_MODULE,LOG_TRACE,errno,__FILE__,__LINE__,__func__,F,##A)

#define V(F,A...)                                                            \
    log_log(LOG_MODULE,LOG_VIVI,-1,__FILE__,__LINE__,__func__,F,##A)

#define VE(F,A...)                                                           \
    log_log(LOG_MODULE,LOG_VIVI,errno,__FILE__,__LINE__,__func__,F,##A)

#define D(F,A...)                                                            \
    log_log(LOG_MODULE,LOG_DEBUG,-1,__FILE__,__LINE__,__func__,F,##A)

#define DE(F,A...)                                                           \
    log_log(LOG_MODULE,LOG_DEBUG,errno,__FILE__,__LINE__,__func__,F,##A)

#define I(F,A...)                                                            \
    log_log(LOG_MODULE,LOG_INFO,-1,__FILE__,__LINE__,__func__,F,##A)

#define IE(F,A...)                                                           \
    log_log(LOG_MODULE,LOG_INFO,errno,__FILE__,__LINE__,__func__,F,##A)

#define N(F,A...)                                                            \
    log_log(LOG_MODULE,LOG_NOTICE,-1,__FILE__,__LINE__,__func__,F,##A)

#define NE(F,A...)                                                           \
    log_log(LOG_MODULE,LOG_NOTICE,errno,__FILE__,__LINE__,__func__,F,##A)

#define W(F,A...)                                                            \
    log_log(LOG_MODULE,LOG_WARNING,-1,__FILE__,__LINE__,__func__,F,##A)

#define WE(F,A...)                                                           \
    log_log(LOG_MODULE,LOG_WARNING,errno,__FILE__,__LINE__,__func__,F,##A)

#define E(F,A...)                                                            \
    log_log(LOG_MODULE,LOG_ERR,-1,__FILE__,__LINE__,__func__,F,##A)

#define EE(F,A...)                                                           \
    log_log(LOG_MODULE,LOG_ERR,errno,__FILE__,__LINE__,__func__,F,##A)

#define C(F,A...) do {                                                       \
    log_log(LOG_MODULE,LOG_CRIT,-1,__FILE__,__LINE__,__func__,F,##A);        \
    exit(EXIT_FAILURE); } while (0)

#define CE(F,A...) do {                                                      \
    log_log(LOG_MODULE,LOG_CRIT,errno,__FILE__,__LINE__,__func__,F,##A);     \
    exit(EXIT_FAILURE); } while (0)


/* logger control interface */
void log_stderr(void);
void log_syslog(const char *ident, int facility);

void log_setlvl(int mod, int lvl);
int log_getlvl(int mod);

void log_setfancy(bool fancy);
bool log_getfancy(void);

void log_regmod(size_t modnumber, const char *modname);
ssize_t log_modnum(const char *modname);
ssize_t log_maxmodnum(void);
const char* log_modname(size_t modnum);

void log_setprgnam(const char *prgnam);

/* backend */
void log_log(int mod, int lvl, int errn, const char *file, int line,
    const char *func, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 7, 8)))
#endif
    ;


#endif /* LIBSRSIRC_ICAT_DEBUG_H */
