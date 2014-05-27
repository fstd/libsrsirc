/* intdefs.h - internal definitions, data types, etc
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_INTDEFS_H
#define LIBSRSIRC_IRC_INTDEFS_H 1

#include <stdbool.h>
#include <stdint.h>

/* receive buffer size */
#define WORKBUF_SZ 4096

/* default supported user modes (as per the RFC noone cares about...) */
#define DEF_UMODES "iswo"

/* default supported channel modes (as per the RFC noone cares about...) */
#define DEF_CMODES "opsitnml"

/* arbitrary but empirically established */
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

/* this allows us to handle both plaintext and ssl connections the same way */
typedef struct sckhld {
	int sck;
	SSLTYPE shnd;
} sckhld;

/* read context structure - holds the receive buffer, primarily*/
struct readctx {
	char workbuf[WORKBUF_SZ];
	char *wptr; /* pointer to begin of current valid data */
	char *eptr; /* pointer to one after end of current valid data */
};

/* this is a relict of the former design */
typedef struct iconn_s* iconn;
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

/* this is our main IRC object context structure (typedef'd as `*irc') */
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
	uint64_t scto_us;/*overall irc_connect() timeout*/

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

