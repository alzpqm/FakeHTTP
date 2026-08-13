#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

usage() {
    cat <<EOF
Usage: $0 <openwrt-sdk-dir> [output-dir]

Build FakeHTTP and LuCI IPK packages from this working tree.

The SDK must match the router target and OpenWrt release. The script adds
temporary recipe symlinks, builds with the OpenWrt package system, and removes
only those symlinks when it exits.

Environment overrides:
  V          OpenWrt build verbosity, default: s
EOF
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ] || [ "$#" -lt 1 ]; then
    usage
    [ "$#" -lt 1 ] && exit 1 || exit 0
fi

SDK_DIR=$(CDPATH= cd -- "$1" && pwd)
OUT_DIR=${2:-"$ROOT_DIR/build/openwrt-ipk"}
PACKAGE_DIR="$SDK_DIR/package"
V=${V:-s}

[ -f "$SDK_DIR/include/toplevel.mk" ] || {
    echo "not an OpenWrt SDK/buildroot: $SDK_DIR" >&2
    exit 1
}
[ -d "$PACKAGE_DIR" ] || {
    echo "missing OpenWrt package directory: $PACKAGE_DIR" >&2
    exit 1
}

LINKS=""
cleanup() {
    for package in $LINKS; do
        rm -f "$PACKAGE_DIR/$package"
    done
}
trap cleanup EXIT HUP INT TERM

for package in fakehttp luci-app-fakehttp; do
    if [ -e "$PACKAGE_DIR/$package" ] || [ -L "$PACKAGE_DIR/$package" ]; then
        echo "package path already exists in SDK: $PACKAGE_DIR/$package" >&2
        echo "remove it or use a clean SDK before running this helper" >&2
        exit 1
    fi
    ln -s "$ROOT_DIR/openwrt/$package" "$PACKAGE_DIR/$package"
    LINKS="$LINKS $package"
done

mkdir -p "$OUT_DIR"

FAKEHTTP_SOURCE_DIR="$ROOT_DIR" make -C "$SDK_DIR" defconfig
FAKEHTTP_SOURCE_DIR="$ROOT_DIR" make -C "$SDK_DIR" \
    package/fakehttp/compile \
    package/luci-app-fakehttp/compile \
    V="$V"

copy_latest_ipk() {
    pattern=$1
    candidate=$(find "$SDK_DIR/bin" -type f -name "$pattern" -print |
        LC_ALL=C sort | tail -n 1)
    [ -n "$candidate" ] || {
        echo "could not find built package matching $pattern" >&2
        exit 1
    }
    cp -f "$candidate" "$OUT_DIR/"
    printf '%s\n' "$OUT_DIR/$(basename "$candidate")"
}

copy_latest_ipk 'fakehttp_*.ipk'
copy_latest_ipk 'luci-app-fakehttp_*.ipk'

printf 'OpenWrt IPK packages written to %s\n' "$OUT_DIR"
