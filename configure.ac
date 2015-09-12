#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libsrsirc], [0.0.8], [van.fisted@googlemail.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/icat_core.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
LT_PREREQ([2.4])
AM_PROG_AR
LT_INIT

# Checks for programs.
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_HEADER_STDC

AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stdbool.h stddef.h stdlib.h string.h strings.h sys/select.h sys/socket.h sys/time.h sys/types.h syslog.h unistd.h])
AC_ARG_WITH(ssl,
[  --with-ssl            Build with SSL support],
	if test x$withval = xno; then
		want_ssl=no
	else
		want_ssl=yes
	fi,
	want_ssl=no)

dnl **
dnl ** check for ssl
dnl **
if test "x$want_ssl" = "xyes"; then
	AC_CHECK_LIB([ssl], [SSL_new], [], [AC_MSG_ERROR([no crypto lib for ssl installed?])])
	AC_DEFINE(WITH_SSL,, Build with SSL support)
#	AC_CHECK_LIB([ssl], [SSL_new], [
#		LIBS="$LIBS -lssl"
#		AC_CHECK_HEADERS([openssl/ssl.h])
#	])
fi

if test "$(uname)" = "FreeBSD"; then
AC_DEFINE([_WITH_GETLINE], [1], [FreeBSD needs this to be defined])
:
fi

if test "$(uname)" = "Linux"; then
AC_DEFINE([_POSIX_C_SOURCE], [200809L], [getaddrinfo and friends, mainly])
:
else
#AC_DEFINE([DUMPSIG], [SIGINFO], [SIGUSR1 on Linux, SIGINFO on the BSDs])
:
fi

AC_HEADER_STDBOOL

AC_CHECK_TYPES([ptrdiff_t])
AC_CHECK_TYPES([struct sockaddr_storage], [], [], [#include <sys/socket.h>])
AC_CHECK_TYPES([struct sockaddr_in], [], [], [#include <netinet/in.h>])
AC_CHECK_TYPES([struct sockaddr_in6], [], [], [#include <netinet/in.h>])
AC_CHECK_TYPES([socklen_t], [], [], [#include <sys/socket.h>])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T


# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([atexit close connect fcntl fileno getaddrinfo getopt getsockopt gettimeofday htons inet_addr inet_pton memmove memset nanosleep read select send setsockopt sigaction socket strcasecmp strchr strncasecmp strspn strstr strtol strtoul strtoull])


AX_HAVE_CTIME_R(
  [AX_CONFIG_FEATURE_ENABLE(ctime_r)],
  [AX_CONFIG_FEATURE_DISABLE(ctime_r)])
AX_CONFIG_FEATURE(
  [ctime_r], [This platform has ctime_r(3)],
  [HAVE_CTIME_R], [This platform has ctime_r(3).])

AX_HAVE_EAGAIN(
  [AX_CONFIG_FEATURE_ENABLE(EAGAIN)],
  [AX_CONFIG_FEATURE_DISABLE(EAGAIN)])
AX_CONFIG_FEATURE(
  [EAGAIN], [This platform has EAGAIN],
  [HAVE_EAGAIN], [This platform has EAGAIN.])

AX_HAVE_EWOULDBLOCK(
  [AX_CONFIG_FEATURE_ENABLE(EWOULDBLOCK)],
  [AX_CONFIG_FEATURE_DISABLE(EWOULDBLOCK)])
AX_CONFIG_FEATURE(
  [EWOULDBLOCK], [This platform has EWOULDBLOCK],
  [HAVE_EWOULDBLOCK], [This platform has EWOULDBLOCK.])

AX_HAVE_SIGINT(
  [AX_CONFIG_FEATURE_ENABLE(SIGINT)],
  [AX_CONFIG_FEATURE_DISABLE(SIGINT)])
AX_CONFIG_FEATURE(
  [SIGINT], [This platform has SIGINT],
  [HAVE_SIGINT], [This platform has SIGINT.])

AX_HAVE_SIGUSR1(
  [AX_CONFIG_FEATURE_ENABLE(SIGUSR1)],
  [AX_CONFIG_FEATURE_DISABLE(SIGUSR1)])
AX_CONFIG_FEATURE(
  [SIGUSR1], [This platform has SIGUSR1],
  [HAVE_SIGUSR1], [This platform has SIGUSR1.])

AX_HAVE_SIGINFO(
  [AX_CONFIG_FEATURE_ENABLE(SIGINFO)],
  [AX_CONFIG_FEATURE_DISABLE(SIGINFO)])
AX_CONFIG_FEATURE(
  [SIGINFO], [This platform has SIGINFO],
  [HAVE_SIGINFO], [This platform has SIGINFO.])

AX_HAVE_SIGPIPE(
  [AX_CONFIG_FEATURE_ENABLE(SIGPIPE)],
  [AX_CONFIG_FEATURE_DISABLE(SIGPIPE)])
AX_CONFIG_FEATURE(
  [SIGPIPE], [This platform has SIGPIPE],
  [HAVE_SIGPIPE], [This platform has SIGPIPE.])

AX_HAVE_MSG_NOSIGNAL(
  [AX_CONFIG_FEATURE_ENABLE(MSG_NOSIGNAL)],
  [AX_CONFIG_FEATURE_DISABLE(MSG_NOSIGNAL)])
AX_CONFIG_FEATURE(
  [MSG_NOSIGNAL], [This platform has MSG_NOSIGNAL],
  [HAVE_MSG_NOSIGNAL], [This platform has MSG_NOSIGNAL.])

AX_HAVE_SO_NOSIGPIPE(
  [AX_CONFIG_FEATURE_ENABLE(SO_NOSIGPIPE)],
  [AX_CONFIG_FEATURE_DISABLE(SO_NOSIGPIPE)])
AX_CONFIG_FEATURE(
  [SO_NOSIGPIPE], [This platform has SO_NOSIGPIPE],
  [HAVE_SO_NOSIGPIPE], [This platform has SO_NOSIGPIPE.])

case "$CFLAGS" in
*-O@<:@234@:>@*)
AC_DEFINE([NOTRACE], [1], [Exclude function tracing when optimizing])
: ;;
*)
AC_DEFINE([NOTRACE], [0], [Exclude function tracing when optimizing])
: ;;
esac

#AX_HAVE_VSYSLOG(
#  [AX_CONFIG_FEATURE_ENABLE(vsyslog)],
#  [AX_CONFIG_FEATURE_DISABLE(vsyslog)])
#AX_CONFIG_FEATURE(
#  [vsyslog], [This platform has vsyslog(3)],
#  [HAVE_VSYSLOG], [This platform has vsyslog(3).])

AC_CONFIG_FILES([Makefile
		 platform/Makefile
		 include/Makefile
		 include/libsrsirc/Makefile
		 logger/Makefile
		 libsrsirc/Makefile
		 src/Makefile
                 unittests/Makefile])
AC_OUTPUT

echo \
"-------------------------------------------------

 ${PACKAGE_NAME} Version ${PACKAGE_VERSION}

 Prefix: '${prefix}'.
 Compiler: '${CC} ${CFLAGS} ${CPPFLAGS}'
 Libraries: '${LIBS}'

--------------------------------------------------"
scripts/gentests.sh
