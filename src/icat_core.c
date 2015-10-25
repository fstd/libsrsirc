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
#include <platform/base_net.h>

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
icat_core_init(void)
{
	icat_serv_init();
	return;
}

void
icat_core_destroy(void)
{
	icat_serv_destroy();
	return;
}

int
icat_core_run(void)
{
	tokarr tok;
	char ln[1024];
	bool idle;

	bool ignoreuser = false;
	uint64_t nextop = 0;

	while (!g_interrupted) {
		idle = true;

		if (g_inforequest) {
			dump_info();
			g_inforequest = false;
		}

		if (icat_serv_online()) {
			int r = icat_serv_read(&tok, 1);
			if (r < 0)
				continue;
			
			if (r == 1) {
				lsi_ut_sndumpmsg(ln, sizeof ln, NULL, &tok);

				I("From server: '%s'", ln);

				if (strcmp(tok[1], "PRIVMSG") == 0)
					handle_ircmsg(&tok);

				idle = false;
			}
		}

		if (!ignoreuser) {
			if (icat_user_canread()) {
				size_t olen = icat_user_readline(ln, sizeof ln);
				I("From user: '%s'", ln);
				if (olen >= sizeof ln)
					W("Line was too long -- truncated!");

				handle_usermsg(ln);

				idle = false;
			} else if (icat_user_eof()) {
				N("EOF on the user end, queuing a QUIT");
				icat_serv_printf("QUIT :%s\r\n", g_sett.qmsg);
				ignoreuser = true;
			}
		}

		uint64_t tquit = icat_serv_sentquit();
		bool on = icat_serv_online();

		if (tquit) {
			if (!on)
				break;

			if (tquit + g_sett.waitquit_us < lsi_b_tstamp_us()) {
				N("Waitquit exceeded, breaking connection");
				break;
			}
		}

		if (!on && (!g_sett.reconnect || icat_user_eof())) {
			N("IRC offline, not going to reconnect%s.",
			    g_sett.reconnect?" (because EOF on stdin)":"");
			break;
		}

		if (nextop <= lsi_b_tstamp_us()) {
			if (!icat_serv_operate()) {
				idle = false;
				E("icat_serv_operate() failed");
				if (!on) {
					N("Delaying next connection attempt");
					nextop = lsi_b_tstamp_us()
					    + g_sett.cfwait_us;
				}
			}
		}

#ifdef __unix__
		/* on unix, we can greatly reduce system call overhead
		 * by select()ing the irc connection and stdin at the same
		 * time, with exactly the right timeout (that is, infinite
		 * or else the time until icat_serv needs attention.
		 * I'm not 100% sure that this is a legtitimate thing to do
		 * on SSL connections, but it Should Work(TM) because if
		 * we enter this branch, we have already tried to SSL_read()
		 * above (as part of irc_read()), which should have dealt
		 * with SSL's WANT_WRITE/WANT_READ twist.
		 * We might theoretically get stuck if a WANT_WRITE condition
		 * appears after the last call to icat_serv_read() and before
		 * this. But I think that can't happen (can it? We're not doing
		 * any I/O in the meantime). Still, to be completely sure,
		 * we will never use a timeout longer than 10 seconds for SSL
		 */
		if (idle && icat_serv_online()) {
			size_t fdc = 1;
			int fds[2];
			fds[0] = icat_serv_fd();
			if (!ignoreuser) {
				fds[1] = icat_user_fd();
				fdc++;
			}

			uint64_t to;
			uint64_t now = lsi_b_tstamp_us();
			if (tquit)
				to = tquit + g_sett.waitquit_us;
			else
				to = icat_serv_attention_at();

			if (to && now >= to) {
				continue; //need immediate attention?
			}

			if (to)
				to -= now;

			if (icat_serv_ssl() && to > 10000000u)
				to = 10000000u; //see comment above

			D("idle-select %zd fds with timeout %"PRIu64, fdc, to);

			if (lsi_b_select(fds, fdc, false, true, to) >= 0)
				continue;
		}
#endif

		if (idle) {
			lsi_b_usleep(100000L);
		}
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

	s_readcnt++;

	lsi_ut_ident2nick(nick, sizeof nick, (*tok)[0]);

	if (g_sett.ignore_cs
	    && !lsi_ut_istrcmp(nick, "ChanServ", icat_serv_casemap()))
		return;

	if (g_sett.trgmode)
		icat_user_printf("%s %s %s %s\n",
		    nick, (*tok)[0], (*tok)[2], (*tok)[3]);
	else if (icat_ismychan((*tok)[2]) || !g_sett.ignore_pm)
		icat_user_printf("%s\n", (*tok)[3]);
	return;
}

static void
handle_usermsg(char *lnbuf)
{
	s_writecnt++;

	char *end = lnbuf + strlen(lnbuf) - 1;
	while (end >= lnbuf && (*end == '\r' || *end == '\n'))
		*end-- = '\0';

	if (strlen(lnbuf) == 0)
		return;

	/* protocol escape */
	if (g_sett.esc && !strncmp(lnbuf, g_sett.esc, strlen(g_sett.esc))) {
		icat_serv_printf("%s", lnbuf += strlen(g_sett.esc));
		return;
	}

	char *dst = g_sett.chanlist;

	if (g_sett.trgmode) {
		char *tok = strtok(lnbuf, " ");
		dst = tok;
		lnbuf = tok + strlen(tok) + 1;
	}

	icat_serv_printf("%s %s :%s",
	    g_sett.notices?"NOTICE":"PRIVMSG", dst, lnbuf);
	return;
}

static void
dump_info(void)
{
	fprintf(stderr, "icat: %sline; read %uln wrote %uln\n",
	    icat_serv_online()?"on":"off", s_readcnt, s_writecnt);
	return;
}
