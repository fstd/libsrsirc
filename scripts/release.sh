#!/bin/sh

if ! [ "$(git rev-parse --abbrev-ref HEAD)" = 'devel' ]; then
	echo 'Be on the devel branch first.' >&2
	exit 1
fi

newver="$(grep '^AC_INIT' configure.ac | grep -o '[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*')"
echo "Be on the devel branch." >&2
echo "Manually update AND STAGE all version references, but do not commit yet." >&2
echo "Then, run this script." >&2
echo "Have you done that, and is $newver the correct (new) version? (Answer YES)" >&2
read -r ans
if ! [ "x$ans" = 'xYES' ]; then
	echo "That wasn't a YES." >&2
	exit 1
fi

set -e

git commit -m "bump version to $newver"
git push origin devel
git checkout master
git merge devel
git push origin master
git tag -a "v$newver" -m "version $newver"
git push --tags

./autoclean.sh
./autogen.sh
./configure
make dist

echo "Copy libsrsirc-$newver.tar.gz to C:\libsrsirc-$newver.tar.gz" >&2
echo "on the Windows build box." >&2
echo "Follow the build instructions (doc/winbuild.txt) to build the stuff" >&2
echo "Copy back the generated libsrsirc-$newver-winbin.tarbomb.gz here." >&2
echo "Hit enter when done." >&2

read -r dummy
while ! [ -f libsrsirc-$newver-winbin.tarbomb.gz ]; do
	echo "Copy back the generated libsrsirc-$newver-winbin.tarbomb.gz here." >&2
	echo "Hit enter when done." >&2
	read -r dummy
done


mkdir dist windist
cd dist/

tar -xzpf ../libsrsirc-$newver.tar.gz
cd ../windist/
tar -xzf ../libsrsirc-$newver-winbin.tarbomb.gz
cp *.exe libsrsirc-0.dll ../dist/libsrsirc-$newver/
cd ../dist/
zip -r libsrsirc-$newver-win.zip libsrsirc-$newver
mv libsrsirc-$newver-win.zip ..
cd ..
rm -rf dist windist


git checkout tar
git add libsrsirc-$newver.tar.gz
git add libsrsirc-$newver-win.zip
git commit -m "$newver tarball and zip"
git push origin tar

git checkout devel

echo "Okay."
exit 0
