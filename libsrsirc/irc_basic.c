/* irc_basic.c - Front-end implementation
 * libsrsirc - a lightweight serious IRC lib - (C) 2012, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE 1

#include <libsrsirc/irc_basic.h>

#include <common.h>
#include <libsrsirc/irc_con.h>
#include <libsrsirc/irc_util.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "debug.h"

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

#define XCALLOC(num) ic_xcalloc(1, num)
#define XMALLOC(num) ic_xmalloc(num)
#define XREALLOC(p, num) ic_xrealloc((p),(num))
#define XFREE(p) do{  if(p) free(p); p=0;  }while(0)
#define XSTRDUP(s) ic_xstrdup(s)


struct ibhnd
{
	char *mynick;
	char *myhost;
	bool service;
	char *umodes;
	char *cmodes;
	char *ver;
	char *lasterr;

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
static char *strmdup(const char *str, size_t minlen);
static bool send_logon(ibhnd_t hnd);

static bool onread(ibhnd_t hnd, char **tok, size_t tok_len);
static char** clonearr(char **arr, size_t nelem);
static void freearr(char **arr, size_t nelem);

bool 
ircbas_regcb_mutnick(ibhnd_t hnd, fp_mut_nick cb)
{
	if (!irccon_valid(hnd->con))
		return false;
	hnd->cb_mut_nick = cb;
	return true;
}

bool 
ircbas_regcb_conread(ibhnd_t hnd, fp_con_read cb, void *tag)
{
	if (!irccon_valid(hnd->con))
		return false;
	hnd->cb_con_read = cb;
	hnd->tag_con_read = tag;
	return true;
}

ibhnd_t 
ircbas_init(void)
{
	ichnd_t con = irccon_init();
	if (!con)
		return NULL;

	ibhnd_t r = XMALLOC(sizeof (*(ibhnd_t)0));


	r->con = con;
	/* persistent after reset */
	r->mynick = NULL;
	r->myhost = NULL;
	r->service = false;
	r->umodes = NULL;
	r->cmodes = NULL;
	r->ver = NULL;
	r->lasterr = NULL;

	r->casemapping = CASEMAPPING_RFC1459;

	r->restricted = false;
	r->banned = false;
	r->banmsg = NULL;

	r->pass = XSTRDUP(DEF_PASS);
	r->nick = strmdup(DEF_NICK, 9); //ensure room for mutilate_nick
	r->uname = XSTRDUP(DEF_UNAME);
	r->fname = XSTRDUP(DEF_FNAME);
	r->conflags = DEF_CONFLAGS;
	r->serv_con = false;
	r->serv_dist = XSTRDUP(DEF_SERV_DIST);
	r->serv_type = DEF_SERV_TYPE;
	r->serv_info = XSTRDUP(DEF_SERV_INFO);

	r->cb_con_read = NULL;
	r->tag_con_read = NULL;
	r->cb_mut_nick = mutilate_nick;

	for(int i = 0; i < 4; i++)
		r->logonconv[i] = NULL;

	r->m005chanmodes[0] = XSTRDUP("b");
	r->m005chanmodes[1] = XSTRDUP("k");
	r->m005chanmodes[2] = XSTRDUP("l");
	r->m005chanmodes[3] = XSTRDUP("psitnm");

	r->m005modepfx[0] = XSTRDUP("ov");
	r->m005modepfx[1] = XSTRDUP("@+");

	WVX("(%p) irc_bas initialized (backend: %p)", r, r->con);
	return r;
}

bool 
ircbas_reset(ibhnd_t hnd)
{
	WVX("(%p) resetting backend", hnd);
	if (!irccon_reset(hnd->con))
		return false;

	return true;
}

bool 
ircbas_dispose(ibhnd_t hnd)
{
	if (!irccon_dispose(hnd->con))
		return false;

	XFREE(hnd->mynick);
	XFREE(hnd->myhost);
	XFREE(hnd->umodes);
	XFREE(hnd->cmodes);
	XFREE(hnd->ver);
	XFREE(hnd->lasterr);
	XFREE(hnd->banmsg);

	XFREE(hnd->pass);
	XFREE(hnd->nick);
	XFREE(hnd->uname);
	XFREE(hnd->fname);
	XFREE(hnd->serv_dist);
	XFREE(hnd->serv_info);

	WVX("(%p) disposed", hnd);
	XFREE(hnd);

	return true;
}

bool 
ircbas_connect(ibhnd_t hnd, unsigned long to_us)
{
	int64_t tsend = to_us ? ic_timestamp_us() + to_us : 0;
	XFREE(hnd->lasterr);
	hnd->lasterr = NULL;
	XFREE(hnd->banmsg);
	hnd->banmsg = NULL;
	hnd->banned = false;

	for(int i = 0; i < 4; i++) {
		freearr(hnd->logonconv[i], MAX_IRCARGS);
		hnd->logonconv[i] = NULL;
	}

	WVX("(%p) wanna connect, connecting backend (timeout: %lu)", hnd, to_us);
	if (!irccon_connect(hnd->con, to_us)) {
		WX("(%p) backend failed to establish connection", hnd);
		return false;
	}

	WVX("(%p) sending IRC logon sequence", hnd);
	if (!send_logon(hnd)) {
		WX("(%p) failed writing IRC logon sequence", hnd);
		ircbas_reset(hnd);
		return false;
	}

	WVX("(%p) connection established, IRC logon sequence sent", hnd);
	char *msg[MAX_IRCARGS];
	XFREE(hnd->mynick);
	hnd->mynick = strmdup(hnd->nick, 9);

	int64_t trem = 0;
	for(;;)
	{
		if (irccon_canceled(hnd->con)) {
			WX("(%p) cancel requested", hnd);
			ircbas_reset(hnd);
			return false;
		}
		if(tsend) {
			trem = tsend - ic_timestamp_us();
			if (trem <= 0) {
				WX("(%p) timeout hit while waiting for 004", hnd);
				ircbas_reset(hnd);
				return false;
			}
		}

		if (trem > 500000)
			trem = 500000; //limiting time spent in read so we can cancel w/o much delay

		int r = irccon_read(hnd->con, msg, MAX_IRCARGS, (unsigned long)trem);
		if (r < 0)
		{
			WX("(%p) irccon_read() failed", hnd);
			ircbas_reset(hnd);
			return false;
		}

		if (r == 0)
			continue;

		if (hnd->cb_con_read && !hnd->cb_con_read(msg, MAX_IRCARGS, hnd->tag_con_read)) {
			WX("(%p) further logon prohibited by conread", hnd);
			ircbas_reset(hnd);
			return false;
		}

		if (strcmp(msg[1], "001") == 0)
		{
			hnd->logonconv[0] = clonearr(msg, MAX_IRCARGS);
			XFREE(hnd->mynick);
			hnd->mynick = XSTRDUP(msg[2]);
			char *tmp;
			if ((tmp = strchr(hnd->mynick, '@')))
				*tmp = '\0';
			if ((tmp = strchr(hnd->mynick, '!')))
				*tmp = '\0';

			XFREE(hnd->myhost);//XXX ensure argcount
			XFREE(hnd->umodes);
			XFREE(hnd->cmodes);
			XFREE(hnd->ver);
			hnd->myhost = XSTRDUP(msg[0] ? msg[0] 
			                                : irccon_get_host(hnd->con));
			hnd->umodes = XSTRDUP(DEF_UMODES);
			hnd->cmodes = XSTRDUP(DEF_CMODES);
			hnd->ver = XSTRDUP("");
			hnd->service = false;
		}
		else if (strcmp(msg[1], "002") == 0)
		{
			hnd->logonconv[1] = clonearr(msg, MAX_IRCARGS);
		}
		else if (strcmp(msg[1], "003") == 0)
		{
			hnd->logonconv[2] = clonearr(msg, MAX_IRCARGS);
		}
		else if (strcmp(msg[1], "004") == 0)
		{
			hnd->logonconv[3] = clonearr(msg, MAX_IRCARGS);
			XFREE(hnd->myhost);//XXX ensure argcount
			hnd->myhost = XSTRDUP(msg[3]);
			XFREE(hnd->umodes);
			hnd->umodes = XSTRDUP(msg[5]);
			XFREE(hnd->cmodes);
			hnd->cmodes = XSTRDUP(msg[6]);
			XFREE(hnd->ver);
			hnd->ver = XSTRDUP(msg[4]);
			WVX("(%p) got beloved 004", hnd);
			break;
		}
		else if (strcmp(msg[1], "PING") == 0)
		{
			char buf[64];
			snprintf(buf, sizeof buf, "PONG :%s\r\n", msg[2]);
			if (!irccon_write(hnd->con, buf))
			{
				WX("(%p) write failed (1)", hnd);
				ircbas_reset(hnd);
				return false;
			}
		}
		else if ((strcmp(msg[1], "432") == 0)//ERR_ERRONEUSNICKNAME
		      || (strcmp(msg[1], "433") == 0)//ERR_NICKNAMEINUSE
		      || (strcmp(msg[1], "436") == 0)//ERR_NICKCOLLISION
		      || (strcmp(msg[1], "437") == 0))//ERR_UNAVAILRESOURCE
		{
			if (!hnd->cb_mut_nick)
			{
				WX("(%p) got no mutnick, wat do? (failing)", hnd);
				ircbas_reset(hnd);
				return false;
			}
			hnd->cb_mut_nick(hnd->mynick, 10); //XXX hc
			char buf[64];
			snprintf(buf,sizeof buf,"NICK %s\r\n",hnd->mynick);
			if (!irccon_write(hnd->con, buf))
			{
				WX("(%p) write failed (2)", hnd);
				ircbas_reset(hnd);
				return false;
			}
		}
		else if (strcmp(msg[1], "464") == 0)/*ERR_PASSWDMISMATCH*/
		{
			ircbas_reset(hnd);
			WX("(%p) wrong server password", hnd);
			return false;
		}
		else if (strcmp(msg[1], "383") == 0)/*RPL_YOURESERVICE*/
		{
			XFREE(hnd->mynick);
			hnd->mynick = XSTRDUP(msg[2]);
			char *tmp;
			if ((tmp = strchr(hnd->mynick, '@')))
				*tmp = '\0';
			if ((tmp = strchr(hnd->mynick, '!')))
				*tmp = '\0';

			hnd->myhost = XSTRDUP(msg[0] ? msg[0] 
			                                : irccon_get_host(hnd->con));
			hnd->umodes = XSTRDUP(DEF_UMODES);
			hnd->cmodes = XSTRDUP(DEF_CMODES);
			hnd->ver = XSTRDUP("");
			hnd->service = true;
			break;
		}
		else if (strcmp(msg[1], "484") == 0)/*ERR_RESTRICTED*/
		{
			hnd->restricted = true;
		}
		else if (strcmp(msg[1], "465") == 0)//ERR_YOUREBANNEDCREEP
		{
			WX("(%p) we're banned", hnd);
			hnd->banned = true;
			XFREE(hnd->banmsg);
			hnd->banmsg = XSTRDUP(msg[3]?msg[3]:"");
		}
		else if (strcmp(msg[1], "466") == 0)//ERR_YOUWILLBEBANNED
		{
			WX("(%p) we will be banned", hnd); //XXX not strictly part of irc
		}
		else if (strcmp(msg[1], "ERROR") == 0)/*ERR_RESTRICTED*/
		{
			XFREE(hnd->lasterr);
			hnd->lasterr = XSTRDUP(msg[2]?msg[2]:"");
			WX("(%p) received error while logging on: %s", hnd, msg[2]);
			ircbas_reset(hnd);
			return false;
		}
	}
	WVX("(%p) irc logon finished, U R online", hnd);
	return true;
}

void
ircbas_cancel(ibhnd_t hnd)
{
	WVX("(%p) async cancel requested", hnd);
	irccon_cancel(hnd->con);
}

int 
ircbas_read(ibhnd_t hnd, char **tok, size_t tok_len, unsigned long to_us)
{
	//WVX("(%p) wanna read (timeout: %lu)", hnd, to_us);
	int r = irccon_read(hnd->con, tok, tok_len, to_us);

	if (r == -1 || (r != 0 && !onread(hnd, tok, tok_len)))
	{
		WX("(%p) irccon_read() failed or onread() denied (r:%d)", hnd, r);
		ircbas_reset(hnd);
		return -1;
	}
	//WVX("(%p) done reading", hnd);

	return r;
}

bool 
ircbas_write(ibhnd_t hnd, const char *line)
{
	bool r = irccon_write(hnd->con, line);

	if (!r) {
		WX("(%p) irccon_write() failed", hnd);
		ircbas_reset(hnd);
	}

	return r;
}

bool 
ircbas_online(ibhnd_t hnd)
{
	return irccon_online(hnd->con);
}

int
ircbas_sockfd(ibhnd_t hnd)
{
	return irccon_sockfd(hnd->con);
}

bool 
ircbas_set_pass(ibhnd_t hnd, const char *srvpass)
{
	if (!irccon_valid(hnd->con))
		return false;
	XFREE(hnd->pass);
	hnd->pass = XSTRDUP(srvpass);
	return true;
}

bool 
ircbas_set_uname(ibhnd_t hnd, const char *uname)
{
	if (!irccon_valid(hnd->con))
		return false;
	XFREE(hnd->uname);
	hnd->uname = XSTRDUP(uname);
	return true;
}

bool 
ircbas_set_fname(ibhnd_t hnd, const char *fname)
{
	if (!irccon_valid(hnd->con))
		return false;
	XFREE(hnd->fname);
	hnd->fname = XSTRDUP(fname);
	return true;
}

bool 
ircbas_set_conflags(ibhnd_t hnd, unsigned flags)
{
	if (!irccon_valid(hnd->con))
		return false;
	hnd->conflags = flags;
	return true;
}

bool 
ircbas_set_nick(ibhnd_t hnd, const char *nick)
{
	if (!irccon_valid(hnd->con))
		return false;
	XFREE(hnd->nick);
	hnd->nick = XSTRDUP(nick);
	return true;
}

bool 
ircbas_set_service_connect(ibhnd_t hnd, bool enabled)
{
	if (!irccon_valid(hnd->con))
		return false;
	hnd->serv_con = enabled;
	return true;
}

bool 
ircbas_set_service_dist(ibhnd_t hnd, const char *dist)
{
	if (!irccon_valid(hnd->con))
		return false;
	XFREE(hnd->serv_dist);
	hnd->serv_dist = XSTRDUP(dist);
	return true;
}

bool 
ircbas_set_service_type(ibhnd_t hnd, long type)
{
	if (!irccon_valid(hnd->con))
		return false;
	hnd->serv_type = type;
	return true;
}

bool 
ircbas_set_service_info(ibhnd_t hnd, const char *info)
{
	if (!irccon_valid(hnd->con))
		return false;
	XFREE(hnd->serv_info);
	hnd->serv_info = XSTRDUP(info);
	return true;
}

bool 
ircbas_set_proxy(ibhnd_t hnd, const char *host, unsigned short port, int ptype)
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
	if (!irccon_valid(hnd->con))
		return -1;
	return hnd->casemapping;
}

const char*
ircbas_mynick(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->mynick;
}

const char*
ircbas_myhost(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->myhost;
}

bool 
ircbas_service(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->service;
}

const char*
ircbas_umodes(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->umodes;
}

const char*
ircbas_cmodes(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->cmodes;
}

const char*
ircbas_version(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->ver;
}

const char*
ircbas_lasterror(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->lasterr;
}
const char*
ircbas_banmsg(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->banmsg;
}
bool
ircbas_banned(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return false;
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
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->pass;
}

const char*
ircbas_get_uname(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->uname;
}

const char*
ircbas_get_fname(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->fname;
}

unsigned 
ircbas_get_conflags(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return 0;
	return hnd->conflags;
}

const char*
ircbas_get_nick(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->nick;
}

bool 
ircbas_get_service_connect(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return false;
	return hnd->serv_con;
}

const char*
ircbas_get_service_dist(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->serv_dist;
}

long 
ircbas_get_service_type(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return 0;
	return hnd->serv_type;
}

const char*
ircbas_get_service_info(ibhnd_t hnd)
{
	if (!irccon_valid(hnd->con))
		return NULL;
	return hnd->serv_info;
}

static void 
mutilate_nick(char *nick, size_t nick_sz)
{if (nick_sz){} //XXX
	size_t len = strlen(nick);
	if (len < 9)
	{
		nick[len++] = '_';
		nick[len] = '\0';
	}
	else
	{
		char last = nick[len-1];
		if  (last == '9')
			nick[rand() % len] = '0' + rand() % 10;
		else if ('0' <= last && last <= '8')
			nick[len - 1] = last + 1;
		else
			nick[len - 1] = '0';
	}
}

static char*
strmdup(const char *str, size_t minlen)
{
	size_t len = strlen(str);
	char *s = XCALLOC((len > minlen ? len : minlen) + 1);
	strcpy(s, str);
	return s;
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

	if (hnd->pass && strlen(hnd->pass) > 0)
	{
		r = snprintf(pBuf, rem, "PASS :%s\r\n", hnd->pass);
		rem -= r;
		pBuf += r;
	}
	
	if (rem <= 0)
		return false;

	if (hnd->service)
	{
		r = snprintf(pBuf, rem, "SERVICE %s 0 %s %ld 0 :%s\r\n", 
		                hnd->nick, hnd->serv_dist,
		                hnd->serv_type, hnd->serv_info);
	}
	else
	{
		r = snprintf(pBuf, rem, "NICK %s\r\nUSER %s %u * :%s\r\n", 
		                hnd->nick, hnd->uname,
		                hnd->conflags, hnd->fname);
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
	if (strcmp(tok[1], "NICK") == 0)
	{
		if (!pfx_extract_nick(nick, sizeof nick, tok[0]))
			return false;

		if (istrcasecmp(nick, hnd->mynick, hnd->casemapping) == 0) {
			XFREE(hnd->mynick);
			hnd->mynick = XSTRDUP(tok[2]);
		}
	}
	else if (strcmp(tok[1], "ERROR") == 0)
	{
		XFREE(hnd->lasterr);
		hnd->lasterr = XSTRDUP(tok[2]);
	}
	else if (strcmp(tok[1], "005") == 0)
	{
		for (size_t z = 3; z < tok_len; ++z)
		{
			if (!tok[z]) break;

			if (strncasecmp(tok[z], "CASEMAPPING=", 12) == 0)
			{
				const char *value = tok[z] + 12;
				if (strcasecmp(value, "ascii") == 0) {
					hnd->casemapping = CASEMAPPING_ASCII;
				} else if (strcasecmp(value, "strict-rfc1459") == 0) {
					hnd->casemapping = CASEMAPPING_STRICT_RFC1459;
				} else {
					hnd->casemapping = CASEMAPPING_RFC1459;
				}
			}
			else if (strncasecmp(tok[z], "PREFIX=", 7) == 0)
			{
				char *str = strdup(tok[z] + 8);
				char *p = strchr(str, ')');
				*p++ = '\0';
				XFREE(hnd->m005modepfx[0]);
				hnd->m005modepfx[0] = XSTRDUP(str);

				XFREE(hnd->m005modepfx[1]);
				hnd->m005modepfx[1] = XSTRDUP(p);

				free(str);
			}
			else if (strncasecmp(tok[z], "CHANMODES=", 10) == 0)
			{
				for (int z = 0; z < 4; ++z) {
					XFREE(hnd->m005chanmodes[z]);
					hnd->m005chanmodes[z] = NULL;
				}

				int c = 0;
				char* argbuf = strdup(tok[z] + 10);
				char *ptr = strtok(argbuf, ",");

				while (ptr) {
					if (c < 4)
						hnd->m005chanmodes[c++]=XSTRDUP(ptr);
					ptr = strtok(NULL, ",");
				}

				if (c != 4) {
					WX("005 chanmodes parse element: expected 4 parameters, got %i. arg is: \"%s\"", c, tok[z] + 10);
				}

				free(argbuf);
			}
		}
	}
	return true;
}

const char* const* const*
ircbas_logonconv(ibhnd_t hnd)
{
	return (const char* const* const*)hnd->logonconv;
}

const char* const*
ircbas_005chanmodes(ibhnd_t hnd)
{
	return (const char* const*)hnd->m005chanmodes;
}

const char* const*
ircbas_005modepfx(ibhnd_t hnd)
{
	return (const char* const*)hnd->m005modepfx;
}


static char**
clonearr(char **arr, size_t nelem)
{
	char **res = malloc((nelem+1) * sizeof *arr);
	for(size_t i = 0; i < nelem; i++)
		res[i] = arr[i] ? strdup(arr[i]) : NULL;
	res[nelem] = NULL;
	return res;
}


static void
freearr(char **arr, size_t nelem)
{
	if (arr) {
		for(size_t i = 0; i < nelem; i++)
			free(arr[i]);
		free(arr);
	}
}
