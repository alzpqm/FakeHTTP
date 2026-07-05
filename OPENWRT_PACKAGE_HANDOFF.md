# FakeHTTP OpenWrt Package Handoff

This file records the OpenWrt package/LuCI work so it survives chat context
compaction. It intentionally excludes router passwords and secrets.

## Current Branch

- Branch: `codex/bug-hunt`
- Base bug-hunt commits are already in this branch:
  - `9d81228 Fix packet handling and document OpenWrt audit`
  - `772bb8d Handle IPv6 extension headers`
  - `ddaf1fb Document three-exit OpenWrt deployment test`
  - `9b9337c Harden logging signals and nfqueue setup`

## Added Package Files

- `openwrt/fakehttp/Makefile`
- `openwrt/fakehttp/files/etc/config/fakehttp`
- `openwrt/fakehttp/files/etc/init.d/fakehttp`
- `openwrt/fakehttp/files/usr/sbin/fakehttp-setup`
- `openwrt/luci-app-fakehttp/Makefile`
- `openwrt/luci-app-fakehttp/htdocs/luci-static/resources/view/fakehttp.js`
- `openwrt/luci-app-fakehttp/root/usr/share/luci/menu.d/luci-app-fakehttp.json`
- `openwrt/luci-app-fakehttp/root/usr/share/rpcd/acl.d/luci-app-fakehttp.json`
- `openwrt/README.md`
- `scripts/smoke-openwrt-package.sh`

## Package Design

- The package installs the binary to `/usr/bin/fakehttp`, matching the upstream
  `make install` path.
- The init script uses procd and UCI.
- UCI schema is intentionally compatible with the older third-party package:
  - `config globals 'globals'`
  - `config payload`
  - `config advanced 'advanced'`
- Interfaces may be Linux device names such as `pppoe-wan2`, or logical network
  names that OpenWrt can resolve with `network_get_device`.
- LuCI is available at `Services -> FakeHTTP`.
- LuCI includes configuration fields plus service status, Restart, and Stop
  controls.
- The package post-install enables the init.d boot hook, but the default UCI
  config keeps FakeHTTP disabled until the user enables it or runs
  `fakehttp-setup`.

## Build Notes

OpenWrt SDK package dependency names are build-system package names, not APK
runtime names:

- `libnetfilter-queue`
- `libnfnetlink`
- `libmnl`
- `nftables-nojson`
- `kmod-nfnetlink-queue`
- `kmod-nft-queue`

Using APK runtime names such as `libnetfilter-queue1` in `DEPENDS` causes SDK
dependency warnings.

The local smoke test is:

```sh
scripts/smoke-openwrt-package.sh
```

It validates shell syntax, LuCI JavaScript syntax, JSON ACL/menu files, and
basic package file presence.

## Router State Observed On 2026-07-05

OpenWrt target:

- OpenWrt `25.12.5`, x86_64, kernel `6.12.94`
- Hostname: `cache1`
- Package manager: APK

Installed FakeHTTP package state before the final `99.2-r2` upgrade:

- `fakehttp-99.1-r1 x86_64 {local/fakehttp}`
- `luci-app-fakehttp-99.1-r1 x86_64 {local/luci-app-fakehttp}`
- Active binary: `/usr/bin/fakehttp`
- `/usr/sbin/fakehttp` no longer exists on the router.

Important difference from the older bug-hunt runtime notes:

- The older hand-deployed r3 binary was documented as `/usr/sbin/fakehttp` with
  sha256 `a353e9f909b4a48c0dec819b94539c8755fbb3357a14ef4203685b89d9aa49ba`.
- Current runtime uses the packaged `/usr/bin/fakehttp` instead. Do not use the
  old `/usr/sbin/fakehttp` sha256 as proof for the current package binary.

Runtime command observed:

```text
/usr/bin/fakehttp -i pppoe-wan2 -i pppoe-wancm -i pppoe-wanct -s -h cgw.mil.cn -e cgw.mil.cn -h download.mail.mil.cn -e download.mail.mil.cn
```

Health checks observed before the `99.2-r2` upgrade:

- FakeHTTP process was running for more than five hours.
- RSS was about 904 KiB.
- File descriptors: 5.
- Threads: 1.
- NFQUEUE 512 had zero kernel/user drops.
- `ip fakehttp` and `ip6 fakehttp` nft tables were present.
- `pppoe-wan2`, `pppoe-wancm`, and `pppoe-wanct` each passed forced-interface
  ping checks with 0% packet loss.

## Final Package Deployment

Current source backup:

- Branch: `codex/bug-hunt`
- Pushed to fork: `https://github.com/alzpqm/FakeHTTP.git`
- Current commit after package release bump: `ae030a0`

Built local test packages:

- Debian build output:
  `/root/fakehttp-openwrt-test/manual/out-ae030a0/`
- Local Mac artifact backup:
  `dist/openwrt/`
- `fakehttp-99.2-r2.apk` sha256:
  `070f7592c3346cb32576f3a7c52d565bcfb89b8942fd0d76cf0d05142ba10c28`
- `luci-app-fakehttp-99.2-r2.apk` sha256:
  `e021d54e2cd8588f17b5973a1fd678ce97e5f6c1765438dcb04954645f77f3e2`

Router package state after upgrade:

- `fakehttp-99.2-r2 x86_64 {local/fakehttp}`
- `luci-app-fakehttp-99.2-r2 x86_64 {local/luci-app-fakehttp}`
- `kmod-nfnetlink-queue-6.12.94-r1` installed
- `kmod-nft-queue-6.12.94-r1` installed
- Active binary: `/usr/bin/fakehttp`
- Active binary sha256:
  `a66f16a0124c05740b78c79f9296807314315bc16da5c425d8df698464e453bd`
- Embedded version string:
  `FakeHTTP version 99.2-r2-ae030a0`

Router runtime after upgrade:

- Service status: running.
- Init boot hook: enabled.
- Process observed: `/usr/bin/fakehttp`.
- Active config preserved at `/etc/config/fakehttp`.
- The temporary `/etc/config/fakehttp.apk-new` file was removed after verifying
  the active config was preserved.
- `ip fakehttp` and `ip6 fakehttp` nft tables were present.
- NFQUEUE 512 was owned by FakeHTTP and had zero kernel/user drops.
- `pppoe-wan2`, `pppoe-wancm`, and `pppoe-wanct` all passed ping checks with
  0% packet loss.
- Packet captures on all three PPPoE exits saw fake HTTP payloads before the
  real HTTP request:
  - `pppoe-wan2`: fake `Host: cgw.mil.cn`, real `Host: 118.64.0.36`
  - `pppoe-wancm`: fake `Host: download.mail.mil.cn`, real
    `Host: 118.64.0.36`
  - `pppoe-wanct`: fake `Host: cgw.mil.cn`, real `Host: 118.64.0.36`
- Each successful capture reported zero tcpdump kernel drops.

## Router Backups

Older router files were backed up under:

```text
/root/fakehttp-backup-20260705-043348
```

That backup included the older package files/config before local package
replacement.

Upgrade backups from the final package work:

```text
/root/fakehttp-upgrade-backup-20260705-151630
/root/fakehttp-upgrade-backup-20260705-151827
```

The second backup is the one taken immediately before upgrading to
`fakehttp-99.2-r2`.
