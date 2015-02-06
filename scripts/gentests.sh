#!/bin/sh

set -e

tmp=$(mktemp /tmp/gentests.XXXXX)

trap 'rm -f "$tmp"' EXIT

cd unittests
for f in test_*.c; do
	
	echo "#include \"$f\"" >"$tmp"
	echo 'int main(void) {' >>"$tmp"
	echo 'const char *errmsg;' >>"$tmp"
	echo 'int ret = 0;' >>"$tmp"

	grep -FA1 '/*UNITTEST*/' "$f" | while read line1; do
		if [ "$line1" = "--" ]; then #grep's context separator; skip it
			read line1
		fi
		read line2
		#echo "line1 is '$line1', line2 is '$line2'"

		tname=$(basename "$f" .c)
		fname=$(echo "$line2" | sed 's/(.*$//')
		arglist="$(echo "$line2" | sed -e 's/^[^(]*(//' -e 's/)[ \t]*$//')"
		#echo "fname is '$fname', arglist is '$arglist'"

		if [ "$arglist" != "void" ]; then
			echo "non-void args not supported ATM" >&2
			exit 1
		fi

		
		
		cat <<EOF | sed -e "s/UTNAME/${tname}.${fname}/g" -e "s/UTFUNC/$fname/g" >>"$tmp"
printf("Running UTNAME...");
fflush(stdout);
errmsg = UTFUNC();
if (errmsg) {
	printf("FAILED: %s\n", errmsg);
	ret = 1;
} else
	printf("Ok\n");
EOF
	done
	
	echo 'return ret;' >>"$tmp"
	echo '}' >>"$tmp"

	cat "$tmp" >"run_${f}"
done

