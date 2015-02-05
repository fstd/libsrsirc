#     AX_HAVE_CTIME_R(
#       [AX_CONFIG_FEATURE_ENABLE(ctime_r)],
#       [AX_CONFIG_FEATURE_DISABLE(ctime_r)])
#     AX_CONFIG_FEATURE(
#       [ctime_r], [This platform supports ctime_r(7)],
#       [HAVE_CTIME_R], [This platform supports ctime_r(7).])
#

AC_DEFUN([AX_HAVE_CTIME_R], [dnl
  ax_have_ctime_r_cppflags="${CPPFLAGS}"
  AC_MSG_CHECKING([for ctime_r()])
  AC_CACHE_VAL([ax_cv_have_ctime_r], [dnl
    AC_RUN_IFELSE([dnl
      AC_LANG_PROGRAM([dnl
#include <time.h>
], [dnl
	char timebuf@<:@32@:>@;
	time_t now = time(NULL);
	return !ctime_r(&now, timebuf);])],
      [ax_cv_have_ctime_r=yes],
      [ax_cv_have_ctime_r=no])])
  CPPFLAGS="${ax_have_ctime_r_cppflags}"
  AS_IF([test "${ax_cv_have_ctime_r}" = "yes"],
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
