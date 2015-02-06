#!/bin/sh

set -e

tmp=$(mktemp /tmp/runtests.XXXXX)

trap 'rm -f "$tmp"' EXIT

cd unittests
for f in test_*; do
	if [ ! -x "$f" -o ! -f "$f" ]; then
		continue
	fi

	if ! ./${f}; then
		echo 1 >"$tmp"
	fi
done

if [ -z "$(cat "$tmp")" ]; then
	exit 0
else
	echo "!!! Some unit tests failed !!!"
	exit 1
fi
