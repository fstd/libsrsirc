/* icat_common.h - Declarations used across icat
 * icat - IRC netcat on top of libsrsirc - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_ICAT_COMMON_H
#define LIBSRSIRC_ICAT_COMMON_H 1


#include <stdbool.h>
#include <stdint.h>


#define MOD_INIT 0
#define MOD_CORE 1
#define MOD_SERV 2
#define MOD_USER 3
#define MOD_MISC 4

#define MAX_CHANLIST 512

#define DEF_CONTO_SOFT_MS 15000u
#define DEF_CONTO_HARD_MS 120000u
#define DEF_LINEDELAY_MS 2000u
#define DEF_FREELINES 0u
#define DEF_WAITQUIT_MS 3000u
#define DEF_CONFAILWAIT_MS 10000u
#define DEF_HEARTBEAT_MS 30000
#define DEF_IGNORE_PM true
#define DEF_IGNORE_CHANSERV true
#define DEF_VERB 1
#define DEF_QMSG "IRC netcat - http://github.com/fstd/libsrsirc"


struct srvlist_s {
	char *host;
	uint16_t port;
	bool ssl;
	struct srvlist_s *next;
};

struct settings_s {
	char nick[64];
	char uname[64];
	char fname[128];
	uint8_t conflags;
	char pass[64];

	char pxhost[128];
	uint16_t pxport;
	int pxtype;

	char saslname[128];
	char saslpass[128];
	bool starttls;
	bool starttls_req;
	int starttls_mode;

	bool trgmode;
	uint64_t cto_soft_us;
	uint64_t cto_hard_us;
	uint64_t linedelay;
	unsigned freelines;
	uint64_t waitquit_us;
	bool keeptrying;
	uint64_t cfwait_us;
	uint64_t hbeat_us;
	bool reconnect;
	bool nojoin;
	bool ignore_pm;
	bool ignore_cs;
	bool notices;
	char chanlist[MAX_CHANLIST];
	char keylist[MAX_CHANLIST];
	char *esc;
	char qmsg[512];
	struct srvlist_s *srvlist;
};


extern struct settings_s g_sett;
extern bool g_interrupted;
extern bool g_inforequest;


#endif /* LIBSRSIRC_ICAT_COMMON_H */
