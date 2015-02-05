#     AX_HAVE_EAGAIN(
#       [AX_CONFIG_FEATURE_ENABLE(EAGAIN)],
#       [AX_CONFIG_FEATURE_DISABLE(EAGAIN)])
#     AX_CONFIG_FEATURE(
#       [EAGAIN], [This platform supports EAGAIN],
#       [HAVE_EAGAIN], [This platform supports EAGAIN.])
#

AC_DEFUN([AX_HAVE_EAGAIN], [dnl
  ax_have_eagain_cppflags="${CPPFLAGS}"
  AC_MSG_CHECKING([for EAGAIN])
  AC_CACHE_VAL([ax_cv_have_eagain], [dnl
    AC_RUN_IFELSE([dnl
      AC_LANG_PROGRAM([dnl
#include <errno.h>
], [dnl
	return !EAGAIN;])],
      [ax_cv_have_eagain=yes],
      [ax_cv_have_eagain=no])])
  CPPFLAGS="${ax_have_eagain_cppflags}"
  AS_IF([test "${ax_cv_have_eagain}" = "yes"],
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
