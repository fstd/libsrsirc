/* intlog.h - library debugging interface
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
* See README for contact-, COPYING for license information. */
/* former interface
#define W(FMT, A...) W_(warn, 1, FMT, ##A)
#define DE(FMT, A...) W_(warn, 2, FMT, ##A)

#define W(FMT, A...) W_(warnx, 1, FMT, ##A)
#define D(FMT, A...) W_(warnx, 2, FMT, ##A)
*/

#ifndef INTLOG_H
#define INTLOG_H 1

#include <stdbool.h>
#include <errno.h>

#include <syslog.h>

//[DINWE](): log with Debug, Info, Notice, Warn, Error severity.
//[DINWE]E(): similar, but also append ``errno'' message

// ----- logging interface -----

#define LOG_VIVI (LOG_DEBUG+1)

#define V(F,A...)                                               \
    ircdbg_log(LOG_VIVI,-1,__FILE__,__LINE__,__func__,F,##A)

#define VE(F,A...)                                              \
    ircdbg_log(LOG_VIVI,errno,__FILE__,__LINE__,__func__,F,##A)

#define D(F,A...)                                               \
    ircdbg_log(LOG_DEBUG,-1,__FILE__,__LINE__,__func__,F,##A)

#define DE(F,A...)                                              \
    ircdbg_log(LOG_DEBUG,errno,__FILE__,__LINE__,__func__,F,##A)

#define I(F,A...)                                               \
    ircdbg_log(LOG_INFO,-1,__FILE__,__LINE__,__func__,F,##A)

#define IE(F,A...)                                              \
    ircdbg_log(LOG_INFO,errno,__FILE__,__LINE__,__func__,F,##A)

#define N(F,A...)                                               \
    ircdbg_log(LOG_NOTICE,-1,__FILE__,__LINE__,__func__,F,##A)

#define NE(F,A...)                                              \
    ircdbg_log(LOG_NOTICE,errno,__FILE__,__LINE__,__func__,F,##A)

#define W(F,A...)                                               \
    ircdbg_log(LOG_WARNING,-1,__FILE__,__LINE__,__func__,F,##A)

#define WE(F,A...)                                              \
    ircdbg_log(LOG_WARNING,errno,__FILE__,__LINE__,__func__,F,##A)

#define E(F,A...)                                               \
    ircdbg_log(LOG_ERR,-1,__FILE__,__LINE__,__func__,F,##A)

#define EE(F,A...)                                              \
    ircdbg_log(LOG_ERR,errno,__FILE__,__LINE__,__func__,F,##A)

#define C(F,A...) do {                                          \
    ircdbg_log(LOG_CRIT,-1,__FILE__,__LINE__,__func__,F,##A);   \
    exit(EXIT_FAILURE); } while(0)

#define CE(F,A...) do {                                         \
    ircdbg_log(LOG_CRIT,errno,__FILE__,__LINE__,__func__,F,##A);\
    exit(EXIT_FAILURE); } while(0)

// ----- logger control interface -----

void ircdbg_stderr(void);
void ircdbg_syslog(const char *ident, int facility);

void ircdbg_setlvl(int lvl);
int ircdbg_getlvl(void);

void ircdbg_setfancy(bool fancy);
bool ircdbg_getfancy(void);


// ----- backend -----
void ircdbg_log(int lvl, int errn, const char *file, int line,
    const char *func, const char *fmt, ...);

#endif /* INTLOG_H */
