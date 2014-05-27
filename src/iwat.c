/* iwat.c - IRC netwat, the example appliwation of libsrsirc
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>

#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#include <err.h>

#include <libsrsirc/irc_ext.h>
#include <libsrsirc/util.h>

#define DEF_PORT_PLAIN 6667
#define DEF_PORT_SSL 6697

#define DEF_CONTO_SOFT_MS 15000u
#define DEF_CONTO_HARD_MS 120000u
#define DEF_CONFAILWAIT_S 10
#define DEF_VERB 1

#define W_(FNC, THR, FMT, A...) do {                              \
    if (g_sett.verb < THR) break;                                 \
    FNC("%s:%d:%s() - " FMT, __FILE__, __LINE__, __func__, ##A);  \
    } while (0)

#define W(FMT, A...) W_(warn, 1, FMT, ##A)
#define WV(FMT, A...) W_(warn, 2, FMT, ##A)

#define WX(FMT, A...) W_(warnx, 1, FMT, ##A)
#define WVX(FMT, A...) W_(warnx, 2, FMT, ##A)

#define E(FMT, A...) do { W_(warn, 0, FMT, ##A); \
    exit(EXIT_FAILURE);} while (0)

#define EX(FMT, A...) do { W_(warnx, 0, FMT, ##A); \
    exit(EXIT_FAILURE);} while (0)

static struct settings_s {
	uint64_t scto_us;
	uint64_t hcto_us;
	int cfwait_s;
	bool recon;
	int verb;
} g_sett;

static struct srvlist_s {
	char *host;
	uint16_t port;
	bool ssl;
	struct srvlist_s *next;
} *g_srvlist;

static irc g_irc;


static bool tryconnect(void);
static void process_args(int *argc, char ***argv, struct settings_s *sett);
static void init(int *argc, char ***argv, struct settings_s *sett);
static bool conread(tokarr *msg, void *tag);
static void usage(FILE *str, const char *a0, int ec);
void cleanup(void);
int main(int argc, char **argv);;


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
		WVX("trying '%s:%"PRIu16"'",
		    irc_get_host(g_irc), irc_get_port(g_irc));

		if (!irc_set_ssl(g_irc, s->ssl) && s->ssl)
			EX("failed to enable SSL");

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

	for (int ch; (ch = getopt(*argc, *argv, "n:u:f:p:P:T:W:rqvh")) != -1;) {
		switch (ch) {
		      case 'n':
			irc_set_nick(g_irc, optarg);
		break;case 'u':
			irc_set_uname(g_irc, optarg);
		break;case 'f':
			irc_set_fname(g_irc, optarg);
		break;case 'p':
			irc_set_pass(g_irc, optarg);
		break;case 'P':
			{
			char host[256];
			uint16_t port;
			int ptype;
			if (!ut_parse_pxspec(&ptype, host, sizeof host, &port,
			    optarg))
				EX("failed to parse pxspec '%s'", optarg);

			irc_set_proxy(g_irc, host, port, ptype);
			}
		break;case 'T':
			{
			char *arg = strdup(optarg);
			if (!arg)
				E("strdup failed");

			char *p = strchr(arg, ':');
			if (!p)
				sett->hcto_us = strtoull(arg, NULL, 10) * 1000u;
			else {
				*p = '\0';
				sett->hcto_us = strtoull(arg, NULL, 10) * 1000u;
				sett->scto_us = strtoull(p+1, NULL, 10) * 1000u;
			}

			WVX("connect timeout %"PRIu64"ms (s), %"PRIu64"ms (h)",
			    sett->scto_us / 1000u, sett->hcto_us / 1000u);

			free(arg);
			}
		break;case 'W':
			sett->cfwait_s = (int)strtol(optarg, NULL, 10);
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
	*argc -= optind;
	*argv += optind;
}

static void
init(int *argc, char ***argv, struct settings_s *sett)
{
	if (setvbuf(stdin, NULL, _IOLBF, 0) != 0)
		W("setvbuf stdin");

	if (setvbuf(stdout, NULL, _IOLBF, 0) != 0)
		W("setvbuf stdout");

	g_irc = irc_init();

	irc_set_nick(g_irc, "iwat");
	irc_set_uname(g_irc, "iwat");
	irc_set_fname(g_irc, "irc netwat");
	irc_set_conflags(g_irc, 0);
	irc_regcb_conread(g_irc, conread, 0);

	sett->cfwait_s = DEF_CONFAILWAIT_S;
	sett->verb = DEF_VERB;
	sett->scto_us = DEF_CONTO_SOFT_MS*1000u;
	sett->hcto_us = DEF_CONTO_HARD_MS*1000u;

	process_args(argc, argv, sett);

	if (!*argc)
		EX("no server given");

	for (int i = 0; i < *argc; i++) {
		char host[256];
		uint16_t port;
		bool ssl = false;
		ut_parse_hostspec(host, sizeof host, &port, &ssl, (*argv)[i]);

		/* we choke on all other sorts of invalid addresses/hostnames later */

		if (port == 0)
			port = ssl ? DEF_PORT_SSL : DEF_PORT_PLAIN;

		struct srvlist_s *node = malloc(sizeof *node);
		if (!node)
			E("malloc failed");

		node->host = strdup(host);
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

	WVX("initialized");
}



static bool
conread(tokarr *msg, void *tag)
{
	char buf[1024];
	ut_sndumpmsg(buf, sizeof buf, tag, msg);
	WVX("%s", buf);
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
	fprintf(str, "usage: %s [-vchtkSnufFspPTCwlL] <hostspec> "
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
	LH("(C) 2012-14, Timo Buhrmester (contact: #fstd @ irc.freenode.org)");
	#undef LH
	#undef STR
	#undef XSTR
	exit(ec);
}

void
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

}

int
main(int argc, char **argv)
{
	init(&argc, &argv, &g_sett);
	atexit(cleanup);
	bool failure = true;

	for (;;) {
		if (!irc_online(g_irc)) {
			WVX("connecting...");

			if (tryconnect())
				continue;

			WX("failed to connect/logon (%s)",
			    g_sett.recon ? "retrying" : "giving up");

			if (!g_sett.recon)
				break;

			sleep(g_sett.cfwait_s);
			continue;
		}

		tokarr tok;
		int r = irc_read(g_irc, &tok, 3000000);

		if (r == 0)
			continue;

		if (r < 0)
			break;

		if (strcmp(tok[1], "PING") == 0)
			iprintf("PONG :%s\r\n", tok[2]);
		else if (strcmp(tok[1], "PRIVMSG") == 0) {
			if (strncmp(tok[3], "ECHO ", 5) == 0) {
				char nick[64];
				ut_pfx2nick(nick, sizeof nick, tok[0]);
				iprintf("PRIVMSG %s :%s\r\n", nick, tok[3]+5);
			} else if (strcmp(tok[3], "DIE") == 0) {
				failure = false;
				break;
			}
		}
	}

	return failure ? EXIT_FAILURE : EXIT_SUCCESS;
}
