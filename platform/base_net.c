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

#include <logger/intlog.h>

#include "base_string.h"
#include "base_time.h"


#if ! HAVE_SOCKLEN_T
# define socklen_t unsigned int
#endif

static bool s_sslinit;


static bool sockaddr_from_addr(struct sockaddr *dst, size_t *dstlen, const struct addrlist *ai);
#if HAVE_GETADDRINFO
static void addrstr_from_sockaddr(char *addr, size_t addr_sz, uint16_t *port, const struct addrinfo *ai);
#endif
static void sslinit(void);


int
b_socket(bool ipv6)
{ T("trace");
	int sck = -1;
#if HAVE_SOCKET
	sck = socket(ipv6 ? PF_INET6 : PF_INET, SOCK_STREAM, 0);
	if (sck < 0)
		EE("Could not create IPv%d socket", ipv6?6:4);
	else {
		D("Created IPv%d socket (fd: %d)", ipv6?6:4, sck);
# if ! HAVE_MSG_NOSIGNAL
#  if HAVE_SETSOCKOPT && HAVE_SO_NOSIGPIPE
		int opt = 1;

		socklen_t optlen = (socklen_t)sizeof opt;

		if (setsockopt(sck, SOL_SOCKET, SO_NOSIGPIPE, &opt, &optlen) != 0) {
			EE("setsockopt(%d, SOL_SOCKET, SO_NOSIGPIPE)");
		}
#  else
		W("No MSG_NOSIGNAL/SO_NOSIGPIPE, we'll SIGPIPE on write error");
#  endif
# endif
	}
#else
	E("We need something like socket()");
#endif

	return sck;
}


int
b_connect(int sck, const struct addrlist *srv)
{ T("trace");
#if HAVE_STRUCT_SOCKADDR_STORAGE
	struct sockaddr_storage sa;
	memset(&sa, 0, sizeof sa); // :S XXX
	size_t addrlen = 0;
	if (!sockaddr_from_addr((struct sockaddr *)&sa, &addrlen, srv)) {
		E("Could not make sockaddr from '%s' port %"PRIu16"'",
		    srv->addrstr, srv->port);
		return -1;
	}

# if HAVE_CONNECT
	D("connect()ing sck %d to '%s' port %"PRIu16"'...",
	    sck, srv->addrstr, srv->port);

	int r = connect(sck, (struct sockaddr *)&sa, addrlen);
	if (r == -1 && errno == EINPROGRESS) {
		D("Connection in progress");
		return 0;
	} else if (r == 0) {
		D("Connected!");
		return 1;
	}

	EE("connect() sck %d to '%s' port %"PRIu16"'", sck, srv->addrstr, srv->port);
# else
	E("We need something like connect()");
# endif

#else
	E("We need something like struct sockaddr_storage");
#endif

	return -1;
}


int
b_close(int sck)
{ T("trace");
#if HAVE_CLOSE
	D("Closing sck %d", sck);
	return close(sck);
#else
	E("We need something like close()");
	return -1;
#endif
}


int
b_select(int sck, bool rdbl, uint64_t to_us)
{ T("trace");
	uint64_t tsend = to_us ? b_tstamp_us() + to_us : 0;

#if HAVE_SELECT
	struct timeval tout;

	for (;;) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sck, &fds);
		uint64_t trem = 0;

		if (tsend) {
			uint64_t now = b_tstamp_us();
			if (now >= tsend)
				return 0;
			else
				trem = tsend - now;

			tout.tv_sec = trem / 1000000;
			tout.tv_usec = trem % 1000000;
		}

		V("select()ing fd %d for %sability (to: %"PRIu64"us)",
		    sck, rdbl?"read":"writ", trem);

		int r = select(sck+1, rdbl ? &fds : NULL, rdbl ? NULL : &fds, NULL,
		    tsend ? &tout : NULL);

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
	E("We need something like select() or poll()");
	return -1;
#endif
}


bool
b_blocking(int sck, bool blocking)
{ T("trace");
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
		EE("fcntl(): failed to %s blocking mode", blocking?"set":"clear");
		return false;
	}
#else
	E("We need something like fcntl()");
	return false;
#endif
	return true;
}


bool
b_sock_ok(int sck)
{ T("trace");
#if HAVE_GETSOCKOPT
	int opt = 0;

	socklen_t optlen = (socklen_t)sizeof opt;

	if (getsockopt(sck, SOL_SOCKET, SO_ERROR, &opt, &optlen) != 0) {
		EE("getsockopt failed");
		return false;
	}

	if (opt == 0)
		return true;

	char berr[128];
# if HAVE_STRERROR_R
	strerror_r(opt, berr, sizeof berr);
# else
	STRACPY(berr, "(no strerror_r() to look up error string)");
# endif
	W("socket error (%d: %s)", opt, berr);

#else
	E("we need something like getsockopt()");
#endif

	return false;
}


long
b_read(int sck, void *buf, size_t sz, bool *tryagain)
{ T("trace");
#if HAVE_READ
	V("read()ing from sck %d (bufsz: %zu)", sck, sz);
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

		if (tryagain) {
			*tryagain = wb;
		}

		if (wb)
			V("read() would block...");
		else
			EE("read() from sck %d (bufsz: %zu)", sck, sz);

	} else if (r > LONG_MAX) {
		W("read too long, capping return value");
		r = LONG_MAX;
	}

	return (long)r;
#else
	E("We need something like read()");
	return -1;
#endif
}


long
b_write(int sck, const void *buf, size_t len)
{ T("trace");
#if HAVE_SEND
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
	E("We need something like read()");
	return -1;
#endif
}


long
b_read_ssl(SSLTYPE ssl, void *buf, size_t sz, bool *tryagain)
{ T("trace");
#ifdef WITH_SSL
	V("SSL_read()ing from ssl hnd %p (bufsz: %zu)", (void *)ssl, sz);
	int r = SSL_read(ssl, buf, sz);
	if (r < 0) {
		int errc = SSL_get_error(ssl, r);
		E("SSL_read() returned %d, error code %d", r, errc);
		r = -1;
	} else
		V("SSL_read(): %d (ssl hnd %p)", r, (void *)ssl);

	return r;
#else
	C("SSL read attempted, but we haven't been compiled with SSL support");
	/* not reached */
#endif
}


long
b_write_ssl(SSLTYPE ssl, const void *buf, size_t len)
{ T("trace");
#ifdef WITH_SSL
	V("send()ing %zu bytes over ssl hnd %p", len, (void *)ssl);
	int r = SSL_write(ssl, buf, len);
	
	if (r < 0) {
		int errc = SSL_get_error(ssl, r);
		E("SSL_write() returned %d, error code %d", r, errc);
		r = -1;
	} else
		V("SSL_write(): %d (ssl hnd %p)", r, (void *)ssl);
	
	return r;
#else
	C("SSL write attempted, but we haven't been compiled with SSL support");
	/* not reached */
#endif
}


int
b_mkaddrlist(const char *host, uint16_t port, struct addrlist **res)
{ T("trace");
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
		node->port = port;
		node->family = ip4addr ? AF_INET : AF_INET6;
		node->socktype = SOCK_STREAM;
		node->protocol = 0;
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
b_freeaddrlist(struct addrlist *al)
{ T("trace");
	while (al) {
		struct addrlist *tmp = al->next;
		free(al);
		al = tmp;
	}
}


SSLCTXTYPE
b_mksslctx(void)
{ T("trace");
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
b_freesslctx(SSLCTXTYPE ctx)
{ T("trace");
#ifdef WITH_SSL
	SSL_CTX_free(ctx);
#else
	E("no ssl support compiled in");
#endif
}


SSLTYPE
b_sslize(int sck, SSLCTXTYPE ctx)
{ T("trace");
	SSLTYPE shnd = NULL;
#ifdef WITH_SSL
	bool fail = !(shnd = SSL_new(ctx));
	fail = fail || !SSL_set_fd(shnd, sck);
	if (!fail) {
		int r = SSL_connect(shnd);
		D("ssl_connect: %d", r);
		if (r != 1) {
			int rr = SSL_get_error(shnd, r);
			D("rr: %d", rr);
		}
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
b_sslfin(SSLTYPE shnd)
{ T("trace");
#ifdef WITH_SSL
	SSL_shutdown(shnd);
	SSL_free(shnd);
#else
	E("no ssl support compiled in");
#endif

}


uint16_t
b_htons(uint16_t h)
{ T("trace");
#if HAVE_HTONS
	return htons(h);
#else
	E("We need something like htons()");
	return h;
#endif
}


uint32_t
b_inet_addr(const char *ip4str)
{ T("trace");
#if HAVE_INET_ADDR
	return inet_addr(ip4str);
#else
	E("We need something like htons()");
	return (uint32_t)-1;
#endif
}


bool
b_inet4_addr(unsigned char *dest, size_t destsz, const char *ip4str)
{ T("trace");
#if HAVE_INET_PTON
	struct in_addr ia4;
	int n = inet_pton(AF_INET, ip4str, &ia4);
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
#else
	E("We need something like inet_pton()");
#endif

}


bool
b_inet6_addr(unsigned char *dest, size_t destsz, const char *ip6str)
{ T("trace");
#if HAVE_INET_PTON
	struct in6_addr ia6;
	int n = inet_pton(AF_INET6, ip6str, &ia6);
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
#else
	E("We need something like inet_pton()");
#endif
}


static bool
sockaddr_from_addr(struct sockaddr *dst, size_t *dstlen, const struct addrlist *ai)
{ T("trace");
#if HAVE_GETADDRINFO
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
	E("We need something like getaddrinfo()");
	return false;
#endif
}


#if HAVE_GETADDRINFO
static void
addrstr_from_sockaddr(char *addr, size_t addr_sz, uint16_t *port,
    const struct addrinfo *ai)
{ T("trace");
	if (ai->ai_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in*)ai->ai_addr;

		if (addr && addr_sz)
			if (!inet_ntop(AF_INET, &sin->sin_addr, addr, addr_sz))
				EE("inet_ntop");
		if (port)
			*port = ntohs(sin->sin_port);
	} else if (ai->ai_family == AF_INET6) {
		struct sockaddr_in6 *sin = (struct sockaddr_in6*)ai->ai_addr;

		if (addr && addr_sz)
			if (!inet_ntop(AF_INET6, &sin->sin6_addr, addr, addr_sz))
				EE("inet_ntop");
		if (port)
			*port = ntohs(sin->sin6_port);
	} else {
		if (addr && addr_sz)
			b_strNcpy(addr, "(non-IPv4/IPv6)", addr_sz);
		if (port)
			*port = 0;
	}
}
#endif


static void
sslinit(void)
{ T("trace");
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
