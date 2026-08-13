# FakeHTTP OpenWrt Package Handoff

This file records the OpenWrt package/LuCI work so it survives chat context
compaction. It intentionally excludes router passwords and secrets.

## Current Branch

- Branch: `codex/input-validation`
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
- `nftables` (the virtual package dependency)
- `kmod-nfnetlink-queue`
- `kmod-nft-queue`

Using APK runtime names such as `libnetfilter-queue1` in `DEPENDS` causes SDK
dependency warnings. Depending on the `nftables` virtual package is also
important: a hard dependency on `nftables-nojson` conflicts with routers that
already use the mutually exclusive `nftables-json` provider required by
`firewall4`.

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
- Latest follow-up source commit: `f48358e`
- Current deployed package source commit: `ae030a0`
- Follow-up package source has been bumped to `99.2-r3` to harden upgrade
  maintainer-script behavior. The `prerm` script now stops the service but only
  disables the boot hook when called with an explicit remove/deinstall/uninstall
  action, preventing local APK upgrades from accidentally clearing
  `/etc/rc.d/S99fakehttp`.

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

Follow-up observation on 2026-07-05 around 23:26 Asia/Taipei:

- Router still had `fakehttp-99.2-r2` and `luci-app-fakehttp-99.2-r2`
  installed.
- Service was running as PID `753` with RSS about 832 KiB, 5 file descriptors,
  and 1 thread.
- `/usr/bin/fakehttp` sha256 was still
  `a66f16a0124c05740b78c79f9296807314315bc16da5c425d8df698464e453bd`.
- Embedded version string was still `FakeHTTP version 99.2-r2-ae030a0`.
- `/etc/init.d/fakehttp enabled` initially returned non-zero after the local APK
  upgrade. Running `/etc/init.d/fakehttp enable` recreated
  `/etc/rc.d/S99fakehttp` and `/etc/rc.d/K10fakehttp`; it now returns `0`.
- NFQUEUE 512 was owned by FakeHTTP PID `753`, with zero kernel/user drops and
  over 10k packets observed.
- `ip fakehttp` and `ip6 fakehttp` nft tables were present for all three exits.
- `pppoe-wan2`, `pppoe-wancm`, and `pppoe-wanct` each passed forced-interface
  ping checks to `223.5.5.5` with 0% packet loss.
- `dmesg` showed no fakehttp/NFQUEUE/OOM/segfault messages.

8-hour follow-up observation on 2026-07-06 around 07:18 Asia/Taipei:

- Router time was `Sun Jul 5 23:18:10 GMT 2026`.
- Router uptime was 4 days and 21 hours; system load was about
  `0.08, 0.11, 0.09`.
- Installed packages were still `fakehttp-99.2-r2` and
  `luci-app-fakehttp-99.2-r2`.
- FakeHTTP service was still boot-enabled and running:
  `/etc/init.d/fakehttp enabled` returned `0`, status was `running`, PID was
  `753`.
- Active binary remained `/usr/bin/fakehttp`, sha256
  `a66f16a0124c05740b78c79f9296807314315bc16da5c425d8df698464e453bd`, with
  embedded version `FakeHTTP version 99.2-r2-ae030a0`.
- Process resource usage remained small: VmRSS about 872 KiB, 5 file
  descriptors, and 1 thread.
- NFQUEUE 512 was still owned by FakeHTTP PID `753`; queue length was 0,
  kernel drops were 0, user drops were 0, and the observed packet sequence
  counter was 523,610.
- `ip fakehttp` and `ip6 fakehttp` nft tables remained present for
  `pppoe-wan2`, `pppoe-wancm`, and `pppoe-wanct`.
- Forced-interface pings from all three exits to `223.5.5.5` returned 3/3
  packets with 0% loss. Average RTTs were about 32.7 ms, 34.1 ms, and 14.8 ms.
- The router was running with `fakehttp.globals.silent=1`, so per-connection
  runtime trace messages were intentionally suppressed. The `logread` check only
  proves that non-silent startup/exit/error logs were clean; it is not evidence
  of verbose per-connection logging.
- `logread` fakehttp entries after the package restart only showed normal
  startup lines. A suspicious log grep for fakehttp segfault/OOM/fail/cannot/
  invalid/drop/crash terms was empty.
- `dmesg` suspicious grep was empty.
- Gemini CLI was run with temporary
  `GOOGLE_CLOUD_PROJECT=rock-strength-463610-g1` and no API key. It agreed
  there was no major blocker for promoting the package based on the health
  counters and error-log checks; the only residual risks it noted were normal
  long-horizon stability and untested high-pressure traffic profiles.

3-minute non-silent runtime log test on 2026-07-06 around 07:34-07:38
Asia/Taipei:

- Before the test, `fakehttp.globals.silent=1`, service status was running, and
  NFQUEUE 512 had zero kernel/user drops.
- Temporarily set `fakehttp.globals.silent=0`, committed UCI, and restarted
  FakeHTTP. The non-silent test process was PID `4893`.
- During the 3-minute window, ran 9 rounds of forced-interface HTTP requests
  through `pppoe-wan2`, `pppoe-wancm`, and `pppoe-wanct`. All 27 requests
  completed and returned HTTP 404 from the test endpoint, which is acceptable
  for the endpoint and proves the TCP/HTTP path stayed usable.
- At the end of the non-silent window, NFQUEUE 512 was still owned by PID
  `4893`, queue length was 0, and kernel/user drops were both 0. The observed
  queue packet counter for that PID was 6,137.
- Non-silent `logread` captured verbose runtime activity:
  - 907 lines for PID `4893`
  - 150 lines containing `FAKE`
  - 150 lines containing `SYN-ACK`
- The verbose log contained normal connection trace lines such as `SYN`,
  `SYN-ACK`, and `FAKE(*)`. The sample includes both induced test traffic and
  ambient router traffic; it should not be interpreted as only the 27 curl
  requests.
- A grep for PID `4893` messages containing `ERROR`, `WARNING`, `too many`,
  `failed`, `segfault`, `oom`, or `invalid` returned no lines.
- After the test, restored `fakehttp.globals.silent=1`, committed UCI, and
  restarted FakeHTTP. Final service state was boot-enabled and running as PID
  `6557`, with NFQUEUE 512 owned by PID `6557` and zero kernel/user drops.
- Final `dmesg` suspicious grep was empty.

## Router Backups

Older router files were backed up under:

```text
/root/fakehttp-backup-20260705-043348
```

## 2026-08-13 Pre-25 Compatibility Update

The package source now claims OpenWrt 21.02 through 25.12. OpenWrt 25.12 and
newer use APK/firewall4; 24.10, 23.05, and 22.03 use IPK/opkg with firewall4;
21.02 uses IPK/opkg with firewall3/iptables. OpenWrt 19.07 and older remain
outside the release claim.

The current source package revisions are FakeHTTP `99.2-r11` and
`luci-app-fakehttp 99.2-r11`. FakeHTTP's `DEPENDS` now conditionally selects
firewall4 nftables packages or firewall3 iptables NFQUEUE/connbytes packages,
and its build recipe uses `TARGET_STRIP`. LuCI now declares `rpcd-mod-file`
for its `fs.exec` service controls. The reusable IPK builder is
`tools/build-openwrt-ipk.sh`.

Verified local artifacts from the OpenWrt 22.03.7 GCC 11/musl toolchain and
SDK IPK packager are under `dist/openwrt-22.03-manual/`:

- `fakehttp_99.2-11_x86_64.ipk`, SHA-256
  `201ba0843fb34b6faaee638ad588b211bc82269f0dadeda90752dc992acd44eb`
- `luci-app-fakehttp_99.2-11_all.ipk`, SHA-256
  `9742fdf023f7c69dd7c2e3f3b8b1db10b2525536aaf7d18ac96e2a04d75d4568`

The supplied 22.03 SDK's normal package target was not counted as a complete
pass because its buildbot/all-packages configuration began an unrelated
Linux firmware download. The binary cross-compile and SDK IPK assembly did
pass. No compatibility package was installed on the production router; the
live r10 FakeHTTP service and queue 512 were left unchanged.

The OpenWrt 25.12.5 x86_64 APK release artifacts are also verified locally:

- `fakehttp-99.2-r11.apk`, SHA-256
  `95877781fba988bde87de7cb7d68824f5d1c4296d6f238a075507ef62da1a415`
- `luci-app-fakehttp-99.2-r11.apk`, SHA-256
  `8436c1985c4ec2cc1833ca9ee9a4e0b29a0dabbd19d185b8b849fbf3427fbaa9`

The matching 25.12.5 SDK `apk verify --allow-untrusted` check passed for both
files. These release artifacts were not installed on the production router in
this release operation.

## 2026-08-14 Version synchronization

Both package recipes now use `99.2-r11`; no current release artifact uses a
different LuCI revision. The rebuilt IPK/APK artifacts carry the same revision
in their embedded metadata and filenames.

That backup included the older package files/config before local package
replacement.

Upgrade backups from the final package work:

```text
/root/fakehttp-upgrade-backup-20260705-151630
/root/fakehttp-upgrade-backup-20260705-151827
```

The second backup is the one taken immediately before upgrading to
`fakehttp-99.2-r2`.

## Formal Release Validation On 2026-07-16

- The router ran `fakehttp-99.2-r2` continuously for about 9 days before the
  release upgrade, with roughly 12.37 million NFQUEUE 512 packets observed and
  zero kernel/user queue drops.
- The formal package is `fakehttp-99.2-r5`; the LuCI package remains
  `luci-app-fakehttp-99.2-r2` because its contents did not change.
- The APK package has both `post-install` and `post-upgrade` hooks. The latter
  is required by OpenWrt 25.12 APK upgrades and enables then restarts the
  service after an upgrade.
- The r4-to-r5 upgrade changed the running PID from `6495` to `7917`
  automatically, proving that the new executable was loaded without a manual
  restart.
- The active UCI configuration sha256 remained
  `6be2e6d257024198d2dd867d3c466e2456b953fb2b9fb7755d61c2be44214e8b`
  across the upgrade. The service remained boot-enabled and running in silent
  mode on all three configured PPPoE exits.
- Forced-interface HTTP checks through `pppoe-wan2`, `pppoe-wancm`, and
  `pppoe-wanct` all completed successfully after the upgrade. NFQUEUE 512
  remained active with zero kernel/user drops, and the error scans were empty.
- Release artifact checksums:
  - `fakehttp-99.2-r5.apk`:
    `4b115deb5442401578778e18e9c43cb9ae8f52e284e74685c35963ba600f72f2`
  - `luci-app-fakehttp-99.2-r2.apk`:
    `9bddf64c1bfee9c109a3ddafdfc0dd45534a510acd650ba4f20efc82fe3cfc74`
- The valid pre-upgrade router backup is:
  `/root/fakehttp-release-backup-r4-to-r5-20260716-051738`.

## r6 And LuCI r3 Validation On 2026-07-29

- Candidate packages `fakehttp-99.2-r6` and `luci-app-fakehttp-99.2-r3` were
  built with the OpenWrt 25.12.5 x86_64 SDK and installed on the production
  router after creating `/root/fakehttp-backups/r6-test-20260729-015133`.
- The first install attempt exposed an APK dependency bug: requiring
  `nftables-nojson` conflicted with the installed `nftables-json` provider.
  Changing the package to depend on virtual `nftables` allowed installation
  without replacing `firewall4` dependencies or changing the active service.
- The LuCI page now separates status read permission from Start, Restart, and
  Stop write permission. It uses `fs.exec`, reports action failures, polls for
  the expected state, and no longer applies unrelated pending LuCI changes when
  Restart is pressed. Payload validation now matches backend ASCII/path rules,
  disabled empty payload rows remain editable, and queue number 0 is accepted.
- Real LuCI testing confirmed Running and Stopped states, button enablement,
  Stop removing both the process and queue 512, Start creating PID 29341, and
  Restart replacing it with PID 29713. A 90-character Unicode hostname was
  rejected before UCI write, and reloading restored the original form value.
- A 900-second non-silent window kept PID 13013 unchanged. Every one-minute
  sample had queue 512 backlog 0, kernel drops 0, and userspace drops 0. The
  packet sequence advanced from 9,172 to 19,067 between the first and final
  minute samples.
- The window captured 13,857 new log lines, 13,825 from FakeHTTP, with zero
  anomaly matches and no related kernel messages. RSS warmed from 888 to 924
  KiB and stayed at 924 KiB from minute 9 through minute 15; VmSize 1,152 KiB,
  swap 0, five file descriptors, and one thread stayed fixed.
- Fifteen fixed 64 MiB TUNA downloads all returned HTTP 206 and the complete
  67,108,864 bytes from `101.6.15.130`. Median throughput was 210.98 Mbps, with
  an 86.86-328.43 Mbps range. The variation prevents a performance conclusion,
  but download completeness and queue health show no runtime failure.
- Linux release tests, ASan/LeakSan/UBSan tests, GCC `-fanalyzer`, CLI and unit
  tests, LuCI JavaScript/JSON checks, shell checks, package smoke checks, and
  `git diff --check` all passed.
- Production was restored to silent mode as PID 30280 with the original three
  WAN interfaces, six enabled payloads, `repeat=1`, and config SHA-256
  `ce2442350bb96c5896025fb7081a4a85465624beae2f1b1cabbee27dd55a7c6c`.
  Queue 512 again had zero backlog and zero kernel/userspace drops. After 63
  seconds it had processed 464 packets with no verbose SYN/FAKE log growth.
- Non-blocking residual risks are slow leaks beyond the 15-minute window and
  untested fault injection for queue saturation, verdict/send failures, and
  live interface disappearance/recreation.

## r7 Local Numeric Parsing Validation On 2026-07-30

- r6 interpreted all numeric CLI arguments with C base autodetection. This made
  leading-zero values octal, conflicting with the decimal semantics shown by
  LuCI and normally expected from UCI.
- r7 accepts hexadecimal only with an explicit `0x` or `0X` prefix; all other
  numeric values, including those with leading zeros, are decimal. Optional
  leading `+` remains supported.
- This is an intentional compatibility change: legacy octal `010` changes from
  8 to 10, while values such as octal `0377` no longer fit the decimal TTL
  range. Migrate such settings to ordinary decimal or explicit hexadecimal.
- Regression tests distinguish the old and new behavior across fwmark, queue,
  repeat, TTL, and percentage limits and reject malformed prefixes.
- Linux release and sanitizer suites, GCC `-fanalyzer`, local package checks,
  and the OpenWrt 25.12.5 x86_64 SDK build passed.
- Local artifact: `fakehttp-99.2-r7.apk`, SHA-256
  `4a7bbec495f152ea563a3aa5517cce56bcf7e45ec7fa29a10b492a4e3c8cf02c`.
- This candidate was not installed on the production router. No router login,
  service restart, configuration change, temporary mark, or queue 512/513
  access occurred during this validation.
