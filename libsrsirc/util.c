/* util.c - Implementation of misc. functions related to IRC
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-18, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_IRC_UTIL

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <libsrsirc/util.h>

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <platform/base_misc.h>
#include <platform/base_string.h>

#include <logger/intlog.h>

#include "common.h"
#include "intdefs.h"
#include "px.h"


void
lsi_ut_ident2nick(char *dest, size_t dest_sz, const char *pfx)
{
	if (!dest_sz)
		return;

	lsi_b_strNcpy(dest, pfx, dest_sz);

	char *ptr = strchr(dest, '@');
	if (ptr)
		*ptr = '\0';

	ptr = strchr(dest, '!');
	if (ptr)
		*ptr = '\0';
	return;
}

void
lsi_ut_ident2uname(char *dest, size_t dest_sz, const char *pfx)
{
	if (!dest_sz)
		return;

	lsi_b_strNcpy(dest, pfx, dest_sz);

	char *ptr = strchr(dest, '@');
	if (ptr)
		*ptr = '\0';

	ptr = strchr(dest, '!');
	if (ptr)
		memmove(dest, ptr+1, strlen(ptr+1)+1);
	else
		*dest = '\0';
	return;
}

void
lsi_ut_ident2host(char *dest, size_t dest_sz, const char *pfx)
{
	if (!dest_sz)
		return;

	lsi_b_strNcpy(dest, pfx, dest_sz);

	char *ptr = strchr(dest, '@');
	if (ptr)
		memmove(dest, ptr+1, strlen(ptr+1)+1);
	else
		*dest = '\0';
	return;
}

int
lsi_ut_istrcmp(const char *n1, const char *n2, int casemap)
{
	size_t l1 = strlen(n1);
	size_t l2 = strlen(n2);

	return lsi_ut_istrncmp(n1, n2, (l1 < l2 ? l1 : l2) + 1, casemap);
}

int
lsi_ut_istrncmp(const char *n1, const char *n2, size_t len, int casemap)
{
	if (len == 0)
		return 0;

	while (len && *n1 && *n2) {
		char c1 = lsi_ut_tolower(*n1, casemap);
		char c2 = lsi_ut_tolower(*n2, casemap);
		if (c1 != c2)
			return c1 - c2;

		n1++;
		n2++;
		len--;
	}

	if (!len)
		return 0;
	if (*n1)
		return 1;
	if (*n2)
		return -1;

	return 0;
}

char
lsi_ut_tolower(char c, int casemap)
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
lsi_ut_strtolower(char *dest, size_t destsz, const char *str, int casemap)
{
	size_t c = 0;
	char *ptr = dest;
	while (c < destsz) {
		*ptr++ = lsi_ut_tolower(*str, casemap);

		if (!*str)
			break;
		str++;
	}

	dest[destsz-1] = '\0';
	return;
}

bool
lsi_ut_parse_pxspec(int *ptype, char *hoststr, size_t hoststr_sz,
    uint16_t *port, const char *pxspec)
{
	char linebuf[128];
	STRACPY(linebuf, pxspec);

	char *ptr = strchr(linebuf, ':');
	if (!ptr)
		return false;

	char pxtypestr[7];
	size_t num = (size_t)(ptr - linebuf) < sizeof pxtypestr ?
	    (size_t)(ptr - linebuf) : sizeof pxtypestr - 1;

	lsi_b_strNcpy(pxtypestr, linebuf, num + 1);

	int p = lsi_px_typenum(pxtypestr);
	if (p == -1)
		return false;

	*ptype = p;

	lsi_ut_parse_hostspec(hoststr, hoststr_sz, port, NULL, ptr + 1);
	return true;
}

void
lsi_ut_parse_hostspec(char *hoststr, size_t hoststr_sz, uint16_t *port,
    bool *ssl, const char *hostspec)
{
	if (ssl)
		*ssl = false;

	lsi_b_strNcpy(hoststr, hostspec + (hostspec[0] == '['), hoststr_sz);

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
	return;
}

char *
lsi_ut_snrcmsg(char *dest, size_t destsz, tokarr *msg, bool coltr)
{
	if ((*msg)[0])
		snprintf(dest, destsz, ":%s %s", (*msg)[0], (*msg)[1]);
	else
		snprintf(dest, destsz, "%s", (*msg)[1]);

	size_t i = 2;
	while (i < COUNTOF(*msg) && (*msg)[i]) {
		lsi_b_strNcat(dest, " ", destsz);
		if ((i+1 == COUNTOF(*msg) || !(*msg)[i+1])
		    && (coltr || strchr((*msg)[i], ' ')))
			lsi_b_strNcat(dest, ":", destsz);
		lsi_b_strNcat(dest, (*msg)[i], destsz);
		i++;
	}

	lsi_b_strNcat(dest, "\r\n", destsz);

	return dest;
}

char *
lsi_ut_sndumpmsg(char *dest, size_t dest_sz, void *tag, tokarr *msg)
{
	snprintf(dest, dest_sz, "(%p) '%s' '%s'",
	    (void *)tag, (*msg)[0], (*msg)[1]);

	for (size_t i = 2; i < COUNTOF(*msg); i++) {
		if (!(*msg)[i])
			break;
		lsi_b_strNcat(dest, " '", dest_sz);
		lsi_b_strNcat(dest, (*msg)[i], dest_sz);
		lsi_b_strNcat(dest, "'", dest_sz);
	}

	return dest;
}

void
lsi_ut_dumpmsg(void *tag, tokarr *msg)
{
	char buf[1024];
	lsi_ut_sndumpmsg(buf, sizeof buf, tag, msg);
	fprintf(stderr, "%s\n", buf);
	return;
}


bool
lsi_ut_conread(tokarr *msg, void *tag)
{
	lsi_ut_dumpmsg(tag, msg);
	return true;
}

char **
lsi_ut_parse_MODE(irc *ctx, tokarr *msg, size_t *num, bool is324)
{
	size_t ac = 2;
	while (ac < COUNTOF(*msg) && (*msg)[ac])
		ac++;

	char *modes = STRDUP((*msg)[3 + is324]);
	if (!modes)
		return NULL;

	const char *arg;
	size_t nummodes = strlen(modes) - (lsi_com_strCchr(modes,'-')
	    + lsi_com_strCchr(modes,'+'));

	char **modearr = MALLOC(nummodes * sizeof *modearr);
	if (!modearr)
		goto fail;

	for (size_t i = 0; i < nummodes; i++)
		modearr[i] = NULL; //for safe cleanup

	size_t i = 4 + is324;
	int j = 0, cl;
	char *ptr = modes;
	int enable = 1;
	D("modes: '%s', nummodes: %zu, m005modepfx: '%s'",
	    modes, nummodes, ctx->m005modepfx[0]);
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
			cl = lsi_ut_classify_chanmode(ctx, c);
			D("classified mode '%c' to class %d", c, cl);
			switch (cl) {
			case CHANMODE_CLASS_A:
				arg = i >= ac ? "*" : (*msg)[i++];
				break;
			case CHANMODE_CLASS_B:
				arg = i >= ac ? "*" : (*msg)[i++];
				break;
			case CHANMODE_CLASS_C:
				if (enable)
					arg = i >= ac ?  "*" : (*msg)[i++];
				break;
			case CHANMODE_CLASS_D:
				break;
			default:/*error?*/
				if (strchr(ctx->m005modepfx[0], c))
					arg = i >= ac ? "*" : (*msg)[i++];
				else {
					W("unknown chanmode '%c'", c);
					nummodes--;
					ptr++;
					continue;
				}
			}
		}
		if (arg)
			D("arg is '%s'", arg);

		modearr[j] = MALLOC((3 + (arg ? strlen(arg) + 1 : 0)));
		if (!modearr[j])
			goto fail;

		modearr[j][0] = enable ? '+' : '-';
		modearr[j][1] = c;
		modearr[j][2] = arg ? ' ' : '\0';
		if (arg)
			strcpy(modearr[j] + 3, arg);

		j++;
		ptr++;
	}
	D("done parsing, result:");
	for (i = 0; i < nummodes; i++) {
		D("modearr[%zu]: '%s'", i, modearr[i]);
	}

	*num = nummodes;
	free(modes);
	return modearr;

fail:
	if (modearr)
		for (i = 0; i < nummodes; i++)
			free(modearr[i]);

	free(modearr);
	free(modes);
	return NULL;
}

int
lsi_ut_classify_chanmode(irc *ctx, char c)
{
	for (int z = 0; z < 4; ++z) {
		if (ctx->m005chanmodes[z] && strchr(ctx->m005chanmodes[z], c))
			/*XXX this locks the chantype class constants */
			return z+1;
	}
	return 0;
}

void
lsi_ut_mut_nick(char *nick, size_t nick_sz)
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
	return;
}

tokarr *
lsi_ut_clonearr(tokarr *arr)
{
	tokarr *res = MALLOC(sizeof *res);
	if (!res)
		return NULL;

	for (size_t i = 0; i < COUNTOF(*arr); i++) {
		if ((*arr)[i]) {
			if (!((*res)[i] = STRDUP((*arr)[i])))
				goto fail;
		} else
			(*res)[i] = NULL;
	}
	return res;

fail:

	lsi_ut_freearr(res);
	return NULL;
}


void
lsi_ut_freearr(tokarr *arr)
{
	if (arr) {
		for (size_t i = 0; i < COUNTOF(*arr); i++)
			free((*arr)[i]);
		free(arr);
	}
	return;
}


/* in-place tokenize an IRC protocol message pointed to by `buf'
 * the array pointed to by `tok' is populated with pointers to the identified
 * tokens; if there are less tokens than elements in the array, the remaining
 * elements are set to NULL.  returns true on success, false on failure */
bool
lsi_ut_tokenize(char *buf, tokarr *tok)
{
	for (size_t i = 0; i < COUNTOF(*tok); ++i)
		(*tok)[i] = NULL;

	if (*buf == ':') { /* message has a prefix */
		(*tok)[0] = buf + 1; /* disregard the colon */
		if (!(buf = lsi_com_next_tok(buf, ' '))) {
			E("protocol error (no more tokens after prefix)");
			return false;
		}
	} else if (*buf == ' ') { /* this would lead to parsing issues */
		E("protocol error (leading whitespace)");
		return false;
	} else if (!*buf) {
		E("bug (empty line)"); //this shouldn't be possible anymore
		return false;
	}

	(*tok)[1] = buf; /* command */

	size_t argc = 2;
	while (argc < COUNTOF(*tok) && (buf = lsi_com_next_tok(buf, ' '))) {
		if (*buf == ':') { /* `trailing' arg */
			(*tok)[argc++] = buf + 1; /* disregard the colon */
			break;
		}

		(*tok)[argc++] = buf;
	}

	return true;
}

char *
lsi_ut_extract_tags(char *line, char **dest, size_t *ndest)
{
	size_t destmax = ndest ? *ndest : 0;
	size_t tc = 0;
	char *end = strchr(line, ' ');
	if (!end)
		return NULL;
	*end = '\0';

	do {
		if (tc < destmax)
			dest[tc++] = line;
	} while ((line = lsi_com_next_tok(line, ';')));

	if (ndest)
		*ndest = tc;

	return end + 1;
}
/*
*/

const char *
lsi_ut_casemap_nam(int cm)
{
	switch (cm)
	{
		case CMAP_RFC1459:
			return "RFC1459";
		case CMAP_STRICT_RFC1459:
			return "STRICT_RFC1459";
		case CMAP_ASCII:
			return "ASCII";
		default:
			return "UNKNOWN";
	}
}

bool
lsi_ut_ischan(irc *ctx, const char *str)
{
	if (!str)
		return false;

	return strchr(ctx->m005chantypes, str[0]);
}

/* note: `*destsz` should be the size of what is pointed to by `dest` when
 *       calling this, before returning it is set to the actual size of the
 *       resulting auth blob. */
bool
lsi_ut_sasl_mkplauth(void *dest, size_t *destsz,
    const char *name, const char *pass, bool base64)
{
	bool ret = false;
	uint8_t *tmpbuf = MALLOC(*destsz);
	if (!tmpbuf)
		return false;

	int r = snprintf((char *)tmpbuf, *destsz,
	    "%s%c%s%c%s", name, '\0', name, '\0', pass);

	if (r < 0 || (size_t)r >= *destsz) {
		E("Could not create auth string (too big?)");
		goto fail;
	}

	if (base64) {
		size_t s = lsi_com_base64(dest, *destsz, tmpbuf, r);
		if (!s)
			goto fail;
		*destsz = s;
	} else {
		memcpy(dest, tmpbuf, r);
		*destsz = (size_t)r;
	}

	ret = true;

fail:
	free(tmpbuf);
	return ret;
}
