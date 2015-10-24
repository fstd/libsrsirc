#!/bin/sh

if ! [ -d ./tracer ]; then
	echo "please run from top-level directory" >&2
	exit 1
fi

if [ $(git status --porcelain | grep -v '^??' | wc -l) -ne 0 ]; then
	echo "please clean up your working directory first (untracked files are okay)" >&2
	exit 1
fi

echo "NOTE: use savetrace.sh to save updated tracers, use git reset --hard to get rid of the mess" >&2

tmp=$(mktemp /tmp/droptrace.sh.XXXXXXXX)
trap "rm -f '$tmp'" EXIT

find tracer -name 'tracer@*' | while read -r f; do
	file="$(printf '%s\n' "$f" | cut -d '@' -f 2 | tr '%' '/')"
	func="$(printf '%s\n' "$f" | cut -d '@' -f 3)"

	lno="$(grep -nh -A4 "^$func(" "$file" | grep '^[0-9][0-9]*-{' | cut -d - -f 1)"

	if [ -z "$lno" ]; then
		echo "Not found: '$func' in '$file'" >&2
		continue
	fi

	echo "File: '$file', Func: '$func', Line: $lno" >&2
	cat "$file" | sed -e "${lno}q" | sed '$d' >$tmp
	cat "$f" >>$tmp
	cat "$file" | sed -n "$((lno+1)),\$p" >>$tmp
	cat $tmp >$file
done

find libsrsirc platform src -name '*.c' | grep -vF -e 'base_log.c' -e 'helloworld.c' | while read -r f; do
	echo "doing returns for $f" >&2
	scripts/replret.sh "$f"
done
