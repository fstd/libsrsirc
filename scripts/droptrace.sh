#!/bin/sh

if ! [ -d ./tracer ]; then
	echo "please run from top-level directory" >&2
	exit 1
fi

tmp3=$(mktemp /tmp/droptrace.sh.XXXXXXXX)
tmp2=$(mktemp /tmp/droptrace.sh.XXXXXXXX)
tmp4=$(mktemp /tmp/droptrace.sh.XXXXXXXX)
tmp=$(mktemp /tmp/droptrace.sh.XXXXXXXX)
trap "rm -f '$tmp' '$tmp2' '$tmp3' '$tmp4'" EXIT

rm ./tracer/*

find libsrsirc src platform -name '*.c' | while read -r f; do
	echo "Processing $f" >&2
	cat "$f" >$tmp
	sed 's/^/x/' "$f" >$tmp4

	fnn="$(printf '%s\n' "$f" | tr '/' '%')"

	lno=0
	cat "$tmp4" | while read -r ln; do
		ln="${ln#x}"
		if [ -z "$lno" ]; then
			lno=1
		else
			lno=$((lno+1))
		fi

		if printf '%s\n' "$ln" | grep -o '^[a-zA-Z_][a-zA-Z0-9_]*(' >$tmp3; then
			lastfunc="$(cat $tmp3)"
			lastfunc="${lastfunc%?}"
			continue;
		fi

		case "$ln" in
			"{ T("*) sed -n ${lno}p $tmp >"tracer/tracer@${fnn}@${lastfunc}"
			         cat "$tmp" | awk "NR == $lno { print \"{\"; } NR != $lno { print;}" >$tmp2; cat $tmp2 >$tmp ;;
		esac
	done

	cat $tmp >"$f"
done