/* util.c - Implementation of misc. functions related to IRC
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_IRC_UTIL

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <limits.h>

#include "px.h"
#include "common.h"

#include <intlog.h>

#include <libsrsirc/util.h>

#define CHANMODE_CLASS_A 1 /*don't change;see int classify_chanmode(char)*/
#define CHANMODE_CLASS_B 2
#define CHANMODE_CLASS_C 3
#define CHANMODE_CLASS_D 4

static int classify_chanmode(char c, const char *const *chmodes);


void
ut_pfx2nick(char *dest, size_t dest_sz, const char *pfx)
{
	if (!dest_sz)
		return;

	com_strNcpy(dest, pfx, dest_sz);

	char *ptr = strchr(dest, '@');
	if (ptr)
		*ptr = '\0';

	ptr = strchr(dest, '!');
	if (ptr)
		*ptr = '\0';
}

void
ut_pfx2uname(char *dest, size_t dest_sz, const char *pfx)
{
	if (!dest_sz)
		return;

	com_strNcpy(dest, pfx, dest_sz);

	char *ptr = strchr(dest, '@');
	if (ptr)
		*ptr = '\0';

	ptr = strchr(dest, '!');
	if (ptr)
		memmove(dest, ptr+1, strlen(ptr+1)+1);
	else
		*dest = '\0';
}

void
ut_pfx2host(char *dest, size_t dest_sz, const char *pfx)
{
	if (!dest_sz)
		return;

	com_strNcpy(dest, pfx, dest_sz);

	char *ptr = strchr(dest, '@');
	if (ptr)
		memmove(dest, ptr+1, strlen(ptr+1)+1);
	else
		*dest = '\0';
}

int
ut_istrcmp(const char *n1, const char *n2, int casemap)
{
	size_t l1 = strlen(n1);
	size_t l2 = strlen(n2);

	return ut_istrncmp(n1, n2, (l1 < l2 ? l1 : l2) + 1, casemap);
}

int
ut_istrncmp(const char *n1, const char *n2, size_t len, int casemap)
{
	if (len == 0)
		return 0;

	while (*n1 && *n2) {
		char c1 = ut_tolower(*n1, casemap);
		char c2 = ut_tolower(*n2, casemap);
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
ut_tolower(char c, int casemap)
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
ut_strtolower(char *dest, size_t destsz, const char *str, int casemap)
{
	size_t c = 0;
	char *ptr = dest;
	while (c < destsz) {
		*ptr++ = ut_tolower(*str, casemap);

		if (!*str)
			break;
		str++;
	}

	dest[destsz-1] = '\0';
}

bool
ut_parse_pxspec(int *ptype, char *hoststr, size_t hoststr_sz, uint16_t *port,
    const char *pxspec)
{
	char linebuf[128];
	com_strNcpy(linebuf, pxspec, sizeof linebuf);

	char *ptr = strchr(linebuf, ':');
	if (!ptr)
		return false;

	char pxtypestr[7];
	size_t num = (size_t)(ptr - linebuf) < sizeof pxtypestr ?
	    (size_t)(ptr - linebuf) : sizeof pxtypestr - 1;

	com_strNcpy(pxtypestr, linebuf, num + 1);

	int p = px_typenum(pxtypestr);
	if (p == -1)
		return false;

	*ptype = p;

	ut_parse_hostspec(hoststr, hoststr_sz, port, NULL, ptr + 1);
	return true;

}

void
ut_parse_hostspec(char *hoststr, size_t hoststr_sz, uint16_t *port,
    bool *ssl, const char *hostspec)
{
	if (ssl)
		*ssl = false;

	com_strNcpy(hoststr, hostspec + (hostspec[0] == '['), hoststr_sz);

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
		*port = (uint16_t)strtoul(ptr+1, NULL, 10);
		*ptr = '\0';
	} else
		*port = 0;
}


char*
ut_sndumpmsg(char *dest, size_t dest_sz, void *tag, tokarr *msg)
{
	snprintf(dest, dest_sz, "(%p) '%s' '%s'", tag, (*msg)[0], (*msg)[1]);
	for (size_t i = 2; i < COUNTOF(*msg); i++) {
		if (!(*msg)[i])
			break;
		com_strNcat(dest, " '", dest_sz);
		com_strNcat(dest, (*msg)[i], dest_sz);
		com_strNcat(dest, "'", dest_sz);
	}

	return dest;
}

void
ut_dumpmsg(void *tag, tokarr *msg)
{
	char buf[1024];
	ut_sndumpmsg(buf, sizeof buf, tag, msg);
	fprintf(stderr, "%s\n", buf);
}


bool
ut_conread(tokarr *msg, void *tag)
{
	ut_dumpmsg(tag, msg);
	return true;
}

char**
ut_parse_005_cmodes(const char *const *arr, size_t argcount, size_t *num,
    const char *modepfx005chr, const char *const *chmodes)
{
	char *modes = com_strdup(arr[0]);
	if (!modes)
		return NULL;

	const char *arg;
	size_t nummodes = strlen(modes) - (com_strCchr(modes,'-')
	    + com_strCchr(modes,'+'));

	char **modearr = com_malloc(nummodes * sizeof *modearr);
	if (!modearr)
		goto ut_parse_005_cmodes_fail;

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
					W("unknown chanmode '%c'", c);
					ptr++;
					continue;
				}
			}
		}
		if (arg)
			D("arg is '%s'", arg);

		modearr[j] = com_malloc((3 + (arg ? strlen(arg) + 1 : 0)));
		if (!modearr[j])
			goto ut_parse_005_cmodes_fail;

		modearr[j][0] = enable ? '+' : '-';
		modearr[j][1] = c;
		modearr[j][2] = arg ? ' ' : '\0';
		if (arg)
			strcpy(modearr[j] + 3, arg);

		j++;
		ptr++;
	}
	D("done parsing, result:");
	for (size_t i = 0; i < nummodes; i++) {
		D("modearr[%zu]: '%s'", i, modearr[i]);
	}

	*num = nummodes;
	free(modes);
	return modearr;

ut_parse_005_cmodes_fail:
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
ut_mut_nick(char *nick, size_t nick_sz)
{
	size_t len = strlen(nick);
	if (len < 9) {
		nick[len++] = '_';
		nick[len] = '\0';
	} else {
		char last = nick[len-1];
		if (last == '9')
			nick[rand() % (len-1) + 1u] = '0' + rand() % 10;
		else if ('0' <= last && last <= '8')
			nick[len - 1] = last + 1;
		else
			nick[len - 1] = '0';
	}
}

tokarr *ut_clonearr(tokarr *arr)
{
	tokarr *res = com_malloc(sizeof *res);
	if (!res)
		return NULL;

	for (size_t i = 0; i < COUNTOF(*arr); i++) {
		if ((*arr)[i]) {
			if (!((*res)[i] = com_strdup((*arr)[i])))
				goto clonearr_fail;
		} else
			(*res)[i] = NULL;
	}
	return res;

clonearr_fail:

	ut_freearr(res);
	return NULL;
}


void
ut_freearr(tokarr *arr)
{
	if (arr) {
		for (size_t i = 0; i < COUNTOF(*arr); i++)
			free((*arr)[i]);
		free(arr);
	}
}
