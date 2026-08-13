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

The compatibility-only follow-up above was performed on the Debian build host
and local repository. The later deployment test changed only FakeHTTP/UCI and
queue512; queue513 was not read or modified.

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
  `1bee2dfcf66217d341f637f1f55c87caa54856c27065b4baf632395ae40348f2`
- `luci-app-fakehttp-99.2-r11.apk`: SHA-256
  `5bb359b60b65fb30d23e6abb698f3751702036ededa10be46d2d904130b1e5d2`

The SDK `apk verify --allow-untrusted` check passed for both.

## 2026-08-14 deployment evidence

The corrected APKs were installed on the OpenWrt 25.12.5 x86_64 router.
FakeHTTP and LuCI both report `99.2-r11 x86_64`. The 30-minute non-silent test
had one PID, RSS `860-928 kB`, VmSize `1152 kB`, one thread, and five FDs.
Queue512 backlog/kernel/user drop stayed `0/0/0` for all 30 samples, and all
three WAN error/drop counters stayed zero. Silent mode was restored afterward;
the final PID was `20449`.
