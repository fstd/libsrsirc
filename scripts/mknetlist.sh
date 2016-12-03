#!/bin/sh
#set -x

site="$1"

prgnam="$(basename "$0")"

Warn()
{
	printf '%s: WARNING: %s\n' "$prgnam" "$*" >&2
}

Nuke()
{
	printf '%s: ERROR: %s\n' "$prgnam" "$*" >&2
	exit 1
}

[ -z "$site" ] && Nuke "Usage: $prgnam <site> [<throttle>]"

throttle=
[ -n "$2" ] && throttle="$2"

tmp="$(mktemp /tmp/$prgnam.netlist.XXXXXXXXX)"
netnames="$(mktemp /tmp/$prgnam.netnames.XXXXXXXXX)"
tab="$(printf '\t')"

trap "rm '$netnames' '$tmp'" EXIT
trap 'exit 1' HUP INT TERM QUIT

grep='pcregrep'
command -v "$grep" >/dev/null 2>/dev/null || grep="grep -P"

wget -qO - http://$site/networks/ | grep -o 'href="/networks/[^/]*/"' | sed -n 's,href="/networks/\([^/]*\)/",\1,p' | sort | uniq >"$netnames"
cat /tmp/nl >"$netnames"

c=0
while read -r netname; do
	chunk="$(mktemp /tmp/$prgnam.chunk.$netname.XXXXXXXXX)"
	wget -qO - "http://$site/servers/?net=$(printf '%s\n' "$netname" | sed 's/ /%20/g')" >$chunk
	grep -FA9001 "servers used to connect" "$chunk" | grep -FB9001 "servers last connected to" | grep -FA9001 "Main<br>Server"  \
	    | sed 's/&#8203;//g' | $grep -o '<tr>.*?</tr>' | sed -e "s/ style='[^']*' *//g" -e "s/ align='[^']*' *//g" | sed 's/<td  *>/<td>/g' \
	    | sed -e 's,</td><td>, ,g' -e 's/<tr><td>//' -e 's,</td></tr>,,' | while read -r host port ssl rest; do # oh well
		case "$ssl" in
			[oO][fF][fF]) sslstr= ;;
			[oO][nN]) sslstr='/SSL' ;;
			*) Nuke "??? (ssl is '$ssl')" ;;
		esac

		printf "%s$tab%s:%s%s\\n" "$netname" "$host" "$port" "$sslstr"
	done >$tmp

	if ! [ -s "$tmp" ]; then
		Warn "Got nothing for netname '$netname', left output in '$chunk'"
	else
		cat "$tmp"
		rm "$chunk"
	fi

	[ -n "$throttle" ] && sleep $throttle
done <"$netnames"
