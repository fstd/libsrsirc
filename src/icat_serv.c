/* icat_serv.c - Server I/O (i.e. IRC) handling
 * icat - IRC netcat on top of libsrsirc - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_SERV

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "icat_serv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>
#include <sys/select.h>
#include <sys/time.h>

#include <libsrsirc/irc_ext.h>
#include <libsrsirc/util.h>

#include "icat_common.h"
#include "icat_debug.h"
#include "icat_misc.h"
#include "icat_serv.h"


static struct outline_s {
	char *line;
	struct outline_s *next;
} *s_outQ;

static uint64_t s_nexthb;
static irc s_irc;
static bool s_on;
static int s_casemap = CMAP_RFC1459;
static uint64_t s_quitat;

static bool first_connect(void);
static bool tryconnect(struct srvlist_s *s);
static bool conread(tokarr *msg, void *tag);
static int process_sendq(void);
static bool do_heartbeat(void);
static bool handle_PING(irc irc, tokarr *tok, size_t nargs, bool pre);
static bool handle_005(irc irc, tokarr *tok, size_t nargs, bool pre);

void
serv_init(void)
{
	if (!(s_irc = irc_init()))
		C("failed to initialize irc object");

	irc_set_nick(s_irc, g_sett.nick);
	irc_set_uname(s_irc, g_sett.uname);
	irc_set_fname(s_irc, g_sett.fname);
	irc_set_conflags(s_irc, g_sett.conflags);
	if (g_sett.pxtype != -1)
		irc_set_px(s_irc, g_sett.pxhost, g_sett.pxport, g_sett.pxtype);
	irc_regcb_conread(s_irc, conread, 0);
	irc_reg_msghnd(s_irc, "PING", handle_PING, true);
	irc_reg_msghnd(s_irc, "005", handle_005, false);

	irc_set_connect_timeout(s_irc, g_sett.conto_soft_us, g_sett.conto_hard_us);

	if (!first_connect())
		C("failed to establish IRC connection");

	s_on = true;

	I("Serv initialized");
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

void
serv_destroy(void)
{
	if (s_irc) {
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
serv_online(void)
{
	return s_on;
}

bool
serv_operate(void)
{
	if (!s_on) {
		D("connecting...");

		if (!tryconnect(g_sett.srvlist)) {
			E("failed to reconnect/relogon");
			return false;
		}
		N("logged on!");
		return s_on = true; //return here so we can determine logon fail
	}

	if (process_sendq() == -1) {
		E("process_sendq() failed");
		return false;
	}

	if (do_heartbeat() == -1) {
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

	int isck = irc_sockfd(s_irc);

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(isck, &fds);

	struct timeval tout = {0, 0};
	int r = select(isck+1, &fds, NULL, NULL, &tout);

	if (r == -1) {
		EE("select failed");
		return false;
	}

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
		E("irc read failed");
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
	if (!(ptr->line = strdup(buf)))
		CE("strdup failed");
	return r;
}

static bool
handle_PING(irc irc, tokarr *tok, size_t nargs, bool pre)
{
	char buf[1024];
	snprintf(buf, sizeof buf, "PONG :%s\r\n", (*tok)[2]);
	if (!irc_write(irc, buf)) {
		E("irc_write failed on '%s'", buf);
		return s_on = false;
	}

	return true;

}

static bool
handle_005(irc irc, tokarr *tok, size_t nargs, bool pre)
{
	s_casemap = irc_casemap(s_irc);
	return true;
}

static bool
first_connect(void)
{
	for (;;) {
		D("connecting...");

		if (!tryconnect(g_sett.srvlist)) {
			E("failed to connect/logon (%s)",
			    g_sett.keeptrying ?"retrying":"giving up");

			if (g_sett.keeptrying) {
				N("delaying next connection attempt");
				sleep_us(g_sett.confailwait_us);
				continue;
			}

			return false;
		}
		D("logged on!");
		break;
	}

	return true;
}

static int
process_sendq(void)
{
	static uint64_t nextsend = 0;
	if (!s_outQ)
		return 0;

	if (g_sett.freelines > 0 || nextsend <= timestamp_us()) {
		struct outline_s *ptr = s_outQ;
		I("to server: %s", ptr->line);
		if (!irc_write(s_irc, ptr->line)) {
			E("irc_write failed on '%s'", ptr->line);
			s_on = false;
			return -1;
		}
		s_outQ = s_outQ->next;
		if (strncasecmp(ptr->line, "QUIT", 4) == 0)
			s_quitat = timestamp_us();
		free(ptr->line);
		free(ptr);
		if (g_sett.freelines)
			g_sett.freelines--;
		nextsend = timestamp_us() + g_sett.linedelay;

		return 1;
	}

	return 0;
}

static bool
do_heartbeat(void)
{
	if (g_sett.heartbeat_us && s_nexthb <= timestamp_us()) {
		char buf[512];
		snprintf(buf, sizeof buf, "PING %s\r\n", irc_myhost(s_irc));
		s_nexthb = timestamp_us() + g_sett.heartbeat_us;
		D("sending heartbeat");
		if (!irc_write(s_irc, buf)) {
			E("irc_write failed on '%s'", buf);
			s_on = false;
			return false;
		}
	}

	return true;
}

static bool
tryconnect(struct srvlist_s *s)
{
	while (s) {
		irc_set_server(s_irc, s->host, s->port);
		D("set server to '%s:%"PRIu16"'", irc_get_host(s_irc),
		    irc_get_port(s_irc));

		if (s->ssl) {
#ifdef WITH_SSL
			if (!irc_set_ssl(s_irc, true))
				C("failed to enable SSL");
#else
			C("we haven't been compiled with SSL support...");
#endif
			D("SSL selected");
		} else
			irc_set_ssl(s_irc, false);

		if (irc_connect(s_irc)) {
			if (!g_sett.nojoin) {
				char jmsg[512];
				snprintf(jmsg, sizeof jmsg, "JOIN %s %s\r\n",
				    g_sett.chanlist, g_sett.keylist);
				irc_write(s_irc, jmsg);
			}
			return true;
		} else
			D("failed to connect to %s:%"PRIu16, s->host, s->port);

		s = s->next;
	}

	return false;
}

static bool
conread(tokarr *msg, void *tag)
{
	char buf[1024];
	ut_sndumpmsg(buf, sizeof buf, tag, msg);
	D("%s", buf);
	return true;
}
