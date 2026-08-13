# FakeHTTP Handoff 01: Current State

Handoff ID: FH-20260813-01
Repository: /Users/sirtungshenghsiao/Documents/fakehttp
Branch: codex/input-validation
Scope: FakeHTTP and NFQUEUE 512 only.

The OpenWrt router is 192.168.9.1:33501, reached through Debian
192.168.9.190. FakeSIP owns NFQUEUE 513. Do not read, stop, restart, or modify
FakeSIP or queue 513 during FakeHTTP work.

OpenWrt currently runs FakeHTTP 99.2-r10 in silent mode. The process observed
on 2026-08-13 was PID 24841 with the original three PPPoE interfaces and the
original payload configuration. The process starttime was 56449108 and did
not change during the audit.

The r10 package was installed at 2026-08-11 08:30:15 UTC. At the audit time,
the process had run approximately 44 hours and 45 minutes, which is not a
full 48-hour proof.

Do not claim a speed improvement from this handoff. No speed test was run in
this audit.

Compatibility work completed on 2026-08-13:

- Source package revision is FakeHTTP `99.2-r11`; LuCI package revision is
  `99.2-r6`. These revisions were not installed on the production router.
- Claimed release range is OpenWrt 21.02 through 25.12. OpenWrt 19.07 and
  older remain unclaimed.
- The package recipe now conditionally selects firewall4/nftables or
  firewall3/iptables extensions. The LuCI package declares `rpcd-mod-file`.
- No live FakeHTTP queue 512 or FakeSIP queue 513 operation was performed for
  this compatibility work.
- The 25.12 release artifacts are `fakehttp-99.2-r11.apk` and
  `luci-app-fakehttp-99.2-r6.apk`; they were built and verified but not
  installed on the production router.
