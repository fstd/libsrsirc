/* icat_init.c - Parse command line, initialize icat core
 * icat - IRC netcat on top of libsrsirc - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_INIT

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <inttypes.h>
#include <signal.h>
#include <unistd.h>

#include <libsrsirc/irc_ext.h>
#include <libsrsirc/util.h>

#include "icat_common.h"
#include "icat_core.h"
#include "icat_debug.h"
#include "icat_misc.h"
#include "icat_serv.h"


struct settings_s g_sett;
bool g_interrupted = false;


static void process_args(int *argc, char ***argv);
static void usage(FILE *str, const char *a0, int ec, bool sh);
static void set_defaults(void);
static void dump_settings(void);
static void cleanup(void);
static void sighnd(int s);

static void
process_args(int *argc, char ***argv)
{
	char *a0 = (*argv)[0];

	for (int ch; (ch = getopt(*argc, *argv,
	    "vqchHn:u:f:F:p:P:tT:C:kw:l:L:b:W:rNjiIE:")) != -1;) {
		switch (ch) {
		      case 'n':
			STRACPY(g_sett.nick, optarg);
		break;case 'u':
			STRACPY(g_sett.uname, optarg);
		break;case 'f':
			STRACPY(g_sett.fname, optarg);
		break;case 'F':
			g_sett.conflags = strtou8(optarg, NULL, 10);
		break;case 'p':
			STRACPY(g_sett.pass, optarg);
		break;case 'P':
			if (!ut_parse_pxspec(&g_sett.pxtype, g_sett.pxhost,
			    sizeof g_sett.pxhost, &g_sett.pxport, optarg))
				C("failed to parse pxspec '%s'", optarg);
		break;case 't':
			g_sett.trgmode = true;
		break;case 'T':
			{
			char *arg = strdup(optarg);
			if (!arg)
				CE("strdup failed");

			char *ptr = strchr(arg, ':');
			if (!ptr)
				g_sett.conto_hard_us = strtou64(arg, NULL, 10) * 1000ul;
			else {
				*ptr = '\0';
				g_sett.conto_hard_us = strtou64(arg, NULL, 10) * 1000ul;
				g_sett.conto_soft_us = strtou64(ptr+1, NULL, 10) * 1000ul;
			}

			free(arg);
			}
		break;case 'C':
			{
			char *str = strdup(optarg);
			if (!str)
				CE("strdup failed");
			char *tok = strtok(str, " ");
			bool first = true;
			do {
				char *p = strchr(tok, ',');
				if (!first) {
					STRACAT(g_sett.chanlist, ",");
					STRACAT(g_sett.keylist, ",");
				}
				first = false;
				if (p) {
					*p = '\0';
					STRACAT(g_sett.chanlist, tok);
					STRACAT(g_sett.keylist, p+1);
					*p = ',';
				} else {
					STRACAT(g_sett.chanlist, tok);
					STRACAT(g_sett.keylist, "x");
				}
			} while ((tok = strtok(NULL, " ")));
			free(str);
			}
		break;case 'E':
			if (!(g_sett.esc = strdup(optarg)))
				CE("strdup failed");
		break;case 'l':
			g_sett.linedelay = strtou64(optarg, NULL, 10) * 1000u;
		break;case 'L':
			g_sett.freelines = (unsigned)strtoul(optarg, NULL, 10);
		break;case 'w':
			g_sett.waitquit_us = strtou64(optarg, NULL, 10) * 1000u;
		break;case 'k':
			g_sett.keeptrying = true;
		break;case 'W':
			g_sett.confailwait_us = strtou64(optarg, NULL, 10) * 1000u;
		break;case 'b':
			g_sett.heartbeat_us = strtou64(optarg, NULL, 10) * 1000u;
		break;case 'r':
			g_sett.reconnect = true;
		break;case 'j':
			g_sett.nojoin = true;
		break;case 'I':
			g_sett.ignore_cs = false;
		break;case 'i':
			g_sett.ignore_pm = false;
		break;case 'N':
			g_sett.notices = true;
		break;case 'c':
			log_setfancy(true);
		break;case 'v':
			log_setlvl(-1, log_getlvl(-1) + 1);
		break;case 'q':
			if (log_getlvl(-1) > 0)
				log_setlvl(-1, log_getlvl(-1) - 1);
		break;case 'H':
			usage(stdout, a0, EXIT_SUCCESS, false);
		break;case 'h':
			usage(stdout, a0, EXIT_SUCCESS, true);
		break;case '?':default:
			usage(stderr, a0, EXIT_FAILURE, true);
		}
	}
	*argc -= optind;
	*argv += optind;

	if (!*argc)
		C("no server given");

	for (int i = 0; i < *argc; i++) {
		char host[256];
		uint16_t port;
		bool ssl = false;
		ut_parse_hostspec(host, sizeof host, &port, &ssl, (*argv)[i]);

		if (isdigitstr(host)) /* catch netcat and telnet invocation syntax */
			C("invalid server specified (use 'srv:port' instead of 'srv port')");

		/* we choke on all other sorts of invalid addresses/hostnames later */

		struct srvlist_s *node = malloc(sizeof *node);
		if (!node)
			CE("malloc failed");

		node->host = strdup(host);
		node->port = port;
		node->ssl = ssl;
		node->next = NULL;

		if (!g_sett.srvlist)
			g_sett.srvlist = node;
		else {
			struct srvlist_s *n = g_sett.srvlist;
			while (n->next)
				n = n->next;
			n->next = node;
		}
	}
}

static void
usage(FILE *str, const char *a0, int ec, bool sh)
{
	#define XSTR(s) STR(s)
	#define STR(s) #s
	#define SH(STR) if (sh) fputs(STR "\n", str)
	#define LH(STR) if (!sh) fputs(STR "\n", str)
	#define BH(STR) fputs(STR "\n", str)
	BH("=============================");
	BH("== icat "PACKAGE_VERSION" - IRC netcat ==");
	BH("=============================");
	fprintf(str, "usage: %s [-vchtkSnufFspPTCwlL] <hostspec> "
	    "[<hostspec> ...]\n", a0);
	BH("");
	BH("\t<hostspec> specifies an IRC server to connect against");
	BH("\t           multiple hostspecs may be given.");
	LH("\t\thostspec := srvaddr[:port]['/ssl']");
	LH("\t\tsrvaddr  := ip4addr|ip6addr|dnsname");
	LH("\t\tport     := int(0..65535)");
	LH("\t\tip4addr  := 'aaa.bbb.ccc.ddd'");
	LH("\t\tip6addr  :=  '[aa:bb:cc::dd:ee]'");
	LH("\t\tdnsname  := 'irc.example.org'");
	BH("");
	BH("\t-v: Increase verbosity on stderr (use -vv or -vvv for more)");
	BH("\t-q: Decrease verbosity on stderr (use -qq or -qqq for less)");
	BH("\t-c: Use Bash color sequences on stderr");
	LH("\t-h: Display brief usage statement and terminate");
	BH("\t-H: Display longer usage statement and terminate");
	BH("\t-t: Enable target-mode. See man page for more information");
	LH("\t-k: Keep trying to connect, if first connect/logon fails");
	BH("\t-r: Reconnect on disconnect, rather than terminating");
	LH("\t-N: Use NOTICE instead of PRIVMSG for messages");
	LH("\t-j: Do not join channel given by -C");
	LH("\t-i: Don't ignore non-channel messages");
	LH("\t-I: Don't ignore messages from ChanServ");
	LH("");
	BH("\t-n <str>: Use <str> as nick. Subject to mutilation if N/A");
	LH("\t-u <str>: Use <str> as (IRC) username/ident");
	LH("\t-f <str>: Use <str> as (IRC) fullname");
	LH("\t-F <int>: Specify USER flags. 0=None, 8=usermode +i");
	LH("\t-W <int>: Wait <int> ms between attempts to connect."
	    "[def: "XSTR(DEF_CONFAILWAIT_MS)"]");
	LH("\t-b <int>: Send heart beat PING every <int> ms."
	    "[def: "XSTR(DEF_HEARTBEAT_MS)"]");
	BH("\t-p <str>: Use <str> as server password");
	fprintf(str, "\t-P <pxspec>: Use <pxspec> as proxy. "
	    "See %s for format\n", sh?"man page":"below");
	BH("\t-T <int>[:<int>]: Connect/Logon hard[:soft]-timeout in ms."
	    "[def: "XSTR(DEF_CONTO_HARD_MS)":"XSTR(DEF_CONTO_SOFT_MS)"]");
	fprintf(str, "\t-C <chanlst>: List of chans to join + keys. "
	    "See %s for format\n", sh?"man page":"below");
	BH("\t-w <int>: wait <int> ms for server disconnect after QUIT."
	    "[def: "XSTR(DEF_WAITQUIT_MS)"]");
	BH("\t-l <int>: Wait <int> ms between sending messages to IRC."
	    "[def: "XSTR(DEF_LINEDELAY_MS)"]");
	BH("\t-L <int>: Number of 'first free' lines to send unthrottled."
	    "[def: "XSTR(DEF_FREELINES)"]");
	BH("\t-E <str>: Escape prefix. Send lines prefixed w/ this as-is. [def: none]");
	LH("");
	LH("\t<pxspec> specifies a proxy and mostly works like hostspec:");
	LH("\t\tpxspec   := pxtype:hostspec");
	LH("\t\tpxtype   := 'HTTP' | 'SOCKS4' | 'SOCKS5'");
	LH("");
	LH("\t<chanlst> is a space-separated list of '#channel,key'-pairs");
	LH("\t\tchanlist := (channel[,key] )*");
	LH("\t\tchannel  := '#example'");
	LH("\t\tkey      := 'verysecret'");
	SH("!! Try -H for a more complete usage statement. !!");
	BH("");
	BH("(C) 2012-14, Timo Buhrmester (contact: #fstd @ irc.freenode.org)");
	#undef SH
	#undef LH
	#undef BH
	#undef STR
	#undef XSTR
	exit(ec);
}

static void
set_defaults(void)
{
	STRACPY(g_sett.nick, "icat");
	STRACPY(g_sett.uname, "icat");
	STRACPY(g_sett.fname, "IRC netcat");
	g_sett.conflags = 0;
	g_sett.pass[0] = '\0';
	g_sett.pxhost[0] = '\0';
	g_sett.pxport = 0;
	g_sett.pxtype = -1;
	g_sett.trgmode = false;
	g_sett.conto_soft_us = DEF_CONTO_SOFT_MS*1000u;
	g_sett.conto_hard_us = DEF_CONTO_HARD_MS*1000u;
	g_sett.linedelay = DEF_LINEDELAY_MS*1000u;
	g_sett.freelines = DEF_FREELINES;
	g_sett.waitquit_us = DEF_WAITQUIT_MS*1000u;
	g_sett.keeptrying = false;
	g_sett.confailwait_us = DEF_CONFAILWAIT_MS*1000u;
	g_sett.heartbeat_us = DEF_HEARTBEAT_MS*1000u;
	g_sett.reconnect = false;
	g_sett.nojoin = false;
	g_sett.ignore_pm = DEF_IGNORE_PM;
	g_sett.ignore_cs = DEF_IGNORE_CHANSERV;
	g_sett.notices = false;
	g_sett.chanlist[0] = '\0';
	g_sett.keylist[0] = '\0';
	g_sett.esc = NULL;
}

static void
dump_settings(void)
{
	D("nick: '%s'", g_sett.nick);
	D("uname: '%s'", g_sett.uname);
	D("fname: '%s'", g_sett.fname);
	D("conflags: %"PRIu8, g_sett.conflags);
	D("pass: '%s'", g_sett.pass);
	D("pxhost: '%s'", g_sett.pxhost);
	D("pxport: %"PRIu16, g_sett.pxport);
	D("pxtype: %d", g_sett.pxtype);
	D("trgmode: %s", g_sett.trgmode?"yes":"no");
	D("conto_soft_us: %"PRIu64, g_sett.conto_soft_us);
	D("conto_hard_us: %"PRIu64, g_sett.conto_hard_us);
	D("linedelay: %"PRIu64, g_sett.linedelay);
	D("freelines: %u", g_sett.freelines);
	D("waitquit_us: %"PRIu64, g_sett.waitquit_us);
	D("keeptrying: %s", g_sett.keeptrying?"yes":"no");
	D("confailwait_us: %"PRIu64, g_sett.confailwait_us);
	D("heartbeat_us: %"PRIu64, g_sett.heartbeat_us);
	D("reconnect: %s", g_sett.reconnect?"yes":"no");
	D("nojoin: %s", g_sett.nojoin?"yes":"no");
	D("ignore_pm: %s", g_sett.ignore_pm?"yes":"no");
	D("ignore_cs: %s", g_sett.ignore_cs?"yes":"no");
	D("notices: %s", g_sett.notices?"yes":"no");
	D("chanlist: '%s'", g_sett.chanlist);
	D("keylist: '%s'", g_sett.keylist);
	D("esc: '%s'", g_sett.esc);

	struct srvlist_s *s = g_sett.srvlist;
	while (s) {
		D("Server '%s:%"PRIu16" (%sssl)",
		    s->host, s->port, s->ssl?"":"no ");
		s = s->next;
	}
}

static void
cleanup(void)
{
	I("Cleaning up");
	core_destroy();

	/* make valgrind happy */
	struct srvlist_s *n = g_sett.srvlist;
	while (n) {
		struct srvlist_s *next = n->next;
		free(n->host);
		free(n);
		n = next;
	}
	I("Cleanup done");
}

static void
sighnd(int s)
{
	g_interrupted = true;
}


int
main(int argc, char **argv)
{
	char *p = strrchr(argv[0], '/');
	log_setprgnam(p ? p + 1 : argv[0]);
	log_regmod(MOD_INIT, "INIT");
	log_regmod(MOD_CORE, "CORE");
	log_regmod(MOD_SERV, "SERV");
	log_regmod(MOD_USER, "USER");
	log_regmod(MOD_MISC, "MISC");

	set_defaults();
	process_args(&argc, &argv);

	if (!g_sett.trgmode && strlen(g_sett.chanlist) == 0)
		C("no targetmode and no chans given. this won't work.");

	signal(SIGINT, sighnd);

	dump_settings();

	core_init();

	if (atexit(cleanup) == -1)
		EE("atexit");

	exit(core_run());
}
