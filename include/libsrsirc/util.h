/* util.h - Interface to misc. useful functions related to IRC
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_UTIL_H
#define LIBSRSIRC_IRC_UTIL_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>

struct hostspec {
	char addrstr[48];
	uint16_t port;
};

struct pxspec {
	int type;
	char addrstr[48];
	uint16_t port;
};


/* resolve and connect */
bool ut_pfx2nick(char *dest, size_t dest_sz, const char *pfx);
bool ut_pfx2uname(char *dest, size_t dest_sz, const char *pfx);
bool ut_pfx2host(char *dest, size_t dest_sz, const char *pfx);
int ut_istrcmp(const char *n1, const char *n2, int casemapping);
int ut_istrncmp(const char *n1, const char *n2, size_t len, int casemap);
char ut_tolower(char c, int casemap);
void ut_strtolower(char *dest, size_t destsz, const char *str, int casemap);
bool ut_parse_pxspec(int *ptype, char *hoststr, size_t hoststr_sz,
    uint16_t *port, const char *pxspec);

void ut_parse_hostspec(char *hoststr, size_t hoststr_sz, uint16_t *port,
    bool *ssl, const char *hostspec);

char* ut_sndumpmsg(char *dest, size_t dest_sz, void *tag, tokarr *msg);

void ut_dumpmsg(void *tag, tokarr *msg);

bool ut_conread(tokarr *msg, void *tag);
char** ut_parse_005_cmodes(const char *const *arr, size_t argcnt, size_t *num,
    const char *modepfx005chr, const char *const *chmodes);

void ut_mut_nick(char *nick, size_t nick_sz);

tokarr *ut_clonearr(tokarr *arr);
void ut_freearr(tokarr *arr);

#endif /* LIBSRSIRC_IRC_UTIL_H */
