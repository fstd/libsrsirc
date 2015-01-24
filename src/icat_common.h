#ifndef SRC_ICAT_COMMON_H
#define SRC_ICAT_COMMON_H 1

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
#define DEF_HEARTBEAT_MS 300000
#define DEF_IGNORE_PM true
#define DEF_IGNORE_CHANSERV true
#define DEF_VERB 1


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

	bool trgmode;
	uint64_t conto_soft_us;
	uint64_t conto_hard_us;
	uint64_t linedelay;
	unsigned freelines;
	uint64_t waitquit_us;
	bool keeptrying;
	uint64_t confailwait_us;
	uint64_t heartbeat_us;
	bool reconnect;
	bool flush;
	bool nojoin;
	bool ignore_pm;
	bool ignore_cs;
	bool notices;
	//int verb;
	char chanlist[MAX_CHANLIST];
	char keylist[MAX_CHANLIST];
	char *esc;
	struct srvlist_s *srvlist;

};

extern struct settings_s g_sett;

#endif /* SRC_ICAT_COMMON_H */
