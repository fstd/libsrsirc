/* defs.h - definitions, data types, etc
 * libsrsirc - a lightweight serious IRC lib - (C) 2014, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_DEFS_H
#define LIBSRSIRC_IRC_DEFS_H 1

#include <stdbool.h>
#include <stdint.h>

#ifdef WITH_SSL
/* ssl */
# include <openssl/ssl.h>
# include <openssl/err.h>
#endif

/* max number of tokens as per RFC1459/2812 */
#define MAX_IRCARGS ((size_t)17)

/* default server to connect to */
#define DEF_HOST "localhost"

/* default TCP port for plaintext IRC */
#define DEF_PORT_PLAIN ((uint16_t)6667)

/* default TCP port for SSL IRC */
#define DEF_PORT_SSL ((uint16_t)6697)

/* default nickname to use */
#define DEF_NICK "srsirc"

/* default IRC user name */
#define DEF_UNAME "bsnsirc"

/* default IRC full name */
#define DEF_FNAME "serious business irc"

/* default USER message flags */
#define DEF_CONFLAGS 0

/* default `distribution' for service logon */
#define DEF_SERV_DIST "*"

/* default `distribution; string for service logon */
#define DEF_SERV_TYPE 0

/* default `info' string for service logon */
#define DEF_SERV_INFO "srsbsns srvc"

/* hard connect timeout in us (overall) */
#define DEF_HCTO_US 120000000ul

/* soft connect timeout (per A/AAAA record) */
#define DEF_SCTO_US 15000000ul

/* the three `case mappings' 005 ISUPPORT "specify" */
#define CMAP_RFC1459 0        /* [}{|A-Z] are uppercase [][\a-z] */
#define CMAP_STRICT_RFC1459 1 /* [}{|^A-Z] are uppercase [][\~a-z] */
#define CMAP_ASCII 2          /* [A-Z] are uppercase [a-z] */

/* proxy type constants */
#define IRCPX_HTTP 0
#define IRCPX_SOCKS4 1 /* NOT socks4a */
#define IRCPX_SOCKS5 2


/* the main handle type for our irc objects - this is what irc_init() returns */
typedef struct irc_s* irc;

/* tokarr is the type we store pointers to a tokenized IRC protocol msg in */
typedef char *tokarr[MAX_IRCARGS];

/* type of the function which is called on IRC protocol messages read at logon
 * time.  Return false to abort the attempt to log on. see irc_regcb_conread */
typedef bool (*fp_con_read)(tokarr *msg, void* tag);

/* type of the function to come up with new nicknames, if at logon time our
 * desired nickname is unavailable. see irc_regcb_mutnick */
typedef void (*fp_mut_nick)(char *nick, size_t nick_sz);

#endif /* LIBSRSIRC_IRC_DEFS_H */
