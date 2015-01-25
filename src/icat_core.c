/* icat_core.c - icat core, mainly links together icat_serv and icat_user
 * icat - IRC netcat on top of libsrsirc - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_CORE

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "icat_core.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <err.h>
#include <inttypes.h>
#include <sys/time.h>
#include <unistd.h>

#include <libsrsirc/defs.h>
#include <libsrsirc/irc_ext.h>
#include <libsrsirc/util.h>

#include "icat_common.h"
#include "icat_debug.h"
#include "icat_misc.h"
#include "icat_serv.h"
#include "icat_user.h"


static void handle_ircmsg(tokarr *tok);
static void handle_usermsg(char *linebuf);


void
core_init(void)
{
	serv_init();
}

void
core_destroy(void)
{
	serv_destroy();
}

int
core_run(void)
{
	tokarr tok;
	char line[1024];
	bool idle;

	bool ignoreuser = false;
	uint64_t nextop = 0;

	while (!g_interrupted) {
		idle = true;
		if (serv_canread()) {
			int r = serv_read(&tok);
			if (r <= 0) {
				E("serv_read: %d", r);
				continue;
			}

			ut_sndumpmsg(line, sizeof line, NULL, &tok);

			if (strcmp(tok[1], "PRIVMSG") == 0) {
				I("from server: '%s'", line);
				handle_ircmsg(&tok);
			} else if (strcmp(tok[1], "372"))
				D("from server: '%s'", line);

			idle = false;
		}

		if (!ignoreuser) {
			if (user_canread()) {
				size_t olen = user_readline(line, sizeof line);
				I("from user: '%s'", line);
				if (olen >= sizeof line)
					W("line was too long -- truncated!");

				handle_usermsg(line);

				idle = false;
			} else if (user_eof()) {
				serv_printf("QUIT\r\n");
				ignoreuser = true;
			}
		}

		uint64_t tquit = serv_sentquit();
		if (tquit && tquit + g_sett.waitquit_us < timestamp_us()) {
			N("waitquit exceeded, breaking connection");
			break;
		}


		bool on = serv_online();
		if (!on && (!g_sett.reconnect || user_eof())) {
			N("irc offline, not going to reconnect%s.",
			    g_sett.reconnect?" (because EOF on stdin)":"");
			break;
		}

		if (nextop <= timestamp_us()) {
			if (!serv_operate()) {
				E("serv_operate() failed");
				if (!on) {
					N("delaying next connection attempt");
					nextop = timestamp_us() + g_sett.confailwait_us;
				}
			}
		}
		if (idle)
			usleep(100000);
	}

	if (g_interrupted)
		N("Interrupted by user");

	return 0;
}


static void
handle_ircmsg(tokarr *tok)
{
	char nick[32];
	if (!(*tok)[0])
		return;

	ut_pfx2nick(nick, sizeof nick, (*tok)[0]);

	if (g_sett.ignore_cs && !ut_istrcmp(nick, "ChanServ", serv_casemap()))
		return;

	if (g_sett.trgmode)
		user_printf("%s %s %s %s\n",
		    nick, (*tok)[0], (*tok)[2], (*tok)[3]);
	else if (ismychan((*tok)[2]) || !g_sett.ignore_pm)
		user_printf("%s\n", (*tok)[3]);
}

static void
handle_usermsg(char *linebuf)
{
	char *end = linebuf + strlen(linebuf) - 1;
	while (end >= linebuf && (*end == '\r' || *end == '\n'))
		*end-- = '\0';

	if (strlen(linebuf) == 0)
		return;

	/* protocol escape */
	if (g_sett.esc && strncmp(linebuf, g_sett.esc, strlen(g_sett.esc)) == 0) {
		serv_printf("%s", linebuf += strlen(g_sett.esc));
		return;
	}

	char *dst = g_sett.chanlist;

	if (g_sett.trgmode) {
		char *tok = strtok(linebuf, " ");
		dst = tok;
		linebuf = tok + strlen(tok) + 1;
	}

	serv_printf("%s %s :%s", g_sett.notices?"NOTICE":"PRIVMSG", dst, linebuf);
}
