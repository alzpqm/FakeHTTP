# FakeHTTP OpenWrt packages

[正體中文](README.md)

This directory contains the FakeHTTP OpenWrt package definitions, procd
service, UCI defaults, LuCI control page, and setup helper.

## Release scope

GitHub Releases provide only router-tested OpenWrt 25.12+ x86_64 APKs.
Prebuilt IPKs are not provided for OpenWrt 24.10 and older; users of those
releases must build with an SDK matching the router release, target, and
architecture.

| OpenWrt release | Package format | Default firewall | Distribution |
| --- | --- | --- | --- |
| 25.12 and newer | APK / `apk` | firewall4 / nftables | Tested x86_64 APKs on Releases |
| 24.10, 23.05, 22.03 | IPK / `opkg` | firewall4 / nftables | Build with a matching SDK |
| 21.02 | IPK / `opkg` | firewall3 / iptables | Build with a matching SDK |

The current FakeHTTP and LuCI package versions are both `99.2-r15`. OpenWrt
19.07 and older are end-of-life and are not supported by this project.

## Install on OpenWrt 25.12+

Download both x86_64 APKs from the
[99.2-r15 release](https://github.com/alzpqm/FakeHTTP/releases/tag/openwrt-99.2-r15),
verify their checksums, and copy them to the router:

```sh
scp fakehttp-99.2-r15.apk root@192.168.1.1:/tmp/
scp luci-app-fakehttp-99.2-r15.apk root@192.168.1.1:/tmp/
ssh root@192.168.1.1
apk add --allow-untrusted /tmp/fakehttp-99.2-r15.apk
apk add --allow-untrusted /tmp/luci-app-fakehttp-99.2-r15.apk
```

The APKs must match the router release and architecture. Release artifacts are
validated only on OpenWrt 25.12.5 x86_64.

## Build OpenWrt 25.12+ APKs

Use an SDK that exactly matches the router release and architecture:

```sh
./tools/build-openwrt-apk.sh /path/to/openwrt-sdk /tmp/fakehttp-apk
```

The helper uses the SDK compiler, `apk`, and `po2lmo`, and embeds the
Traditional Chinese catalog in the LuCI APK.

## Build IPKs for OpenWrt 24.10 and older

Older packages are not prebuilt on GitHub Releases. Prepare a matching SDK and
run:

```sh
./tools/build-openwrt-ipk.sh /path/to/openwrt-sdk /tmp/fakehttp-ipk
```

Equivalent manual SDK commands are:

```sh
ln -s /path/to/FakeHTTP/openwrt/fakehttp package/fakehttp
ln -s /path/to/FakeHTTP/openwrt/luci-app-fakehttp package/luci-app-fakehttp
make defconfig
make package/fakehttp/compile V=s FAKEHTTP_SOURCE_DIR=/path/to/FakeHTTP
make package/luci-app-fakehttp/compile V=s
```

OpenWrt 22.03, 23.05, and 24.10 use the default nftables backend. OpenWrt
21.02 requires the iptables NFQUEUE and connbytes extensions and the legacy
backend:

```sh
uci set fakehttp.advanced.use_iptables='1'
uci commit fakehttp
/etc/init.d/fakehttp restart
```

Install both locally built IPKs with `opkg install`. Do not mix packages built
for different OpenWrt releases or architectures.

## Configure and run

After installation, open `Services -> FakeHTTP` in LuCI. Traditional Chinese
is embedded in `luci-app-fakehttp`, and the page supports the Bootstrap light
and dark themes.

Quick command-line setup:

```sh
fakehttp-setup www.example.com wan
fakehttp-setup --https www.example.com wan
```

Advanced UCI example:

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

To bypass both directions of a custom HTTP-like TCP service port:

```sh
uci add_list fakehttp.advanced.bypass_port='65499'
uci commit fakehttp
/etc/init.d/fakehttp restart
```

Check service status and logs:

```sh
/etc/init.d/fakehttp status
logread -e fakehttp
```
