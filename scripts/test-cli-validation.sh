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
expect_invalid -m 010000000000 "leading-zero decimal fwmark overflow"
expect_invalid -m + "positive sign without digits"
expect_invalid -m 0x "hexadecimal prefix without digits"
expect_invalid -m +0x "positive hexadecimal prefix without digits"
expect_invalid -m ++1 "duplicate positive sign"
expect_invalid -m 00x10 "misplaced hexadecimal prefix"
expect_invalid -m 0b1 "unsupported binary prefix"
expect_invalid -m "1 " "trailing whitespace"
expect_invalid -m 0x100000000 "hexadecimal fwmark overflow"
expect_invalid -n 65536 "NFQUEUE number overflow"
expect_invalid -p 0 "bypass port lower bound"
expect_invalid -p 65536 "bypass port upper bound"
expect_invalid -n 0100000 "leading-zero decimal queue overflow"
expect_invalid -r 11 "repeat upper bound"
expect_invalid -r 011 "leading-zero decimal repeat upper bound"
expect_invalid -t 256 "TTL upper bound"
expect_invalid -t 0256 "leading-zero decimal TTL upper bound"
expect_invalid -x 0x10junk "mask trailing characters"
expect_invalid -y 100 "dynamic percentage upper bound"
expect_invalid -y 0123 "leading-zero decimal percentage upper bound"
expect_invalid -y " 50" "leading whitespace"

expect_parsed -m 0xffffffff "maximum fwmark"
expect_parsed -m +0X10 "explicit positive hexadecimal fwmark"
expect_parsed -n 0 "minimum NFQUEUE number"
expect_parsed -n 65535 "maximum NFQUEUE number"
expect_parsed -p 65499 "bypass TCP port"
expect_parsed -r 0xa "hexadecimal repeat"
expect_parsed -r 010 "leading-zero decimal repeat"
expect_parsed -t 018 "leading-zero decimal TTL"
expect_parsed -t 0xff "hexadecimal TTL"
expect_parsed -x +1 "explicit positive mask"
expect_parsed -y 99 "maximum dynamic percentage"
expect_tls_hostname_too_long

echo "CLI validation tests passed."
