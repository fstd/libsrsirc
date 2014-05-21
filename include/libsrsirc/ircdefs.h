/* msghandle.h - protocl message handler prototypes
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef IRCDEFS_H
#define IRCDEFS_H 1

#ifdef WITH_SSL
/* ssl */
# include <openssl/ssl.h>
# include <openssl/err.h>
#endif

#define MAX_IRCARGS ((size_t)15)

#define RXBUF_SZ 4096
#define LINEBUF_SZ 1024
#define OVERBUF_SZ 1024

#define DEF_HOST "localhost"
#define DEF_PORT 6667

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


typedef struct ichnd* ichnd_t;
typedef struct ibhnd* ibhnd_t;

typedef bool (*fp_con_read)(char *(*msg)[MAX_IRCARGS], void* tag);
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

	char *(*logonconv[4])[MAX_IRCARGS];
	char m005chanmodes[4][64];
	char m005modepfx[2][32];

	fp_con_read cb_con_read;
	void *tag_con_read;
	fp_mut_nick cb_mut_nick;

	struct ichnd *con;
};


#endif /* IRCDEFS_H */
