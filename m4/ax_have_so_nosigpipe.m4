#     AX_HAVE_SO_NOSIGPIPE(
#       [AX_CONFIG_FEATURE_ENABLE(SO_NOSIGPIPE)],
#       [AX_CONFIG_FEATURE_DISABLE(SO_NOSIGPIPE)])
#     AX_CONFIG_FEATURE(
#       [SO_NOSIGPIPE], [This platform supports SO_NOSIGPIPE],
#       [HAVE_SO_NOSIGPIPE], [This platform supports SO_NOSIGPIPE.])
#

AC_DEFUN([AX_HAVE_SO_NOSIGPIPE], [dnl
  ax_have_so_nosigpipe_cppflags="${CPPFLAGS}"
  AC_MSG_CHECKING([for SO_NOSIGPIPE])
  AC_CACHE_VAL([ax_cv_have_so_nosigpipe], [dnl
    AC_RUN_IFELSE([dnl
      AC_LANG_PROGRAM([dnl
#include <sys/socket.h>
], [dnl
	return !SO_NOSIGPIPE;])],
      [ax_cv_have_so_nosigpipe=yes],
      [ax_cv_have_so_nosigpipe=no])])
  CPPFLAGS="${ax_have_so_nosigpipe_cppflags}"
  AS_IF([test "${ax_cv_have_so_nosigpipe}" = "yes"],
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
