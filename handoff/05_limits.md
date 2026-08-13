# FakeHTTP Handoff 05: Evidence Limits

Handoff ID: FH-20260814-05

The current evidence supports these claims only:

- FakeHTTP r10 stayed alive on one PID for approximately 44 hours and 45
  minutes after installation.
- The sampled memory, file descriptor, thread, queue 512, and WAN counters
  were stable and healthy.
- No current FakeHTTP or dmesg anomaly matched the audit filters.
- The r9 and r10 error-path fixes pass their Linux regression tests.

The evidence does not prove:

- A full 48-hour or mathematical long-term memory-leak absence.
- Any carrier whitelist, destination-specific limit, or Global Speed Test
  behavior.
- That FakeHTTP caused or fixed a 150 Mbps speed ceiling.
- That the historical nfq_handle_packet event caused a throughput problem.

A possible write-return-zero infinite-loop edge was considered during review.
It was not reproduced, is not confirmed, and must not be reported as a bug
without a reproducer.

Compatibility limits:

- OpenWrt 21.02 was covered by source/package compatibility design and the
  legacy dependency names, but no 21.02 router or 21.02 SDK runtime install
  was available in this work.
- The 22.03 artifact was produced with the old cross-toolchain and the SDK
  packager, not by a clean full dependency-graph package target because the
  supplied SDK was configured to build unrelated firmware packages.
- No pre-25 package was installed on the production router. Do not report
  pre-25 runtime service behavior as field-verified.
- The new 25.12 APKs were built and verified but were not installed on the
  production router. Do not report r11 as the live router version.
- OpenWrt 19.07 and older, firewall variants outside the listed defaults, and
  unusual LuCI images without the matching rpcd feed remain unverified.
