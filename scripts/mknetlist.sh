#!/bin/sh
#set -x

site="$1"

prgnam="$(basename "$0")"

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

trap "rm '$netnames' '$tmp'" EXIT
trap 'exit 1' HUP INT TERM QUIT

grep='pcregrep'
command -v "$grep" >/dev/null 2>/dev/null || grep="grep -P"

wget -qO - http://$site/networks/ | grep -o 'href="/networks/[^/]*/"' | sed -n 's,href="/networks/\([^/]*\)/",\1,p' | sort | uniq >"$netnames"

c=0
while read -r netname; do
	wget -qO - "http://$site/servers/?net=$netname" >$tmp
	grep -FA9001 "servers used to connect" "$tmp" | grep -FB9001 "servers last connected to" | grep -FA9001 "Main<br>Server"  \
	    | sed 's/&#8203;//g' | $grep -o '<tr>.*?</tr>' | sed -e "s/ style='[^']*' *//g" -e "s/ align='[^']*' *//g" | sed 's/<td  *>/<td>/g' \
	    | sed -e 's,</td><td>, ,g' -e 's/<tr><td>//' -e 's,</td></tr>,,' | while read -r host port ssl rest; do # oh well
		case "$ssl" in
			[oO][fF][fF]) sslstr= ;;
			[oO][nN]) sslstr='/SSL' ;;
			*) Nuke "??? (ssl is '$ssl')" ;;
		esac

		printf '%s %s:%s%s\n' "$netname" "$host" "$port" "$sslstr"
	done
	[ -n "$throttle" ] && sleep $throttle
done <"$netnames"
