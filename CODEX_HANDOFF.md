# FakeHTTP Codex Handoff

This file is the durable handoff for investigation work. Update it before
context compaction. Keep facts, hypotheses, and unverified work explicitly
separate. Do not report a test as complete unless its output was captured and
reviewed.

## Scope and safety

- Repository: `/Users/sirtungshenghsiao/Documents/fakehttp`
- Current branch: `codex/input-validation`
- FakeHTTP owns NFQUEUE 512 on the OpenWrt router.
- FakeSIP owns NFQUEUE 513. Do not read, restart, stop, or modify it during
  FakeHTTP work unless the user explicitly coordinates a joint window.
- Router access is through Debian `192.168.9.190` to `192.168.9.1:33501`.

## Verified on 2026-08-11, 2026-08-13, and 2026-08-14

- Local worktree was clean before this investigation.
- Router uptime at 08:10:37 GMT was 6 days, 12:28.
- FakeHTTP package state was `silent=1`, PID `15563`, with the original three
  PPPoE interfaces and the current payload configuration.
- Two captured snapshots 60 seconds apart kept PID, start time, VmSize,
  VmRSS, RssAnon, FD count, and thread count unchanged: 1152 kB, 932 kB,
  176 kB, 5 FDs, and 1 thread.
- Queue 512 packet count advanced from 2,336,579 to 2,336,995. Backlog,
  kernel drop, and userspace drop stayed zero.
- All rx/tx errors and drops for `pppoe-wan2`, `pppoe-wancm`, and
  `pppoe-wanct` stayed zero during that sample.
- No matching FakeHTTP/dmesg anomaly was added during the 60-second sample.
- The 30-minute non-silent test from the prior session was captured and
  completed; it kept queue 512 healthy and restored silent mode afterward.

## Confirmed issues and r10 verification

- Router log contained one real line at `2026-08-10 20:30:59`:
  `nfq_handle_packet(): failure`.
- FakeHTTP did not restart, and the current queue counters do not show a
  corresponding backlog or drop.
- The old code checked `nfq_handle_packet()` with `res < 0`, although the
  libnetfilter_queue contract defines any non-zero return as failure. It also
  discarded the return value, receive length, and errno context.
- The patch changes the check to `res != 0` and records `result`, `len`, and
  errno. It does not change verdict selection or packet transformation.
- Package revision was bumped from `99.2-r8` to `99.2-r9` for this fix.

- Installing r9 at 08:21:35 GMT exposed a second confirmed issue during the
  expected service restart: `waitpid(): Interrupted system call` from the old
  process. The new process started normally and queue 512 stayed healthy.
- The next patch retries `write()` and `waitpid()` on `EINTR` and adds
  `tests/test-process.c`, which uses a non-restarting `SIGALRM` to interrupt a
  real child wait. Package revision is now `99.2-r10`.

- The r10 source checks passed in a fresh Linux VM directory: release build,
  CLI/core/packet/process/signal tests, ASan/LeakSan/UBSan tests,
  `-Wformat=2 -Werror=format`, GCC `-fanalyzer`, clang-format, diff check, and
  OpenWrt package smoke.
- Debian's x86_64 OpenWrt 25.12.5 SDK built
  `fakehttp-99.2-r10.apk` successfully. Local SHA-256:
  `56e028957b4e72b0f6d207d26987e53a45e7df61ec488040364b37a00101282f`.
- r10 was installed on the router at `2026-08-11 08:30:15 UTC`. The pre-r10
  binary backup SHA-256 was
  `2c8465a40a1896caee5bd32dc0980aafb50633c866f9f77482915355158497e2` and
  the pre-r10 config backup was
  `6c81b6839a940f879982390cf274028ecb484ebd0ed231d9a87c9a367c98930b`.
- After installation, FakeHTTP was PID `24841`, command line and payloads were
  unchanged, `silent=1`, and the executable SHA-256 was
  `e76f3c011cbb3ef4974b5f74a0ff13df8493fd328730ad8837d4415677cc75de`.
  The UCI config SHA stayed
  `6c81b6839a940f879982390cf274028ecb484ebd0ed231d9a87c9a367c98930b`.
- New PID `24841` startup logs contain no `waitpid` or `nfq_handle_packet`
  error. The log ring still contains the historical r9 `waitpid(): Interrupted
  system call` line for PID `15563`; it is not a new r10 event.
- A 60-second silent health sample from `08:31:11` to `08:32:12 UTC` kept
  PID/start time, VmSize 1152 kB, FD count 5, and thread count 1 unchanged.
  RSS was 860 -> 864 kB and RssAnon 108 -> 112 kB. Queue 512 advanced from
  `512 24841 0 2 65531 0 0 1042 1` to
  `512 24841 0 2 65531 0 0 2084 1`; backlog, kernel drops, and userspace
  drops stayed zero. All three WAN error/drop counters stayed zero. dmesg
  anomaly count was zero; the two application anomaly matches were historical
  lines already in the ring.
- No post-r10 speed test was run. These fixes improve error handling and
  diagnostics; the captured evidence does not establish that they caused or
  remove a carrier/host-specific throughput cap.
- A final read-only check at `2026-08-11 08:34:07 UTC` still showed PID
  `24841`, `silent=1`, queue 512 as `512 24841 0 2 65531 0 0 4691 1`, and no
  new `waitpid`/`nfq_handle_packet` line for that PID.

## Two-day r10 audit on 2026-08-13

- The router read-only audit ran from `2026-08-13 05:15:32` to
  `05:16:33 UTC` (`13:15:32` to `13:16:33` Taipei). PID `24841` and
  `/proc` starttime `56449108` were unchanged. This is approximately 44 hours
  and 45 minutes after the r10 installation, not a full 48-hour measurement.
- Both snapshots reported `silent=1`, VmSize `1152 kB`, VmRSS `928 kB`,
  RssAnon `176 kB`, VmSwap `0 kB`, one thread, and five file descriptors.
  CPU ticks changed only from `utime=471/stime=2657` to
  `utime=473/stime=2658`; this is not evidence of a CPU runaway.
- Queue 512 advanced from
  `512 24841 0 2 65531 0 0 1964643 1` to
  `512 24841 0 2 65531 0 0 1965711 1`. The queue backlog, kernel drop,
  and userspace drop fields stayed zero. Only the exact queue 512 line was
  read in this audit; queue 513 was not read or modified.
- For `pppoe-wan2`, `pppoe-wancm`, and `pppoe-wanct`, rx/tx errors and drops
  were zero in both snapshots. The packet counters advanced, showing normal
  traffic during the sample.
- The FakeHTTP anomaly count and the relevant dmesg count were both zero at
  both snapshots. This is a current ring-buffer observation, not proof that
  no historical event occurred during the entire uptime.
- No new confirmed bug was found in the source review. The existing r9/r10
  fixes remain the only confirmed runtime findings in this investigation.

## Test environment truthfulness

## OpenWrt pre-25 compatibility work on 2026-08-13

- The source tree now targets OpenWrt 21.02 through 25.12. The compatibility
  claim is intentionally not extended to 19.07 or older releases without a
  separate toolchain and LuCI validation pass.
- `openwrt/fakehttp/Makefile` is now `99.2-r11`. Its firewall dependencies are
  conditional: firewall4 selects `nftables-json` and `kmod-nft-queue`; the
  legacy `firewall` package selects `iptables-mod-nfqueue` and
  `iptables-mod-conntrack-extra`. `kmod-nfnetlink-queue` and the three user
  space NFQUEUE libraries remain common dependencies.
- `STRIP` now uses the SDK-provided `TARGET_STRIP`, which is portable across
  the older SDK toolchains reviewed here.
- `openwrt/luci-app-fakehttp/Makefile` is now `99.2-r11` and declares
  `rpcd-mod-file`, required by the LuCI `fs.exec` service controls on minimal
  images.
- `tools/build-openwrt-ipk.sh` builds both IPK recipes from a matching SDK,
  copies the resulting artifacts, and removes only its temporary recipe
  symlinks. Local shell syntax, package smoke, JavaScript syntax, and diff
  checks passed after this change.
- OpenWrt 22.03.7 package definitions were inspected and contain the named
  iptables, NFQUEUE kernel, nft queue, and `rpcd-mod-file` packages. A Debian
  22.03 SDK cross-toolchain compiled the current FakeHTTP binary successfully
  with GCC 11/musl. The SDK `ipkg-build` script then produced valid IPK
  containers for both packages:
  `dist/openwrt-22.03-manual/fakehttp_99.2-11_x86_64.ipk` with SHA-256
  `201ba0843fb34b6faaee638ad588b211bc82269f0dadeda90752dc992acd44eb`, and
  `dist/openwrt-22.03-manual/luci-app-fakehttp_99.2-11_all.ipk` with SHA-256
  `9742fdf023f7c69dd7c2e3f3b8b1db10b2525536aaf7d18ac96e2a04d75d4568`.
- The 22.03 SDK's normal package target was not claimed as a complete build:
  its existing buildbot/all-packages configuration began an unrelated
  281 MB Linux firmware build before FakeHTTP packaging, so that attempt was
  stopped. The verified 22.03 result is therefore cross-compilation plus SDK
  IPK assembly, not a complete clean dependency-graph build.
- A separate OpenWrt 25.12.5 x86_64 SDK cross-toolchain also compiled the
  current source successfully. No new package was installed on the router in
  this compatibility task; the router remains on the previously installed
  FakeHTTP r10 silent runtime. Queue 512 was not touched by this work, and
  queue 513/FakeSIP was not read or modified.
- The 25.12.5 APK builder produced `fakehttp-99.2-r11.apk` with SHA-256
  `95877781fba988bde87de7cb7d68824f5d1c4296d6f238a075507ef62da1a415` and
  `luci-app-fakehttp-99.2-r11.apk` with SHA-256
  `8436c1985c4ec2cc1833ca9ee9a4e0b29a0dabbd19d185b8b849fbf3427fbaa9`.
  SDK `apk verify --allow-untrusted` passed for both artifacts, and the
  extracted data contains the expected binary, init/UCI, LuCI, and ACL files.

## 2026-08-14 Version synchronization

- Both OpenWrt recipes now use package revision `99.2-r11`.
- The 22.03 IPK and 25.12 APK artifacts were rebuilt so their embedded
  package versions and filenames are synchronized for FakeHTTP and LuCI.
- The release update does not install anything on the production router. The
  live router remains on FakeHTTP r10 silent mode, and queue 512 was not
  modified. FakeSIP and queue 513 were not read or modified.

- A direct test command accidentally ran on macOS first. It failed because
  this project requires Linux headers/libraries and Apple Clang does not
  support the requested LeakSanitizer flag. Those failures are environment
  failures, not code-test passes or code-test regressions.
- Debian Linux then ran release tests, ASan/LeakSan/UBSan tests,
  `-Wformat=2 -Werror=format`, and GCC `-fanalyzer`; all passed, including the
  process EINTR regression. Local OpenWrt package smoke and git diff checks
  passed. Debian could not run clang-format because `clang-format` is not
  installed there; no format-pass claim is made for this audit.
- Six secondary handoff files were created as plain ASCII English for robust
  transfer across context compression:
  `handoff/01_state.md`, `handoff/02_live_audit.md`,
  `handoff/03_confirmed_fixes.md`, `handoff/04_test_matrix.md`,
  `handoff/05_limits.md`, and `handoff/06_resume.md`.

- During one read intended to inspect queue 512 formatting, the full proc queue
  file was displayed and therefore also showed queue 513. No queue 513 rule,
  process, or configuration was changed. Future checks must avoid reading the
  full file and select only the queue 512 line.
- The local health script and Debian copy were removed. OpenWrt BusyBox has no
  `unlink` or `find -delete`, so the two small router-side temporary files
  `/tmp/codex-fakehttp-health-60s.sh` and
  `/tmp/fakehttp-r10-health-60s.out` remain until a supported cleanup method is
  used. They are not executed and do not affect FakeHTTP.

## Evidence limits

- The single historical `nfq_handle_packet()` line does not prove the cause of
  the user's speed variation. It is an observability/runtime-error signal,
  not a demonstrated throughput regression.
- The two-snapshot sample is not a 48-hour leak proof. It only confirms no
  short-window growth or queue/WAN error increase.
- The r10 process has now accumulated approximately 44 hours and 45 minutes,
  but the two snapshots still do not prove a mathematical absence of a very
  slow leak. A longer scheduled trend is still stronger evidence.
- No speed test was run during this audit. Nothing here proves or disproves a
  carrier whitelist, destination-specific cap, or Global Speed Test behavior.
- A possible `write()` return-zero infinite-loop edge was considered during
  source review but was not reproduced and has no current evidence. Do not
  label it a confirmed bug without a reproducer.

## Next steps

1. Keep r10 in silent production mode and collect a scheduled multi-hour or
   24-hour RSS/FD/queue512 trend before making a stronger long-term leak claim.
2. If a controlled download is run, inspect any new `nfq_handle_packet()` line
   for its result/length/errno data; do not infer a speed conclusion from the
   absence of an error line.
3. Keep the r10 APK and pre-install backups until rollback is no longer needed.
4. Before every context compaction and at the end of every research cycle,
   update this file and the six files under `handoff/`.
