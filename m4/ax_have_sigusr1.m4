#     AX_HAVE_SIGUSR1(
#       [AX_CONFIG_FEATURE_ENABLE(SIGUSR1)],
#       [AX_CONFIG_FEATURE_DISABLE(SIGUSR1)])
#     AX_CONFIG_FEATURE(
#       [SIGUSR1], [This platform supports SIGUSR1],
#       [HAVE_SIGUSR1], [This platform supports SIGUSR1.])
#

AC_DEFUN([AX_HAVE_SIGUSR1], [dnl
  ax_have_sigusr1_cppflags="${CPPFLAGS}"
  AC_MSG_CHECKING([for SIGUSR1])
  AC_CACHE_VAL([ax_cv_have_sigusr1], [dnl
    AC_RUN_IFELSE([dnl
      AC_LANG_PROGRAM([dnl
#include <signal.h>
], [dnl
	return !SIGUSR1;])],
      [ax_cv_have_sigusr1=yes],
      [ax_cv_have_sigusr1=no])])
  CPPFLAGS="${ax_have_sigusr1_cppflags}"
  AS_IF([test "${ax_cv_have_sigusr1}" = "yes"],
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
