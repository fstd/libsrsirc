#!/bin/sh

# this is utterly inefficient

FmtSpec()
{
	t="$1"
	fld="$2" # 2: fmt spec, 3: cast type
	cat scripts/retprint | while read -r lin; do
		rty="$(printf '%s\n' "$lin" | cut -d : -f 1)"
		if [ "x$rty" '=' "x$t" ]; then
			printf '%s\n' "$lin" | cut -d : -f $fld
			break;
		fi
	done
}

file="$1"

tmp="$(mktemp /tmp/replret.sh.XXXXXX)"
work="$(mktemp /tmp/replret.sh.XXXXXX)"

cat "$file" >$work

TAB="$(printf '\tx')"; TAB="${TAB%x}"

grep -whn "^[ $TAB]*return" "$file" | while read -r ln; do
	lno="$(printf '%s\n' "$ln" | cut -d : -f 1)"
	ln="${ln#*:}"

	semilno="$lno"
	if ! printf '%s\n' "$ln" | grep -qF ';'; then
		while ! sed -n ${semilno}p "$file" | grep -qF ';'; do
			semilno=$((semilno+1))
		done
	fi

	typelno=$lno
	while ! sed -n ${typelno}p "$file" | grep -q '^{'; do
		typelno=$((typelno-1))
	done

	typelno=$((typelno-1))

	while sed -n ${typelno}p "$file" | grep -q "^[ $TAB]"; do
		typelno=$((typelno-1))
	done

	typelno=$((typelno-1))

	type="$(sed -n ${typelno}p "$file")"
	type="${type#static }"

	fmtspec="$(FmtSpec "$type" 2)"
	castto="$(FmtSpec "$type" 3)"
	if [ -z "$fmtspec" ]; then
		printf 'No fmt spec found for type `%s`!\n' "$type" >&2
		exit 1
	fi

	if [ "x$type" '=' "xtokarr *(*" ]; then
		# special case, the logonconv function
		type="void *"
	fi

	if [ "x$type" '=' "xvoid" ]; then
		sed "${lno}s/return/TRCRETURNVOID/" $work >$tmp
		cat $tmp >$work
	else
		sed "${lno}s/return/TRCRETURN($type, \"$fmtspec\", $castto, (/" $work >$tmp
		sed "${semilno}s/;/));/" $tmp >$work
	fi
done

cat >$tmp  <<'EOF'
#define TRCRETURN(TYPE, FMTSPEC, CASTTO, EXPR) do { TYPE _ret = (EXPR); T("RET: " FMTSPEC, (CASTTO)_ret); return _ret; } while(0)
#define TRCRETURNVOID do { T("RET: (void)"); return; } while(0)
EOF

cat $work >>$tmp
cat $tmp >"$file"
