/* iwat.c - IRC netwat, the example appliwation of libsrsirc
 * iwat - IRC netwat on top of libsrsirc - (C) 2012-2024, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_IWAT

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <platform/base_misc.h>
#include <platform/base_string.h>
#include <platform/base_time.h>

#include <logger/intlog.h>

#include <libsrsirc/irc_ext.h>
#include <libsrsirc/irc_track.h>
#include <libsrsirc/util.h>

#define DEF_CONTO_SOFT_MS 15000u
#define DEF_CONTO_HARD_MS 120000u
#define DEF_CONFAILWAIT_S 10
#define DEF_HEARTBEAT_MS 300000
#define DEF_VERB 1


static struct settings_s {
	uint64_t scto_us;
	uint64_t hcto_us;
	int cfwait_s;
	uint64_t heartbeat_us;
	bool recon;
	int verb;
} g_sett;

static struct srvlist_s {
	char *host;
	uint16_t port;
	bool ssl;
	struct srvlist_s *next;
} *g_srvlist;

static irc *g_irc;
static uint64_t g_nexthb;
static bool g_dumpplx = 0;



static bool tryconnect(void);
static void process_args(int *argc, char ***argv, struct settings_s *sett);
static void init(int *argc, char ***argv, struct settings_s *sett);
static bool conread(tokarr *msg, void *tag);
static void usage(FILE *str, const char *a0, int ec);
static void cleanup(void);
int main(int argc, char **argv);


static int
iprintf(const char *fmt, ...)
{
	char buf[1024];
	va_list l;
	va_start(l, fmt);
	int r = vsnprintf(buf, sizeof buf, fmt, l);
	va_end(l);
	return !irc_write(g_irc, buf) ? -1 : r;
}

static bool
tryconnect(void)
{
	struct srvlist_s *s = g_srvlist;

	while (s) {
		irc_set_server(g_irc, s->host, s->port);
		D("trying '%s:%"PRIu16"'",
		    irc_get_host(g_irc), irc_get_port(g_irc));

		if (!irc_set_ssl(g_irc, s->ssl) && s->ssl)
			C("failed to enable SSL");

		if (irc_connect(g_irc))
			return true;

		s = s->next;
	}

	return false;
}

static void
process_args(int *argc, char ***argv, struct settings_s *sett)
{
	char *a0 = (*argv)[0];
	const char *optstr = "n:u:f:p:P:T:W:rb:qvh";

	for (int ch; (ch = lsi_b_getopt(*argc, *argv, optstr)) != -1;) {
		switch (ch) {
		      case 'n':
			irc_set_nick(g_irc, lsi_b_optarg());
		break;case 'u':
			irc_set_uname(g_irc, lsi_b_optarg());
		break;case 'f':
			irc_set_fname(g_irc, lsi_b_optarg());
		break;case 'p':
			irc_set_pass(g_irc, lsi_b_optarg());
		break;case 'P':
			{
			char host[256];
			uint16_t port;
			int ptype;
			if (!lsi_ut_parse_pxspec(&ptype, host, sizeof host,
			    &port, lsi_b_optarg()))
				C("failed to parse proxy '%s'", lsi_b_optarg());

			irc_set_px(g_irc, host, port, ptype);
			}
		break;case 'T':
			{
			char *arg = STRDUP(lsi_b_optarg());
			if (!arg)
				CE("strdup failed");

			char *p = strchr(arg, ':');
			if (!p)
				sett->hcto_us = strtoull(arg, NULL, 10) * 1000u;
			else {
				*p = '\0';
				sett->hcto_us = strtoull(arg, NULL, 10) * 1000u;
				sett->scto_us = strtoull(p+1, NULL, 10) * 1000u;
			}

			D("connect timeout %"PRIu64"ms (s), %"PRIu64"ms (h)",
			    sett->scto_us / 1000u, sett->hcto_us / 1000u);

			free(arg);
			}
		break;case 'W':
			sett->cfwait_s = (int)strtol(lsi_b_optarg(), NULL, 10);
		break;case 'b':
			sett->heartbeat_us =
			    strtoull(lsi_b_optarg(), NULL, 10) * 1000u;
		break;case 'r':
			sett->recon = true;
		break;case 'q':
			sett->verb--;
		break;case 'v':
			sett->verb++;
		break;case 'h':
			usage(stdout, a0, EXIT_SUCCESS);
		break;case '?':default:
			usage(stderr, a0, EXIT_FAILURE);
		}
	}
	*argc -= lsi_b_optind();
	*argv += lsi_b_optind();
	return;
}

static void
init(int *argc, char ***argv, struct settings_s *sett)
{
	if (setvbuf(stdin, NULL, _IOLBF, 0) != 0)
		WE("setvbuf stdin");

	if (setvbuf(stdout, NULL, _IOLBF, 0) != 0)
		WE("setvbuf stdout");

	g_irc = irc_init();

	irc_set_track(g_irc, true);

	irc_set_nick(g_irc, "iwat");
	irc_set_uname(g_irc, "iwat");
	irc_set_fname(g_irc, "irc netwat");
	irc_set_conflags(g_irc, 0);
	irc_regcb_conread(g_irc, conread, 0);

	sett->heartbeat_us = DEF_HEARTBEAT_MS * 1000u;
	sett->cfwait_s = DEF_CONFAILWAIT_S;
	sett->verb = DEF_VERB;
	sett->scto_us = DEF_CONTO_SOFT_MS * 1000u;
	sett->hcto_us = DEF_CONTO_HARD_MS * 1000u;

	process_args(argc, argv, sett);

	if (!*argc)
		C("no server given");

	for (int i = 0; i < *argc; i++) {
		char host[256];
		uint16_t port;
		bool ssl = false;
		lsi_ut_parse_hostspec(host, sizeof host, &port, &ssl, (*argv)[i]);

		/* we choke on all other sorts of invalid addrs/hosts later */

		struct srvlist_s *node = MALLOC(sizeof *node);
		if (!node)
			CE("malloc failed");

		node->host = STRDUP(host);
		node->port = port;
		node->ssl = ssl;
		node->next = NULL;

		if (!g_srvlist)
			g_srvlist = node;
		else {
			struct srvlist_s *n = g_srvlist;
			while (n->next)
				n = n->next;
			n->next = node;
		}
	}

	irc_set_connect_timeout(g_irc, g_sett.scto_us, g_sett.hcto_us);

	D("initialized");
	return;
}



static bool
conread(tokarr *msg, void *tag)
{
	char buf[1024];
	lsi_ut_sndumpmsg(buf, sizeof buf, tag, msg);
	D("%s", buf);
	return true;
}


static void
usage(FILE *str, const char *a0, int ec)
{
	#define XSTR(s) STR(s)
	#define STR(s) #s
	#define LH(STR) fputs(STR "\n", str)
	LH("==========================");
	LH("== iwat "PACKAGE_VERSION" - IRC wat ==");
	LH("==========================");
	fprintf(str, "usage: %s [-vchtkSnufFspPTCwlLb] <hostspec> "
	    "[<hostspec> ...]\n", a0);
	LH("");
	LH("\t<hostspec> specifies an IRC server to connect against");
	LH("\t           multiple hostspecs may be given.");
	LH("\t\thostspec := srvaddr[:port]['/ssl']");
	LH("\t\tsrvaddr  := ip4addr|ip6addr|dnsname");
	LH("\t\tport     := int(0..65535)");
	LH("\t\tip4addr  := 'aaa.bbb.ccc.ddd'");
	LH("\t\tip6addr  :=  '[aa:bb:cc::dd:ee]'");
	LH("\t\tdnsname  := 'irc.example.org'");
	LH("");
	LH("\t-v: Increase verbosity on stderr (use -vv or -vvv for more)");
	LH("\t-q: Decrease verbosity on stderr (use -qq or -qqq for less)");
	LH("\t-h: Display usage statement and terminate");
	LH("\t-k: Keep trying to connect, if first connect/logon fails");
	LH("\t-r: Reconnect on disconnect, rather than terminating");
	LH("");
	LH("\t-n <str>: Use <str> as nick. Subject to mutilation if N/A");
	LH("\t-u <str>: Use <str> as (IRC) username/ident");
	LH("\t-f <str>: Use <str> as (IRC) fullname");
	LH("\t-b <int>: Send heart beat PING every <int> ms."
	    "[def: "XSTR(DEF_HEARTBEAT_MS)"]");
	LH("\t-W <int>: Wait <int> seconds between attempts to connect."
	    "[def: "XSTR(DEF_CONFAILWAIT_S)"]");
	LH("\t-p <str>: Use <str> as server password");
	LH("\t-P <pxspec>: Use <pxspec> as proxy. See below for format.");
	LH("\t-T <int>[:<int>]: Connect/Logon hard[:soft]-timeout in ms."
	    "[def: "XSTR(DEF_CONTO_HARD_MS)":"XSTR(DEF_CONTO_SOFT_MS)"]");
	LH("");
	LH("\t<pxspec> specifies a proxy and mostly works like hostspec:");
	LH("\t\tpxspec   := pxtype:hostspec");
	LH("\t\tpxtype   := 'HTTP' | 'SOCKS4' | 'SOCKS5'");
	LH("");
	LH("(C) 2012-2024, Timo Buhrmester (contact: #fstd @ irc.libera.chat)");
	#undef LH
	#undef STR
	#undef XSTR
	exit(ec);
}

static void
cleanup(void)
{
	if (g_irc)
		irc_dispose(g_irc);

	/* make valgrind happy */
	struct srvlist_s *n = g_srvlist;
	while (n) {
		struct srvlist_s *next = n->next;
		free(n->host);
		free(n);
		n = next;
	}

	return;
}

static void
infohnd(int s)
{
	g_dumpplx = true;
	return;
}

int
main(int argc, char **argv)
{
	init(&argc, &argv, &g_sett);
	atexit(cleanup);
#if HAVE_SIGINFO
	lsi_b_regsig(SIGINFO, infohnd);
#elif HAVE_SIGUSR1
	lsi_b_regsig(SIGUSR1, infohnd);
#endif
	bool failure = true;

	for (;;) {
		if (!irc_online(g_irc)) {
			D("connecting...");

			if (tryconnect()) {
				g_nexthb =
				    lsi_b_tstamp_us() + g_sett.heartbeat_us;
				continue;
			}

			W("failed to connect/logon (%s)",
			    g_sett.recon ? "retrying" : "giving up");

			if (!g_sett.recon)
				break;

			lsi_b_usleep(1000000ul * g_sett.cfwait_s);
			continue;
		}

		tokarr tok;
		uint64_t to = g_sett.heartbeat_us;
		int r = irc_read(g_irc, &tok, to ? to : 1000000);

		if (r < 0)
			break;

		if (g_dumpplx) {
			irc_dump(g_irc);
			if (irc_tracking_enab(g_irc))
				lsi_trk_dump(g_irc, false);
			g_dumpplx = false;
		}

		if (r > 0)
			g_nexthb = lsi_b_tstamp_us() + g_sett.heartbeat_us;
		else if (g_sett.heartbeat_us && g_nexthb <= lsi_b_tstamp_us()) {
			iprintf("PING %s\r\n", irc_myhost(g_irc));
			g_nexthb = lsi_b_tstamp_us() + g_sett.heartbeat_us;
		}

		if (r == 0)
			continue;

		if (strcmp(tok[1], "PING") == 0)
			iprintf("PONG :%s\r\n", tok[2]);
		else if (strcmp(tok[1], "PRIVMSG") == 0) {
			if (strncmp(tok[3], "ECHO ", 5) == 0) {
				char nick[64];
				lsi_ut_ident2nick(nick, sizeof nick, tok[0]);
				iprintf("PRIVMSG %s :%s\r\n", nick, tok[3]+5);
			} else if (strncmp(tok[3], "DO ", 3) == 0) {
				iprintf("%s\r\n", tok[3]+3);
			} else if (strcmp(tok[3], "DIE") == 0) {
				failure = false;
				break;
			} else if (strcmp(tok[3], "DUMP") == 0) {
				irc_dump(g_irc);
				lsi_trk_dump(g_irc, false);
			} else if (strcmp(tok[3], "FULLDUMP") == 0) {
				irc_dump(g_irc);
				lsi_trk_dump(g_irc, true);
			}
		}
	}

	return failure ? EXIT_FAILURE : EXIT_SUCCESS;
}
