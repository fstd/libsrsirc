/* irc_basic.c - Front-end implementation
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <common.h>
#include <libsrsirc/irc_util.h>
#include "irc_con.h"

#include <intlog.h>

#include <libsrsirc/irc_basic.h>

#define MAX_IRCARGS ((size_t)15)

#define RFC1459 0

#define DEF_PASS ""
#define DEF_NICK "srsirc"
#define DEF_UNAME "bsnsirc"
#define DEF_FNAME "serious business irc"
#define DEF_CONFLAGS 0
#define DEF_SERV_DIST "*"
#define DEF_SERV_TYPE 0
#define DEF_SERV_INFO "serious business irc service"
#define DEF_UMODES "iswo"
#define DEF_CMODES "opsitnml"

#define DEF_CONTO_HARD 120000000ul
#define DEF_CONTO_SOFT 15000000ul

#define MAX_NICK_LEN 64
#define MAX_HOST_LEN 128
#define MAX_UMODES_LEN 64
#define MAX_CMODES_LEN 64
#define MAX_VER_LEN 128

struct ibhnd
{
	char mynick[MAX_NICK_LEN];
	char myhost[MAX_HOST_LEN];
	bool service;
	char umodes[MAX_UMODES_LEN];
	char cmodes[MAX_CMODES_LEN];
	char ver[MAX_VER_LEN];
	char *lasterr;

	/* zero timeout means no timeout */
	unsigned long conto_hard_us;/*connect() timeout per A/AAAA record*/
	unsigned long conto_soft_us;/*overall ircbas_connect() timeout*/

	bool restricted;
	bool banned;
	char *banmsg;

	int casemapping;

	char *pass;
	char *nick;
	char *uname;
	char *fname;
	unsigned conflags;
	bool serv_con;
	char *serv_dist;
	long serv_type;
	char *serv_info;

	char **logonconv[4];
	char *m005chanmodes[4];
	char *m005modepfx[2];

	fp_con_read cb_con_read;
	void *tag_con_read;
	fp_mut_nick cb_mut_nick;

	ichnd_t con;
};



static void mutilate_nick(char *nick, size_t nick_sz);
static bool send_logon(ibhnd_t hnd);

static bool onread(ibhnd_t hnd, char **tok, size_t tok_len);
static char** clonearr(char **arr, size_t nelem);
static void freearr(char **arr, size_t nelem);

bool
ircbas_regcb_mutnick(ibhnd_t hnd, fp_mut_nick cb)
{
	hnd->cb_mut_nick = cb;
	return true;
}

bool
ircbas_regcb_conread(ibhnd_t hnd, fp_con_read cb, void *tag)
{
	hnd->cb_con_read = cb;
	hnd->tag_con_read = tag;
	return true;
}

ibhnd_t
ircbas_init(void)
{
	ichnd_t con;
	ibhnd_t r = NULL;
	int preverrno = errno;
	errno = 0;
	if (!(con = irccon_init()))
		goto ircbas_init_fail;


	if (!(r = malloc(sizeof (*(ibhnd_t)0))))
		goto ircbas_init_fail;

	r->pass = NULL;
	r->nick = NULL;
	r->uname = NULL;
	r->fname = NULL;
	r->serv_dist = NULL;
	r->serv_info = NULL;
	r->m005chanmodes[0] = NULL;
	r->m005chanmodes[1] = NULL;
	r->m005chanmodes[2] = NULL;
	r->m005chanmodes[3] = NULL;
	r->m005modepfx[0] = NULL;
	r->m005modepfx[1] = NULL;

	if (!(r->pass = strdup(DEF_PASS)))
		goto ircbas_init_fail;

	size_t len = strlen(DEF_NICK);
	if (!(r->nick = malloc((len > 9 ? len : 9) + 1)))
		goto ircbas_init_fail;
	strcpy(r->nick, DEF_NICK);

	if ((!(r->uname = strdup(DEF_UNAME)))
	    || (!(r->fname = strdup(DEF_FNAME)))
	    || (!(r->serv_dist = strdup(DEF_SERV_DIST)))
	    || (!(r->serv_info = strdup(DEF_SERV_INFO)))
	    || (!(r->m005chanmodes[0] = strdup("b")))
	    || (!(r->m005chanmodes[1] = strdup("k")))
	    || (!(r->m005chanmodes[2] = strdup("l")))
	    || (!(r->m005chanmodes[3] = strdup("psitnm")))
	    || (!(r->m005modepfx[0] = strdup("ov")))
	    || (!(r->m005modepfx[1] = strdup("@+"))))
		goto ircbas_init_fail;

	errno = preverrno;

	r->con = con;
	/* persistent after reset */
	r->mynick[0] = '\0';
	r->myhost[0] = '\0';
	r->service = false;
	r->umodes[0] = '\0';
	r->cmodes[0] = '\0';
	r->ver[0] = '\0';
	r->lasterr = NULL;

	r->casemapping = CMAP_RFC1459;

	r->restricted = false;
	r->banned = false;
	r->banmsg = NULL;

	r->conflags = DEF_CONFLAGS;
	r->serv_con = false;
	r->serv_type = DEF_SERV_TYPE;

	r->cb_con_read = NULL;
	r->tag_con_read = NULL;
	r->cb_mut_nick = mutilate_nick;

	r->conto_soft_us = DEF_CONTO_SOFT;
	r->conto_hard_us = DEF_CONTO_HARD;

	for(int i = 0; i < 4; i++)
		r->logonconv[i] = NULL;


	D("(%p) irc_bas initialized (backend: %p)", r, r->con);
	return r;

ircbas_init_fail:
	EE("failed to initialize irc_basic handle");
	if (r) {
		free(r->pass);
		free(r->nick);
		free(r->uname);
		free(r->fname);
		free(r->serv_dist);
		free(r->serv_info);
		free(r->m005chanmodes[0]);
		free(r->m005chanmodes[1]);
		free(r->m005chanmodes[2]);
		free(r->m005chanmodes[3]);
		free(r->m005modepfx[0]);
		free(r->m005modepfx[1]);
		free(r);
	}

	if (con)
		irccon_dispose(con);

	return NULL;
}

bool
ircbas_reset(ibhnd_t hnd)
{
	D("(%p) resetting backend", hnd);
	if (!irccon_reset(hnd->con))
		return false;

	return true;
}

bool
ircbas_dispose(ibhnd_t hnd)
{
	if (!irccon_dispose(hnd->con))
		return false;

	XFREE(hnd->lasterr);
	XFREE(hnd->banmsg);

	XFREE(hnd->pass);
	XFREE(hnd->nick);
	XFREE(hnd->uname);
	XFREE(hnd->fname);
	XFREE(hnd->serv_dist);
	XFREE(hnd->serv_info);

	D("(%p) disposed", hnd);
	XFREE(hnd);

	return true;
}

bool
ircbas_connect(ibhnd_t hnd)
{
	int64_t tsend = hnd->conto_hard_us ?
	    ic_timestamp_us() + hnd->conto_hard_us : 0;

	XFREE(hnd->lasterr);
	hnd->lasterr = NULL;
	XFREE(hnd->banmsg);
	hnd->banmsg = NULL;
	hnd->banned = false;

	for(int i = 0; i < 4; i++) {
		freearr(hnd->logonconv[i], MAX_IRCARGS);
		hnd->logonconv[i] = NULL;
	}

	D("(%p) connecting backend (timeout: %luus (soft), %luus (hard))",
	    hnd, hnd->conto_soft_us, hnd->conto_hard_us);

	if (!irccon_connect(hnd->con, hnd->conto_soft_us,
	    hnd->conto_hard_us)) {
		W("(%p) backend failed to establish connection", hnd);
		return false;
	}

	D("(%p) sending IRC logon sequence", hnd);
	if (!send_logon(hnd)) {
		W("(%p) failed writing IRC logon sequence", hnd);
		ircbas_reset(hnd);
		return false;
	}

	D("(%p) connection established, IRC logon sequence sent", hnd);
	char *msg[MAX_IRCARGS];
	ic_strNcpy(hnd->mynick, hnd->nick, sizeof hnd->mynick);

	int64_t trem = 0;
	for(;;) {
		if(tsend) {
			trem = tsend - ic_timestamp_us();
			if (trem <= 0) {
				W("(%p) timeout waiting for 004", hnd);
				ircbas_reset(hnd);
				return false;
			}
		}

		int r = irccon_read(hnd->con, msg, MAX_IRCARGS,
		    (unsigned long)trem);

		if (r < 0) {
			W("(%p) irccon_read() failed", hnd);
			ircbas_reset(hnd);
			return false;
		}

		if (r == 0)
			continue;

		if (hnd->cb_con_read && !hnd->cb_con_read(msg, MAX_IRCARGS,
		    hnd->tag_con_read)) {
			W("(%p) further logon prohibited by conread", hnd);
			ircbas_reset(hnd);
			return false;
		}

		if (strcmp(msg[1], "001") == 0) {
			hnd->logonconv[0] = clonearr(msg, MAX_IRCARGS);
			ic_strNcpy(hnd->mynick, msg[2],sizeof hnd->mynick);
			char *tmp;
			if ((tmp = strchr(hnd->mynick, '@')))
				*tmp = '\0';
			if ((tmp = strchr(hnd->mynick, '!')))
				*tmp = '\0';

			//XXX ensure argcount
			ic_strNcpy(hnd->myhost,
			    msg[0] ? msg[0] : irccon_get_host(hnd->con),
			    sizeof hnd->mynick);

			ic_strNcpy(hnd->umodes, DEF_UMODES, sizeof hnd->umodes);
			ic_strNcpy(hnd->cmodes, DEF_CMODES, sizeof hnd->cmodes);
			hnd->ver[0] = '\0';
			hnd->service = false;
		} else if (strcmp(msg[1], "002") == 0) {
			hnd->logonconv[1] = clonearr(msg, MAX_IRCARGS);
		} else if (strcmp(msg[1], "003") == 0) {
			hnd->logonconv[2] = clonearr(msg, MAX_IRCARGS);
		} else if (strcmp(msg[1], "004") == 0) {
			hnd->logonconv[3] = clonearr(msg, MAX_IRCARGS);
			//XXX ensure argcount
			ic_strNcpy(hnd->myhost, msg[3],sizeof hnd->myhost);
			ic_strNcpy(hnd->umodes, msg[5], sizeof hnd->umodes);
			ic_strNcpy(hnd->cmodes, msg[6], sizeof hnd->cmodes);
			ic_strNcpy(hnd->ver, msg[4], sizeof hnd->ver);
			D("(%p) got beloved 004", hnd);
			break;
		} else if (strcmp(msg[1], "PING") == 0) {
			char buf[64];
			snprintf(buf, sizeof buf, "PONG :%s\r\n", msg[2]);
			if (!irccon_write(hnd->con, buf)) {
				W("(%p) write failed (1)", hnd);
				ircbas_reset(hnd);
				return false;
			}
		} else if ((strcmp(msg[1], "432") == 0)//errorneous nick
		    || (strcmp(msg[1], "433") == 0)//nick in use
		    || (strcmp(msg[1], "436") == 0)//nick collision
		    || (strcmp(msg[1], "437") == 0)) {//unavail resource
			if (!hnd->cb_mut_nick) {
				W("(%p) got no mutnick.. (failing)", hnd);
				ircbas_reset(hnd);
				return false;
			}
			hnd->cb_mut_nick(hnd->mynick, 10); //XXX hc
			char buf[MAX_NICK_LEN];
			snprintf(buf,sizeof buf,"NICK %s\r\n",hnd->mynick);
			if (!irccon_write(hnd->con, buf)) {
				W("(%p) write failed (2)", hnd);
				ircbas_reset(hnd);
				return false;
			}
		} else if (strcmp(msg[1], "464") == 0) {//passwd missmatch
			ircbas_reset(hnd);
			W("(%p) wrong server password", hnd);
			return false;
		} else if (strcmp(msg[1], "383") == 0) { //we're service
			ic_strNcpy(hnd->mynick, msg[2],sizeof hnd->mynick);
			char *tmp;
			if ((tmp = strchr(hnd->mynick, '@')))
				*tmp = '\0';
			if ((tmp = strchr(hnd->mynick, '!')))
				*tmp = '\0';

			ic_strNcpy(hnd->myhost,
			    msg[0] ? msg[0] : irccon_get_host(hnd->con),
			    sizeof hnd->mynick);

			ic_strNcpy(hnd->umodes, DEF_UMODES, sizeof hnd->umodes);
			ic_strNcpy(hnd->cmodes, DEF_CMODES, sizeof hnd->cmodes);
			hnd->ver[0] = '\0';
			hnd->service = true;
			break;
		} else if (strcmp(msg[1], "484") == 0) { //restricted
			hnd->restricted = true;
		} else if (strcmp(msg[1], "465") == 0) { //banned
			W("(%p) we're banned", hnd);
			hnd->banned = true;
			XFREE(hnd->banmsg);
			hnd->banmsg = XSTRDUP(msg[3]?msg[3]:"");
		} else if (strcmp(msg[1], "466") == 0) { //will be banned
			W("(%p) we will be banned", hnd); //XXX not in RFC
		} else if (strcmp(msg[1], "ERROR") == 0) {
			XFREE(hnd->lasterr);
			hnd->lasterr = XSTRDUP(msg[2]?msg[2]:"");
			W("(%p) received error while logging on: %s",
			    hnd, msg[2]);
			ircbas_reset(hnd);
			return false;
		}
	}
	D("(%p) irc logon finished, U R online", hnd);
	return true;
}

int
ircbas_read(ibhnd_t hnd, char **tok, size_t tok_len, unsigned long to_us)
{
	//D("(%p) wanna read (timeout: %lu)", hnd, to_us);
	int r = irccon_read(hnd->con, tok, tok_len, to_us);

	if (r == -1 || (r != 0 && !onread(hnd, tok, tok_len))) {
		W("(%p) irccon_read() failed or onread() denied (r:%d)",
		    hnd, r);
		ircbas_reset(hnd);
		return -1;
	}
	//D("(%p) done reading", hnd);

	return r;
}

bool
ircbas_write(ibhnd_t hnd, const char *line)
{
	bool r = irccon_write(hnd->con, line);

	if (!r) {
		W("(%p) irccon_write() failed", hnd);
		ircbas_reset(hnd);
	}

	return r;
}

bool
ircbas_online(ibhnd_t hnd)
{
	return irccon_online(hnd->con);
}
bool
ircbas_set_ssl(ibhnd_t hnd, bool on)
{
	return irccon_set_ssl(hnd->con, on);
}

bool
ircbas_get_ssl(ibhnd_t hnd)
{
	return irccon_get_ssl(hnd->con);
}

int
ircbas_sockfd(ibhnd_t hnd)
{
	return irccon_sockfd(hnd->con);
}

bool
ircbas_set_pass(ibhnd_t hnd, const char *srvpass)
{
	XFREE(hnd->pass);
	hnd->pass = XSTRDUP(srvpass);
	return true;
}

bool
ircbas_set_uname(ibhnd_t hnd, const char *uname)
{
	XFREE(hnd->uname);
	hnd->uname = XSTRDUP(uname);
	return true;
}

bool
ircbas_set_fname(ibhnd_t hnd, const char *fname)
{
	XFREE(hnd->fname);
	hnd->fname = XSTRDUP(fname);
	return true;
}

bool
ircbas_set_conflags(ibhnd_t hnd, unsigned flags)
{
	hnd->conflags = flags;
	return true;
}

bool
ircbas_set_nick(ibhnd_t hnd, const char *nick)
{
	XFREE(hnd->nick);
	hnd->nick = XSTRDUP(nick);
	return true;
}

bool
ircbas_set_service_connect(ibhnd_t hnd, bool enabled)
{
	hnd->serv_con = enabled;
	return true;
}

bool
ircbas_set_service_dist(ibhnd_t hnd, const char *dist)
{
	XFREE(hnd->serv_dist);
	hnd->serv_dist = XSTRDUP(dist);
	return true;
}

bool
ircbas_set_service_type(ibhnd_t hnd, long type)
{
	hnd->serv_type = type;
	return true;
}

bool
ircbas_set_service_info(ibhnd_t hnd, const char *info)
{
	XFREE(hnd->serv_info);
	hnd->serv_info = XSTRDUP(info);
	return true;
}

bool
ircbas_set_connect_timeout(ibhnd_t hnd,
    unsigned long soft, unsigned long hard)
{
	hnd->conto_hard_us = hard;
	hnd->conto_soft_us = soft;
	return true;
}

bool
ircbas_set_proxy(ibhnd_t hnd, const char *host, unsigned short port,
    int ptype)
{
	return irccon_set_proxy(hnd->con, host, port, ptype);
}

bool
ircbas_set_server(ibhnd_t hnd, const char *host, unsigned short port)
{
	return irccon_set_server(hnd->con, host, port);
}

int
ircbas_casemap(ibhnd_t hnd)
{
	return hnd->casemapping;
}

const char*
ircbas_mynick(ibhnd_t hnd)
{
	return hnd->mynick;
}

const char*
ircbas_myhost(ibhnd_t hnd)
{
	return hnd->myhost;
}

bool
ircbas_service(ibhnd_t hnd)
{
	return hnd->service;
}

const char*
ircbas_umodes(ibhnd_t hnd)
{
	return hnd->umodes;
}

const char*
ircbas_cmodes(ibhnd_t hnd)
{
	return hnd->cmodes;
}

const char*
ircbas_version(ibhnd_t hnd)
{
	return hnd->ver;
}

const char*
ircbas_lasterror(ibhnd_t hnd)
{
	return hnd->lasterr;
}
const char*
ircbas_banmsg(ibhnd_t hnd)
{
	return hnd->banmsg;
}
bool
ircbas_banned(ibhnd_t hnd)
{
	return hnd->banned;
}
bool
ircbas_colon_trail(ibhnd_t hnd)
{
	return irccon_colon_trail(hnd->con);
}

const char*
ircbas_get_proxy_host(ibhnd_t hnd)
{
	return irccon_get_proxy_host(hnd->con);
}

unsigned short
ircbas_get_proxy_port(ibhnd_t hnd)
{
	return irccon_get_proxy_port(hnd->con);
}

int
ircbas_get_proxy_type(ibhnd_t hnd)
{
	return irccon_get_proxy_type(hnd->con);
}

const char*
ircbas_get_host(ibhnd_t hnd)
{
	return irccon_get_host(hnd->con);
}

unsigned short
ircbas_get_port(ibhnd_t hnd)
{
	return irccon_get_port(hnd->con);
}

const char*
ircbas_get_pass(ibhnd_t hnd)
{
	return hnd->pass;
}

const char*
ircbas_get_uname(ibhnd_t hnd)
{
	return hnd->uname;
}

const char*
ircbas_get_fname(ibhnd_t hnd)
{
	return hnd->fname;
}

unsigned
ircbas_get_conflags(ibhnd_t hnd)
{
	return hnd->conflags;
}

const char*
ircbas_get_nick(ibhnd_t hnd)
{
	return hnd->nick;
}

bool
ircbas_get_service_connect(ibhnd_t hnd)
{
	return hnd->serv_con;
}

const char*
ircbas_get_service_dist(ibhnd_t hnd)
{
	return hnd->serv_dist;
}

long
ircbas_get_service_type(ibhnd_t hnd)
{
	return hnd->serv_type;
}

const char*
ircbas_get_service_info(ibhnd_t hnd)
{
	return hnd->serv_info;
}

static void
mutilate_nick(char *nick, size_t nick_sz)
{if (nick_sz){} //XXX
	size_t len = strlen(nick);
	if (len < 9) {
		nick[len++] = '_';
		nick[len] = '\0';
	} else {
		char last = nick[len-1];
		if (last == '9')
			nick[1 + rand() % (len-1)] = '0' + rand() % 10;
		else if ('0' <= last && last <= '8')
			nick[len - 1] = last + 1;
		else
			nick[len - 1] = '0';
	}
}

static bool
send_logon(ibhnd_t hnd)
{
	if (!irccon_online(hnd->con))
		return false;
	char aBuf[256];
	char *pBuf = aBuf;
	aBuf[0] = '\0';
	int rem = (int)sizeof aBuf;
	int r;

	if (hnd->pass && strlen(hnd->pass) > 0) {
		r = snprintf(pBuf, rem, "PASS :%s\r\n", hnd->pass);
		rem -= r;
		pBuf += r;
	}

	if (rem <= 0)
		return false;

	if (hnd->service) {
		r = snprintf(pBuf, rem, "SERVICE %s 0 %s %ld 0 :%s\r\n",
		    hnd->nick, hnd->serv_dist, hnd->serv_type,
		    hnd->serv_info);
	} else {
		r = snprintf(pBuf, rem, "NICK %s\r\nUSER %s %u * :%s\r\n",
		    hnd->nick, hnd->uname, hnd->conflags, hnd->fname);
	}

	rem -= r;
	pBuf += r;
	if (rem < 0)
		return false;

	return ircbas_write(hnd, aBuf);
}

static bool
onread(ibhnd_t hnd, char **tok, size_t tok_len)
{
	char nick[128];
	if (strcmp(tok[1], "NICK") == 0) {
		if (!pfx_extract_nick(nick, sizeof nick, tok[0]))
			return false;

		if (!istrcasecmp(nick, hnd->mynick, hnd->casemapping))
			ic_strNcpy(hnd->mynick, tok[2],sizeof hnd->mynick);
	} else if (strcmp(tok[1], "ERROR") == 0) {
		XFREE(hnd->lasterr);
		hnd->lasterr = XSTRDUP(tok[2]);
	} else if (strcmp(tok[1], "005") == 0) {
		for (size_t z = 3; z < tok_len; ++z) {
			if (!tok[z]) break;

			if (!strncasecmp(tok[z], "CASEMAPPING=", 12)) {
				const char *value = tok[z] + 12;
				if (strcasecmp(value, "ascii") == 0) {
					hnd->casemapping = CMAP_ASCII;
				} else if (strcasecmp(value,
				    "strict-rfc1459") == 0) {
					hnd->casemapping
					    = CMAP_STRICT_RFC1459;
				} else {
					hnd->casemapping = CMAP_RFC1459;
				}
			} else if (!strncasecmp(tok[z], "PREFIX=", 7)) {
				char *str = XSTRDUP(tok[z] + 8);
				char *p = strchr(str, ')');
				*p++ = '\0';
				XFREE(hnd->m005modepfx[0]);
				hnd->m005modepfx[0] = XSTRDUP(str);

				XFREE(hnd->m005modepfx[1]);
				hnd->m005modepfx[1] = XSTRDUP(p);

				XFREE(str);
			} else if (!strncasecmp(tok[z], "CHANMODES=", 10)){
				for (int z = 0; z < 4; ++z) {
					XFREE(hnd->m005chanmodes[z]);
					hnd->m005chanmodes[z] = NULL;
				}

				int c = 0;
				char *argbuf = XSTRDUP(tok[z] + 10);
				char *ptr = strtok(argbuf, ",");

				while (ptr) {
					if (c < 4)
						hnd->m005chanmodes[c++] =
						    XSTRDUP(ptr);
					ptr = strtok(NULL, ",");
				}

				if (c != 4) {
					W("005 chanmodes parse element: "
					    "expected 4 params, got %i. "
					    "arg: \"%s\"", c, tok[z] + 10);
				}

				XFREE(argbuf);
			}
		}
	}
	return true;
}

const char *const *const*
ircbas_logonconv(ibhnd_t hnd)
{
	return (const char *const *const*)hnd->logonconv;
}

const char *const*
ircbas_005chanmodes(ibhnd_t hnd)
{
	return (const char *const*)hnd->m005chanmodes;
}

const char *const*
ircbas_005modepfx(ibhnd_t hnd)
{
	return (const char *const*)hnd->m005modepfx;
}


static char**
clonearr(char **arr, size_t nelem)
{
	char **res = XMALLOC((nelem+1) * sizeof *arr);
	for(size_t i = 0; i < nelem; i++)
		res[i] = arr[i] ? XSTRDUP(arr[i]) : NULL;
	res[nelem] = NULL;
	return res;
}


static void
freearr(char **arr, size_t nelem)
{
	if (arr) {
		for(size_t i = 0; i < nelem; i++)
			XFREE(arr[i]);
		XFREE(arr);
	}
}
