#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([libsrsirc], [0.0.0], [van.fisted@googlemail.com])
# AC_SUBST([AM_CFLAGS], [-std=c99 -g -O0 -Wall -Wextra])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/icat.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_PREREQ([2.2])
LT_INIT


# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_CC_C_O

# Checks for header files (1).
AC_HEADER_STDC

AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stdbool.h stddef.h stdlib.h string.h strings.h sys/socket.h sys/time.h sys/types.h unistd.h])
AC_CHECK_LIB([srslog], [log_is_fancy], [], [AC_MSG_ERROR([libsrslog not installed?])])

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
