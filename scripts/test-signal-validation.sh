#!/bin/sh
set -eu

BIN=${1:-build/test-signals}

if [ ! -r /proc/self/exe ]; then
	echo "Signal replacement test skipped: /proc/self/exe is unavailable."
	exit 0
fi

TMP=${TMPDIR:-/tmp}/fakehttp-signal-validation.$$
TARGET=$TMP/fakehttp-signal-test
PID=

cleanup()
{
	if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
		kill "$PID" 2>/dev/null || true
		wait "$PID" 2>/dev/null || true
	fi
	rm -rf "$TMP"
}

trap cleanup EXIT INT TERM
mkdir -p "$TMP"
cp "$BIN" "$TARGET"

"$TARGET" hold "$TMP/ready" &
PID=$!

i=0
while [ ! -f "$TMP/ready" ]; do
	i=$((i + 1))
	[ "$i" -lt 50 ] || {
		echo "holder process did not start" >&2
		exit 1
	}
	sleep 0.02
done

cp "$BIN" "$TARGET.new"
mv -f "$TARGET.new" "$TARGET"

case "$(readlink "/proc/$PID/exe")" in
	"$TARGET (deleted)") ;;
	*)
		echo "atomic replacement did not produce a deleted executable path" >&2
		exit 1
		;;
esac

"$TARGET" kill
wait "$PID"
PID=

echo "Signal replacement test passed."
