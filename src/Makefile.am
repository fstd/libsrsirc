bin_PROGRAMS = icat iwat
icat_SOURCES = icat_core.c icat_init.c icat_misc.c icat_serv.c icat_user.c icat_common.h icat_core.h icat_misc.h icat_serv.h icat_user.h
icat_CPPFLAGS = -I$(top_srcdir)/include
icat_LDADD = $(top_srcdir)/logger/libsrsirclog.la $(top_srcdir)/libsrsirc/libsrsirc.la
iwat_SOURCES = iwat.c
iwat_CPPFLAGS = -I$(top_srcdir)/include
iwat_LDADD = $(top_srcdir)/libsrsirc/libsrsirc.la
