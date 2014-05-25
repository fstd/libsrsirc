/* irc_defs.h - definitions, data types, etc
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_DEFS_H
#define LIBSRSIRC_IRC_DEFS_H 1

#include <stdint.h>

#ifdef WITH_SSL
/* ssl */
# include <openssl/ssl.h>
# include <openssl/err.h>
#endif

#define MAX_IRCARGS ((size_t)17)

#define WORKBUF_SZ 4096

#define DEF_HOST "localhost"
#define DEF_PORT ((uint16_t)6667)

#define DEF_PASS ""
#define DEF_NICK "srsirc"
#define DEF_UNAME "bsnsirc"
#define DEF_FNAME "serious business irc"
#define DEF_CONFLAGS 0
#define DEF_SERV_DIST "*"
#define DEF_SERV_TYPE 0
#define DEF_SERV_INFO "serious business irc service"
#define DEF_HCTO_US 120000000ul /* hard connect timeout (overall) */
#define DEF_SCTO_US 15000000ul /* soft connect timeout (per A/AAAA record) */
#define DEF_UMODES "iswo"
#define DEF_CMODES "opsitnml"


#define MAX_NICK_LEN 64
#define MAX_HOST_LEN 128
#define MAX_UMODES_LEN 64
#define MAX_CMODES_LEN 64
#define MAX_VER_LEN 128


#ifdef WITH_SSL
typedef SSL *SSLTYPE;
typedef SSL_CTX *SSLCTXTYPE;
#else
typedef void *SSLTYPE;
typedef void *SSLCTXTYPE;
#endif

typedef struct sckhld {
	int sck;
	SSLTYPE shnd;
} sckhld;

typedef struct iconn_s* iconn;
typedef struct irc_s* irc;

typedef char *tokarr[MAX_IRCARGS];

typedef bool (*fp_con_read)(tokarr *msg, void* tag);
typedef void (*fp_mut_nick)(char *nick, size_t nick_sz);

struct readctx {
	char workbuf[WORKBUF_SZ];
	char *wptr; //pointer to begin of current valid data
	char *eptr; //pointer to one after end of current valid data
};

struct iconn_s {
	char *host;
	uint16_t port;

	char *phost;
	uint16_t pport;
	int ptype;

	sckhld sh;
	int state;

	struct readctx rctx;
	bool colon_trail;
	bool ssl;
	SSLCTXTYPE sctx;
};


struct irc_s {
	char mynick[MAX_NICK_LEN];
	char myhost[MAX_HOST_LEN];
	bool service;
	char umodes[MAX_UMODES_LEN];
	char cmodes[MAX_CMODES_LEN];
	char ver[MAX_VER_LEN];
	char *lasterr;

	/* zero timeout means no timeout */
	uint64_t hcto_us;/*connect() timeout per A/AAAA record*/
	uint64_t scto_us;/*overall ircbas_connect() timeout*/

	bool restricted;
	bool banned;
	char *banmsg;

	int casemapping;

	char *pass;
	char *nick;
	char *uname;
	char *fname;
	uint8_t conflags;
	bool serv_con;
	char *serv_dist;
	long serv_type;
	char *serv_info;

	tokarr *logonconv[4];
	char m005chanmodes[4][64];
	char m005modepfx[2][32];

	fp_con_read cb_con_read;
	void *tag_con_read;
	fp_mut_nick cb_mut_nick;

	struct iconn_s *con;
};


#endif /* LIBSRSIRC_IRC_DEFS_H */
