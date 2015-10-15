#!/bin/sh

mingwroot='/c/MinGW' #MUST be an absolute path starting with /<drive letter>/
tardir='/c/'  #where the tarball has to sit
workdir='/c/libsrsirc'  #where we build. NOTE THAT we rm -rf and recreate this

# Constructed from $mingwroot
compiler="$(printf '%c' "${mingwroot#?}"):/${mingwroot#???}/bin/gcc.exe"

if [ "x$1" = 'x-h' ]; then
	echo "Usage: $0 [<version (e.g. 0.0.10)>] ["release"]" >&2
	echo "  version can be omitted if there is only one libsrsirc tarball in C:/" >&2
	echo "  if the string 'release' is present, tar up the resulted executables and the DLL" >&2
	exit 0
fi

if [ -z "$1" ] || [ "x$1" = 'xrelease' ]; then
	if [ $(ls $tardir | grep 'libsrsirc-[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.tar\.gz' | wc -l) -eq 1 ]; then
		VERSION="$(ls $tardir | grep 'libsrsirc-[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.tar\.gz' | grep -o '[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*')"
		printf "autodetected " >&2
	else
		echo "Usage: $0 <version (e.g. 0.0.10)> ["release"]" >&2
		exit 1
	fi
else
	if ! printf '%s\n' "$1" | grep -q '^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$'; then
		echo "'$1' does not look like a correct version" >&2
		exit 1
	fi
	VERSION="$1"
	shift
fi

echo "version: $VERSION" >&2

rel=false
if [ "x$1" = "xrelease" ]; then
	rel=true
fi

if ! [ -f $tardir/libsrsirc-$VERSION.tar.gz ]; then
	echo "Coult not find $tardir/libsrsirc-$VERSION.tar.gz" >&2
	exit 1
fi

rm -rf $workdir

if [ -e $workdir ]; then
	echo "Could not get rid of $workdir; remove manually" >&2
	exit 1
fi

mkdir $workdir

if ! [ -d $workdir ]; then
	echo "Could not create $workdir; bad permissions?" >&2
	exit 1
fi

cd $workdir
tar -xzf $tardir/libsrsirc-$VERSION.tar.gz
cd libsrsirc-$VERSION
export PATH="$mingwroot/bin:/bin"
echo "Configuring (stdout goes to configure.out, stderr to configure.err)" >&2
if ! ./configure CC=$compiler >configure.out 2>configure.err; then
	echo "Configure failed" >&2
	exit 1
fi

echo "Building (stdout goes to make.out, stderr to make.err)" >&2
if ! make >make.out 2>make.err; then
	echo "Make failed" >&2
	exit 1
fi

if ! $rel; then
	echo "All done" >&2
	exit 0
fi

mkdir disttmp

cd disttmp

mkdir buildinfo
cp ../configure.out ../configure.err ../make.out ../make.err ../config.status ../config.log ../config.h buildinfo/
date >buildinfo/timestamp
uname -a >buildinfo/buildbox

for f in ../src/*.exe; do
	cp "$f" "stripped_$(basename $f)"
done
cp ../src/.libs/*.exe ./
cp ../libsrsirc/.libs/*.dll ./

tar -czf ../libsrsirc-$VERSION-winbin.tarbomb.gz *
cd ..
#rm -rf disttmp

echo "Tarbomb with .exe and .dll created: libsrsirc-$VERSION-winbin.tarbomb.gz" >&2
echo "All done" >&2
exit 0
