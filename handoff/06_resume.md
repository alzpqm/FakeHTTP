# FakeHTTP Handoff 06: Safe Continuation

Handoff ID: FH-20260814-06

Before continuing, read this file and CODEX_HANDOFF.md. Preserve the current
FakeHTTP UCI configuration and silent mode unless a coordinated test explicitly
requires a temporary change.

Safe next steps:

1. Keep r11 running and collect a scheduled multi-hour or 24-hour trend of
   PID, starttime, RSS, FD count, thread count, exact queue 512 line, and the
   three WAN error/drop counters.
2. If a download test is requested, record the exact endpoint, route, source
   IP, time, and full-file result. Do not infer causation from one speed value.
3. If a new nfq_handle_packet error appears, preserve its result, length, and
   errno fields before changing anything.
4. Keep the r11 APK and `/root/fakehttp-upgrade-backup-20260814-r11` until
   rollback is unnecessary.
5. Update all six handoff files and CODEX_HANDOFF.md before context
   compaction and when the research cycle ends.

Never convert an unexecuted command, an unavailable tool, or a model opinion
into a test result. Mark each item as verified, failed, unavailable, or
unverified.

After this compatibility cycle:

1. Use `tools/build-openwrt-ipk.sh` with an SDK matching the exact target for
   OpenWrt 24.10, 23.05, 22.03, or 21.02.
2. On OpenWrt 21.02, install the iptables NFQUEUE/connbytes extensions and set
   `fakehttp.advanced.use_iptables='1'` before starting the service.
3. Treat the captured 22.03 IPK hashes as local artifacts, not as universal
   packages for other architectures or release SDKs.
4. If a clean 22.03 or 21.02 SDK is available, run the full package target and
   record its exact output before claiming a complete old-SDK build.
5. Keep the production router on its current r11 silent state. Roll back only
   from `/root/fakehttp-upgrade-backup-20260814-r11` if a router issue appears.

The current release assets are `fakehttp-99.2-r11.apk` and
`luci-app-fakehttp-99.2-r11.apk`; verify their SHA-256 values from
`CODEX_HANDOFF.md` before installing. The GitHub release is a distribution
step; the matching router install and 30-minute non-silent validation were
completed on 2026-08-14.

Update all six handoff files and CODEX_HANDOFF.md again before context
compaction or the next research-cycle completion.
