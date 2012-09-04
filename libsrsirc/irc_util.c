/* irc_util.c - Implementation of misc. functions related to IRC
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE 1

#include <libsrsirc/irc_util.h>

#include <common.h>


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <limits.h>

int pxtypeno(const char *typestr)
{
	return (strcasecmp(typestr, "socks4") == 0) ? IRCPX_SOCKS4 :
	       (strcasecmp(typestr, "socks5") == 0) ? IRCPX_SOCKS5 :
	       (strcasecmp(typestr, "http") == 0) ? IRCPX_HTTP : -1;
}

const char *pxtypestr(int type)
{
	return (type == IRCPX_HTTP) ? "HTTP" :
	       (type == IRCPX_SOCKS4) ? "SOCKS4" :
	       (type == IRCPX_SOCKS5) ? "SOCKS5" : "unknown";
}

bool pfx_extract_nick(char *dest, size_t dest_sz, const char *pfx)
{
	if (!dest || !dest_sz || !pfx)
		return false;
	strncpy(dest, pfx, dest_sz);
	dest[dest_sz-1] = '\0';

	char *ptr = strchr(dest, '@');
	if (ptr)
		*ptr = '\0';

	ptr = strchr(dest, '!');
	if (ptr)
		*ptr = '\0';

	return true;
}

bool pfx_extract_uname(char *dest, size_t dest_sz, const char *pfx)
{
	if (!dest || !dest_sz || !pfx)
		return false;
	strncpy(dest, pfx, dest_sz);
	dest[dest_sz-1] = '\0';

	char *ptr = strchr(dest, '@');
	if (ptr)
		*ptr = '\0';

	ptr = strchr(dest, '!');
	if (ptr)
		memmove(dest, ptr+1, strlen(ptr+1)+1);
	else
		*dest = '\0';

	return true;
}

bool pfx_extract_host(char *dest, size_t dest_sz, const char *pfx)
{
	if (!dest || !dest_sz || !pfx)
		return false;
	strncpy(dest, pfx, dest_sz);
	dest[dest_sz-1] = '\0';

	char *ptr = strchr(dest, '@');
	if (ptr)
		memmove(dest, ptr+1, strlen(ptr+1)+1);
	else
		*dest = '\0';

	return true;
}

int
istrcasecmp(const char *n1, const char *n2, int casemap)
{
	size_t l1 = strlen(n1);
	size_t l2 = strlen(n2);

	return istrncasecmp(n1, n2,
			(l1 < l2) ? (l1 + 1) : (l2 + 1), casemap);
}

int
istrncasecmp(const char *n1, const char *n2, size_t len, int casemap)
{
	if (len == 0)
		return 0;

	char c1, c2;
	int rangeinc, offset = 'a' - 'A';

	switch (casemap)
	{
	case CASEMAPPING_RFC1459:
		rangeinc = 4;
		break;
	case CASEMAPPING_STRICT_RFC1459:
		rangeinc = 3;
		break;
	default:
		rangeinc = 0;
	}
	size_t pos = 0;
	while((pos < len) && ((c1 = (*n1)) & (c2 = (*n2))))
	{
		if (c1 != c2) {
			if ((c1 >= 'A') && (c1 <= ('Z' + rangeinc)))
			{
				if ((c1 + offset) != c2)
					return c1 < c2 ? -1 : 1;
			}
			else if ((c1 >= 'a') && (c1 <= ('z' + rangeinc)))
			{
				if ((c1 - offset) != c2)
					return c1 < c2 ? -1 : 1;
			}
			else
				return c1 < c2 ? -1 : 1;
		}
		n1++;
		n2++;
		pos++;
	}
	if (c1 == c2)
		return 0;

	return c1 < c2 ? -1 : 1;
}


bool
parse_pxspec(char *pxtypestr, size_t pxtypestr_sz, char *hoststr,
		size_t hoststr_sz, unsigned short *port, const char *line)
{
	char linebuf[128];
	strncpy(linebuf, line, sizeof linebuf);
	linebuf[sizeof linebuf - 1] = '\0';

	char *ptr = strchr(linebuf, ':');
	if (!ptr)
		return false;
	
	size_t num = (size_t)(ptr - linebuf) < pxtypestr_sz
			? (size_t)(ptr - linebuf)
			: pxtypestr_sz - 1;

	strncpy(pxtypestr, linebuf, num);
	pxtypestr[num] = '\0';

	parse_hostspec(hoststr, hoststr_sz, port, ptr + 1);
	return true;

}

void parse_hostspec(char *hoststr, size_t hoststr_sz, unsigned short *port,
		const char *line)
{
	strncpy(hoststr, line, hoststr_sz);
	char *ptr = strchr(hoststr, ']');
	if (!ptr)
		ptr = hoststr;
	ptr = strchr(ptr, ':');
	if (ptr) {
		*port = (unsigned short)strtol(ptr+1, NULL, 10);
		*ptr = '\0';
	} else
		*port = 0;
}

bool parse_identity(char *nick, size_t nicksz, char *uname, size_t unamesz,
		char *fname, size_t fnamesz, const char *identity)
{
	char ident[256];
	strNcpy(ident, identity, sizeof ident);

	char *p = strchr(ident, ' ');
	if (p)
		strNcpy(fname, (*p = '\0', p + 1), fnamesz);
	else
		return false;
	p = strchr(ident, '!');
	if (p)
		strNcpy(uname, (*p = '\0', p + 1), unamesz);
	else
		return false;

	strNcpy(nick, ident, nicksz);
	return true;
}



void
sndumpmsg(char *dest, size_t dest_sz, void *tag, char **msg, size_t msg_len)
{
	snprintf(dest, dest_sz, "(%p) '%s' '%s'", tag, msg[0], msg[1]);
	for(size_t i = 2; i < msg_len; i++) {
		if (!msg[i])
			break;
		strNcat(dest, " '", dest_sz);
		strNcat(dest, msg[i], dest_sz);
		strNcat(dest, "'", dest_sz);
	}
}

void
dumpmsg(void *tag, char **msg, size_t msg_len)
{
	char buf[1024];
	sndumpmsg(buf, sizeof buf, tag, msg, msg_len);
	fprintf(stderr, "%s\n", buf);
}


bool
cr(char **msg, size_t msg_len, void *tag)
{
	dumpmsg(tag, msg, msg_len);
	return true;
}
