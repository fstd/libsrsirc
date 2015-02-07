#     AX_HAVE_SIGPIPE(
#       [AX_CONFIG_FEATURE_ENABLE(SIGPIPE)],
#       [AX_CONFIG_FEATURE_DISABLE(SIGPIPE)])
#     AX_CONFIG_FEATURE(
#       [SIGPIPE], [This platform supports SIGPIPE],
#       [HAVE_SIGPIPE], [This platform supports SIGPIPE.])
#

AC_DEFUN([AX_HAVE_SIGPIPE], [dnl
  ax_have_sigpipe_cppflags="${CPPFLAGS}"
  AC_MSG_CHECKING([for SIGPIPE])
  AC_CACHE_VAL([ax_cv_have_sigpipe], [dnl
    AC_RUN_IFELSE([dnl
      AC_LANG_PROGRAM([dnl
#include <signal.h>
], [dnl
	return !SIGPIPE;])],
      [ax_cv_have_sigpipe=yes],
      [ax_cv_have_sigpipe=no])])
  CPPFLAGS="${ax_have_sigpipe_cppflags}"
  AS_IF([test "${ax_cv_have_sigpipe}" = "yes"],
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
