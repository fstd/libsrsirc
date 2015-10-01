/* icat_core.c - icat core, mainly links together icat_serv and icat_user
 * icat - IRC netcat on top of libsrsirc - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_ICATCORE

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "icat_core.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <platform/base_time.h>
#include <platform/base_misc.h>

#include <libsrsirc/defs.h>
#include <libsrsirc/irc_ext.h>
#include <libsrsirc/util.h>

#include <logger/intlog.h>

#include "icat_common.h"
#include "icat_misc.h"
#include "icat_serv.h"
#include "icat_user.h"


static unsigned s_readcnt;
static unsigned s_writecnt;


static void handle_ircmsg(tokarr *tok);
static void handle_usermsg(char *linebuf);
static void dump_info(void);


void
lsi_core_init(void)
{ T("trace");
	lsi_serv_init();
}

void
lsi_core_destroy(void)
{ T("trace");
	lsi_serv_destroy();
}

int
lsi_core_run(void)
{ T("trace");
	tokarr tok;
	char line[1024];
	bool idle;

	bool ignoreuser = false;
	uint64_t nextop = 0;

	while (!g_interrupted) {
		idle = true;

		if (g_inforequest) {
			dump_info();
			g_inforequest = false;
		}

		if (lsi_serv_canread()) {
			int r = lsi_serv_read(&tok);
			if (r <= 0)
				continue;

			lsi_ut_sndumpmsg(line, sizeof line, NULL, &tok);

			I("From server: '%s'", line);

			if (strcmp(tok[1], "PRIVMSG") == 0)
				handle_ircmsg(&tok);

			idle = false;
		}

		if (!ignoreuser) {
			if (lsi_user_canread()) {
				size_t olen = lsi_user_readline(line, sizeof line);
				I("From user: '%s'", line);
				if (olen >= sizeof line)
					W("Line was too long -- truncated!");

				handle_usermsg(line);

				idle = false;
			} else if (lsi_user_eof()) {
				N("EOF on the user end, queuing a QUIT");
				lsi_serv_printf("QUIT :%s\r\n", g_sett.qmsg);
				ignoreuser = true;
			}
		}

		uint64_t tquit = lsi_serv_sentquit();
		bool on = lsi_serv_online();

		if (tquit) {
			if (!on)
				break;

			if (tquit + g_sett.waitquit_us < lsi_b_tstamp_us()) {
				N("Waitquit exceeded, breaking connection");
				break;
			}
		}

		if (!on && (!g_sett.reconnect || lsi_user_eof())) {
			N("IRC offline, not going to reconnect%s.",
			    g_sett.reconnect?" (because EOF on stdin)":"");
			break;
		}

		if (nextop <= lsi_b_tstamp_us()) {
			if (!lsi_serv_operate()) {
				E("lsi_serv_operate() failed");
				if (!on) {
					N("Delaying next connection attempt");
					nextop = lsi_b_tstamp_us() + g_sett.cfwait_us;
				}
			}
		}
		if (idle)
			lsi_b_usleep(100000L);
	}

	if (g_interrupted)
		N("Interrupted by user");

	return 0;
}


static void
handle_ircmsg(tokarr *tok)
{ T("trace");
	char nick[32];
	if (!(*tok)[0])
		return;

	s_readcnt++;

	lsi_ut_pfx2nick(nick, sizeof nick, (*tok)[0]);

	if (g_sett.ignore_cs && !lsi_ut_istrcmp(nick, "ChanServ", lsi_serv_casemap()))
		return;

	if (g_sett.trgmode)
		lsi_user_printf("%s %s %s %s\n",
		    nick, (*tok)[0], (*tok)[2], (*tok)[3]);
	else if (lsi_ismychan((*tok)[2]) || !g_sett.ignore_pm)
		lsi_user_printf("%s\n", (*tok)[3]);
}

static void
handle_usermsg(char *lnbuf)
{ T("trace");
	s_writecnt++;

	char *end = lnbuf + strlen(lnbuf) - 1;
	while (end >= lnbuf && (*end == '\r' || *end == '\n'))
		*end-- = '\0';

	if (strlen(lnbuf) == 0)
		return;

	/* protocol escape */
	if (g_sett.esc && !strncmp(lnbuf, g_sett.esc, strlen(g_sett.esc))) {
		lsi_serv_printf("%s", lnbuf += strlen(g_sett.esc));
		return;
	}

	char *dst = g_sett.chanlist;

	if (g_sett.trgmode) {
		char *tok = strtok(lnbuf, " ");
		dst = tok;
		lnbuf = tok + strlen(tok) + 1;
	}

	lsi_serv_printf("%s %s :%s", g_sett.notices?"NOTICE":"PRIVMSG", dst, lnbuf);
}

static void
dump_info(void)
{ T("trace");
	fprintf(stderr, "icat: %sline; read %uln wrote %uln\n",
	    lsi_serv_online()?"on":"off", s_readcnt, s_writecnt);
}
