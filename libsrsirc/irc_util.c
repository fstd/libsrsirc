/* irc_util.c - Implementation of misc. functions related to IRC
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <limits.h>

#include "common.h"

#include <intlog.h>

#include <libsrsirc/irc_util.h>

#define CHANMODE_CLASS_A 1 /*don't change;see int classify_chanmode(char)*/
#define CHANMODE_CLASS_B 2
#define CHANMODE_CLASS_C 3
#define CHANMODE_CLASS_D 4

static int classify_chanmode(char c, const char *const *chmodes);

int
pxtypeno(const char *typestr)
{
	return (strcasecmp(typestr, "socks4") == 0) ? IRCPX_SOCKS4 :
	       (strcasecmp(typestr, "socks5") == 0) ? IRCPX_SOCKS5 :
	       (strcasecmp(typestr, "http") == 0) ? IRCPX_HTTP : -1;
}

const char*
pxtypestr(int type)
{
	return (type == IRCPX_HTTP) ? "HTTP" :
	       (type == IRCPX_SOCKS4) ? "SOCKS4" :
	       (type == IRCPX_SOCKS5) ? "SOCKS5" : "unknown";
}

bool
pfx_extract_nick(char *dest, size_t dest_sz, const char *pfx)
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

bool
pfx_extract_uname(char *dest, size_t dest_sz, const char *pfx)
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

bool
pfx_extract_host(char *dest, size_t dest_sz, const char *pfx)
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

	return istrncasecmp(n1, n2, (l1 < l2 ? l1 : l2) + 1, casemap);
}

int
istrncasecmp(const char *n1, const char *n2, size_t len, int casemap)
{
	if (len == 0)
		return 0;

	while (*n1 && *n2) {
		char c1 = ichartolower(*n1, casemap);
		char c2 = ichartolower(*n2, casemap);
		if (c1 != c2)
			return c1 - c2;

		n1++;
		n2++;
	}

	if (*n1)
		return 1;
	if (*n2)
		return -1;

	return 0;
}

char
ichartolower(char c, int casemap)
{
	int rangeinc;
	switch (casemap) {
	case CMAP_RFC1459:
		rangeinc = 4;
		break;
	case CMAP_STRICT_RFC1459:
		rangeinc = 3;
		break;
	default:
		rangeinc = 0;
	}

	if (c >= 'A' && c <= ('Z'+rangeinc))
		return c + ('a'-'A');
	else
		return c;
}

void
itolower(char *dest, size_t destsz, const char *str, int casemap)
{
	size_t c = 0;
	char *ptr = dest;
	while(c < destsz) {
		*ptr++ = ichartolower(*str, casemap);

		if (!*str)
			break;
		str++;
	}

	dest[destsz-1] = '\0';
}

bool
parse_pxspec(char *pxtypestr, size_t pxtypestr_sz, char *hoststr,
    size_t hoststr_sz, uint16_t *port, const char *line)
{
	char linebuf[128];
	strncpy(linebuf, line, sizeof linebuf);
	linebuf[sizeof linebuf - 1] = '\0';

	char *ptr = strchr(linebuf, ':');
	if (!ptr)
		return false;

	size_t num = (size_t)(ptr - linebuf) < pxtypestr_sz ?
	    (size_t)(ptr - linebuf) : pxtypestr_sz - 1;

	strncpy(pxtypestr, linebuf, num);
	pxtypestr[num] = '\0';

	parse_hostspec(hoststr, hoststr_sz, port, NULL, ptr + 1);
	return true;

}

void
parse_hostspec(char *hoststr, size_t hoststr_sz, uint16_t *port,
    bool *ssl, const char *line)
{
	if (ssl)
		*ssl = false;

	strncpy(hoststr, line + (line[0] == '['), hoststr_sz);
	
	char *ptr = strstr(hoststr, "/ssl");
	if (!ptr)
		ptr = strstr(hoststr, "/SSL");

	if (ptr && !ptr[4]) {
		if (ssl)
			*ssl = true;
		*ptr = '\0';
	}

	ptr = strchr(hoststr, ']');
	if (!ptr)
		ptr = hoststr;
	else
		*ptr++ = '\0';

	ptr = strchr(ptr, ':');
	if (ptr) {
		*port = (uint16_t)strtol(ptr+1, NULL, 10);
		*ptr = '\0';
	} else
		*port = 0;
}

bool
parse_identity(char *nick, size_t nicksz, char *uname, size_t unamesz,
    char *fname, size_t fnamesz, const char *identity)
{
	char ident[256];
	ic_strNcpy(ident, identity, sizeof ident);

	char *p = strchr(ident, ' ');
	if (p)
		ic_strNcpy(fname, (*p = '\0', p + 1), fnamesz);
	else
		return false;
	p = strchr(ident, '!');
	if (p)
		ic_strNcpy(uname, (*p = '\0', p + 1), unamesz);
	else
		return false;

	ic_strNcpy(nick, ident, nicksz);
	return true;
}



void
sndumpmsg(char *dest, size_t dest_sz, void *tag, tokarr *msg)
{
	snprintf(dest, dest_sz, "(%p) '%s' '%s'", tag, (*msg)[0], (*msg)[1]);
	for(size_t i = 2; i < COUNTOF(*msg); i++) {
		if (!(*msg)[i])
			break;
		ic_strNcat(dest, " '", dest_sz);
		ic_strNcat(dest, (*msg)[i], dest_sz);
		ic_strNcat(dest, "'", dest_sz);
	}
}

void
dumpmsg(void *tag, tokarr *msg)
{
	char buf[1024];
	sndumpmsg(buf, sizeof buf, tag, msg);
	fprintf(stderr, "%s\n", buf);
}


bool
cr(tokarr *msg, void *tag)
{
	dumpmsg(tag, msg);
	return true;
}

char**
parse_chanmodes(const char *const *arr, size_t argcount, size_t *num,
    const char *modepfx005chr, const char *const *chmodes)
{
	char *modes = strdup(arr[0]);
	if (!modes) {
		EE("strdup");
		return NULL;
	}

	const char *arg;
	size_t nummodes = strlen(modes) - (ic_strCchr(modes,'-')
	    + ic_strCchr(modes,'+'));

	char **modearr = malloc(nummodes * sizeof *modearr);
	if (!modearr) {
		EE("malloc");
		goto parse_chanmodes_fail;
	}

	for (size_t i = 0; i < nummodes; i++)
		modearr[i] = NULL; //for safe cleanup

	size_t i = 1;
	int j = 0, cl;
	char *ptr = modes;
	int enable = 1;
	D("modes: '%s', nummodes: %zu, modepfx005chr: '%s'",
	    modes, nummodes, modepfx005chr);
	while (*ptr) {
		char c = *ptr;
		D("next modechar is '%c', enable ATM: %d", c, enable);
		arg = NULL;
		switch (c) {
		case '+':
			enable = 1;
			ptr++;
			continue;
		case '-':
			enable = 0;
			ptr++;
			continue;
		default:
			cl = classify_chanmode(c, chmodes);
			D("classified mode '%c' to class %d", c, cl);
			switch (cl) {
			case CHANMODE_CLASS_A:
				arg = (i > argcount) ? ("*") : arr[i++];
				break;
			case CHANMODE_CLASS_B:
				arg = (i > argcount) ? ("*") : arr[i++];
				break;
			case CHANMODE_CLASS_C:
				if (enable)
					arg = (i > argcount) ?
					    ("*") : arr[i++];
				break;
			case CHANMODE_CLASS_D:
				break;
			default:/*error?*/
				if (strchr(modepfx005chr, c)) {
					arg = (i > argcount) ?
					    ("*") : arr[i++];
				} else {
					W("unknown chanmode '%c' (0x%X)\n",
					    c, (unsigned)c);
					ptr++;
					continue;
				}
			}
		}
		if (arg)
			D("arg is '%s'", arg);
		modearr[j] = malloc((3 + ((arg != NULL) ?
		    strlen(arg) + 1 : 0)));
		if (!modearr[j]) {
			EE("malloc");
			goto parse_chanmodes_fail;
		}

		modearr[j][0] = enable ? '+' : '-';
		modearr[j][1] = c;
		modearr[j][2] = arg ? ' ' : '\0';
		if (arg) {
			strcpy(modearr[j] + 3, arg);
		}
		j++;
		ptr++;
	}
	D("done parsing, result:");
	for(size_t i = 0; i < nummodes; i++) {
		D("modearr[%zu]: '%s'", i, modearr[i]);
	}

	*num = nummodes;
	free(modes);
	return modearr;

parse_chanmodes_fail:
	if (modearr)
		for (size_t i = 0; i < nummodes; i++)
			free(modearr[i]);

	free(modearr);
	free(modes);
	return NULL;
}

static int
classify_chanmode(char c, const char *const *chmodes)
{
	for (int z = 0; z < 4; ++z) {
		if ((chmodes[z]) && (strchr(chmodes[z], c) != NULL))
			/*XXX this locks the chantype class constants */
			return z+1;
	}
	return 0;
}

void
mutilate_nick(char *nick, size_t nick_sz)
{
	size_t len = strlen(nick);
	if (len < 9) {
		nick[len++] = '_';
		nick[len] = '\0';
	} else {
		char last = nick[len-1];
		if (last == '9')
			nick[1 + rand() % (len-1)] = '0' + rand() % 10;
		else if ('0' <= last && last <= '8')
			nick[len - 1] = last + 1;
		else
			nick[len - 1] = '0';
	}
}

size_t
countargs(tokarr *tok)
{
	size_t ac = 2;
	while (ac < COUNTOF(*tok) && (*tok)[ac])
		ac++;
	return ac;
}


tokarr *ic_clonearr(tokarr *arr)
{
	tokarr *res = malloc(sizeof *res);
	if (!res) {
		EE("malloc");
		return NULL;
	}

	for(size_t i = 0; i < COUNTOF(*arr); i++) {
		if ((*arr)[i]) {
			if (!((*res)[i] = strdup((*arr)[i]))) {
				EE("strdup");
				goto clonearr_fail;
			}
		} else
			(*res)[i] = NULL;
	}
	return res;

clonearr_fail:

	ic_freearr(res);
	return NULL;
}


void
ic_freearr(tokarr *arr)
{
	if (arr) {
		for(size_t i = 0; i < COUNTOF(*arr); i++)
			free((*arr)[i]);
		free(arr);
	}
}
