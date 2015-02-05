#     AX_HAVE_SIGINT(
#       [AX_CONFIG_FEATURE_ENABLE(SIGINT)],
#       [AX_CONFIG_FEATURE_DISABLE(SIGINT)])
#     AX_CONFIG_FEATURE(
#       [SIGINT], [This platform supports SIGINT],
#       [HAVE_SIGINT], [This platform supports SIGINT.])
#

AC_DEFUN([AX_HAVE_SIGINT], [dnl
  ax_have_sigint_cppflags="${CPPFLAGS}"
  AC_MSG_CHECKING([for SIGINT])
  AC_CACHE_VAL([ax_cv_have_sigint], [dnl
    AC_RUN_IFELSE([dnl
      AC_LANG_PROGRAM([dnl
#include <signal.h>
], [dnl
	return !SIGINT;])],
      [ax_cv_have_sigint=yes],
      [ax_cv_have_sigint=no])])
  CPPFLAGS="${ax_have_sigint_cppflags}"
  AS_IF([test "${ax_cv_have_sigint}" = "yes"],
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
