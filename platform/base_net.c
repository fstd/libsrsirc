/* base_net.c -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#define LOG_MODULE MOD_BASENET

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "base_net.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#if HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#if HAVE_FCNTL_H
# include <fcntl.h>
#endif

#if HAVE_NETDB_H
# include <netdb.h>
#endif

#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_WINSOCK2_H
# include <winsock2.h>
# include <ws2tcpip.h>
#endif


#include <logger/intlog.h>

#include "base_string.h"
#include "base_time.h"


#if ! HAVE_SOCKLEN_T
# define socklen_t unsigned int
#endif

static bool s_sslinit;

#if HAVE_LIBWS2_32
static WSADATA wsa;
static bool wsainit;

static bool wsa_init(void);
#endif

#if HAVE_STRUCT_SOCKADDR || HAVE_LIBWS2_32
static bool sockaddr_from_addr(struct sockaddr *dst, size_t *dstlen,
    const struct addrlist *ai);
# if HAVE_STRUCT_ADDRINFO || HAVE_LIBWS2_32
static void addrstr_from_sockaddr(char *addr, size_t addrsz, uint16_t *port,
    const struct addrinfo *ai);
# endif
#endif
static void sslinit(void);

#if HAVE_LIBWS2_32
static bool
wsa_init(void)
{
	if (wsainit)
		return true;

	if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
		E("WSAStartup failed, code %d", WSAGetLastError());
		return false;
	}

	return wsainit = true;
}

static void filladdr(char *dest, const char *addr);
#endif


int
lsi_b_socket(bool ipv6)
{
	int sck = -1;
	int invsck = -1;
#if HAVE_LIBWS2_32
	if (!wsa_init())
		return -1;

	invsck = INVALID_SOCKET;
#endif

#if HAVE_SOCKET || HAVE_LIBWS2_32
	sck = socket(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
	if (sck == invsck) {
# if HAVE_LIBWS2_32
		E("Could not create IPv%d socket, code: %d",
		    ipv6?6:4, WSAGetLastError());
# else
		EE("Could not create IPv%d socket", ipv6?6:4);
# endif
	} else {
		D("Created IPv%d socket (fd: %d)", ipv6?6:4, sck);
# if ! HAVE_MSG_NOSIGNAL
#  if HAVE_SETSOCKOPT && HAVE_SO_NOSIGPIPE
		int opt = 1;

		socklen_t ol = (socklen_t)sizeof opt;

		if (setsockopt(sck, SOL_SOCKET, SO_NOSIGPIPE, &opt, ol) != 0) {
			EE("setsockopt(%d, SOL_SOCKET, SO_NOSIGPIPE)", sck);
		}
#  else
		W("No MSG_NOSIGNAL/SO_NOSIGPIPE, we'll SIGPIPE on write error");
#  endif
# endif
	}
#else
# error "We need something like socket()"
#endif

	return sck;
}


int
lsi_b_connect(int sck, const struct addrlist *srv)
{
#if HAVE_STRUCT_SOCKADDR_STORAGE || HAVE_LIBWS2_32
	struct sockaddr_storage sa;
	memset(&sa, 0, sizeof sa); // :S XXX
	size_t addrlen = 0;
	if (!sockaddr_from_addr((struct sockaddr *)&sa, &addrlen, srv)) {
		E("Could not make sockaddr from '%s' port %"PRIu16"'",
		    srv->addrstr, srv->port);
		return -1;
	}

# if HAVE_CONNECT || HAVE_LIBWS2_32
	D("connect()ing sck %d to '%s' port %"PRIu16"'...",
	    sck, srv->addrstr, srv->port);

	int r = connect(sck, (struct sockaddr *)&sa, addrlen);
#  if HAVE_LIBWS2_32
	if (r == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) {
#  else
	if (r == -1 && errno == EINPROGRESS) {
#  endif
		D("Connection in progress");
		return 0;
	} else if (r == 0) {
		D("Connected!");
		return 1;
	}

	EE("connect() sck %d to '%s' port %"PRIu16"'",
	    sck, srv->addrstr, srv->port);
# else
# error "We need something like connect()"
# endif

#else
# error "We need something like struct sockaddr_storage"
#endif

	return -1;
}


int
lsi_b_close(int sck)
{
	D("Closing sck %d", sck);
#if HAVE_LIBWS2_32
	return closesocket(sck);
#elif HAVE_CLOSE
	return close(sck);
#else
# error "We need something like close()"
#endif
}


int
lsi_b_select(int sck, bool rdbl, uint64_t to_us)
{
	uint64_t tsend = to_us ? lsi_b_tstamp_us() + to_us : 0;

#if HAVE_SELECT || HAVE_LIBWS2_32
	struct timeval tout;

	for (;;) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sck, &fds);
		uint64_t trem = 0;

		if (tsend) {
			uint64_t now = lsi_b_tstamp_us();
			if (now >= tsend)
				return 0;
			else
				trem = tsend - now;

			tout.tv_sec = trem / 1000000;
			tout.tv_usec = trem % 1000000;
		}

		V("select()ing fd %d for %sability (to: %"PRIu64"us)",
		    sck, rdbl?"read":"writ", trem);

		int r = select(sck+1, rdbl ? &fds : NULL, rdbl ? NULL : &fds,
		    NULL, tsend ? &tout : NULL);

		if (r < 0) {
			int e = errno;
			EE("select() fd %d for %c", sck, rdbl?'r':'w');
			return e == EINTR ? 0 : -1;
		}

		if (r == 1) {
			V("Selected!");
			return 1;
		}

		V("Nothing selected");
	}
#else
# error "We need something like select() or poll()"
#endif
}


bool
lsi_b_blocking(int sck, bool blocking)
{
#if HAVE_FCNTL
	int flg = fcntl(sck, F_GETFL);
	if (flg == -1) {
		EE("fcntl(): failed to get file descriptor flags");
		return false;
	}

	if (blocking)
		flg &= ~O_NONBLOCK;
	else
		flg |= O_NONBLOCK;

	D("Setting sck %d to %sblocking mode", sck, blocking?"":"non-");
	if (fcntl(sck, F_SETFL, flg) == -1) {
		EE("fcntl(): failed to %s blocking", blocking?"set":"clear");
		return false;
	}
#elif HAVE_LIBWS2_32
	unsigned long on = blocking;
	if (ioctlsocket(sck, FIONBIO, &on) != 0) {
		E("ioctlsocket failed to %s blocking", blocking?"set":"clear");
		return false;
	}
#else
# error "We need something to set a socket (non-)blocking"
#endif
	return true;
}


bool
lsi_b_sock_ok(int sck)
{
#if HAVE_GETSOCKOPT || HAVE_LIBWS2_32
	int opt = 0;

	socklen_t optlen = (socklen_t)sizeof opt;

# if HAVE_LIBWS2_32
	char *ov = (char *)&opt;
# else
	int *ov = &opt;
# endif

	if (getsockopt(sck, SOL_SOCKET, SO_ERROR, ov, &optlen) != 0) {
		EE("getsockopt failed");
		return false;
	}

	if (*ov == 0)
		return true;

	char berr[128];
# if HAVE_STRERROR_R
	strerror_r(opt, berr, sizeof berr);
# else
	STRACPY(berr, "(no strerror_r() to look up error string)");
# endif
	W("socket error (%d: %s)", opt, berr);

#else
# error "We need something like getsockopt()"
#endif

	return false;
}


long
lsi_b_read(int sck, void *buf, size_t sz, bool *tryagain)
{
	V("read()ing from sck %d (bufsz: %zu)", sck, sz);
#if HAVE_LIBWS2_32
	int r = recv(sck, buf, sz, 0);
	if (r == SOCKET_ERROR) {
		bool wb = WSAGetLastError() == WSAEWOULDBLOCK
		       || WSAGetLastError() == WSAEINPROGRESS;

#elif HAVE_READ
	ssize_t r = read(sck, buf, sz);
	if (r == -1) {
		bool wb =
# if HAVE_EWOULDBLOCK
		    errno == EWOULDBLOCK ||
# endif
# if HAVE_EAGAIN
		    errno == EAGAIN ||
# endif
		    false;
#else
# error "We need something like read()"
#endif

		if (tryagain)
			*tryagain = wb;

		if (wb)
			V("read() would block...");
		else
			EE("read() from sck %d (bufsz: %zu)", sck, sz);

	} else if (r > LONG_MAX) {
		W("read too long, capping return value");
		r = LONG_MAX;
	}

	return (long)r;
}


long
lsi_b_write(int sck, const void *buf, size_t len)
{
#if HAVE_SEND || HAVE_LIBWS2_32
	int flags = 0;
# if HAVE_MSG_NOSIGNAL
	flags = MSG_NOSIGNAL;
# endif
	V("send()ing %zu bytes over sck %d", len, sck);
	ssize_t r = send(sck, buf, len, flags);
	if (r == -1)
		EE("send() (sck %d, len %zu)", sck, len);
	else
		V("sent %zu bytes over sck %d", (size_t)r, sck);

	if (r > LONG_MAX) {
		W("write too long, capping return value from %zu to %ld",
		    (size_t)r, LONG_MAX);
		r = LONG_MAX;
	}

	return (long)r;
#else
# error "We need something like send()"
#endif
}


long
lsi_b_read_ssl(SSLTYPE ssl, void *buf, size_t sz, bool *tryagain)
{
#ifdef WITH_SSL
	V("SSL_read()ing from ssl hnd %p (bufsz: %zu)", (void *)ssl, sz);
	int r = SSL_read(ssl, buf, sz);
	if (r < 0) {
		int errc = SSL_get_error(ssl, r);
		if (errc == SSL_ERROR_SYSCALL)
			EE("SSL_read() failed");
		else
			E("SSL_read() returned %d, error code %d", r, errc);
		r = -1;
	} else
		V("SSL_read(): %d (ssl hnd %p)", r, (void *)ssl);

	return r;
#else
	E("SSL read attempted, but we haven't been compiled with SSL support");
	return -1;
#endif
}


long
lsi_b_write_ssl(SSLTYPE ssl, const void *buf, size_t len)
{
#ifdef WITH_SSL
	V("send()ing %zu bytes over ssl hnd %p", len, (void *)ssl);
	int r = SSL_write(ssl, buf, len);

	if (r < 0) {
		int errc = SSL_get_error(ssl, r);
		if (errc == SSL_ERROR_SYSCALL)
			EE("SSL_write() failed");
		else
			E("SSL_write() returned %d, error code %d", r, errc);
		r = -1;
	} else
		V("SSL_write(): %d (ssl hnd %p)", r, (void *)ssl);

	return r;
#else
	E("SSL write attempted, but we haven't been compiled with SSL support");
	return -1;
#endif
}


int
lsi_b_mkaddrlist(const char *host, uint16_t port, struct addrlist **res)
{
#if HAVE_GETADDRINFO
	struct addrinfo *ai_list = NULL;
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
		.ai_flags = AI_NUMERICSERV
	};

	char portstr[6];
	snprintf(portstr, sizeof portstr, "%"PRIu16, port);

	D("calling getaddrinfo on '%s:%s' (AF_UNSPEC, STREAM)", host, portstr);

	int r = getaddrinfo(host, portstr, &hints, &ai_list);

	if (r != 0) {
		E("getaddrinfo() failed: %s", gai_strerror(r));
		return -1;
	}
	int count = 0;
	for (struct addrinfo *ai = ai_list; ai; ai = ai->ai_next) {
		count++;
	}

	if (!count)
		W("getaddrinfo result address list empty");
	else
		D("got %d results, creating addrlist", count);

	struct addrlist *head = NULL, *node = NULL, *tmp;
	for (struct addrinfo *ai = ai_list; ai; ai = ai->ai_next) {
		tmp = malloc(sizeof *head);
		if (node)
			node->next = tmp;
		node = tmp;
		node->next = NULL;

		if (!head)
			head = node;

		STRACPY(node->reqname, host);
		node->ipv6 = ai->ai_family == AF_INET6;
		addrstr_from_sockaddr(node->addrstr, sizeof node->addrstr,
		    &node->port, ai);

		D("addrlist node: '%s': '%s:%"PRIu16"'",
		    node->reqname, node->addrstr, node->port);
	}

	freeaddrinfo(ai_list);

	*res = head;

	return count;

#elif HAVE_LIBWS2_32
	if (!wsa_init())
		return -1;

	struct hostent *he = gethostbyname(host);
	if (!he) {
		int e = WSAGetLastError();
		if (e)
			E("gethostbyname failed, code %d (%s)\n", e,
			    e == WSAHOST_NOT_FOUND ? "Host not found" :
			    e == WSANO_DATA ? "No record for host" : "");
		return -1;
	}

	if (he->h_addrtype != AF_INET) {
		E("bad address type %d from gethostbyname", he->h_addrtype);
		return -1;
	}

	struct addrlist *head = NULL, *node = NULL, *tmp;
	int i = 0;
	while (he->h_addr_list[i]) {
		tmp = malloc(sizeof *head);
		if (node)
			node->next = tmp;
		node = tmp;
		node->next = NULL;

		if (!head)
			head = node;

		struct in_addr ia =
		    { .S_un.S_addr = *(uint32_t *)he->h_addr_list[i] };

		STRACPY(node->addrstr, inet_ntoa(ia));
		STRACPY(node->reqname, host);
		node->ipv6 = false;
		node->port = port;
		i++;

		D("addrlist node: '%s': '%s:%"PRIu16"'",
		    node->reqname, node->addrstr, node->port);

	}

	*res = head;

	return i;
#else
	bool ip4addr = false, ip6addr = false;
	if (strspn(host, "0123456789.") == strlen(host))
		ip4addr = true;
	else if (strspn(host, "0123456789abcdefABCDEF:") == strlen(host))
		ip6addr = true;

	if (ip4addr || ip6addr) {
		struct addrlist *node = malloc(sizeof *node);
		node->next = NULL;
		STRACPY(node->reqname, host);
		STRACPY(node->addrstr, host);

		node->ipv6 = ip6addr;
		node->port = port;
		*res = node;

		W("fake \"resolved\" '%s:%"PRIu16"' - getaddrinfo() anyone?",
		    node->addrstr, node->port);

		return 1;
	}
	E("Cannot resolve '%s' -- we need something like getaddrinfo!");
	return -1;
#endif
}


void
lsi_b_freeaddrlist(struct addrlist *al)
{
	while (al) {
		struct addrlist *tmp = al->next;
		free(al);
		al = tmp;
	}
}


SSLCTXTYPE
lsi_b_mksslctx(void)
{
	if (!s_sslinit)
		sslinit();

	SSLCTXTYPE ctx = NULL;
#ifdef WITH_SSL
	ctx = SSL_CTX_new(SSLv23_client_method());
	if (!ctx)
		E("SSL_CTX_new failed");
#else
	E("no ssl support compiled in");
#endif
	return ctx;
}


void
lsi_b_freesslctx(SSLCTXTYPE ctx)
{
#ifdef WITH_SSL
	SSL_CTX_free(ctx);
#else
	E("no ssl support compiled in");
#endif
}


SSLTYPE
lsi_b_sslize(int sck, SSLCTXTYPE ctx)
{
	SSLTYPE shnd = NULL;
#ifdef WITH_SSL
	bool fail = !(shnd = SSL_new(ctx));
	fail = fail || !SSL_set_fd(shnd, sck);
	if (!fail) {
		D("calling SSL_connect()");
		int r = SSL_connect(shnd);
		if (r != 1) {
			int rr = SSL_get_error(shnd, r);
			if (rr == SSL_ERROR_SYSCALL)
				EE("SSL_connect() failed");
			else
				E("SSL_connect() failed, error code %d", rr);
		} else
			D("SSL_connect: %d", r);
		fail = fail || (r != 1);
	}

	if (fail) {
		ERR_print_errors_fp(stderr);
		if (shnd) {
			SSL_free(shnd);
			shnd = NULL;
		}
		return false;
	}

#else
	E("no ssl support compiled in");
#endif
	return shnd;
}


void
lsi_b_sslfin(SSLTYPE shnd)
{
#ifdef WITH_SSL
	SSL_shutdown(shnd);
	SSL_free(shnd);
#else
	E("no ssl support compiled in");
#endif

}


uint16_t
lsi_b_htons(uint16_t h)
{
#if HAVE_HTONS || HAVE_LIBWS2_32
	return htons(h);
#else
# error "We need something like htons()"
#endif
}


uint32_t
lsi_b_inet_addr(const char *ip4str)
{
#if HAVE_INET_ADDR || HAVE_LIBWS2_32
	return inet_addr(ip4str);
#else
# error "We need something like inet_addr()"
#endif
}


#if HAVE_LIBWS2_32
/* This assumes a little-endian Windows, orobably a rather safe bet.
 * On real operating systems, we're endian-independent (supposedly) */
static int
lsi_b_win_inet_pton(int af, const char *src, void *dst)
{
	char buf[40];
	unsigned char *dptr = dst;

	char *tok;
	if (af == AF_INET) {
		strncpy(buf, src, sizeof buf);
		buf[sizeof buf - 1] = '\0';
		if (!(tok = strtok(buf, "."))) {
			fprintf(stderr, "not an ipv4 address: '%s'", src);
			return 0;
		}
		int n = strtol(tok, NULL, 10);
		*dptr++ = (unsigned char)n;
		for (int x = 0; x < 3; x++) {
			if (!(tok = strtok(NULL, "."))) {
				fprintf(stderr, "not an ipv4 address: '%s'", src);
				return 0;
			}
			n = strtol(tok, NULL, 10);
			*dptr++ = (unsigned char)n;
		}

		return 1;
	} else if (af == AF_INET6) {
		filladdr(buf, src); // FIXME

		if (!(tok = strtok(buf, ":"))) {
			fprintf(stderr, "not an ipv6 address: '%s'", src);
			return 0;
		}
		long n = strtol(tok, NULL, 16);
		*dptr++ = (unsigned char)(n/256);
		*dptr++ = (unsigned char)(n%256);
		for (int x = 0; x < 7; x++) {
			if (!(tok = strtok(NULL, ":"))) {
				fprintf(stderr, "not an ipv4 address: '%s'", src);
				return 0;
			}
			n = strtol(tok, NULL, 16);
			*dptr++ = (unsigned char)(n/256);
			*dptr++ = (unsigned char)(n%256);
		}

		return 1;
	} else {
		fprintf(stderr, "Cannot handle anything but IPv4/IPv6 on windows...");
	}

	return -1;

}

/* turn a possibly abbreviated ipv6 address into its full form
 * consisting of eight x 4 hex digits, separated by colons.
 * we need this to simulate inet_pton on windows, because it is
 * missing [from a sufficiently ancient winapi i want compat with]
 * On platforms other than windows, this won't be used (nor compiled in)
 * probably unsafe, use w/ caution */
static void
filladdr(char *dest, const char *addr)
{
	int part = 0;

	int done = 0;
	const char *cur = addr;

	int fillgr = 0;
	if (strstr(addr, "::")) {
		int ncol = 0;
		const char *s = addr;
		while (*s)
			if (*s++ == ':')
				ncol++;
		fillgr=8-ncol;

		if (strncmp(addr, "::", 2) == 0)
			fillgr++;

	}

	do {
		if (part)
			*dest++ = ':';
		if (*cur == ':') {
			for (int y = 0; y < fillgr; y++) {
				if (y)
					*dest++ = ':';
				*dest++ = '0';
				*dest++ = '0';
				*dest++ = '0';
				*dest++ = '0';
			}
			cur++;
			if (*cur == ':')
				cur++;

		} else {

			const char *c = strchr(cur, ':');
			if (!c) {
				c = cur+strlen(cur);
				done=1;
			}
			int len = c - cur;
			for (int x = 0; x < 4-len; x++)
				*dest++ = '0';

			while (cur != c)
				*dest++ = *cur++;
			cur=c+1;
		}

		part++;
	} while (!done);

	*dest = '\0';
}
#endif

bool
lsi_b_inet4_addr(unsigned char *dest, size_t destsz, const char *ip4str)
{
#if HAVE_INET_PTON
	struct in_addr ia4;
	int n = inet_pton(AF_INET, ip4str, &ia4);
#elif HAVE_LIBWS2_32
	struct in_addr ia4;
	int n = lsi_b_win_inet_pton(AF_INET, ip4str, &ia4);
#else
# error "We need something like inet_pton()"
#endif
	if (n == -1) {
		EE("inet_pton failed on '%s'", ip4str);
		return false;
	}
	if (n == 0) {
		E("inet_pton: illegal ipv4 addr: '%s'", ip4str);
		return false;
	}
	if (destsz > 4)
		destsz = 4;
	memcpy(dest, &ia4.s_addr, destsz);
	return true;

}


bool
lsi_b_inet6_addr(unsigned char *dest, size_t destsz, const char *ip6str)
{
#if HAVE_INET_PTON
	struct in6_addr ia6;
	int n = inet_pton(AF_INET6, ip6str, &ia6);
#elif HAVE_LIBWS2_32
	struct in6_addr ia6;
	int n = lsi_b_win_inet_pton(AF_INET6, ip6str, &ia6);
#else
# error "We need something like inet_pton()"
#endif
	if (n == -1) {
		EE("inet_pton failed on '%s'", ip6str);
		return false;
	}
	if (n == 0) {
		E("inet_pton: illegal ipv6 addr: '%s'", ip6str);
		return false;
	}
	if (destsz > 16)
		destsz = 16;
	memcpy(dest, &ia6.s6_addr, destsz);
	return true;
}


static bool
sockaddr_from_addr(struct sockaddr *dst, size_t *dstlen,
    const struct addrlist *ai)
{
#if HAVE_LIBWS2_32
	if (ai->ipv6) {
		E("No IPv6 on windows, srz");
		return false;
	}

	struct sockaddr_in *sain = (struct sockaddr_in *)dst;

	memset(sain, 0, sizeof *sain);
	sain->sin_family = AF_INET;
	sain->sin_port = htons(ai->port);
	sain->sin_addr.S_un.S_addr = lsi_b_inet_addr(ai->addrstr);

	*dstlen = sizeof *sain;

	return true;
#elif HAVE_GETADDRINFO
	struct addrinfo *ai_list = NULL;
	struct addrinfo hints = {
		.ai_family = ai->ipv6 ? AF_INET6 : AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
		.ai_flags = AI_NUMERICSERV|AI_NUMERICHOST
	};
	char portstr[6];
	snprintf(portstr, sizeof portstr, "%"PRIu16, ai->port);

	int r = getaddrinfo(ai->addrstr, portstr, &hints, &ai_list);
	if (r != 0) {
		E("getaddrinfo('%s', '%s', ...): %s",
		    ai->addrstr, portstr, gai_strerror(r));
		return false;
	}

	memcpy(dst, ai_list->ai_addr, ai_list->ai_addrlen);
	*dstlen = ai_list->ai_addrlen;

	freeaddrinfo(ai_list);

	return true;
#else
# error "We need something like getaddrinfo()"
#endif
}


#if HAVE_GETADDRINFO
static void
addrstr_from_sockaddr(char *addr, size_t addrsz, uint16_t *port,
    const struct addrinfo *ai)
{
	if (ai->ai_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)ai->ai_addr;

		if (addr && addrsz)
			if (!inet_ntop(AF_INET, &sin->sin_addr, addr, addrsz))
				EE("inet_ntop");
		if (port)
			*port = ntohs(sin->sin_port);
	} else if (ai->ai_family == AF_INET6) {
		struct sockaddr_in6 *sin = (struct sockaddr_in6 *)ai->ai_addr;

		if (addr && addrsz)
			if (!inet_ntop(AF_INET6, &sin->sin6_addr, addr, addrsz))
				EE("inet_ntop");
		if (port)
			*port = ntohs(sin->sin6_port);
	} else {
		if (addr && addrsz)
			lsi_b_strNcpy(addr, "(non-IPv4/IPv6)", addrsz);
		if (port)
			*port = 0;
	}
}
#endif


static void
sslinit(void)
{
#ifdef WITH_SSL
	/* XXX not reentrant FIXME */
	SSL_load_error_strings();
	SSL_library_init();
	s_sslinit = true;
	D("SSL initialized");
#else
	E("no ssl support compiled in");
#endif
}
