/* msghandle.c - handlers for protocol messages
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* C */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <common.h>
#include <libsrsirc/ihnd.h>
#include "irc_con.h"
#include <libsrsirc/irc_util.h>
#include <intlog.h>
#include "ircdefs.h"

#include "msghandle.h"

bool
handle_001(ibhnd_t hnd, char **msg, size_t nargs)
{
	if (nargs < 3)
		return false;

	ic_freearr(hnd->logonconv[0], MAX_IRCARGS);
	hnd->logonconv[0] = ic_clonearr(msg, MAX_IRCARGS);
	ic_strNcpy(hnd->mynick, msg[2],sizeof hnd->mynick);
	char *tmp;
	if ((tmp = strchr(hnd->mynick, '@')))
		*tmp = '\0';
	if ((tmp = strchr(hnd->mynick, '!')))
		*tmp = '\0';

	ic_strNcpy(hnd->myhost, msg[0] ?
	    msg[0] : irccon_get_host(hnd->con), sizeof hnd->mynick);

	ic_strNcpy(hnd->umodes, DEF_UMODES, sizeof hnd->umodes);
	ic_strNcpy(hnd->cmodes, DEF_CMODES, sizeof hnd->cmodes);
	hnd->ver[0] = '\0';
	hnd->service = false;

	return true;
}

bool
handle_002(ibhnd_t hnd, char **msg, size_t nargs)
{
	ic_freearr(hnd->logonconv[1], MAX_IRCARGS);
	hnd->logonconv[1] = ic_clonearr(msg, MAX_IRCARGS);

	return true;
}

bool
handle_003(ibhnd_t hnd, char **msg, size_t nargs)
{
	ic_freearr(hnd->logonconv[2], MAX_IRCARGS);
	hnd->logonconv[2] = ic_clonearr(msg, MAX_IRCARGS);

	return true;
}

bool
handle_004(ibhnd_t hnd, char **msg, size_t nargs)
{
	if (nargs < 7)
		return false;

	ic_freearr(hnd->logonconv[3], MAX_IRCARGS);
	hnd->logonconv[3] = ic_clonearr(msg, MAX_IRCARGS);
	ic_strNcpy(hnd->myhost, msg[3],sizeof hnd->myhost);
	ic_strNcpy(hnd->umodes, msg[5], sizeof hnd->umodes);
	ic_strNcpy(hnd->cmodes, msg[6], sizeof hnd->cmodes);
	ic_strNcpy(hnd->ver, msg[4], sizeof hnd->ver);
	D("(%p) got beloved 004", hnd);

	return true;
}

bool
handle_PING(ibhnd_t hnd, char **msg, size_t nargs)
{
	if (nargs < 3)
		return false;

	char buf[64];
	snprintf(buf, sizeof buf, "PONG :%s\r\n", msg[2]);
	return irccon_write(hnd->con, buf);
}

bool
handle_432(ibhnd_t hnd, char **msg, size_t nargs)
{
	return handle_XXX(hnd, msg, nargs);
}

bool
handle_433(ibhnd_t hnd, char **msg, size_t nargs)
{
	return handle_XXX(hnd, msg, nargs);
}

bool
handle_436(ibhnd_t hnd, char **msg, size_t nargs)
{
	return handle_XXX(hnd, msg, nargs);
}

bool
handle_437(ibhnd_t hnd, char **msg, size_t nargs)
{
	return handle_XXX(hnd, msg, nargs);
}

bool
handle_XXX(ibhnd_t hnd, char **msg, size_t nargs)
{
	if (!hnd->cb_mut_nick) {
		W("(%p) got no mutnick.. (failing)", hnd);
		return false;
	}

	hnd->cb_mut_nick(hnd->mynick, 10); //XXX hc
	char buf[MAX_NICK_LEN];
	snprintf(buf,sizeof buf,"NICK %s\r\n",hnd->mynick);
	return irccon_write(hnd->con, buf);
}

bool
handle_464(ibhnd_t hnd, char **msg, size_t nargs)
{
	W("(%p) wrong server password", hnd);
	return true;
}

bool
handle_383(ibhnd_t hnd, char **msg, size_t nargs)
{
	if (nargs < 3)
		return false;

	ic_strNcpy(hnd->mynick, msg[2],sizeof hnd->mynick);
	char *tmp;
	if ((tmp = strchr(hnd->mynick, '@')))
		*tmp = '\0';
	if ((tmp = strchr(hnd->mynick, '!')))
		*tmp = '\0';

	ic_strNcpy(hnd->myhost,
	    msg[0] ? msg[0] : irccon_get_host(hnd->con),
	    sizeof hnd->mynick);

	ic_strNcpy(hnd->umodes, DEF_UMODES, sizeof hnd->umodes);
	ic_strNcpy(hnd->cmodes, DEF_CMODES, sizeof hnd->cmodes);
	hnd->ver[0] = '\0';
	hnd->service = true;

	return true;
}

bool
handle_484(ibhnd_t hnd, char **msg, size_t nargs)
{
	hnd->restricted = true;

	return true;
}

bool
handle_465(ibhnd_t hnd, char **msg, size_t nargs)
{
	W("(%p) we're banned", hnd);
	hnd->banned = true;
	free(hnd->banmsg);
	hnd->banmsg = strdup(msg[3] ? msg[3] : "");
	if (!hnd->banned)
		EE("strdup");

	return true;
}

bool
handle_466(ibhnd_t hnd, char **msg, size_t nargs)
{
	W("(%p) we will be banned", hnd);

	return true;
}

bool
handle_ERROR(ibhnd_t hnd, char **msg, size_t nargs)
{
	free(hnd->lasterr);
	hnd->lasterr = strdup(msg[2] ? msg[2] : "");
	if (!hnd->lasterr)
		EE("strdup");
	return true;
}

bool
handle_NICK(ibhnd_t hnd, char **msg, size_t nargs)
{
	if (nargs < 3)
		return false;

	char nick[128];
	if (!pfx_extract_nick(nick, sizeof nick, msg[0]))
		return false;

	if (!istrcasecmp(nick, hnd->mynick, hnd->casemapping))
		ic_strNcpy(hnd->mynick, msg[2], sizeof hnd->mynick);
	
	return true;
}
bool
handle_005_CASEMAPPING(ibhnd_t hnd, const char *val)
{
	if (strcasecmp(val, "ascii") == 0)
		hnd->casemapping = CMAP_ASCII;
	else if (strcasecmp(val, "strict-rfc1459") == 0)
		hnd->casemapping = CMAP_STRICT_RFC1459;
	else {
		if (strcasecmp(val, "rfc1459") != 0)
			W("unknown 005 casemapping: '%s'", val);
		hnd->casemapping = CMAP_RFC1459;
	}

	return true;
}

/* XXX not robust enough */
bool
handle_005_PREFIX(ibhnd_t hnd, const char *val)
{
	char str[32];
	ic_strNcpy(str, val + 1, sizeof str);
	char *p = strchr(str, ')');
	*p++ = '\0';
	ic_strNcpy(hnd->m005modepfx[0], str, sizeof hnd->m005modepfx[0]);
	ic_strNcpy(hnd->m005modepfx[1], p, sizeof hnd->m005modepfx[1]);

	return true;
}

bool
handle_005_CHANMODES(ibhnd_t hnd, const char *val)
{
	for (int z = 0; z < 4; ++z)
		hnd->m005chanmodes[z][0] = '\0';

	int c = 0;
	char argbuf[64];
	ic_strNcpy(argbuf, val, sizeof argbuf);
	char *ptr = strtok(argbuf, ",");

	while (ptr) {
		if (c < 4)
			ic_strNcpy(hnd->m005chanmodes[c++], ptr,
			    sizeof hnd->m005chanmodes[0]);
		ptr = strtok(NULL, ",");
	}

	if (c != 4)
		W("005 chanmodes: expected 4 params, got %i. arg: \"%s\"",
		    c, val);

	return true;
}

bool
handle_005(ibhnd_t hnd, char **msg, size_t nargs)
{
	bool failure = false;

	for (size_t z = 3; z < nargs; ++z) {
		if (strncasecmp(msg[z], "CASEMAPPING=", 12) == 0) {
			if (!(handle_005_CASEMAPPING(hnd, msg[z] + 12)))
				failure = true;
		} else if (strncasecmp(msg[z], "PREFIX=", 7) == 0) {
			if (!(handle_005_PREFIX(hnd, msg[z] + 7)))
				failure = true;
		} else if (strncasecmp(msg[z], "CHANMODES=", 10) == 0) {
			if (!(handle_005_CHANMODES(hnd, msg[z] + 10)))
				failure = true;
		}

		if (failure)
			return false;
	}

	return true;
}

