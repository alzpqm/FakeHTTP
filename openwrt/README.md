# OpenWrt package for FakeHTTP

This directory contains OpenWrt package definitions, a procd service, UCI
defaults, a LuCI web page, and a small setup helper for FakeHTTP.

## Install the x86_64 release

These prebuilt APK packages target OpenWrt 25.12 x86_64. On the router, run:

```sh
cd /tmp
wget https://github.com/alzpqm/FakeHTTP/releases/download/openwrt-99.2-r8/fakehttp-99.2-r8.apk
wget https://github.com/alzpqm/FakeHTTP/releases/download/openwrt-99.2-r8/luci-app-fakehttp-99.2-r4.apk
apk add --allow-untrusted ./fakehttp-99.2-r8.apk ./luci-app-fakehttp-99.2-r4.apk
```

Open `Services -> FakeHTTP` in LuCI to configure the service. For a quick
command-line setup, replace the hostname and network name as needed:

```sh
fakehttp-setup www.example.com wan
```

Upgrades preserve `/etc/config/fakehttp`, keep the boot service enabled, and
restart FakeHTTP automatically when the configured service is enabled.

## Build a package with the OpenWrt SDK

From an OpenWrt SDK checkout:

```sh
ln -s /path/to/FakeHTTP/openwrt/fakehttp package/fakehttp
ln -s /path/to/FakeHTTP/openwrt/luci-app-fakehttp package/luci-app-fakehttp
make defconfig
make package/fakehttp/compile V=s FAKEHTTP_SOURCE_DIR=/path/to/FakeHTTP
make package/luci-app-fakehttp/compile V=s
```

The generated packages will be under `bin/packages/*/base/`. OpenWrt 24.10 and
older builds usually emit `.ipk`; newer APK-based builds emit `.apk`.

## Install on a router

```sh
scp bin/packages/*/base/fakehttp_* bin/packages/*/base/luci-app-fakehttp_* root@192.168.1.1:/tmp/
ssh root@192.168.1.1
opkg install /tmp/fakehttp_*.ipk /tmp/luci-app-fakehttp_*.ipk
# or, on APK-based OpenWrt:
# apk add --allow-untrusted /tmp/fakehttp-*.apk /tmp/luci-app-fakehttp-*.apk
fakehttp-setup www.example.com wan
```

For HTTPS-style obfuscation:

```sh
fakehttp-setup --https www.example.com wan
```

## Manual configuration

Edit `/etc/config/fakehttp` when you need advanced options:

```sh
uci set fakehttp.globals='globals'
uci set fakehttp.globals.enabled='1'
uci add_list fakehttp.globals.interface='pppoe-wan'
uci add fakehttp payload
uci set fakehttp.@payload[-1].enabled='1'
uci set fakehttp.@payload[-1].type='http'
uci set fakehttp.@payload[-1].payload='www.example.com'
uci commit fakehttp
/etc/init.d/fakehttp enable
/etc/init.d/fakehttp restart
```

When a service uses a non-standard HTTP-like port with its own request
protocol, skip FakeHTTP injection for that TCP port. The port is skipped in
both directions and can be repeated:

```sh
uci add_list fakehttp.advanced.bypass_port='65499'
uci commit fakehttp
/etc/init.d/fakehttp restart
```

This is useful for China Speed Test data connections, which use port 65499
but require `/speed/...` requests rather than a generic `GET /`.

Use `list interface 'pppoe-wan'` or another Linux interface name in
`/etc/config/fakehttp`. The helper `fakehttp-setup` can accept either a LuCI
network name such as `wan` or a Linux interface name.

Check service status and logs:

```sh
/etc/init.d/fakehttp status
logread -e fakehttp
```

The LuCI page is available at `Services -> FakeHTTP` after installing
`luci-app-fakehttp`.
