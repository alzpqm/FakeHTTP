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

Installed FakeHTTP package state:

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

Health checks observed:

- FakeHTTP process was running for more than five hours.
- RSS was about 904 KiB.
- File descriptors: 5.
- Threads: 1.
- NFQUEUE 512 had zero kernel/user drops.
- `ip fakehttp` and `ip6 fakehttp` nft tables were present.
- `pppoe-wan2`, `pppoe-wancm`, and `pppoe-wanct` each passed forced-interface
  ping checks with 0% packet loss.

## Router Backups

Older router files were backed up under:

```text
/root/fakehttp-backup-20260705-043348
```

That backup included the older package files/config before local package
replacement.

## Current Follow-Up

The next clean step is to rebuild a new APK from this exact `codex/bug-hunt`
worktree after the OpenWrt package and LuCI files are committed, then install it
on the router so the router package version and git commit are traceable.
