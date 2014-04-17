#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libsrsirc], [0.0.2], [van.fisted@googlemail.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/icat.c])
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

AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stdbool.h stddef.h stdlib.h string.h strings.h sys/socket.h sys/time.h sys/types.h unistd.h])
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

AC_HEADER_STDBOOL

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([getaddrinfo gettimeofday memmove memset select socket strcasecmp strchr strdup strncasecmp strtol])

AC_CONFIG_FILES([Makefile
		 common/Makefile
		 include/Makefile
		 include/libsrsirc/Makefile
		 libsrsirc/Makefile
                 src/Makefile])
AC_OUTPUT

echo \
"-------------------------------------------------

 ${PACKAGE_NAME} Version ${PACKAGE_VERSION}

 Prefix: '${prefix}'.
 Compiler: '${CC} ${CFLAGS} ${CPPFLAGS}'
 Libraries: '${LIBS}'

--------------------------------------------------"
