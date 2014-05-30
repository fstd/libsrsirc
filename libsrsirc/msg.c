/* msg.c - protocol message handlers
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define LOG_MODULE MOD_IMSG

/* C */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include <libsrsirc/defs.h>
#include "conn.h"
#include <libsrsirc/util.h>
#include <intlog.h>

#include "msg.h"

struct msghnd {
	char cmd[32];
	hnd_fn hndfn;
	const char *dbginfo;
	void *tag;
};

static struct msghnd msghnds[32]; //XXX enough?

bool
msg_reghnd(const char *cmd, hnd_fn hndfn, const char *dbginfo, void *tag)
{
	size_t i = 0;
	for (;i < COUNTOF(msghnds); i++)
		if (!msghnds[i].cmd[0])
			break;

	if (i == COUNTOF(msghnds)) {
		E("can't register msg handler, array full");
		return false;
	}

	msghnds[i].dbginfo = dbginfo;
	msghnds[i].hndfn = hndfn;
	msghnds[i].tag = tag;
	com_strNcpy(msghnds[i].cmd, cmd, sizeof msghnds[i].cmd);
	return true;
}

uint8_t
msg_handle(irc hnd, tokarr *msg, bool logon)
{
	uint8_t res = 0;
	size_t i = 0;
	size_t ac = 2;
	while (ac < COUNTOF(*msg) && (*msg)[ac])
		ac++;

	for (;i < COUNTOF(msghnds); i++) {
		if (!msghnds[i].cmd[0])
			break;

		if (strcmp((*msg)[1], msghnds[i].cmd) == 0) {
			D("dispatch a '%s' to '%s'", (*msg)[1], msghnds[i].dbginfo);
			res |= msghnds[i].hndfn(hnd, msg, ac, logon, msghnds[i].tag);
			if (!(res & CANT_PROCEED))
				continue;

			if (res & AUTH_ERR) {
				E("failed to authenticate");
			} else if (res & IO_ERR) {
				E("i/o error");
			} else if (res & OUT_OF_NICKS) {
				E("out of nicks");
			} else if (res & PROTO_ERR) {
				char line[1024];
				E("proto error on '%s' (ct:%d)",
				    ut_sndumpmsg(line, sizeof line, NULL, msg),
				    conn_colon_trail(hnd->con));
			} else {
				E("can't proceed for unknown reasons");
			}

			return res;
		}
	}

	return res;
}
