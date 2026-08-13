# FakeHTTP Handoff 02: Live Audit Evidence

Handoff ID: FH-20260814-02
Audit window: 2026-08-13 05:15:32 to 05:16:33 UTC.

Two read-only snapshots showed:

- PID 24841 unchanged.
- silent=1 unchanged.
- VmSize 1152 kB, VmRSS 928 kB, RssAnon 176 kB, VmSwap 0 kB.
- One thread and five file descriptors.
- CPU ticks changed from utime=471/stime=2657 to utime=473/stime=2658.
- Queue 512 changed from `512 24841 0 2 65531 0 0 1964643 1` to
  `512 24841 0 2 65531 0 0 1965711 1`.
- Queue 512 backlog, kernel drops, and userspace drops were zero in both
  snapshots.
- All rx_errors, rx_dropped, tx_errors, and tx_dropped counters were zero on
  pppoe-wan2, pppoe-wancm, and pppoe-wanct in both snapshots.
- FakeHTTP anomaly count and relevant dmesg count were zero in both snapshots.

Only the exact queue 512 line was selected. Queue 513 was not read or changed
in this audit.

The compatibility follow-up was performed on the Debian build host and local
repository only. It did not change the router process, UCI, nftables, mwan3,
queue 512, or queue 513.

Verified build artifacts from the OpenWrt 22.03 toolchain:

- `fakehttp_99.2-11_x86_64.ipk`: SHA-256
  `201ba0843fb34b6faaee638ad588b211bc82269f0dadeda90752dc992acd44eb`
- `luci-app-fakehttp_99.2-11_all.ipk`: SHA-256
  `9742fdf023f7c69dd7c2e3f3b8b1db10b2525536aaf7d18ac96e2a04d75d4568`

These were built by the 22.03 GCC 11/musl cross-toolchain and assembled with
the SDK `ipkg-build` script. The supplied SDK's full package target was not
claimed because its buildbot configuration entered an unrelated firmware
build.

The matching 25.12.5 APK artifacts were also verified:

- `fakehttp-99.2-r11.apk`: SHA-256
  `95877781fba988bde87de7cb7d68824f5d1c4296d6f238a075507ef62da1a415`
- `luci-app-fakehttp-99.2-r11.apk`: SHA-256
  `8436c1985c4ec2cc1833ca9ee9a4e0b29a0dabbd19d185b8b849fbf3427fbaa9`

The SDK `apk verify --allow-untrusted` check passed for both.
