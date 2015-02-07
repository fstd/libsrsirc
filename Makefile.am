ACLOCAL_AMFLAGS = -I m4
SUBDIRS = platform logger include libsrsirc src unittests

EXTRA_DIST = scripts unittests
dist-hook:
	rm -f $(distdir)/unittests/run_*.c $(distdir)/unittests/Makefile
	rm -rf $(distdir)/unittests/.deps

test: all
	scripts/runtests.sh
