#!/bin/sh
set -eu

ROOT="${TMPDIR:-/tmp}/fakehttp-openwrt-smoke.$$"

cleanup()
{
	rm -rf "$ROOT"
}

trap cleanup EXIT INT TERM

mkdir -p "$ROOT/etc/config" "$ROOT/etc/init.d" "$ROOT/usr/sbin"

install -m 0644 openwrt/fakehttp/files/etc/config/fakehttp "$ROOT/etc/config/fakehttp"
install -m 0755 openwrt/fakehttp/files/etc/init.d/fakehttp "$ROOT/etc/init.d/fakehttp"
install -m 0755 openwrt/fakehttp/files/usr/sbin/fakehttp-setup "$ROOT/usr/sbin/fakehttp-setup"

sh -n "$ROOT/etc/init.d/fakehttp"
sh -n "$ROOT/usr/sbin/fakehttp-setup"

grep -q "Package/fakehttp" openwrt/fakehttp/Makefile
grep -q "Package/luci-app-fakehttp" openwrt/luci-app-fakehttp/Makefile
grep -q "config globals 'globals'" "$ROOT/etc/config/fakehttp"
grep -q "fakehttp-setup" openwrt/README.md

json_pp < openwrt/luci-app-fakehttp/root/usr/share/luci/menu.d/luci-app-fakehttp.json >/dev/null
json_pp < openwrt/luci-app-fakehttp/root/usr/share/rpcd/acl.d/luci-app-fakehttp.json >/dev/null
node --check openwrt/luci-app-fakehttp/htdocs/luci-static/resources/view/fakehttp.js >/dev/null

echo "OpenWrt package smoke test passed."
