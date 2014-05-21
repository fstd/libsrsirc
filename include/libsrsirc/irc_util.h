/* irc_util.h - Interface to misc. useful functions related to IRC
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef SRS_IRC_UTIL_H
#define SRS_IRC_UTIL_H 1

#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/irc_defs.h>

#define CMAP_RFC1459 0
#define CMAP_STRICT_RFC1459 1
#define CMAP_ASCII 2

#define IRCPX_HTTP 0
#define IRCPX_SOCKS4 1 // NOT socks4a
#define IRCPX_SOCKS5 2

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


int pxtypeno(const char *typestr);
const char *pxtypestr(int type);
/* resolve and connect */
bool pfx_extract_nick(char *dest, size_t dest_sz, const char *pfx);
bool pfx_extract_uname(char *dest, size_t dest_sz, const char *pfx);
bool pfx_extract_host(char *dest, size_t dest_sz, const char *pfx);
int istrcasecmp(const char *n1, const char *n2, int casemapping);
int istrncasecmp(const char *n1, const char *n2, size_t len, int casemap);
int ichartolower(int c, int casemap);
void itolower(char *dest, size_t destsz, const char *str, int casemap);
bool parse_pxspec(char *pxtypestr, size_t pxtypestr_sz, char *hoststr,
    size_t hoststr_sz, uint16_t *port, const char *line);

void parse_hostspec(char *hoststr, size_t hoststr_sz, uint16_t *port,
    bool *ssl, const char *line);

bool parse_identity(char *nick, size_t nicksz, char *uname, size_t unamesz,
    char *fname, size_t fnamesz, const char *identity);

void sndumpmsg(char *dest, size_t dest_sz, void *tag,
    tokarr *msg);

void dumpmsg(void *tag, tokarr *msg);

bool cr(tokarr *msg, void *tag);
char** parse_chanmodes(const char *const *arr, size_t argcnt, size_t *num,
    const char *modepfx005chr, const char *const *chmodes);

void mutilate_nick(char *nick, size_t nick_sz);

/* will never return less than 2 */
size_t countargs(tokarr *tok);

tokarr *ic_clonearr(tokarr *arr);
void ic_freearr(tokarr *arr);

#endif /* SRS_IRC_UTIL_H */
