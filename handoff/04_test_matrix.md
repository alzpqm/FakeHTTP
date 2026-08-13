# FakeHTTP Handoff 04: Test Matrix

Handoff ID: FH-20260814-04

On Debian Linux, the following passed after synchronizing the current source:

- Release build and test suite.
- ASan, LeakSanitizer, and UBSan test suite.
- `-Wformat=2 -Werror=format` test suite.
- GCC `-fanalyzer` build.
- Process EINTR regression test.
- CLI, core, packet, and signal validation tests.

On the local macOS host, a direct make attempt failed because the project
requires Linux headers and libnetfilter_queue headers. Apple Clang also does
not accept the requested LeakSanitizer flag. These are environment failures,
not code-test passes and not evidence of a source regression.

OpenWrt package smoke passed locally. Local `git diff --check` passed.
Debian could not run clang-format because clang-format is not installed there;
no clang-format pass is claimed for this audit.

Compatibility-specific verification on 2026-08-13 and 2026-08-14:

- `sh -n tools/build-openwrt-ipk.sh tools/build-openwrt-apk.sh`: passed.
- `scripts/smoke-openwrt-package.sh`: passed after dependency changes.
- LuCI `node --check`: passed.
- OpenWrt 22.03.7 GCC 11/musl cross-compile of the current binary: passed.
- OpenWrt 22.03.7 SDK `ipkg-build` assembly of FakeHTTP and LuCI IPK files:
  passed; the files contain `debian-binary`, `control.tar.gz`, and
  `data.tar.gz`.
- OpenWrt 25.12.5 GCC 14/musl cross-compile of the current binary: passed.

The normal OpenWrt 22.03 package target was attempted but not counted as a
pass: the supplied SDK's all-packages/buildbot configuration entered an
unrelated Linux firmware download and the attempt was stopped.

OpenWrt 25.12.5 APK verification:

- APK build from the GCC 14/musl SDK: passed after fixing the builder's
  temporary `/usr/sbin` directory creation and LuCI target-architecture field.
- SDK `apk verify --allow-untrusted` for both APKs: passed.
- APK extraction showed the expected daemon, service/UCI files, LuCI view,
  ACL, and menu files.
- `fakehttp-99.2-r11.apk` SHA-256:
  `1bee2dfcf66217d341f637f1f55c87caa54856c27065b4baf632395ae40348f2`.
- `luci-app-fakehttp-99.2-r11.apk` SHA-256:
  `5bb359b60b65fb30d23e6abb698f3751702036ededa10be46d2d904130b1e5d2`.

Version synchronization on 2026-08-14:

- Both OpenWrt Makefiles declare `PKG_VERSION:=99.2` and `PKG_RELEASE:=11`.
- The rebuilt 22.03 LuCI IPK control metadata reports `Version: 99.2-11`.
- The rebuilt 25.12 APKs passed SDK verification and use `99.2-r11` filenames.

OpenWrt 25.12.5 runtime validation on 2026-08-14:

- Corrected FakeHTTP and LuCI APKs installed successfully as `x86_64`.
- A 30-minute non-silent test produced 30/30 samples on one PID. RSS was
  `860-928 kB`, VmSize `1152 kB`, one thread, and five FDs.
- Queue512 backlog/kernel/user drop was `0/0/0` in all samples. All three
  PPPoE interfaces had zero rx/tx errors and drops in all samples.
- The captured follow log had 22,614 lines; the targeted FakeHTTP error search
  returned no matches. Silent mode was restored after the test.
