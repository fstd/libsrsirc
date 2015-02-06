#!/bin/sh
make clean
make distclean
rm -f Makefile.in
rm -f aclocal.m4
rm -rf autom4te.cache/
rm -rf build-aux
rm -rf m4/libtool.m4 m4/ltoptions.m4 m4/ltsugar.m4 m4/ltversion.m4 m4/lt~obsolete.m4
rm -f config.h.in
rm -f configure
rm -f include/Makefile.in
rm -f include/libsrsirc/Makefile.in
rm -f libsrsirc/Makefile.in
rm -f logger/Makefile.in
rm -f platform/Makefile.in
rm -f src/Makefile.in
rm -f unittests/Makefile.in
rm -f unittests/run_*.c
