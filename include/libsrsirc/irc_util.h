/* irc_util.h - Interface to misc. useful functions related to IRC
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#ifndef SRS_IRC_UTIL_H
#define SRS_IRC_UTIL_H 1

#include <stdbool.h>
#include <sys/types.h>

#define CASEMAPPING_RFC1459 0
#define CASEMAPPING_STRICT_RFC1459 1
#define CASEMAPPING_ASCII 2

#define IRCPX_HTTP 0
#define IRCPX_SOCKS4 1 // NOT socks4a
#define IRCPX_SOCKS5 2

struct sockaddr;
struct hostspec
{
	char addrstr[48];
	unsigned short port;
};

struct pxspec
{
	int type;
	char addrstr[48];
	unsigned short port;
};


int pxtypeno(const char *typestr);
const char *pxtypestr(int type);
/* resolve and connect */
bool pfx_extract_nick(char *dest, size_t dest_sz, const char *pfx);
bool pfx_extract_uname(char *dest, size_t dest_sz, const char *pfx);
bool pfx_extract_host(char *dest, size_t dest_sz, const char *pfx);
int istrcasecmp(const char *n1, const char *n2, int casemapping);
int istrncasecmp(const char *n1, const char *n2, size_t len, int casemap);
bool parse_pxspec(char *pxtypestr, size_t pxtypestr_sz, char *hoststr,
		size_t hoststr_sz, unsigned short *port, const char *line);

void parse_hostspec(char *hoststr, size_t hoststr_sz, unsigned short *port,
		const char *line);

bool parse_identity(char *nick, size_t nicksz, char *uname, size_t unamesz,
		char *fname, size_t fnamesz, const char *identity);

void sndumpmsg(char *dest, size_t dest_sz, void *tag, char **msg, size_t msg_len);

void dumpmsg(void *tag, char **msg, size_t msg_len);

bool cr(char **msg, size_t msg_len, void *tag);

#endif /* SRS_IRC_UTIL_H */
