/* icat.c - IRC netcat, the example application of libsrsirc
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
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
#include <errno.h>

#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#include <err.h>

#include <libsrsirc/irc_ext.h>
#include <libsrsirc/util.h>

#define DEF_PORT_PLAIN 6667
#define DEF_PORT_SSL 6697
#define MAX_CHANS 32
#define MAX_CHANLIST 512

#define DEF_CONTO_SOFT_MS 15000u
#define DEF_CONTO_HARD_MS 120000u
#define DEF_LINEDELAY_MS 2000u
#define DEF_FREELINES 0u
#define DEF_WAITQUIT_MS 3000u
#define DEF_CONFAILWAIT_MS 10000u
#define DEF_HEARBEAT_MS 0
#define DEF_IGNORE_PM true
#define DEF_IGNORE_CHANSERV true
#define DEF_VERB 1

#define STRACAT(DSTARR, STR) strNcat((DSTARR), (STR), sizeof (DSTARR))

#define W_(FNC, THR, FMT, A...) do {                                  \
    if (g_sett.verb < THR) break; \
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
	bool trgmode;
	uint64_t conto_soft_us;
	uint64_t conto_hard_us;
	uint64_t linedelay;
	unsigned freelines;
	uint64_t waitquit_us;
	bool keeptrying;
	uint64_t confailwait_us;
	uint16_t heartbeat_us;
	bool reconnect;
	bool flush;
	bool nojoin;
	bool ignore_pm;
	bool ignore_cs;
	bool notices;
	int verb;
	char chanlist[MAX_CHANLIST];
	char keylist[MAX_CHANLIST];
} g_sett;

static struct srvlist_s {
	char *host;
	uint16_t port;
	bool ssl;
	struct srvlist_s *next;
} *g_srvlist;

static struct outline_s {
	char *line;
	struct outline_s *next;
} *g_outQ;

static irc g_irc;
static uint64_t g_quitat;
static uint64_t g_nexthb;


static int process_stdin(void);
static int process_irc(void);
static bool tryconnect(void);
static void life(void);
static void do_heartbeat(void);
static int process_sendq(void);
static int select2(bool *rdbl1, bool *rdbl2, int fd1, int fd2, uint64_t to_us);
static void handle_stdin(char *line);
static void handle_irc(tokarr *tok);
static bool ismychan(const char *chan);
static unsigned strtou8_cap(const char *nptr, char **endptr, int base);
static uint64_t strtou64(const char *nptr, char **endptr, int base);
static void process_args(int *argc, char ***argv, struct settings_s *sett);
static void init(int *argc, char ***argv, struct settings_s *sett);
static int iprintf(const char *fmt, ...);
static void strNcat(char *dest, const char *src, size_t destsz);
static uint64_t timestamp_us(void);
static void tconv(struct timeval *tv, uint64_t *ts, bool tv_to_ts);
static bool isdigitstr(const char *str);
static bool conread(tokarr *msg, void *tag);
static void usage(FILE *str, const char *a0, int ec, bool sh);
static void sleep_us(uint64_t us);
int main(int argc, char **argv);


static int
process_stdin(void)
{
	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	if ((linelen = getline(&line, &linecap, stdin)) > 0) {
		handle_stdin(line);
		free(line);
	} else if (linelen == -1) {
		if (feof(stdin)) {
			WVX("got EOF on stdin");
			return 0;
		} else {
			WX("read from stdin failed");
			return -1;
		}
	}

	return 1;
}


static int
process_irc(void)
{
	tokarr tok;
	int r = irc_read(g_irc, &tok, 100000);

	if (r == -1) {
		WX("irc read failed");
		return -1;
	} else if (r == 1) {
		char buf[1024];
		ut_sndumpmsg(buf, sizeof buf, NULL, &tok);
		WVX("%s", buf);
		handle_irc(&tok);
		return 1;
	}

	return 0;
}


static bool
tryconnect(void)
{
	struct srvlist_s *s = g_srvlist;

	while (s) {
		irc_set_server(g_irc, s->host, s->port);
		WVX("set server to '%s:%"PRIu16"'", irc_get_host(g_irc),
		    irc_get_port(g_irc));

		if (s->ssl) {
#ifdef WITH_SSL
			if (!irc_set_ssl(g_irc, true))
				EX("failed to enable SSL");
#else
			EX("we haven't been compiled with SSL support...");
#endif
			WVX("SSL selected");
		} else
			irc_set_ssl(g_irc, false);

		if (irc_connect(g_irc)) {
			if (!g_sett.nojoin) {
				char jmsg[512];
				snprintf(jmsg, sizeof jmsg, "JOIN %s %s\r\n",
				    g_sett.chanlist, g_sett.keylist);
				irc_write(g_irc, jmsg);
			}
			return true;
		}

		s = s->next;
	}

	return false;
}


static void
life(void)
{
	time_t quitat = 0;
	int r;
	bool stdineof = false;
	g_nexthb = timestamp_us() + g_sett.heartbeat_us;
	for (;;) {
		bool canreadstdin = false, canreadirc = false;

		r = select2(&canreadstdin, &canreadirc,
		    stdineof ? -1 : fileno(stdin),
		    irc_online(g_irc) ? irc_sockfd(g_irc) : -1,
		    g_outQ ? g_sett.linedelay : 300000000u);

		if (r == -1)
			E("select failed");

		if (!stdineof && canreadstdin) {
			r = process_stdin();

			if (r == 0) {
				stdineof = true;
				iprintf("QUIT");
			} else if (r == -1) {
				break;
			}
		}

		bool fail = false;

		fail = canreadirc && (r = process_irc()) == -1;
		fail = fail || process_sendq() == -1;

		if (fail) {
			if (g_quitat) {
				WVX("irc connection closed");
				return;
			}

			WX("irc connection reset");
			if (!stdineof && g_sett.reconnect) {
				WVX("reconnecting");
				while (!tryconnect()) {
					sleep_us(g_sett.confailwait_us);
					continue;
				}
				WVX("recommencing operation");
				continue;
			}

			break;
		} else if (r > 0)
			g_nexthb = timestamp_us() + g_sett.heartbeat_us;

		if (quitat && time(NULL) > quitat) {
			WX("forcefully terminating irc connection");
			break;
		}

		do_heartbeat();
	}
}

static void
do_heartbeat(void)
{
	if (g_sett.heartbeat_us && g_nexthb <= timestamp_us()) {
		iprintf("PING %s", irc_myhost(g_irc));
		g_nexthb = timestamp_us() + g_sett.heartbeat_us;
	}
}

static int
process_sendq(void)
{
	static uint64_t nextsend = 0;
	if (!g_outQ)
		return 0;

	if (g_sett.freelines > 0 || nextsend <= timestamp_us()) {
		struct outline_s *ptr = g_outQ;
		if (!irc_write(g_irc, ptr->line)) {
			WX("write failed");
			return -1;
		}
		g_outQ = g_outQ->next;
		if (strncasecmp(ptr->line, "QUIT", 4) == 0)
			g_quitat = timestamp_us() + g_sett.waitquit_us;
		free(ptr->line);
		free(ptr);
		if (g_sett.freelines)
			g_sett.freelines--;
		nextsend = timestamp_us() + g_sett.linedelay;

		return 1;
	}

	return 0;
}

static int
select2(bool *rdbl1, bool *rdbl2, int fd1, int fd2, uint64_t to_us)
{
	fd_set read_set;
	struct timeval tout;
	int ret;

	uint64_t tsend = to_us ? timestamp_us() + to_us : 0;
	uint64_t trem = 0;

	if (fd1 < 0 && fd2 < 0) {
		W("both filedescriptors -1");
		return -1;
	}

	int maxfd = fd1 > fd2 ? fd1 : fd2;

	FD_ZERO(&read_set);

	if (fd1 >= 0)
		FD_SET(fd1, &read_set);
	if (fd2 >= 0)
		FD_SET(fd2, &read_set);

	for (;;) {
		if (tsend) {
			trem = tsend - timestamp_us();
			if (trem <= 0)
				trem = 1;
			tconv(&tout, &trem, false);
		}
		errno=0;
		ret = select(maxfd + 1, &read_set, NULL, NULL,
		    tsend ? &tout : NULL);

		if (ret == -1) {
			if (errno == EINTR)
				continue; //suppress warning, retry
			W("select failed");
			return -1;
		}

		break;
	}

	*rdbl1 = *rdbl2 = false;
	if (ret > 0) {
		if (fd1 >= 0 && FD_ISSET(fd1, &read_set))
			*rdbl1 = true;
		if (fd2 >= 0 && FD_ISSET(fd2, &read_set))
			*rdbl2 = true;
	}

	return ret;
}


static void
handle_stdin(char *line)
{
	char *end = line + strlen(line) - 1;
	while (end >= line && (*end == '\r' || *end == '\n'))
		*end-- = '\0';

	if (strlen(line) == 0)
		return;

	char *dst = g_sett.chanlist;

	if (g_sett.trgmode) {
		char *tok = strtok(line, " ");
		dst = tok;
		line = tok + strlen(tok) + 1;
	}

	iprintf("%s %s :%s", g_sett.notices?"NOTICE":"PRIVMSG", dst, line);
}


static void
handle_irc(tokarr *tok)
{
	if (strcmp((*tok)[1], "PING") == 0) {
		char resp[512];
		snprintf(resp, sizeof resp, "PONG :%s", (*tok)[2]);
		irc_write(g_irc, resp);
	} else if (strcmp((*tok)[1], "PRIVMSG") == 0) {
		char nick[32];
		if (!ut_pfx2nick(nick, sizeof nick, (*tok)[0]))
			return;

		if (g_sett.ignore_cs && !ut_istrcmp(nick, "ChanServ", irc_casemap(g_irc)))
			return;

		if (g_sett.trgmode)
			printf("%s %s %s %s\n",
			    nick, (*tok)[0], (*tok)[2], (*tok)[3]);
		else if (ismychan((*tok)[2]) || !g_sett.ignore_pm)
			printf("%s\n", (*tok)[3]);

		if (g_sett.flush)
			fflush(stdout);
	}
}


static bool
ismychan(const char *chan)
{
	char chans[512];
	char search[512];
	snprintf(chans, sizeof chans, ",%s,", g_sett.chanlist);
	snprintf(search, sizeof search, ",%s,", chan);

	return strstr(chans, search);
}


static unsigned
strtou8_cap(const char *nptr, char **endptr, int base)
{
	unsigned long l = strtoul(nptr, endptr, base);
	if (l > UINT8_MAX)
		l = UINT8_MAX;

	return (uint8_t)l;
}

static uint64_t
strtou64(const char *nptr, char **endptr, int base)
{
	uint64_t l = strtoull(nptr, endptr, base);
	return (uint64_t)l;
}


static void
process_args(int *argc, char ***argv, struct settings_s *sett)
{
	char *a0 = (*argv)[0];

	for (int ch; (ch = getopt(*argc, *argv,
	    "vqchHn:u:f:F:p:P:tT:C:kw:l:L:Sb:W:rNjiI")) != -1;) {
		switch (ch) {
		      case 'n':
			irc_set_nick(g_irc, optarg);
		break;case 'u':
			irc_set_uname(g_irc, optarg);
		break;case 'f':
			irc_set_fname(g_irc, optarg);
		break;case 'F':
			irc_set_conflags(g_irc, strtou8_cap(optarg, NULL, 10));
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

			irc_set_px(g_irc, host, port, ptype);
			}
		break;case 't':
			WVX("enabled target mode");
			sett->trgmode = true;
		break;case 'T':
			{
			char *arg = strdup(optarg);
			if (!arg)
				E("strdup failed");

			char *ptr = strchr(arg, ':');
			if (!ptr)
				sett->conto_hard_us = strtou64(arg, NULL, 10) * 1000ul;
			else {
				*ptr = '\0';
				sett->conto_hard_us = strtou64(arg, NULL, 10) * 1000ul;
				sett->conto_soft_us = strtou64(ptr+1, NULL, 10) * 1000ul;
			}

			WVX("set connect timeout to %"PRIu64"ms (soft), %"PRIu64"ms (hard)",
			    sett->conto_soft_us / 1000u, sett->conto_hard_us / 1000u);

			free(arg);
			}
		break;case 'C':
			{
			char *str = strdup(optarg);
			if (!str)
				E("strdup failed");
			char *tok = strtok(str, " ");
			bool first = true;
			do {
				char *p = strchr(tok, ',');
				if (!first) {
					STRACAT(sett->chanlist, ",");
					STRACAT(sett->keylist, ",");
				}
				first = false;
				if (p) {
					*p = '\0';
					STRACAT(sett->chanlist, tok);
					STRACAT(sett->keylist, p+1);
					*p = ',';
				} else {
					STRACAT(sett->chanlist, tok);
					STRACAT(sett->keylist, "x");
				}
			} while ((tok = strtok(NULL, " ")));
			free(str);
			}
		break;case 'l':
			sett->linedelay = strtou64(optarg, NULL, 10) * 1000u;
			WVX("set linedelay to %"PRIu64" ms/line", sett->linedelay / 1000u);
		break;case 'L':
			sett->freelines = (unsigned)strtoul(optarg, NULL, 10);
			WVX("set freelines to %d", sett->freelines);
		break;case 'w':
			sett->waitquit_us = strtou64(optarg, NULL, 10) * 1000u;
			WVX("set waitquit time to %"PRIu64" ms", sett->waitquit_us / 1000u);
		break;case 'k':
			sett->keeptrying = true;
		break;case 'W':
			sett->confailwait_us = strtou64(optarg, NULL, 10) * 1000u;
		break;case 'b':
			sett->heartbeat_us = strtou64(optarg, NULL, 10) * 1000u;
		break;case 'r':
			sett->reconnect = true;
		break;case 'S':
			sett->flush = true;
		break;case 'j':
			sett->nojoin = true;
		break;case 'I':
			sett->ignore_cs = false;
		break;case 'i':
			sett->ignore_pm = false;
		break;case 'N':
			sett->notices = true;
			WVX("switched to NOTICE mode");
		break;case 'q':
			sett->verb--;
		break;case 'v':
			sett->verb++;
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
}

static void
init(int *argc, char ***argv, struct settings_s *sett)
{
	if (setvbuf(stdin, NULL, _IOLBF, 0) != 0)
		W("setvbuf stdin");

	if (setvbuf(stdout, NULL, _IOLBF, 0) != 0)
		W("setvbuf stdout");

	g_irc = irc_init();

	irc_set_nick(g_irc, "icat");
	irc_set_uname(g_irc, "icat");
	irc_set_fname(g_irc, "irc netcat");
	irc_set_conflags(g_irc, 0);
	irc_regcb_conread(g_irc, conread, 0);

	sett->linedelay = DEF_LINEDELAY_MS*1000u;
	sett->freelines = DEF_FREELINES;
	sett->waitquit_us = DEF_WAITQUIT_MS*1000u;
	sett->confailwait_us = DEF_CONFAILWAIT_MS*1000u;
	sett->heartbeat_us = DEF_HEARBEAT_MS*1000u;
	sett->verb = DEF_VERB;
	sett->conto_soft_us = DEF_CONTO_SOFT_MS*1000u;
	sett->conto_hard_us = DEF_CONTO_HARD_MS*1000u;
	sett->ignore_pm = DEF_IGNORE_PM;
	sett->ignore_cs = DEF_IGNORE_CHANSERV;

	process_args(argc, argv, sett);

	if (!*argc)
		EX("no server given");

	for (int i = 0; i < *argc; i++) {
		char host[256];
		uint16_t port;
		bool ssl = false;
		ut_parse_hostspec(host, sizeof host, &port, &ssl, (*argv)[i]);

		if (isdigitstr(host)) /* catch netcat and telnet invocation syntax */
			EX("invalid server specified (use 'srv:port' instead of 'srv port')");

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

	if (!sett->trgmode && strlen(sett->chanlist) == 0)
		EX("no targetmode and no chans given. i can't fap to that.");

	irc_set_connect_timeout(g_irc, g_sett.conto_soft_us,
	    g_sett.conto_hard_us);

	WVX("initialized");
}


static int
iprintf(const char *fmt, ...)
{
	char buf[1024];
	va_list l;
	va_start(l, fmt);
	int r = vsnprintf(buf, sizeof buf, fmt, l);
	va_end(l);

	struct outline_s *ptr = g_outQ;

	if (!ptr) {
		if (!(ptr = g_outQ = malloc(sizeof *g_outQ)))
			E("malloc failed");
	} else {
		while (ptr->next)
			ptr = ptr->next;
		if (!(ptr->next = malloc(sizeof *ptr->next)))
			E("malloc failed");
		ptr = ptr->next;
	}

	ptr->next = NULL;
	if (!(ptr->line = strdup(buf)))
		E("strdup failed");
	return r;
}


static void
strNcat(char *dest, const char *src, size_t destsz)
{
	size_t len = strlen(dest);
	if (len + 1 >= destsz)
		return;
	size_t rem = destsz - (len + 1);

	char *ptr = dest + len;
	while (rem-- && *src) {
		*ptr++ = *src++;
	}
	*ptr = '\0';
}

static uint64_t
timestamp_us(void)
{
	struct timeval t;
	uint64_t ts = 0;
	if (gettimeofday(&t, NULL) != 0)
		E("gettimeofday");
	else
		tconv(&t, &ts, true);

	return ts;
}

static void
tconv(struct timeval *tv, uint64_t *ts, bool tv_to_ts)
{
	if (tv_to_ts)
		*ts = (uint64_t)tv->tv_sec * 1000000u + tv->tv_usec;
	else {
		tv->tv_sec = *ts / 1000000u;
		tv->tv_usec = *ts % 1000000u;
	}
}

static bool
isdigitstr(const char *str)
{
	while (*str)
		if (!isdigit((unsigned char)*str++))
			return false;

	return true;
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
	LH("\t-S: Explicitly flush stdout after every line of output");
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
	    "[def: "XSTR(DEF_HEARBEAT_MS)"]");
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

static void
sleep_us(uint64_t us)
{
	WVX("sleeping %"PRIu64" us", us);
	uint64_t secs = us / 1000000u;
	if (secs > INT_MAX)
		secs = INT_MAX; //eh.. yeah.

	sleep((int)secs);

	useconds_t u = us % 1000000u;
	if (u)
		usleep(u);
}

int
main(int argc, char **argv)
{
	init(&argc, &argv, &g_sett);
	atexit(cleanup);

	bool failure = false;

	for (;;) {
		WVX("connecting...");

		if (!tryconnect()) {
			WX("failed to connect/logon (%s)",
			    g_sett.keeptrying ?"retrying":"giving up");

			if (g_sett.keeptrying) {
				sleep_us(g_sett.confailwait_us);
				continue;
			}
			failure = true;
			break;
		}
		WVX("logged on!");
		break;
	}

	if (!failure)
		life();

	irc_dispose(g_irc);
	g_irc = NULL; /* avoid re-dispose in cleanup */

	return failure ? EXIT_FAILURE : EXIT_SUCCESS;
}
