/* ihnd.h 
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef IHND_H
#define IHND_H 1

#ifdef WITH_SSL
/* ssl */
# include <openssl/ssl.h>
# include <openssl/err.h>
#endif

#include "ircdefs.h"

typedef struct ichnd* ichnd_t;
typedef struct ibhnd* ibhnd_t;

typedef bool (*fp_con_read)(char **msg, size_t msg_len, void* tag);
typedef void (*fp_mut_nick)(char *nick, size_t nick_sz);

struct ichnd
{
	char *host;
	unsigned short port;

	char *phost;
	unsigned short pport;
	int ptype;

	int sck;
	int state;

	char *linebuf;
	char *overbuf;
	char *mehptr;
	bool colon_trail;
#ifdef WITH_SSL
	bool ssl;
	SSL_CTX *sctx;
	SSL *shnd;
#endif
};


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
	char m005chanmodes[4][64];
	char m005modepfx[2][32];

	fp_con_read cb_con_read;
	void *tag_con_read;
	fp_mut_nick cb_mut_nick;

	struct ichnd *con;
};


#endif /* IHND_H */

