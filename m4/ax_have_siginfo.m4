#     AX_HAVE_SIGINFO(
#       [AX_CONFIG_FEATURE_ENABLE(SIGINFO)],
#       [AX_CONFIG_FEATURE_DISABLE(SIGINFO)])
#     AX_CONFIG_FEATURE(
#       [SIGINFO], [This platform supports SIGINFO],
#       [HAVE_SIGINFO], [This platform supports SIGINFO.])
#

AC_DEFUN([AX_HAVE_SIGINFO], [dnl
  ax_have_siginfo_cppflags="${CPPFLAGS}"
  AC_MSG_CHECKING([for SIGINFO])
  AC_CACHE_VAL([ax_cv_have_siginfo], [dnl
    AC_RUN_IFELSE([dnl
      AC_LANG_PROGRAM([dnl
#include <signal.h>
], [dnl
	return !SIGINFO;])],
      [ax_cv_have_siginfo=yes],
      [ax_cv_have_siginfo=no])])
  CPPFLAGS="${ax_have_siginfo_cppflags}"
  AS_IF([test "${ax_cv_have_siginfo}" = "yes"],
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
