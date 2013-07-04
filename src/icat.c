/* icat.c - IRC netcat, the example application of libsrsirc
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE 1

#define _WITH_GETLINE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>

#include <time.h>
#include <unistd.h>
#include <err.h>

#include <common.h>
#include <libsrsirc/irc_basic.h>
#include <libsrsirc/irc_con.h>
#include <libsrsirc/irc_io.h>
#include <libsrsirc/irc_util.h>

#define DEF_PORT 6667
#define MAX_CHANS 32
#define IRC_MAXARGS 16
#define MAX_CHANLIST 512
#define STRACAT(DSTARR, STR) strNcat((DSTARR), (STR), sizeof (DSTARR))

#define W_(FNC, THR, FMT, A...) do {                                  \
		if (g_sett.verb < THR) break; \
		FNC("%s:%d:%s() - " FMT, __FILE__, __LINE__, __func__, ##A);  \
		} while(0)

#define W(FMT, A...) W_(warn, 1, FMT, ##A)
#define WV(FMT, A...) W_(warn, 2, FMT, ##A)

#define WX(FMT, A...) W_(warnx, 1, FMT, ##A)
#define WVX(FMT, A...) W_(warnx, 2, FMT, ##A)

#define E(FMT, A...) do { W_(warn, 0, FMT, ##A); \
		exit(EXIT_FAILURE);} while(0)

#define EX(FMT, A...) do { W_(warnx, 0, FMT, ##A); \
		exit(EXIT_FAILURE);} while(0)

struct settings_s {
	bool trgmode;
	int conto_s;
	int linedelay;
	int freelines;
	int waitquit_s;
	bool keeptrying;
	int confailwait_s;
	int heartbeat;
	bool reconnect;
	bool flush;
	bool nojoin;
	//bool fancy;
	bool notices;
	int verb;
	char chanlist[MAX_CHANLIST];
	char keylist[MAX_CHANLIST];
} g_sett;

static struct outline_s {
	char *line;
	struct outline_s *next;
} *g_outQ;

static ibhnd_t g_irc;
static time_t g_quitat;

void life(void);
void handle_stdin(char *line);
void handle_irc(char **tok, size_t ntok);
void init(int *argc, char ***argv, struct settings_s *sett);
int iprintf(const char *fmt, ...);
void strNcat(char *dest, const char *src, size_t destsz);
bool conread(char **msg, size_t msg_len, void *tag);
void usage(FILE *str, const char *a0, int ec, bool sh);
int main(int argc, char **argv);


int
process_stdin(void)
{
	struct timeval tout = {0, 0};

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fileno(stdin), &fds);

	int r;

	for(;;) {
		errno=0;
		r = select(fileno(stdin)+1, &fds, NULL, NULL, &tout);

		if (r == -1) {
			if (errno == EINTR)
				continue; //suppress warning, retry
			W("select failed");
			return -1;
		}

		break;
	}

	if (r == 1) {
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
	}

	return 1;
}


int
process_irc(void)
{
	static time_t nexthb = 0;
	static time_t nextsend = 0;
	if (nexthb == 0)
		nexthb = time(NULL) + g_sett.heartbeat;
	char *tok[IRC_MAXARGS];

	int r = ircbas_read(g_irc, tok, IRC_MAXARGS, 100000);

	if (r == -1) {
		WX("irc read failed");
		return -1;
	} else if (r == 1) {
		char buf[1024];
		sndumpmsg(buf, sizeof buf, NULL, tok, IRC_MAXARGS);
		WVX("%s", buf);
		handle_irc(tok, IRC_MAXARGS);
	} else {
		if (g_sett.heartbeat && nexthb <= time(NULL)) {
			iprintf("PING %s", ircbas_myhost(g_irc));
			nexthb = time(NULL) + g_sett.heartbeat;
		}
	}

	if (g_outQ) {
		if (g_sett.freelines > 0 || nextsend <= time(NULL)) {
			struct outline_s *ptr = g_outQ;
			if (!ircbas_write(g_irc, ptr->line)) {
				WX("write failed");
				return -1;
			}
			g_outQ = g_outQ->next;
			if (strncasecmp(ptr->line, "QUIT", 4) == 0)
				g_quitat = time(NULL) + g_sett.waitquit_s+1;
			free(ptr->line);
			free(ptr);
			if (g_sett.freelines)
				g_sett.freelines--;
			nextsend = time(NULL) + g_sett.linedelay;
		}
	}
	return 1;

}


void
life(void)
{
	time_t quitat = 0;
	int r;
	bool stdineof = false;
	for(;;) {
		if (!stdineof) {
			r = process_stdin();

			if (r == 0) {
				stdineof = true;
				iprintf("QUIT");
			} else if (r == -1) {
				break;
			}
		}

		r = process_irc();
		if (r == -1) {
			if (g_quitat) {
				WVX("irc connection closed");
				return;
			}

			WX("irc connection reset");
			if (!stdineof && g_sett.reconnect) {
				WVX("reconnecting");
				if(!ircbas_connect(g_irc,
				              g_sett.conto_s * 1000000UL)) {
					WVX("sleeping %d sec",
					              g_sett.confailwait_s);
					sleep(g_sett.confailwait_s);
					continue;
				}
				WVX("connected again!");
				iprintf("JOIN %s %s",
				           g_sett.chanlist, g_sett.keylist);
				WVX("recommencing operation");
				continue;
			}

			break;
		}

		if (quitat && time(NULL) > quitat) {
			WX("forcefully terminating irc connection");
			break;
		}

	}
}


void
handle_stdin(char *line)
{
	char *end = line + strlen(line) - 1;
	while(end >= line && (*end == '\r' || *end == '\n'))
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


void
handle_irc(char **tok, size_t ntok)
{
	if (strcmp(tok[1], "PING") == 0) {
		char resp[512];
		snprintf(resp, sizeof resp, "PONG :%s", tok[2]);
		ircbas_write(g_irc, resp);
	} else if (strcmp(tok[1], "PRIVMSG") == 0) {
		char nick[32];
		pfx_extract_nick(nick, sizeof nick, tok[0]);
		if (g_sett.trgmode)
			printf("%s %s %s %s\n",
			                      nick, tok[0], tok[2], tok[3]);
		else
			printf("%s\n", tok[3]);
		if (g_sett.flush)
			fflush(stdout);
	}
}


void
process_args(int *argc, char ***argv, struct settings_s *sett)
{
	char *a0 = (*argv)[0];

	for(int ch; (ch = getopt(*argc, *argv,
	                "vchHn:u:f:F:s:p:P:tT:C:kw:l:L:Sb:W:rNj")) != -1;) {
		switch (ch) {
		      case 'n':
			ircbas_set_nick(g_irc, optarg);
		break;case 'u':
			ircbas_set_uname(g_irc, optarg);
		break;case 'f':
			ircbas_set_fname(g_irc, optarg);
		break;case 'F':
			ircbas_set_conflags(g_irc,
			                (unsigned)strtol(optarg, NULL, 10));
		break;case 'p':
			ircbas_set_pass(g_irc, optarg);
		break;case 'P':
			{
			char typestr[16];
			char host[256];
			unsigned short port;
			int typeno;
			if (!parse_pxspec(typestr, sizeof typestr, host,
					sizeof host, &port, optarg))
				EX("failed to parse pxspec '%s'", optarg);
			if ((typeno = pxtypeno(typestr)) == -1)
				EX("unknown proxy type '%s'", typestr);

			ircbas_set_proxy(g_irc, host, port, typeno);
			}
		break;case 't':
			WVX("enabled target mode");
			sett->trgmode = true;
		break;case 'T':
			sett->conto_s = (unsigned)strtol(optarg, NULL, 10);
			WVX("set connect timeout to %ds", sett->conto_s);
		break;case 'C':
			{
			char *str = strdup(optarg);
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
			} while((tok = strtok(NULL, " ")));
			free(str);
			}
		break;case 'l':
			sett->linedelay = (int)strtol(optarg, NULL, 10);
			WVX("set linedelay to %d sec/line", sett->linedelay);
		break;case 'L':
			sett->freelines = (int)strtol(optarg, NULL, 10);
			WVX("set freelines to %d", sett->freelines);
		break;case 'w':
			sett->waitquit_s = (int)strtol(optarg, NULL, 10);
			WVX("set waitquit time to %d sec", sett->waitquit_s);
		break;case 'k':
			sett->keeptrying = true;
		break;case 'W':
			sett->confailwait_s = (int)strtol(optarg, NULL, 10);
		break;case 'b':
			sett->heartbeat = (int)strtol(optarg, NULL, 10);
		break;case 'r':
			sett->reconnect = true;
		break;case 'S':
			sett->flush = true;
		break;case 'j':
			sett->nojoin = true;
		break;case 'N':
			sett->notices = true;
			WVX("switched to NOTICE mode");
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

void
init(int *argc, char ***argv, struct settings_s *sett)
{
	g_irc = ircbas_init();

	ircbas_set_nick(g_irc, "icat");
	ircbas_set_uname(g_irc, "icat");
	ircbas_set_fname(g_irc, "irc netcat, descendant of longcat");
	ircbas_set_conflags(g_irc, 0);
	ircbas_regcb_conread(g_irc, conread, 0);

	process_args(argc, argv, sett);

	if (!*argc)
		EX("no server given");

	if (*argc > 1)
		EX("too many args. use 'srv:port' instead of 'srv port'");

	char host[256];
	unsigned short port;
	parse_hostspec(host, sizeof host, &port, (*argv)[0]);
	if (port == 0)
		port = DEF_PORT;

	ircbas_set_server(g_irc, host, port);
	WVX("set server to '%s:%hu'", ircbas_get_host(g_irc),
			ircbas_get_port(g_irc));

	if (!sett->trgmode && strlen(sett->chanlist) == 0)
		EX("no targetmode and no chans given. i can't fap to that.");

	WVX("initialized");
}


int
iprintf(const char *fmt, ...)
{
	char buf[1024];
	va_list l;
	va_start(l, fmt);
	int r = vsnprintf(buf, sizeof buf, fmt, l);
	va_end(l);

	struct outline_s *ptr = g_outQ;

	if (!ptr)
		ptr = g_outQ = malloc(sizeof *g_outQ);
	else {
		while(ptr->next)
			ptr = ptr->next;
		ptr->next = malloc(sizeof *ptr->next);
		ptr = ptr->next;
	}

	ptr->next = NULL;
	ptr->line = strdup(buf);
	return r;
}


void
strNcat(char *dest, const char *src, size_t destsz)
{
	size_t len = strlen(dest);
	if (len + 1 >= destsz)
		return;
	size_t rem = destsz - (len + 1);

	char *ptr = dest + len;
	while(rem-- && *src) {
		*ptr++ = *src++;
	}
	*ptr = '\0';
}


bool
conread(char **msg, size_t msg_len, void *tag)
{
	char buf[1024];
	sndumpmsg(buf, sizeof buf, tag, msg, msg_len);
	WVX("%s", buf);
	return true;
}


void
usage(FILE *str, const char *a0, int ec, bool sh)
{
	#define SH(STR) if (sh) fputs(STR "\n", str)
	#define LH(STR) if (!sh) fputs(STR "\n", str)
	#define BH(STR) fputs(STR "\n", str)
	BH("=============================================================");
	BH("== icat - IRC netcat, the only known descendant of longcat ==");
	BH("=============================================================");
	fprintf(str, "usage: %s [-vchtkSnufFspPTCwlL] <hostspec>\n", a0);
	BH("");
	BH("\t<hostspec> specifies the IRC server to connect against");
	LH("\t\thostspec := srvaddr[:port]");
	LH("\t\tsrvaddr  := ip4addr|ip6addr|dnsname");
	LH("\t\tport     := int(0..65535)");
	LH("\t\tip4addr  := 'aaa.bbb.ccc.ddd'");
	LH("\t\tip6addr  :=  '[aa:bb:cc::dd:ee]'");
	LH("\t\tdnsname  := 'irc.example.org'");
	BH("");
	BH("\t-v: Increase verbosity on stderr (use -vv or -vvv for more)");
	BH("\t-c: Use Bash color sequences on stderr");
	LH("\t-h: Display brief usage statement and terminate");
	BH("\t-H: Display longer usage statement and terminate");
	BH("\t-t: Enable target-mode. See man page for more information");
	LH("\t-k: Keep trying to connect, if first connect/logon fails");
	BH("\t-r: Reconnect on disconnect, rather than terminating");
	LH("\t-S: Explicitly flush stdout after every line of output");
	LH("\t-N: Use NOTICE instead of PRIVMSG for messages");
	LH("");
	BH("\t-n <str>: Use <str> as nick. Subject to mutilation if N/A");
	LH("\t-u <str>: Use <str> as (IRC) username/ident");
	LH("\t-f <str>: Use <str> as (IRC) fullname");
	LH("\t-F <int>: Specify USER flags. 0=None, 8=usermode +i");
	LH("\t-W <int>: Wait <int> seconds between attempts to connect");
	LH("\t-b <int>: Send heart beat PING every <int> seconds");
	BH("\t-p <str>: Use <str> as server password");
	fprintf(str, "\t-P <pxspec>: Use <pxspec> as proxy. "
			"See %s for format\n", sh?"man page":"below");
	BH("\t-T <int>: Connect/Logon timeout in seconds");
	fprintf(str, "\t-C <chanlst>: List of chans to join + keys. "
			"See %s for format\n", sh?"man page":"below");
	BH("\t-w <int>: Secs to wait for server disconnect after QUIT");
	BH("\t-l <int>: Wait <int> sec between sending messages to IRC");
	BH("\t-L <int>: Number of 'first free' lines to send unthrottled");
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
	BH("(C) 2012, Timo Buhrmester (contact: #fstd @ irc.freenode.org)");
	#undef SH
	#undef LH
	#undef BH
	exit(ec);
}


int
main(int argc, char **argv)
{
	init(&argc, &argv, &g_sett);

	bool failure = false;

	for (;;) {
		WVX("connecting...");
		if (!ircbas_connect(g_irc, g_sett.conto_s*1000000UL)) {
			WX("failed to connect/logon (%s)",
			                           g_sett.keeptrying
			                           ?"retrying":"giving up");
			if (g_sett.keeptrying) {
				WVX("sleeping %d sec", g_sett.confailwait_s);
				sleep(g_sett.confailwait_s);
				continue;
			}
			failure = true;
			break;
		}
		WVX("logged on!");

		iprintf("JOIN %s %s", g_sett.chanlist, g_sett.keylist);

		break;
	}

	if (!failure)
		life();

	ircbas_dispose(g_irc);
	return failure ? EXIT_FAILURE : EXIT_SUCCESS;
}
