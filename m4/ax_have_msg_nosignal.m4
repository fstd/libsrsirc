#     AX_HAVE_MSG_NOSIGNAL(
#       [AX_CONFIG_FEATURE_ENABLE(MSG_NOSIGNAL)],
#       [AX_CONFIG_FEATURE_DISABLE(MSG_NOSIGNAL)])
#     AX_CONFIG_FEATURE(
#       [MSG_NOSIGNAL], [This platform supports MSG_NOSIGNAL],
#       [HAVE_MSG_NOSIGNAL], [This platform supports MSG_NOSIGNAL.])
#

AC_DEFUN([AX_HAVE_MSG_NOSIGNAL], [dnl
  ax_have_msg_nosignal_cppflags="${CPPFLAGS}"
  AC_MSG_CHECKING([for MSG_NOSIGNAL])
  AC_CACHE_VAL([ax_cv_have_msg_nosignal], [dnl
    AC_RUN_IFELSE([dnl
      AC_LANG_PROGRAM([dnl
#include <sys/socket.h>
], [dnl
	return !MSG_NOSIGNAL;])],
      [ax_cv_have_msg_nosignal=yes],
      [ax_cv_have_msg_nosignal=no])])
  CPPFLAGS="${ax_have_msg_nosignal_cppflags}"
  AS_IF([test "${ax_cv_have_msg_nosignal}" = "yes"],
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
