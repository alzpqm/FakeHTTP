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
  temporary `/usr/sbin` directory creation.
- SDK `apk verify --allow-untrusted` for both APKs: passed.
- APK extraction showed the expected daemon, service/UCI files, LuCI view,
  ACL, and menu files.
- `fakehttp-99.2-r11.apk` SHA-256:
  `95877781fba988bde87de7cb7d68824f5d1c4296d6f238a075507ef62da1a415`.
- `luci-app-fakehttp-99.2-r11.apk` SHA-256:
  `8436c1985c4ec2cc1833ca9ee9a4e0b29a0dabbd19d185b8b849fbf3427fbaa9`.

Version synchronization on 2026-08-14:

- Both OpenWrt Makefiles declare `PKG_VERSION:=99.2` and `PKG_RELEASE:=11`.
- The rebuilt 22.03 LuCI IPK control metadata reports `Version: 99.2-11`.
- The rebuilt 25.12 APKs passed SDK verification and use `99.2-r11` filenames.
