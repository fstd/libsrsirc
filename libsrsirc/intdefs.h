/* intdefs.h - internal definitions, data types, etc
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-19, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_INTDEFS_H
#define LIBSRSIRC_IRC_INTDEFS_H 1


#include <stdbool.h>
#include <stdint.h>

#include <platform/base_net.h>

#include "skmap.h"

/* receive buffer size */
#define WORKBUF_SZ 4095

/* default supported user modes (as per the RFC noone cares about...) */
#define DEF_UMODES "iswo"

/* default supported channel modes (as per the RFC noone cares about...) */
#define DEF_CMODES "opsitnml"

/* arbitrary but empirically established */
#define MAX_NICK_LEN 64
#define MAX_UNAME_LEN 64
#define MAX_HOST_LEN 128
#define MAX_UMODES_LEN 64
#define MAX_CMODES_LEN 64
#define MAX_VER_LEN 128

/* not so empirically established */
#define MAX_005_CHMD 64
#define MAX_005_MDPFX 32
#define MAX_005_CHTYP 16
#define MAX_CHAN_LEN 256
#define MAX_MODEPFX 8
#define MAX_V3TAGS 16 // IRCv3 message tags
#define MAX_V3CAPS 16
#define MAX_V3TAGLEN 512
#define MAX_V3CAPLEN 128
#define MAX_V3CAPLINE 512


/* this allows us to handle both plaintext and ssl connections the same way */
typedef struct sckhld {
	int sck;
	SSLTYPE shnd;
} sckhld;

/* read context structure - holds the receive buffer, primarily*/
struct readctx {
	char workbuf[WORKBUF_SZ + 1];
	char *wptr; /* pointer to begin of current valid data */
	char *eptr; /* pointer to one after end of current valid data */
};


/* protocol message handler function pointers */
typedef uint16_t (*hnd_fn)(irc *ctx, tokarr *msg, size_t nargs, bool logon);
struct msghnd {
	char cmd[32];
	hnd_fn hndfn;
	const char *module;
};

/* protocol message handler function pointers */
struct umsghnd {
	char cmd[32];
	uhnd_fn hndfn;
};

struct v3tag
{
	const char *key;
	const char *value;
};

struct v3cap
{
	char name[MAX_V3CAPLEN];
	char adddata[MAX_V3CAPLEN];
	bool musthave;
	bool offered;
	bool enabled;
};

/* this is a relict of the former design */
typedef struct iconn_s iconn;
struct iconn_s {
	char *host;
	uint16_t port;
	char *laddr;
	uint16_t lport;

	char *phost;
	uint16_t pport;
	int ptype;

	sckhld sh;
	bool online;
	bool eof;

	struct readctx rctx;
	bool colon_trail;
	bool ssl;
	SSLCTXTYPE sctx;
};

/* this is our main IRC context context structure (typedef'd as `irc') */
struct irc_s {
	/* These are kept up to date as the pertinent messages are seen */
	char mynick[MAX_NICK_LEN];     // Hold our current nickname
	char myhost[MAX_HOST_LEN];     // Hostname of the server as per 004
	bool service;                  // Set if we see a 383 (RPL_YOURESERVICE)
	char cmodes[MAX_CMODES_LEN];   // Supported chanmodes as per 004
	char umodes[MAX_UMODES_LEN];   // Supported usermodes
	char myumodes[MAX_UMODES_LEN]; // Actual usermodes
	char ver[MAX_VER_LEN];         // Server version as per 004
	char *lasterr;                 // Argument of the last ERROR we received

	bool restricted; // Set if the ircd sent us a 484 (ERR_RESTRICTED)
	bool banned;     // Set if the ircd sent us a 465 (ERR_YOUREBANNEDCREEP)
	char *banmsg;    // If `banned`, this holds the argument of the 465
	int casemap;     // Character case mapping in use (CMAP_*, see defs.h)

	tokarr *logonconv[4];   // Holds 001-004 because irc_connect() eats them
	char *m005chanmodes[4]; // Supported channel modes as per 005
	char *m005modepfx[2];   // Supported channel mode prefixes as per 005
	char *m005chantypes;    // Supported channel types as per 005
	skmap *m005attrs;       // Stores all seen 005 attributes

	char *v3tags_raw[MAX_V3TAGS]; // IRCv3 tags of the last-read msg
	size_t v3ntags;         // Number of tags in the last-read msg
	char *v3tags_dec[MAX_V3TAGS]; // Decoded tag cache
	struct v3tag v3tags[MAX_V3TAGS]; // Pointers into v3tags_dec

	struct v3cap *v3caps[MAX_V3CAPS];
	char v3capreq[MAX_V3CAPLINE];
	bool starttls;
	bool starttls_first;

	/* These are set by their respective irc_set_*() functions and
	 * generally changes to these don't take effect before the next
	 * call to irc_connect() */
	char *pass;           // Server password set by irc_set_pass()
	char *nick;           // Nickname set by irc_set_nick()
	char *uname;          // Username set by irc_set_uname()
	char *fname;          // Full name set by irc_set_fname()
	char *sasl_mech;      // SASL mechanism set by irc_set_sasl()
	char *sasl_msg;       // SASL auth string set by irc_set_sasl()
	size_t sasl_msg_len;  // size (in bytes) of the SASL message
	uint8_t conflags;     // USER flags for logging on
	bool serv_con;        // Do we want to be service? irc_set_service_connect()
	char *serv_dist;      // Service logon information (distribution)
	long serv_type;       // Service logon information (service type)
	char *serv_info;      // Service logon information (service info)
	uint64_t hcto_us;     // Overall irc_connect() timeout (0=inf)
	uint64_t scto_us;     // Socket connect() timeout per A/AAAA record (0=inf)
	bool tracking;        // Do we want chan/user tracking? by irc_set_track()
	bool dumb;            // Connect only, leave logon sequence to the user



	/* These are set by registering callbacks using irc_reg*() */
	fp_con_read cb_con_read; // Callback for incoming messages at logon time
	void *tag_con_read;      // Userdata handed back to the above callback
	fp_mut_nick cb_mut_nick; // Callback for unavailable nick at logon time

	struct umsghnd *uprehnds;  // User-registered PRE message handlers
	size_t uprehnds_cnt;       // Amount of the above
	struct umsghnd *uposthnds; // User-registered POST message handlers
	size_t uposthnds_cnt;      // Amount of the above
	struct msghnd *msghnds;    // System-registered message handlers
	size_t msghnds_cnt;        // Amount of the above



	/* These are only used if irc_set_track() was used to enable tracking */
	skmap *chans;       // The channels we're aware of (or in?)
	skmap *users;       // The users we're aware of



	/* These are internal helper structures */
	bool tracking_enab;  // If `tracking`, set once we see 005 CASEMAPPING
	bool endofnames;     // Helper flag for channel names update

	struct iconn_s *con; // Connection-specifics (socket, read buffers, ...)
};


#endif /* LIBSRSIRC_IRC_DEFS_H */
