/* icat_serv.c - Server I/O (i.e. IRC) handling
 * icat - IRC netcat on top of libsrsirc - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_ICATSERV

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "icat_serv.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <platform/base_net.h>
#include <platform/base_string.h>
#include <platform/base_misc.h>
#include <platform/base_time.h>

#include <libsrsirc/irc_ext.h>
#include <libsrsirc/util.h>

#include <logger/intlog.h>

#include "icat_common.h"
#include "icat_misc.h"
#include "icat_serv.h"


struct outline_s {
	char *line;
	struct outline_s *next;
};


static irc s_irc;
static bool s_on;
static struct outline_s *s_outQ;
static uint64_t s_nexthb;
static uint64_t s_quitat;
static int s_casemap = CMAP_RFC1459;


static bool handle_PING(irc irc, tokarr *tok, size_t nargs, bool pre);
static bool handle_005(irc irc, tokarr *tok, size_t nargs, bool pre);
static int process_sendq(void);
static bool do_heartbeat(void);
static bool to_srv(const char *line);
static bool tryconnect(struct srvlist_s *s);
static bool first_connect(void);
static bool conread(tokarr *msg, void *tag);


void
serv_init(void)
{
	D("Initializing");

	if (!(s_irc = irc_init()))
		C("Failed to initialize irc object");

	irc_set_nick(s_irc, g_sett.nick);
	irc_set_uname(s_irc, g_sett.uname);
	irc_set_fname(s_irc, g_sett.fname);
	irc_set_conflags(s_irc, g_sett.conflags);
	irc_set_connect_timeout(s_irc, g_sett.cto_soft_us, g_sett.cto_hard_us);
	if (g_sett.pxtype != -1)
		irc_set_px(s_irc, g_sett.pxhost, g_sett.pxport, g_sett.pxtype);

	irc_regcb_conread(s_irc, conread, 0);
	irc_reg_msghnd(s_irc, "PING", handle_PING, true);
	irc_reg_msghnd(s_irc, "005", handle_005, false);

	if (!first_connect())
		C("Failed to establish IRC connection");

	s_on = true;

	I("Initialized");
}

void
serv_destroy(void)
{
	I("Destroying");
	if (s_irc) {
		D("Disposing of the irc object");
		irc_reset(s_irc);
		irc_dispose(s_irc);
		s_irc = NULL;
	}

	s_quitat = 0;
	s_nexthb = 0;
	s_on = false;
	s_casemap = CMAP_RFC1459;
}

bool
serv_operate(void)
{
	if (!s_on) {
		D("We're not online");

		if (s_quitat) {
			D("Not trying to connect since we're terminating");
			return true;
		}

		if (!tryconnect(g_sett.srvlist)) {
			E("Failed to reconnect/relogon");
			return false;
		}
		N("We're back online!");
		return s_on = true; //return here so we can determine logon fail
	}

	if (process_sendq() == -1) {
		E("process_sendq() failed");
		return false;
	}

	if (!do_heartbeat()) {
		E("do_heartbeat() failed");
		return false;
	}

	return true;
}

bool
serv_canread(void)
{
	if (!s_on)
		return false;

	if (irc_can_read(s_irc))
		return true;

	int r = b_select(irc_sockfd(s_irc), true, 1);

	if (r == -1)
		return false;

	V("Is %sreadable", r?"":"not ");

	if (r == 0)
		return false;

	return true;
}

int
serv_read(tokarr *t)
{
	if (!s_on)
		return -1;

	if (!serv_canread())
		return 0;

	int r = irc_read(s_irc, t, 1000000u);
	if (r == -1) {
		if (s_quitat && irc_eof(s_irc))
			N("Connection closed");
		else
			E("Unexpected %s from server",
			    irc_eof(s_irc)?"EOF":"connection reset");
		s_on = false;
	}

	return r;
}

int
serv_printf(const char *fmt, ...)
{
	char buf[1024];
	va_list l;
	va_start(l, fmt);
	int r = vsnprintf(buf, sizeof buf, fmt, l);
	va_end(l);

	struct outline_s *ptr = s_outQ;

	if (!ptr) {
		if (!(ptr = s_outQ = malloc(sizeof *s_outQ)))
			CE("malloc failed");
	} else {
		while (ptr->next)
			ptr = ptr->next;
		if (!(ptr->next = malloc(sizeof *ptr->next)))
			CE("malloc failed");
		ptr = ptr->next;
	}

	ptr->next = NULL;
	if (!(ptr->line = b_strdup(buf)))
		CE("strdup failed");

	D("Queued line '%s'", buf);
	return r;
}

bool
serv_online(void)
{
	return s_on;
}

int
serv_casemap(void)
{
	return s_casemap;
}

uint64_t
serv_sentquit(void)
{
	return s_quitat;
}


static bool
handle_PING(irc irc, tokarr *tok, size_t nargs, bool pre)
{
	char buf[1024];
	snprintf(buf, sizeof buf, "PONG :%s\r\n", (*tok)[2]);
	if (!to_srv(buf))
		return s_on = false;

	return true;

}

static bool
handle_005(irc irc, tokarr *tok, size_t nargs, bool pre)
{
	int cm = irc_casemap(s_irc);
	if (cm != s_casemap) {
		D("Casemapping changed from %s to %s",
		    ut_casemap_nam(s_casemap), ut_casemap_nam(cm));
		s_casemap = cm;
	}
	return true;
}

static int
process_sendq(void)
{
	static uint64_t nextsend = 0;
	if (!s_outQ)
		return 0;

	if (g_sett.freelines > 0 || nextsend <= b_tstamp_us()) {
		D("SendQ time");
		struct outline_s *ptr = s_outQ;
		if (!to_srv(ptr->line)) {
			s_on = false;
			return -1;
		}
		s_outQ = s_outQ->next;
		if (b_strncasecmp(ptr->line, "QUIT", 4) == 0) {
			I("QUIT seen, forcing d/c in %"PRIu64" seconds",
			    g_sett.waitquit_us / 1000000u);
			s_quitat = b_tstamp_us();
		}

		free(ptr->line);
		free(ptr);
		if (g_sett.freelines)
			g_sett.freelines--;
		nextsend = b_tstamp_us() + g_sett.linedelay;

		return 1;
	}

	return 0;
}

static bool
do_heartbeat(void)
{
	if (g_sett.hbeat_us && s_nexthb <= b_tstamp_us()) {
		D("Heartbeat time");
		char buf[512];
		snprintf(buf, sizeof buf, "PING %s\r\n", irc_myhost(s_irc));
		s_nexthb = b_tstamp_us() + g_sett.hbeat_us;
		if (!to_srv(buf))
			return s_on = false;
	}

	return true;
}

static bool
to_srv(const char *line)
{
	bool ret;
	I("To server: '%s'", line);
	if (!(ret = irc_write(s_irc, line)))
		E("irc_write failed on '%s'", line);

	return ret;
}

static bool
tryconnect(struct srvlist_s *s)
{
	I("Trying to get a connection going...");
	while (s) {
		irc_set_server(s_irc, s->host, s->port);

		if (s->ssl) {
			if (!irc_set_ssl(s_irc, true))
				C("Failed to enable SSL");
			D("SSL selected");
		} else
			irc_set_ssl(s_irc, false);

		I("Next server: '%s:%"PRIu16"' (%sSSL), calling irc_connect()",
		    irc_get_host(s_irc), irc_get_port(s_irc), s->ssl?"":"no ");

		if (irc_connect(s_irc)) {
			I("Logged on, %sjoining channel(s)",
			    g_sett.nojoin?"NOT ":"");

			s_nexthb = b_tstamp_us() + g_sett.hbeat_us;

			if (!g_sett.nojoin) {
				char jmsg[512];
				snprintf(jmsg, sizeof jmsg, "JOIN %s %s\r\n",
				    g_sett.chanlist, g_sett.keylist);
				to_srv(jmsg);
			}
			return true;
		} else
			W("Failed to connect/logon to %s:%"PRIu16,
			    s->host, s->port);

		s = s->next;
	}

	E("No more servers remaining, giving up");

	return false;
}

static bool
first_connect(void)
{
	while (!g_interrupted) {
		if (!tryconnect(g_sett.srvlist)) {
			E("Failed to connect/logon (%s)",
			    g_sett.keeptrying ?"retrying":"giving up");

			if (g_sett.keeptrying) {
				N("Sleeping for %"PRIu64" seconds",
				    g_sett.cfwait_us / 1000000u);
				b_usleep(g_sett.cfwait_us);
				continue;
			}

			return false;
		}
		break;
	}

	if (g_interrupted)
		N("interrupted by user");

	return !g_interrupted;
}

static bool
conread(tokarr *msg, void *tag)
{
	char buf[1024];
	ut_sndumpmsg(buf, sizeof buf, tag, msg);
	I("CR: %s", buf);
	return true;
}
