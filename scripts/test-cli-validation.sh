#!/bin/sh
set -eu

BIN=${1:-build/fakehttp}
TMP=${TMPDIR:-/tmp}/fakehttp-cli-validation.$$

cleanup()
{
	rm -rf "$TMP"
}

trap cleanup EXIT INT TERM
mkdir -p "$TMP"

expect_invalid()
{
	option=$1
	value=$2
	label=$3

	if "$BIN" "$option" "$value" >"$TMP/output" 2>&1; then
		echo "expected failure for $label" >&2
		exit 1
	fi

	if ! grep -Fq "invalid value for $option." "$TMP/output"; then
		echo "missing validation error for $label" >&2
		cat "$TMP/output" >&2
		exit 1
	fi
}

expect_parsed()
{
	option=$1
	value=$2
	label=$3

	"$BIN" "$option" "$value" >"$TMP/output" 2>&1 || true
	if grep -Fq "invalid value for $option." "$TMP/output"; then
		echo "valid value rejected for $label" >&2
		cat "$TMP/output" >&2
		exit 1
	fi
}

expect_tls_hostname_too_long()
{
	hostname=$(printf '%263s' '' | tr ' ' a)

	if "$BIN" -a -f -e "$hostname" >"$TMP/output" 2>&1; then
		echo "expected failure for oversized TLS hostname" >&2
		exit 1
	fi

	if ! grep -Fq "hostname is too long" "$TMP/output"; then
		echo "missing validation error for oversized TLS hostname" >&2
		cat "$TMP/output" >&2
		exit 1
	fi
}

expect_invalid -m 1junk "fwmark trailing characters"
expect_invalid -m -18446744073709551615 "fwmark negative wraparound"
expect_invalid -m 18446744073709551616 "fwmark strtoull overflow"
expect_invalid -n 65536 "NFQUEUE number overflow"
expect_invalid -r 11 "repeat upper bound"
expect_invalid -t 256 "TTL upper bound"
expect_invalid -x 0x10junk "mask trailing characters"
expect_invalid -y 100 "dynamic percentage upper bound"
expect_invalid -y " 50" "leading whitespace"

expect_parsed -m 0xffffffff "maximum fwmark"
expect_parsed -n 0 "minimum NFQUEUE number"
expect_parsed -n 65535 "maximum NFQUEUE number"
expect_parsed -r 0xa "hexadecimal repeat"
expect_parsed -t 0377 "octal TTL"
expect_parsed -x +1 "explicit positive mask"
expect_parsed -y 99 "maximum dynamic percentage"
expect_tls_hostname_too_long

echo "CLI validation tests passed."
