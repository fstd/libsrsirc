#     AX_HAVE_VSYSLOG(
#       [AX_CONFIG_FEATURE_ENABLE(vsyslog)],
#       [AX_CONFIG_FEATURE_DISABLE(vsyslog)])
#     AX_CONFIG_FEATURE(
#       [vsyslog], [This platform supports vsyslog(7)],
#       [HAVE_VSYSLOG], [This platform supports vsyslog(7).])
#

AC_DEFUN([AX_HAVE_VSYSLOG], [dnl
  ax_have_vsyslog_cppflags="${CPPFLAGS}"
  AC_MSG_CHECKING([for vsyslog() without having to define _DEFAULT_SOURCE])
  AC_CACHE_VAL([ax_cv_have_vsyslog], [dnl
    AC_LINK_IFELSE([dnl
      AC_LANG_PROGRAM([dnl
#include <stdarg.h>
#include <syslog.h>
], [dnl
	va_list l;
	vsyslog(1, "dummy", l);])],
      [ax_cv_have_vsyslog=yes],
      [ax_cv_have_vsyslog=no])])
  CPPFLAGS="${ax_have_vsyslog_cppflags}"
  AS_IF([test "${ax_cv_have_vsyslog}" = "yes"],
    [AC_MSG_RESULT([yes])
$1],[AC_MSG_RESULT([no])
$2])
])dnl

#Based on the epoll check, hence:
# LICENSE
#
#   Copyright (c) 2008 Peter Simons <simons@cryp.to>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.
