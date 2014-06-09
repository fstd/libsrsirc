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

bool
msg_reghnd(irc hnd, const char *cmd, hnd_fn hndfn, const char *module)
{
	size_t i = 0;
	D("'%s' registering '%s'-handler", module, cmd);
	for (;i < hnd->msghnds_cnt; i++)
		if (!hnd->msghnds[i].cmd[0])
			break;

	if (i == hnd->msghnds_cnt) {
		size_t ncnt = hnd->msghnds_cnt * 2;
		struct msghnd *narr;
		if (!(narr = malloc(ncnt * sizeof *narr)))
			return false;

		memcpy(narr, hnd->msghnds, hnd->msghnds_cnt * sizeof *narr);
		for (size_t i = hnd->msghnds_cnt; i < ncnt; i++)
			narr[i].cmd[0] = '\0';

		free(hnd->msghnds);

		hnd->msghnds = narr;
		hnd->msghnds_cnt = ncnt;
	}

	hnd->msghnds[i].module = module;
	hnd->msghnds[i].hndfn = hndfn;
	com_strNcpy(hnd->msghnds[i].cmd, cmd, sizeof hnd->msghnds[i].cmd);
	return true;
}

void
msg_unregall(irc hnd, const char *module)
{
	size_t i = 0;
	for (;i < hnd->msghnds_cnt; i++)
		if (hnd->msghnds[i].cmd[0]
		    && strcmp(hnd->msghnds[i].module, module) == 0)
			hnd->msghnds[i].cmd[0] = '\0';
}

uint8_t
msg_handle(irc hnd, tokarr *msg, bool logon)
{
	uint8_t res = 0;
	size_t i = 0;
	size_t ac = 2;
	while (ac < COUNTOF(*msg) && (*msg)[ac])
		ac++;

	for (;i < hnd->msghnds_cnt; i++) {
		if (!hnd->msghnds[i].cmd[0])
			continue;

		if (strcmp((*msg)[1], hnd->msghnds[i].cmd) == 0) {
			D("dispatch a '%s' to '%s'", (*msg)[1], hnd->msghnds[i].module);
			res |= hnd->msghnds[i].hndfn(hnd, msg, ac, logon);
			if (!(res & CANT_PROCEED))
				continue;

			res &= ~CANT_PROCEED;

			if (res & AUTH_ERR) {
				E("failed to authenticate");
				res |= CANT_PROCEED;
			} else if (res & IO_ERR) {
				E("i/o error");
				res |= CANT_PROCEED;
			} else if (res & ALLOC_ERR) {
				E("memory allocation failed");
				/* we do proceed for now */
			} else if (res & OUT_OF_NICKS) {
				E("out of nicks");
				res |= CANT_PROCEED;
			} else if (res & PROTO_ERR) {
				char line[1024];
				E("proto error on '%s' (ct:%d)",
				    ut_sndumpmsg(line, sizeof line, NULL, msg),
				    conn_colon_trail(hnd->con));
				/* we do proceed for now */
			} else {
				E("can't proceed for unknown reasons");
				/* we do proceed for now */
			}


			return res;
		}
	}

	return res;
}
