/* common.h - Interface to routines of common use
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#ifndef SRS_COMMON_H
#define SRS_COMMON_H 1

#define _GNU_SOURCE 1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <sys/time.h>

#define XCALLOC(num) xcalloc(1, num)
#define XMALLOC(num) xmalloc(num)
#define XREALLOC(p, num) xrealloc((p),(num))
#define XFREE(p) do{  if(p) free(p); p=0;  }while(0)
#define XSTRDUP(s) xstrdup(s)

void strNcat(char *dest, const char *src, size_t destsz);

void *xcalloc(size_t num, size_t size);
void *xmalloc(size_t num);
void *xrealloc(void *p, size_t num);
char *xstrdup(const char *string);

int64_t timestamp_us();
void tconv(struct timeval *tv, int64_t *ts, bool tv_to_ts);
char* strNcpy(char *dst, const char *src, size_t len);

int mksocket(const char *host, unsigned short port,
		struct sockaddr *sockaddr, size_t *addrlen);
#endif /* SRS_COMMON_H */
