# FakeHTTP Handoff 03: Confirmed Fixes

Handoff ID: FH-20260814-03

Confirmed finding 1: the old nfqueue loop treated only a negative
nfq_handle_packet result as failure. The r9 patch treats every non-zero result
as failure and logs result, received length, and errno context. This is an
observability and error-path correction; it does not change normal verdict or
packet transformation logic.

Confirmed finding 2: the old process helper treated waitpid interrupted by a
signal as a failure. It also did not retry an interrupted write. The r10 patch
retries write and waitpid on EINTR. tests/test-process.c uses SIGALRM and a
real child process to cover the waitpid regression.

OpenWrt r10 executable SHA-256:
e76f3c011cbb3ef4974b5f74a0ff13df8493fd328730ad8837d4415677cc75de

Local r10 APK SHA-256:
56e028957b4e72b0f6d207d26987e53a45e7df61ec488040364b37a00101282f

The historical r9 waitpid error in the router ring belongs to PID 15563. No
matching error was found for r10 PID 24841 during the final audit.

OpenWrt compatibility fixes completed in source on 2026-08-13:

- FakeHTTP package `99.2-r11` uses conditional firewall4 and firewall3
  dependency groups and the SDK variable `TARGET_STRIP`.
- LuCI package `99.2-r11` declares `rpcd-mod-file` for `fs.exec` controls.
- The compatibility helper is `tools/build-openwrt-ipk.sh`.

The compatibility packages were installed on the production router on
2026-08-13 UTC. The live service is now r11 and silent after validation.

Release artifacts built from this source:

- `fakehttp-99.2-r11.apk`: SHA-256
  `1bee2dfcf66217d341f637f1f55c87caa54856c27065b4baf632395ae40348f2`
- `luci-app-fakehttp-99.2-r11.apk`: SHA-256
  `5bb359b60b65fb30d23e6abb698f3751702036ededa10be46d2d904130b1e5d2`

They were verified by the matching 25.12.5 SDK and installed on the matching
router.

The first LuCI APK was rejected because the custom builder encoded `arch: all`.
The builder now uses the target SDK architecture for LuCI, and the corrected
`x86_64` package installed successfully.
