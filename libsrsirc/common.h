/* common.h - Interface to routines of common use
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef SRS_COMMON_H
#define SRS_COMMON_H 1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <sys/time.h>

#define COUNTOF(ARR) (sizeof (ARR) / sizeof (ARR)[0])

/*
#define XCALLOC(num) ic_xcalloc(1, num)
#define XMALLOC(num) ic_xmalloc(num)
#define XREALLOC(p, num) ic_xrealloc((p),(num))
#define XFREE(p) do{if(p) free(p); p=0;}while(0)
#define XSTRDUP(s) ic_xstrdup(s)
*/
void ic_strNcat(char *dest, const char *src, size_t destsz);
char* ic_strNcpy(char *dst, const char *src, size_t len);
size_t ic_strCchr(const char *dst, char c);

/*
void* ic_xcalloc(size_t num, size_t size);
void* ic_xmalloc(size_t num);
void* ic_xrealloc(void *p, size_t num);
char* ic_xstrdup(const char *string);
char* ic_strmdup(const char *str, size_t minlen);
*/

uint64_t ic_timestamp_us(void);
bool ic_check_timeout(uint64_t tsend, uint64_t *trem);
void ic_tconv(struct timeval *tv, uint64_t *ts, bool tv_to_ts);

int ic_consocket(const char *host, uint16_t port,
    struct sockaddr *sockaddr, size_t *addrlen,
    uint64_t softto, uint64_t hardto);
bool update_strprop(char **field, const char *val);
#endif /* SRS_COMMON_H */
