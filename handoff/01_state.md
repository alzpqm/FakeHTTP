# FakeHTTP Handoff 01: Current State

Handoff ID: FH-20260814-01
Repository: /Users/sirtungshenghsiao/Documents/fakehttp
Branch: codex/input-validation
Scope: FakeHTTP and NFQUEUE 512 only.

The OpenWrt router is 192.168.9.1:33501, reached through Debian
192.168.9.190. FakeSIP owns NFQUEUE 513. Do not read, stop, restart, or modify
FakeSIP or queue 513 during FakeHTTP work.

OpenWrt currently runs FakeHTTP 99.2-r11 in silent mode. After deployment and
the non-silent test, the process is PID 20449 with the original three PPPoE
interfaces and payload configuration. The pre-deployment r10 process was PID
4858.

The r10 package was installed at 2026-08-11 08:30:15 UTC. At the audit time,
the process had run approximately 44 hours and 45 minutes, which is not a
full 48-hour proof.

Do not claim a speed improvement from this handoff. No speed test was run in
this audit.

Compatibility work completed on 2026-08-13 and synchronized on 2026-08-14:

- Source package revision is FakeHTTP `99.2-r11`; LuCI package revision is
  `99.2-r11`. Both were installed on the production router on 2026-08-13 UTC.
- Claimed release range is OpenWrt 21.02 through 25.12. OpenWrt 19.07 and
  older remain unclaimed.
- The package recipe now conditionally selects firewall4/nftables or
  firewall3/iptables extensions. The LuCI package declares `rpcd-mod-file`.
- FakeHTTP queue512 was restarted as part of deployment and was monitored;
  FakeSIP queue513 was not read or modified.
- The 25.12 release artifacts are `fakehttp-99.2-r11.apk` and
  `luci-app-fakehttp-99.2-r11.apk`; they were built, verified, and installed.

The 30-minute non-silent test ran from 2026-08-13 17:53:34 to 18:23:36 UTC.
All 30 queue512 samples had backlog/kernel/user drop `0/0/0`; silent mode was
restored afterward. Runtime evidence is archived at
`/tmp/fakehttp-runtime-audit-20260814/fakehttp-r11-nonsilent-20260814.tar.gz`
with SHA-256
`9ac4e3d311ae86fe5320b7a883742c549b605b77adab4c49cfb5a167b79f58b6`.
