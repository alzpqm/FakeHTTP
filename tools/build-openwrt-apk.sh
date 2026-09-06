#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

usage() {
    cat <<EOF
Usage: $0 <openwrt-25-sdk-dir> [output-dir]

Build FakeHTTP and LuCI APK packages for OpenWrt 25.12 and newer.

The SDK must match the target architecture and release. This builder uses the
SDK target compiler and apk tool, and writes only package artifacts.

Environment overrides:
  ARCH       APK architecture, default: x86_64
  BUILD_DIR  Temporary build root, default: /tmp/fakehttp-openwrt-apk-build
EOF
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ] || [ "$#" -lt 1 ]; then
    usage
    [ "$#" -lt 1 ] && exit 1 || exit 0
fi

SDK_DIR=$(CDPATH= cd -- "$1" && pwd)
OUT_DIR=${2:-"$ROOT_DIR/build/openwrt-apk"}
ARCH=${ARCH:-x86_64}
BUILD_DIR=${BUILD_DIR:-/tmp/fakehttp-openwrt-apk-build}
APK="$SDK_DIR/staging_dir/host/bin/apk"
TARGET_STAGING=$(find "$SDK_DIR/staging_dir" -maxdepth 1 -type d -name 'target-*' | head -n 1)
TARGET_CC=${TARGET_CC:-$(find "$SDK_DIR/staging_dir" -path '*/bin/*-openwrt-linux-musl-gcc' -type f | head -n 1)}
TARGET_STRIP=${TARGET_STRIP:-$(find "$SDK_DIR/staging_dir" -path '*/bin/*-openwrt-linux-musl-strip' -type f | head -n 1)}
PO2LMO=${PO2LMO:-$SDK_DIR/staging_dir/hostpkg/bin/po2lmo}

[ -f "$SDK_DIR/include/toplevel.mk" ] || {
    echo "not an OpenWrt SDK/buildroot: $SDK_DIR" >&2
    exit 1
}
[ -x "$APK" ] || { echo "missing apk tool: $APK" >&2; exit 1; }
[ -d "$TARGET_STAGING" ] || { echo "missing target staging dir" >&2; exit 1; }
[ -x "$TARGET_CC" ] || { echo "missing target compiler" >&2; exit 1; }
[ -x "$TARGET_STRIP" ] || { echo "missing target strip" >&2; exit 1; }
[ -x "$PO2LMO" ] || { echo "missing po2lmo tool: $PO2LMO" >&2; exit 1; }
[ "$(id -u)" -eq 0 ] || {
    echo "run this builder as root so package files are owned by root" >&2
    exit 1
}

pkg_field() {
    awk -F:= -v key="$2" '$1 == key { print $2; exit }' "$1"
}

FAKEHTTP_VERSION=$(pkg_field "$ROOT_DIR/openwrt/fakehttp/Makefile" PKG_VERSION)
FAKEHTTP_RELEASE=$(pkg_field "$ROOT_DIR/openwrt/fakehttp/Makefile" PKG_RELEASE)
LUCI_VERSION=$(pkg_field "$ROOT_DIR/openwrt/luci-app-fakehttp/Makefile" PKG_VERSION)
LUCI_RELEASE=$(pkg_field "$ROOT_DIR/openwrt/luci-app-fakehttp/Makefile" PKG_RELEASE)

FAKEHTTP_ROOT="$BUILD_DIR/fakehttp-root"
LUCI_ROOT="$BUILD_DIR/luci-root"
FAKEHTTP_SCRIPTS="$BUILD_DIR/fakehttp-scripts"
LUCI_SCRIPTS="$BUILD_DIR/luci-scripts"

rm -rf "$BUILD_DIR"
mkdir -p "$FAKEHTTP_ROOT/usr/bin" "$FAKEHTTP_ROOT/etc/config" \
    "$FAKEHTTP_ROOT/etc/init.d" "$FAKEHTTP_ROOT/usr/sbin" \
    "$LUCI_ROOT" "$OUT_DIR"

make -C "$ROOT_DIR" clean
make -C "$ROOT_DIR" \
    CC="$TARGET_CC" \
    STRIP="$TARGET_STRIP" \
    CFLAGS="-I$TARGET_STAGING/usr/include" \
    LDFLAGS="-L$TARGET_STAGING/usr/lib -Wl,-rpath-link,$TARGET_STAGING/usr/lib" \
    VERSION="$FAKEHTTP_VERSION-r$FAKEHTTP_RELEASE"

install -m 0755 "$ROOT_DIR/build/fakehttp" "$FAKEHTTP_ROOT/usr/bin/fakehttp"
install -m 0644 "$ROOT_DIR/openwrt/fakehttp/files/etc/config/fakehttp" \
    "$FAKEHTTP_ROOT/etc/config/fakehttp"
install -m 0755 "$ROOT_DIR/openwrt/fakehttp/files/etc/init.d/fakehttp" \
    "$FAKEHTTP_ROOT/etc/init.d/fakehttp"
install -m 0755 "$ROOT_DIR/openwrt/fakehttp/files/usr/sbin/fakehttp-setup" \
    "$FAKEHTTP_ROOT/usr/sbin/fakehttp-setup"

make_conffile_metadata() {
    root=$1
    package=$2
    conffile=$3

    mkdir -p "$root/lib/apk/packages"
    printf '%s\n' "$conffile" >"$root/lib/apk/packages/$package.conffiles"
    sha256sum "$root$conffile" | awk -v file="$conffile" '{ print file " " $1 }' \
        >"$root/lib/apk/packages/$package.conffiles_static"
}

make_file_metadata() {
    root=$1
    package=$2

    mkdir -p "$root/lib/apk/packages"
    (cd "$root" && find . \( -type f -o -type l \) | \
        sed 's#^\./#/#' | LC_ALL=C sort) \
        >"$root/lib/apk/packages/$package.list"
}

make_script_metadata() {
    dir=$1
    package=$2

    mkdir -p "$dir"
    printf '%s\n' \
        '#!/bin/sh' \
        '[ "${IPKG_NO_SCRIPT:-}" = "1" ] && exit 0' \
        '[ -s "${IPKG_INSTROOT:-}/lib/functions.sh" ] || exit 0' \
        '. "${IPKG_INSTROOT:-}/lib/functions.sh"' \
        'export root="${IPKG_INSTROOT:-}"' \
        "export pkgname=\"$package\"" \
        'default_postinst' \
        >"$dir/post-install"

    if [ "$package" = fakehttp ]; then
        printf '%s\n' \
            '[ -n "${IPKG_INSTROOT:-}" ] || {' \
            '    /etc/init.d/fakehttp enable >/dev/null 2>&1' \
            '    /etc/init.d/fakehttp restart >/dev/null 2>&1' \
            '}' \
            >>"$dir/post-install"
    fi

    if [ "$package" = luci-app-fakehttp ]; then
        printf '%s\n' \
            '[ -n "${IPKG_INSTROOT:-}" ] || {' \
            '    rm -f /tmp/luci-indexcache.*' \
            '    rm -rf /tmp/luci-modulecache/' \
            '    /etc/init.d/rpcd reload >/dev/null 2>&1' \
            '}' \
            >>"$dir/post-install"
    fi

    {
        printf '%s\n' '#!/bin/sh' 'export PKG_UPGRADE=1'
        sed '/^[[:space:]]*#!/d' "$dir/post-install"
    } >"$dir/post-upgrade"

    printf '%s\n' \
        '#!/bin/sh' \
        '[ -s "${IPKG_INSTROOT:-}/lib/functions.sh" ] || exit 0' \
        '. "${IPKG_INSTROOT:-}/lib/functions.sh"' \
        'export root="${IPKG_INSTROOT:-}"' \
        "export pkgname=\"$package\"" \
        'default_prerm' \
        >"$dir/pre-deinstall"
    chmod 0755 "$dir/post-install" "$dir/post-upgrade" "$dir/pre-deinstall"
}

make_conffile_metadata "$FAKEHTTP_ROOT" fakehttp /etc/config/fakehttp
make_file_metadata "$FAKEHTTP_ROOT" fakehttp
make_script_metadata "$FAKEHTTP_SCRIPTS" fakehttp

cp -Rp "$ROOT_DIR/openwrt/luci-app-fakehttp/root/." "$LUCI_ROOT/"
mkdir -p "$LUCI_ROOT/www/luci-static/resources/view"
mkdir -p "$LUCI_ROOT/usr/lib/lua/luci/i18n"
cp -p "$ROOT_DIR/openwrt/luci-app-fakehttp/htdocs/luci-static/resources/view/fakehttp.js" \
    "$LUCI_ROOT/www/luci-static/resources/view/fakehttp.js"
"$PO2LMO" "$ROOT_DIR/openwrt/luci-app-fakehttp/po/zh_Hant/fakehttp.po" \
    "$LUCI_ROOT/usr/lib/lua/luci/i18n/fakehttp.zh-tw.lmo"
make_file_metadata "$LUCI_ROOT" luci-app-fakehttp
make_script_metadata "$LUCI_SCRIPTS" luci-app-fakehttp

chown -R 0:0 "$FAKEHTTP_ROOT" "$LUCI_ROOT"

"$APK" mkpkg \
    --info "name:fakehttp" \
    --info "version:$FAKEHTTP_VERSION-r$FAKEHTTP_RELEASE" \
    --info "description:TCP HTTP obfuscation with NFQUEUE" \
    --info "arch:$ARCH" \
    --info "license:GPL-3.0-or-later" \
    --info "origin:feeds/base/fakehttp" \
    --info "url:https://github.com/alzpqm/FakeHTTP" \
    --info "maintainer:FakeHTTP maintainers" \
    --info "provides:fakehttp-any" \
    --info "depends:libc libnetfilter-queue1 libnfnetlink0 libmnl0 kmod-nfnetlink-queue kmod-nft-queue nftables-json" \
    --script "post-install:$FAKEHTTP_SCRIPTS/post-install" \
    --script "post-upgrade:$FAKEHTTP_SCRIPTS/post-upgrade" \
    --script "pre-deinstall:$FAKEHTTP_SCRIPTS/pre-deinstall" \
    --files "$FAKEHTTP_ROOT" \
    --output "$OUT_DIR/fakehttp-$FAKEHTTP_VERSION-r$FAKEHTTP_RELEASE.apk"

"$APK" mkpkg \
    --info "name:luci-app-fakehttp" \
    --info "version:$LUCI_VERSION-r$LUCI_RELEASE" \
    --info "description:LuCI support for FakeHTTP" \
    --info "arch:$ARCH" \
    --info "license:GPL-3.0-or-later" \
    --info "origin:feeds/base/luci-app-fakehttp" \
    --info "url:https://github.com/alzpqm/FakeHTTP" \
    --info "maintainer:FakeHTTP maintainers" \
    --info "depends:fakehttp luci-base rpcd-mod-file" \
    --script "post-install:$LUCI_SCRIPTS/post-install" \
    --script "post-upgrade:$LUCI_SCRIPTS/post-upgrade" \
    --script "pre-deinstall:$LUCI_SCRIPTS/pre-deinstall" \
    --files "$LUCI_ROOT" \
    --output "$OUT_DIR/luci-app-fakehttp-$LUCI_VERSION-r$LUCI_RELEASE.apk"

sha256sum "$OUT_DIR/fakehttp-$FAKEHTTP_VERSION-r$FAKEHTTP_RELEASE.apk" \
    "$OUT_DIR/luci-app-fakehttp-$LUCI_VERSION-r$LUCI_RELEASE.apk"
