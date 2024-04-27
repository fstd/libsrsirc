/* base_log.h -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-2024, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_BASE_LOG_H
#define LIBSRSIRC_BASE_LOG_H 1


#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>


#if HAVE_SYSLOG_H
# include <syslog.h>
#else
# define LOG_CRIT 2
# define LOG_ERR  3
# define LOG_WARNING 4
# define LOG_NOTICE 5
# define LOG_INFO 6
# define LOG_DEBUG 7
#endif


int lsi_b_strerror(int errnum, char *buf, size_t sz);
char *lsi_b_ctime(const time_t *clock, char *buf);

bool lsi_b_openlog(const char *ident);
void lsi_b_closelog(void);
void lsi_b_syslog(int prio, const char *fmt, ...);


#endif /* LIBSRSIRC_BASE_LOG_H */
