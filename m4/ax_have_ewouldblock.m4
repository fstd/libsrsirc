#     AX_HAVE_EWOULDBLOCK(
#       [AX_CONFIG_FEATURE_ENABLE(EWOULDBLOCK)],
#       [AX_CONFIG_FEATURE_DISABLE(EWOULDBLOCK)])
#     AX_CONFIG_FEATURE(
#       [EWOULDBLOCK], [This platform supports EWOULDBLOCK],
#       [HAVE_EWOULDBLOCK], [This platform supports EWOULDBLOCK.])
#

AC_DEFUN([AX_HAVE_EWOULDBLOCK], [dnl
  ax_have_ewouldblock_cppflags="${CPPFLAGS}"
  AC_MSG_CHECKING([for EWOULDBLOCK])
  AC_CACHE_VAL([ax_cv_have_ewouldblock], [dnl
    AC_RUN_IFELSE([dnl
      AC_LANG_PROGRAM([dnl
#include <errno.h>
], [dnl
	return !EWOULDBLOCK;])],
      [ax_cv_have_ewouldblock=yes],
      [ax_cv_have_ewouldblock=no])])
  CPPFLAGS="${ax_have_ewouldblock_cppflags}"
  AS_IF([test "${ax_cv_have_ewouldblock}" = "yes"],
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
