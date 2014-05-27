/* irc_util.h - Interface to misc. useful functions related to IRC
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_UTIL_H
#define LIBSRSIRC_IRC_UTIL_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/irc_defs.h>

struct sockaddr;
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
bool pfx_extract_nick(char *dest, size_t dest_sz, const char *pfx);
bool pfx_extract_uname(char *dest, size_t dest_sz, const char *pfx);
bool pfx_extract_host(char *dest, size_t dest_sz, const char *pfx);
int istrcasecmp(const char *n1, const char *n2, int casemapping);
int istrncasecmp(const char *n1, const char *n2, size_t len, int casemap);
char ichartolower(char c, int casemap);
void itolower(char *dest, size_t destsz, const char *str, int casemap);
bool parse_pxspec(int *ptype, char *hoststr, size_t hoststr_sz, uint16_t *port,
    const char *line);

void parse_hostspec(char *hoststr, size_t hoststr_sz, uint16_t *port,
    bool *ssl, const char *line);

bool parse_identity(char *nick, size_t nicksz, char *uname, size_t unamesz,
    char *fname, size_t fnamesz, const char *identity);

char* sndumpmsg(char *dest, size_t dest_sz, void *tag, tokarr *msg);

void dumpmsg(void *tag, tokarr *msg);

bool cr(tokarr *msg, void *tag);
char** parse_chanmodes(const char *const *arr, size_t argcnt, size_t *num,
    const char *modepfx005chr, const char *const *chmodes);

void mutilate_nick(char *nick, size_t nick_sz);

/* will never return less than 2 */
size_t countargs(tokarr *tok);

tokarr *ut_clonearr(tokarr *arr);
void ut_freearr(tokarr *arr);

#endif /* LIBSRSIRC_IRC_UTIL_H */
