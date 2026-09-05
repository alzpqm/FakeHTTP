# OpenWrt package for FakeHTTP

This directory contains the OpenWrt package definitions, a procd service, UCI
defaults, a LuCI page, and a small setup helper for FakeHTTP.

## Compatibility matrix

The package uses the native package format and firewall backend of the target
OpenWrt release:

| OpenWrt release | Package/install tool | Default firewall | FakeHTTP backend |
| --- | --- | --- | --- |
| 25.12 and newer | APK / `apk` | firewall4 / nftables | nftables |
| 24.10, 23.05, 22.03 | IPK / `opkg` | firewall4 / nftables | nftables |
| 21.02 | IPK / `opkg` | firewall3 / iptables | iptables |

The package recipe conditionally depends on the firewall4 or firewall3
extensions selected by the SDK. The core binary contains both backends. If
the nft command is unavailable at runtime, FakeHTTP falls back to iptables;
for deterministic operation on OpenWrt 21.02, set `use_iptables` to `1`.

This compatibility work is intended for OpenWrt 21.02 through 25.12. The
19.07 and older series are end-of-life and are not release-claimed until they
receive a separate toolchain and LuCI validation pass.

The current package revisions are `fakehttp 99.2-r14` and
`luci-app-fakehttp 99.2-r14`.

## Build IPK packages for OpenWrt 24.10 and older

Use an SDK matching the router target, architecture, and release. The helper
adds temporary recipe symlinks to the SDK, builds both packages, copies the
result to an output directory, and removes only the symlinks it created:

```sh
./tools/build-openwrt-ipk.sh /path/to/openwrt-sdk /tmp/fakehttp-ipk
```

The helper builds from the current working tree. It refuses to overwrite an
existing `package/fakehttp` or `package/luci-app-fakehttp` path in the SDK.
The output is normally:

```text
/tmp/fakehttp-ipk/fakehttp_99.2-13_<arch>.ipk
/tmp/fakehttp-ipk/luci-app-fakehttp_99.2-13_all.ipk
```

The equivalent manual SDK commands are:

```sh
ln -s /path/to/FakeHTTP/openwrt/fakehttp package/fakehttp
ln -s /path/to/FakeHTTP/openwrt/luci-app-fakehttp package/luci-app-fakehttp
make defconfig
make package/fakehttp/compile V=s FAKEHTTP_SOURCE_DIR=/path/to/FakeHTTP
make package/luci-app-fakehttp/compile V=s
```

For OpenWrt 22.03, 23.05, and 24.10, keep the default `use_iptables='0'` and
install the firewall4 NFQUEUE packages selected by the SDK. For OpenWrt
21.02, install the iptables NFQUEUE and connbytes extensions and select the
legacy backend before starting the service:

```sh
uci set fakehttp.advanced.use_iptables='1'
uci commit fakehttp
/etc/init.d/fakehttp restart
```

## Install on OpenWrt 24.10 and older

Copy packages built for the exact router target, then install them with
`opkg`:

```sh
scp /tmp/fakehttp-ipk/fakehttp_*.ipk root@192.168.1.1:/tmp/
scp /tmp/fakehttp-ipk/luci-app-fakehttp_*.ipk root@192.168.1.1:/tmp/
ssh root@192.168.1.1
opkg install /tmp/fakehttp_*.ipk
opkg install /tmp/luci-app-fakehttp_*.ipk
```

Do not mix an x86_64 package with another target or install a package built
against a different OpenWrt release's staging libraries.

The LuCI package declares `rpcd-mod-file` because its service controls use
LuCI's `fs.exec` RPC. If the package manager reports it as unavailable, add
the matching LuCI/rpcd feed before installing `luci-app-fakehttp`.

## Build and install on OpenWrt 25.12+

OpenWrt 25.12 and newer use APK packages. Build the current APKs with the
matching SDK:

```sh
./tools/build-openwrt-apk.sh /path/to/openwrt-sdk /tmp/fakehttp-apk
```

The SDK must match the router target and release. Install both packages on
the router with:

```sh
scp /tmp/fakehttp-apk/fakehttp-*.apk root@192.168.1.1:/tmp/
scp /tmp/fakehttp-apk/luci-app-fakehttp-*.apk root@192.168.1.1:/tmp/
ssh root@192.168.1.1
apk add --allow-untrusted /tmp/fakehttp-*.apk
apk add --allow-untrusted /tmp/luci-app-fakehttp-*.apk
```

The APK helper uses the SDK target architecture for both packages, including
the LuCI package. OpenWrt's APK database records installed LuCI packages with
the target architecture rather than the source recipe's `PKGARCH:=all` value.

## Configure and run

Open `Services -> FakeHTTP` in LuCI after installing
`luci-app-fakehttp`. For a quick command-line setup, replace the hostname and
network name as needed:

```sh
fakehttp-setup www.example.com wan
```

For HTTPS-style obfuscation:

```sh
fakehttp-setup --https www.example.com wan
```

Manual UCI configuration is available when advanced options are needed:

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

Use `list interface 'pppoe-wan'` or another Linux interface name in
`/etc/config/fakehttp`. The helper accepts either a LuCI network name such as
`wan` or a Linux interface name.

Check service status and logs:

```sh
/etc/init.d/fakehttp status
logread -e fakehttp
```
