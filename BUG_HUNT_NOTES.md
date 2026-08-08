# FakeHTTP Bug Hunt Notes

This file records the bug-hunt work done in the `codex/bug-hunt` branch so the
findings survive outside the chat context.

## Fixed in this branch

- `src/srcinfo.c`: fixed circular-buffer reverse lookup after wraparound. The
  previous `size_t` subtraction could underflow and map to the wrong slot when
  the cache wrapped.
- `src/nfqueue.c`, `src/srcinfo.c`, `include/srcinfo.h`, `src/rawsend.c`:
  preserve and restore the hardware address length instead of assuming the full
  8-byte storage is valid. This makes inbound raw sends use the correct
  `sockaddr_ll.sll_halen`.
- `src/rawsend.c`: fixed a missing argument for the `unknown ethertype` log
  message.
- `include/logging.h`: added compiler printf-format attributes to catch future
  logger format mistakes at build time.
- `src/mainfun.c`: fixed `realloc` error handling so the original allocation is
  not lost on failure, and corrected the error message from `calloc()` to
  `realloc()`.
- `src/nfrules.c`: clean up already-created firewall rules when rule setup fails
  partway through.
- `src/ipv4pkt.c`, `src/ipv6pkt.c`: fixed packet-builder buffer checks that
  previously required one byte more than the actual packet length.
- `src/ipv6pkt.c`, `include/ipv6pkt.h`, `src/rawsend.c`: parse IPv6
  hop-by-hop, routing, destination-options, AH, and atomic-fragment extension
  headers before TCP, reject real fragmented IPv6 TCP packets, and recompute
  IPv6 TCP checksums with the actual TCP segment length after removing TCP Fast
  Open cookies.
- `src/logging.c`: make logger helpers fall back to `stderr` when called before
  explicit logger setup. This prevents test/error paths from dereferencing a
  null log file pointer.
- `include/globvar.h`: store the signal exit flag as `volatile sig_atomic_t` so
  signal handlers update it using the C type intended for asynchronous signal
  access.
- `src/nfqueue.c`: if `SO_RCVBUFFORCE` is rejected while increasing the NFQUEUE
  receive buffer, fall back to `SO_RCVBUF` instead of failing startup outright.
- `src/mainfun.c`: strictly validate all numeric command-line values, including
  overflow, trailing characters, leading whitespace, negative wraparound, and
  per-option ranges. NFQUEUE numbers are now limited to the API's 16-bit range.
- `src/signals.c`: strictly validate numeric `/proc` directory names before
  treating them as PIDs, and use a small fixed buffer for `/proc/<pid>/exe`.
- `src/nfqueue.c`: log packet ID, verdict, and the system error when
  `nfq_set_verdict()` fails.
- `src/payload.c`: use `size_t` for the `snprintf()` destination size and avoid
  calculating the hostname length twice. Validate the TLS SNI hostname length
  before subtracting unsigned sizes, preventing an underflow and out-of-bounds
  padding write for hostnames longer than 262 bytes.

## Verified

Validation was run on Debian `192.168.9.190` using the synced worktree in
`/tmp/fakehttp-bughunt`:

- `make CFLAGS='-Wformat=2 -Werror=format'`
- `make DEBUG=1`
- `make CFLAGS='-fanalyzer'`
- Temporary parser test: constructed an IPv6 packet with a Destination Options
  header before TCP, verified the parsed TCP pointer/payload length/TTL, updated
  and independently validated the TCP checksum, and verified that a real
  fragmented IPv6 TCP packet is rejected.
- Temporary logger test: called `E()` and `E_RAW()` before logger setup and
  verified output is written to `stderr` without crashing.
- Runtime nft + curl smoke test:
  - started FakeHTTP on interface `ens33`
  - `curl -4 http://example.com/` completed successfully
  - log contained `FAKE(*)`
  - no `fakehttp` process or `table ip fakehttp` remained after shutdown
- OpenWrt 25.12.5 x86_64 SDK cross build:
  - output binary: x86_64 musl executable
  - linked against `libnetfilter_queue.so.1`, `libnfnetlink.so.0`,
    `libmnl.so.0`, `libgcc_s.so.1`, and `libc.so`
  - r2 sha256:
    `2af51f2fd755146183a9001afb0ae99f0d6b594df9af83f28cd7e42ab7d805ae`
  - r3 sha256:
    `a353e9f909b4a48c0dec819b94539c8755fbb3357a14ef4203685b89d9aa49ba`

## Reviewed without a code change

- The cast from `uint8_t *` to `char *` in `payload.c` is required by the
  `snprintf()` interface. Removing it would introduce an incompatible-pointer
  warning without improving safety.
- `__attribute__((aligned))` without an explicit value in `rawsend.c` is valid
  GCC syntax and requests the maximum useful alignment for the target. The
  packet buffer does not require a specific protocol alignment value.
- The remaining `PATH_MAX` buffers in `signals.c` hold executable paths returned
  by `readlink()`. Keeping them avoids truncating valid paths; only the short
  `/proc/<pid>/exe` path buffer was reduced.
- C99 mid-function declarations are valid for this project. The hostname length
  declaration was moved to the function's declaration block for readability.

## False positive checked

- A proposed `payload.c` leak on payload generation failure was checked and not
  applied. The newly allocated node is linked into the circular list before
  payload generation, so `fh_payload_cleanup()` can free it on failure.

## OpenWrt runtime audit

Runtime was also inspected on the OpenWrt router reachable from Debian
`192.168.9.190` at `192.168.9.1:33501`.

Observed router state:

- OpenWrt `25.12.5`, x86_64.
- Installed package: `fakehttp-0.9.18-r3`.
- Running command:
  `/usr/sbin/fakehttp -i pppoe-wan2 -i pppoe-wancm -i pppoe-wanct -s -h cgw.mil.cn -e cgw.mil.cn -h download.mail.mil.cn -e download.mail.mil.cn`
- NFQUEUE 512 is owned by fakehttp. Queue drop counters were zero during the
  check.
- `tcpdump -i any` confirmed a real test connection from Debian caused two fake
  outbound HTTP payload packets on `pppoe-wan2` before the real HTTP request:
  the fake payload used `Host: cgw.mil.cn`, then the real request used
  `Host: example.com`.

Operational findings:

- The OpenWrt package is older than this branch and does not include the fixes
  above.
- The running FakeHTTP nft rules cover all three configured PPPoE exits in both
  `fh_prerouting` and `fh_postrouting`:
  - `pppoe-wan2`
  - `pppoe-wancm`
  - `pppoe-wanct`
- Forced-route packet captures were run from Debian with temporary
  `inet fw4 mangle_prerouting` mark rules:
  - mark `0x100`, route table 1: egressed on `pppoe-wancm`; curl completed;
    pcap contained fake payload strings for `cgw.mil.cn` before the real
    `Host: example.com` request.
  - mark `0x200`, route table 2: egressed on `pppoe-wanct`; curl completed;
    pcap contained two fake `GET / HTTP/1.1` payloads with
    `Host: cgw.mil.cn` before the real request.
  - mark `0x300`, route table 3: egressed on `pppoe-wan2`; curl completed;
    pcap contained fake payload strings for `download.mail.mil.cn` before the
    real request.
- Each forced-route test captured 14 packets on the selected PPPoE interface,
  with zero tcpdump kernel drops. The temporary nft rules, tcpdump processes,
  and pcap files were removed after testing.
- OpenClash is running beside FakeHTTP. Debian `192.168.9.190` is currently in
  OpenClash's `lan_ac_black_ips`, so normal traffic from that host bypasses
  OpenClash; this is why a direct Google test from that source timed out.
- OpenClash transparent-proxy behavior was tested by adding a temporary
  secondary Debian address, `192.168.250.190/16`, which is not in the blacklist.
  A curl request sourced from that address to
  `https://www.google.com/generate_204` returned `HTTP/2 204`.
- During that OpenClash test, simultaneous captures on all three PPPoE exits
  observed FakeHTTP fake payload strings (`cgw.mil.cn` and
  `download.mail.mil.cn`) on the WAN side. This confirms that FakeHTTP is active
  while OpenClash traffic is leaving through the configured exits.
- A more targeted temporary nft counter in `ip fakehttp fh_postrouting` using
  `meta skgid 65534` confirmed the Clash process group can enter FakeHTTP's
  postrouting path: one new Clash-group TCP SYN was counted on `pppoe-wancm`
  during a successful transparent-proxy request. This means FakeHTTP can cloak
  new OpenClash outbound TCP connections when they leave via a configured PPPoE
  interface. Existing long-lived Clash proxy connections will not get a new fake
  payload until a new TCP connection is opened.
- The LuCI/init config exposes advanced `repeat`, `ttl`, queue, mark, and
  iptables/nft settings, but it does not expose direction (`-0`/`-1`) or IP
  family (`-4`/`-6`) controls. On this router the service therefore processes
  both inbound and outbound and both IPv4 and IPv6 by default. If the deployment
  only needs outbound obfuscation, adding UCI options for `-1` and optionally
  `-4`/`-6` would reduce unnecessary queue work.
- Silent mode is enabled. This is fine for production, but it suppresses runtime
  `FAKE(*)` visibility. For support/debugging, the OpenWrt wrapper could expose
  a log-file option using `-w <file>` or make temporary non-silent diagnostics
  easier.
- `/etc/config/fakehttp.apk-new` exists beside the active config. It appears to
  be a package-default config left after upgrade. It is not breaking the current
  service, but it is worth cleaning up or merging intentionally.

## OpenWrt deployment

The fixed OpenWrt x86_64 musl binary was deployed to the router after SDK
build validation. The r2 binary then ran for about six hours before the r3
hardening binary was deployed.

- Deployed binary: `/usr/sbin/fakehttp`
- r2 deployed sha256:
  `2af51f2fd755146183a9001afb0ae99f0d6b594df9af83f28cd7e42ab7d805ae`
- r2 previous binary backup:
  `/usr/sbin/fakehttp.bak-codex-20260704-214926`
- Six-hour r2 observation, taken on 2026-07-05 04:13:04 GMT router time:
  - logread showed only normal start/stop entries and no fakehttp errors after
    the r2 start.
  - dmesg had no fakehttp, NFQUEUE, OOM, or segfault messages.
  - fakehttp RSS was about 908 KiB, with 5 file descriptors and 1 thread.
  - `/proc/net/netfilter/nfnetlink_queue` showed NFQUEUE 512 owned by fakehttp,
    about 202k packets processed, and zero kernel/user queue drops.
- r3 deployed sha256:
  `a353e9f909b4a48c0dec819b94539c8755fbb3357a14ef4203685b89d9aa49ba`
- r2 binary backup before r3 deployment:
  `/usr/sbin/fakehttp.bak-codex-r2-20260705-041525`
- Running command after restart:
  `/usr/sbin/fakehttp -i pppoe-wan2 -i pppoe-wancm -i pppoe-wanct -s -h cgw.mil.cn -e cgw.mil.cn -h download.mail.mil.cn -e download.mail.mil.cn`
- NFQUEUE 512 is owned by the new fakehttp process, and both `ip fakehttp` and
  `ip6 fakehttp` nft tables are present.
- Post-deployment smoke test from Debian:
  - `curl -4 --interface 192.168.9.190 -H 'Host: example.com' http://172.66.147.243/`
    returned HTTP 200.
  - `tcpdump -i pppoe-wan2` captured 14 packets with zero kernel drops.
  - Captured payload order included two fake `GET / HTTP/1.1` requests with
    `Host: cgw.mil.cn`, followed by the real `Host: example.com` request.
  - Temporary debug and pcap files were removed after verification.
- Post-deployment three-exit forced-route test from Debian:
  - mark `0x100`, table 1, `pppoe-wancm`: route source `10.47.4.170`, curl
    returned HTTP 200, tcpdump captured 14 packets with zero kernel drops, and
    payload strings showed two fake `Host: download.mail.mil.cn` requests before
    the real `Host: example.com` request.
  - mark `0x200`, table 2, `pppoe-wanct`: route source `125.86.168.66`, curl
    returned HTTP 200, tcpdump captured 14 packets with zero kernel drops, and
    payload strings showed two fake `Host: download.mail.mil.cn` requests before
    the real `Host: example.com` request.
  - mark `0x300`, table 3, `pppoe-wan2`: route source `10.47.54.204`, curl
    returned HTTP 200, tcpdump captured 14 packets with zero kernel drops, and
    payload strings showed two fake `cgw.mil.cn` payloads before the real
    `Host: example.com` request.
  - The temporary `inet codex_fh_test` nft table and test pcap/log files were
    removed after verification.
- Post-r3 deployment three-exit forced-route test repeated successfully:
  - `pppoe-wancm`, `pppoe-wanct`, and `pppoe-wan2` each returned HTTP 200 from
    the Debian curl test.
  - Each selected PPPoE interface capture saw fake payload strings before the
    real `Host: example.com` request and reported zero tcpdump kernel drops.
  - Temporary nft and pcap/log files were removed after verification.

## 2026-07-21 download slowdown audit

Only FakeHTTP was inspected and changed during this audit. FakeSIP was left to
the separate task and was not stopped, reconfigured, or inspected.

- The installed `fakehttp-99.2-r5` process had been running since the router's
  previous boot, used about 896 KiB RSS, one thread, and five file descriptors.
- NFQUEUE 512 had processed about 5.9 million packets with zero queued packets,
  zero kernel drops, and zero userspace drops. The three configured PPPoE
  interfaces also reported zero RX/TX drops.
- Silent mode explained the empty normal runtime log. A controlled 60-second
  non-silent run recorded 3,966 lines and 810 `FAKE(*)` events with no error,
  warning, or verdict-failure lines. This indicated high TCP connection churn
  rather than CPU, memory, or NFQUEUE saturation.
- A short 2 MB download A/B test from Debian showed a repeatable latency and
  throughput penalty with the compiled default `repeat=2`. Initial enabled
  runs took about 6.2-6.6 seconds, while FakeHTTP-disabled runs took about
  0.4-2.0 seconds. A second enabled set remained slower but showed normal WAN
  variance at about 2.0-4.4 seconds.
- A candidate using `repeat=1` completed five runs in about 0.4-2.1 seconds.
  The same setting applied through the production UCI/procd service completed
  five more runs in about 0.5-2.3 seconds.
- Production was therefore left on `fakehttp.advanced.repeat='1'`. The service
  remained in silent mode with the same three interfaces and six HTTP/HTTPS
  payload entries. Final NFQUEUE and interface drop counters remained zero.
- All temporary diagnostic logs, PID files, and configuration snapshots were
  removed after the service was restored.

## 2026-07-24 connection-count audit

Only FakeHTTP and NFQUEUE 512 were inspected or changed. FakeSIP was left to its
separate task and was not inspected, stopped, or reconfigured.

- FakeHTTP still had one process, about 896 KiB RSS, one thread, and five file
  descriptors. Its two socket descriptors were queue/raw sockets, not TCP
  connections.
- The conntrack table initially held about 1,907 of 262,144 entries (0.73%).
  A 60-second sample ranged from 1,127 to 1,689 entries and averaged 1,408,
  proving entries were being reclaimed rather than accumulating.
- During that sample NFQUEUE 512 processed about 18 packets per second, consumed
  about 0.02 CPU seconds, and retained zero queue, kernel-drop, and user-drop
  counters. There were no conntrack-full, OOM, segfault, or NFQUEUE warnings.
- The largest concentration was TCP traffic to `109.244.79.186:23507`.
  `netstat` attributed the live sockets to the `clash` process, not FakeHTTP.
  FakeHTTP can nevertheless influence their lifetime by injecting its fake
  payload into each new proxy connection.
- Thirty-second A/B/A churn samples counted unique original source ports for
  that endpoint:
  - FakeHTTP active: 242 ports, 186 average entries.
  - FakeHTTP inactive: 179 ports, 154 average entries.
  - FakeHTTP active again: 288 ports, 198 average entries.
  This indicates FakeHTTP amplified the proxy connection churn by roughly
  20-60% during the sample, but did not originate the connections.
- A temporary endpoint bypass while FakeHTTP remained active reduced the sample
  to 204 unique ports and 181 average entries. It was not retained because
  bypassing the proxy endpoint would also remove the intended obfuscation from
  that traffic.
- Production was restored to `fakehttp-99.2-r5`, silent mode, `repeat=1`, the
  same three PPPoE exits, and the same six payload entries. Temporary nft rules
  and sample files were removed.

## 2026-07-26 coordinated USTC throughput audit

Only FakeHTTP and NFQUEUE 512 were inspected or changed. The separate FakeSIP
task supplied a stable window; queue 513 and all FakeSIP state were not read,
stopped, or reconfigured. Tests ran from the wired Debian VM (`192.168.9.190`),
fixed to `pppoe-wancm` and public source `183.228.195.226`, against USTC's
LibreSpeed backend. Each run used the site's six download and three upload
streams for four seconds. Mac Wi-Fi measurements were excluded.

- Results collected while either task was restarting its queue were discarded.
- A controlled non-silent run measured 54.72 Mbps down and 180.88 Mbps up. Its
  719 log lines contained 133 normal `FAKE(*)` events and no errors, warnings,
  verdict failures, or NFQUEUE drops.
- Valid FakeHTTP-on runs measured 56.48, 76.45, and 54.09 Mbps down (56.48
  median), with 179.93, 180.46, and 180.33 Mbps up (180.33 median).
- Valid FakeHTTP-off runs measured 54.56, 62.32, and 52.77 Mbps down (54.56
  median), with 183.47, 180.46, and 180.06 Mbps up (180.46 median). This clean
  A/B pair did not reproduce a FakeHTTP download penalty.
- A lower TTL candidate (`ttl=1`) reduced median download throughput to 49.61
  Mbps and was rejected.
- Reducing the optional early-ACK nft rule from conntrack packets 2-4 to only
  packet 2 produced a 53.86 Mbps download median. Removing that optional rule
  entirely produced a 53.58 Mbps median. Neither candidate improved throughput,
  so both were rejected rather than committed.
- Production was restored to the released `fakehttp-99.2-r5` behavior with
  silent mode, `repeat=1`, default TTL 3, three PPPoE exits, and the original
  early-ACK rule. NFQUEUE 512 had zero backlog, kernel drops, and userspace
  drops. The fixed-WAN test rule and all Debian test files were removed.

The audit was then extended to compare all three carriers and payload choices.
The first fixed-file pass was discarded because `curl` had not forced IPv4;
the Debian VM has global IPv6 addresses, so those requests could bypass the
IPv4 source-mark rule. All replacement commands used `curl -4` and recorded
the resolved remote IPv4 address.

- USTC's LibreSpeed service and mirror began rejecting rapid automated
  requests after the initial tests. This matches the site's documented
  temporary blocking policy, so throttled and zero-byte USTC samples were not
  used for the final carrier or service comparison.
- The replacement endpoint was TUNA's Debian 13.6.0 netinst ISO. Every valid
  run fetched the same 64 MiB range from `101.6.15.130` over HTTP/1.1. With
  FakeHTTP enabled, six-run medians were 444.38 Mbps on `pppoe-wancm`, 409.34
  Mbps on `pppoe-wanct`, and 311.75 Mbps on `pppoe-wan2`. This also showed that
  `wancm` was not generally download-limited: its earlier poor USTC result was
  destination/path specific.
- On the fastest TUNA path (`pppoe-wancm`), FakeHTTP ON/OFF/ON medians were
  444.38, 457.94, and 446.65 Mbps. The combined 12-run ON median was 445.70
  Mbps, only 2.7% below the OFF median and well within sample variance.
- A second ON/OFF/ON comparison on `pppoe-wanct` measured 409.34, 377.40, and
  437.21 Mbps. Its combined ON median was 433.03 Mbps. The opposite direction
  of the small differences on the two exits further indicates normal path
  variance rather than a FakeHTTP download penalty.
- A coordinated both-programs-off baseline on `pppoe-wancm` measured 316.57,
  483.35, 485.08, 454.62, 445.58, and 487.82 Mbps (468.99 median). Both
  programs enabled measured roughly 445-449 Mbps in the adjacent controlled
  runs. The approximately 4-5% difference is small relative to the observed
  203-489 Mbps path variance and rules out either program as the source of a
  large download cap, although it does not prove zero combined overhead.
- The existing payload hosts still resolve consistently through AliDNS and
  DNSPod. `cgw.mil.cn` and `download.mail.mil.cn` returned valid HTTP and HTTPS
  responses; `yun.cgw.mil.cn` had HTTPS active while port 80 was closed. All
  three presented matching, currently valid TLS certificates.
- A diagnostic payload set using `test.ustc.edu.cn` and
  `mirrors.ustc.edu.cn` for HTTP and HTTPS produced a 410.64 Mbps median on
  `pppoe-wanct`, below the 433.03 Mbps median of the existing payload set. It
  was rejected and the byte-identical production UCI backup was restored.
- `www.gov.cn` and `www.12306.cn` were verified as active official-domain
  fallbacks with matching DNS, HTTP, HTTPS, and TLS identity, but were not
  deployed. No candidate demonstrated a throughput benefit over the existing
  payloads.
- Production ended on the original six payload entries, silent mode,
  `repeat=1`, default TTL 3, and all three PPPoE exits. NFQUEUE 512 again had
  zero backlog and zero kernel/userspace drops. All temporary source marks,
  UCI backups, and Debian test files were removed.

## 2026-07-26 China Speed Test endpoint audit

The locally cached server list and runtime log from China Speed Test 4.4.9 were
used to verify the protocol before any payload test. Client identifiers and
one-time validation values in the log were not copied into the project notes.

- The control services `dlcv2.cnspeedtest.cn:8443`,
  `dlcv64.cnspeedtest.cn:8443`, and `down.cnspeedtest.cn:8043` all returned
  HTTP 200 with valid TLS. Their standard HTTPS port 443 was not active.
  `down.cnspeedtest.cn` resolved to the Shenzhen Unicom speed-server address
  `112.90.72.190`, but the app used direct IP addresses for speed traffic.
- The actual speed protocol is HTTP on port 65499. It first calls
  `/speed/dovalid`, then opens eight connections to `/speed/File(1G).dl`, and
  uses `/speed/doAnalsLoad.do` for upload. A valid one-time key is required for
  the download path.
- A low-volume matrix covered all supplied Shanghai, Beijing, Tianjin,
  Chongqing, and Shenzhen Telecom, Unicom, and Mobile nodes, plus the two
  Education Network nodes. All 17 port-65499 nodes returned the same protocol
  behavior: an invalid validation request produced HTTP 200 with a three-byte
  rejection, and an invalid file request produced HTTP 403 with a 134-byte
  body. The two additional Shenzhen Mobile port-9443 entries timed out.
- Chongqing was the lowest-latency group from the Debian VM: Telecom, Unicom,
  and Mobile completed the probes in about 15-22 ms, versus roughly 66-148 ms
  for the other tested cities. One node from each Chongqing carrier was
  therefore selected instead of adding the complete server list.
- The temporary candidate payload set used HTTP Host values
  `222.181.15.28:65499`, `113.204.250.254:65499`, and
  `218.201.1.249:65499`. On fixed `pppoe-wancm`, the production/candidate/
  production six-run medians were 447.14, 457.21, and 441.26 Mbps. Combining
  the two production blocks gave 447.14 Mbps, so the candidate's 2.25% lead
  was within path variance and did not establish a throughput benefit.
- The candidate also had an important semantic limitation: FakeHTTP's normal
  Host template emits `GET /` on the real connection's destination port,
  whereas the app uses port 65499, a `/speed/...` path, and a valid one-time
  key. The three-node candidate was rejected rather than promoted.
- The byte-identical production UCI backup was restored. The temporary source
  mark and backup were deleted, and FakeHTTP ended on the released r5 binary,
  the original six HTTP/HTTPS payloads, silent mode, `repeat=1`, TTL 3, and all
  three PPPoE exits. NFQUEUE 512 again had zero backlog, kernel drops, and
  userspace drops.

## 2026-07-27 bidirectional payload and carrier audit

The wired Debian VM was fixed to one IPv4 carrier at a time. A fixed mainland
Ookla server (`24447`, Shanghai Unicom) measured download and upload together;
the TUNA Debian 13.6.0 ISO provided a separate 64 MiB high-bandwidth download
check. FakeSIP supplied a stable window and queue 513 was not inspected or
changed.

- The three China Speed Test control hosts produced about 118 Mbps download and
  192 Mbps upload on `pppoe-wancm`, below the 136/192 Mbps FakeHTTP-off
  reference. They were rejected. Their app-specific ports and request protocol
  remain a semantic mismatch for FakeHTTP's generic templates.
- `cloud.189.cn`, `yun.139.com`, and `pan.wo.cn` were tested together and then
  separately as HTTP and HTTPS payloads. The individual two-run medians were
  about 136, 133, and 134 Mbps download respectively, while upload remained
  about 192 Mbps. None established a bidirectional improvement over the off
  reference.
- An adjacent production/cloud.189.cn/production sequence was inconsistent:
  its download medians were about 97, 135, and 140 Mbps, with upload fixed near
  192 Mbps throughout. The result suggests substantial path variance rather
  than a repeatable whitelist effect.
- TUNA cross-checks on `pppoe-wancm` measured production/cloud.189.cn/off
  medians of about 81/61/74 Mbps. The candidate was worse than both production
  and off. The current low TUNA rate therefore cannot be repaired by that
  payload replacement and is primarily destination/path dependent.
- Carrier comparison found `pppoe-wanct` strongly asymmetric: the Shanghai
  server measured about 553 Mbps down but only 46 Mbps up. `pppoe-wan2` was the
  best balanced path in this window, repeating about 164 Mbps down and 192 Mbps
  up, while its TUNA samples were about 100-121 Mbps. `cloud.189.cn` on wan2
  left the paired test near 162-164/192 Mbps and reduced TUNA to 90-97 Mbps, so
  it was also rejected there.
- No candidate was promoted. The byte-identical production config (SHA-256
  `ce2442350bb96c5896025fb7081a4a85465624beae2f1b1cabbee27dd55a7c6c`)
  was restored with silent mode and the original six payload entries. The
  temporary nft source mark and Debian hosts override were removed. Queue 512
  ended with zero backlog and zero kernel/userspace drops, and the filtered
  runtime log had no anomalies.

## 2026-07-29 SpeedTest.cn host lineage and three-carrier audit

The live OpenWrt config and every retained config backup, including the oldest
2026-07-05 copy, showed that the project's historical HTTP and HTTPS default was
`node-36-250-1-90.speedtest.cn`. The upstream README's `www.example.com` value
is only a placeholder and must not be treated as this deployment's original
payload. The historical host remained disabled alongside `cgw.net.cn`,
`www.12339.gov.cn`, and `dlcv2.cnspeedtest.cn`; production still used the six
`cgw.mil.cn`, `download.mail.mil.cn`, and `yun.cgw.mil.cn` HTTP/HTTPS entries.

Tests ran from the wired Debian VM with one IPv4 exit selected at a time. A
fixed 64 MiB TUNA range tested download throughput. The SpeedTest.cn website's
current encrypted node API was decoded using the public client-side routine,
which showed that current nodes use direct `IP:51090` URLs for `/hello`,
`/download`, and `/upload`. An active Xi'an Mobile node also resolved through
the current synthetic hostname `node-111-20-163-124.speedtest.cn` and was
reachable through all three exits.

- The historical `node-36-250-1-90.speedtest.cn` name still resolved to
  `36.250.1.90`, but both TCP 80 and 443 timed out. As the sole HTTP/HTTPS
  payload, its two-run TUNA medians were about 395, 394, and 373 Mbps on
  `pppoe-wanct`, `pppoe-wan2`, and `pppoe-wancm`. Paired Ookla results remained
  near the established carrier behavior. It was rejected as an inactive legacy
  endpoint with no common-carrier benefit.
- The current active `node-111-20-163-124.speedtest.cn` candidate produced
  two-run TUNA medians of about 419, 399, and 399 Mbps on wanct, wan2, and wancm.
  It did not improve all three exits. The node API also marks this node as HTTP
  only, so using the same name as a TLS SNI payload would not match the service's
  real protocol.
- The live central service `nodes-api.speedtest.cn` was then tested as the sole
  HTTP/HTTPS payload. Its two-run TUNA medians versus adjacent production were
  about 440/394 Mbps on wanct, 498/463 Mbps on wan2, and 365/382 Mbps on wancm.
  The mixed direction of the differences does not establish a shared whitelist.
- A fixed 8 MiB POST to the active node's real `/upload` endpoint provided the
  upload cross-check. Candidate/production medians were about 54/50 Mbps on
  wanct, 78/79 Mbps on wan2, and 70/98 Mbps on wancm. The candidate did not
  improve upload consistently and was materially worse on wancm.
- None of the three SpeedTest.cn candidates was promoted. The byte-identical
  production config was restored with SHA-256
  `ce2442350bb96c5896025fb7081a4a85465624beae2f1b1cabbee27dd55a7c6c`.
  FakeHTTP ended in silent mode on the original six payload entries, repeat 1,
  and all three WAN interfaces. Queue 512 had zero backlog and zero kernel or
  userspace drops; the temporary source mark, Debian hosts override, and remote
  test backup were removed.

## 2026-07-29 r6 core, package, and LuCI validation

LocalAI/Ollama and LM Studio supplied adversarial review candidates; each
accepted change was then checked against source control flow, unit tests, Linux
sanitizers, package metadata, and the OpenWrt 25.12.5 router. Model output alone
was not treated as evidence.

- Core fixes reject IPv4 TCP data offsets below five, key cached source
  metadata by interface index as well as address, accept exactly 1,200-byte
  custom payloads while rejecting 1,201 bytes, preserve daemon logging when a
  log path is configured, accept NFQUEUE number 0, and recognize a running
  executable after atomic replacement produces a `/proc/*/exe` ` (deleted)`
  suffix. Focused regression tests cover each boundary.
- The LuCI service panel gained Start, Restart, and Stop controls with explicit
  Running, Stopped, and Unavailable states. Actions use `fs.exec`, verify return
  codes, poll the resulting service state, and no longer save or apply unrelated
  pending LuCI changes before Restart.
- ACL mutations moved from the read role to the write role. Payload validation
  now permits an empty disabled row, requires an absolute binary path, rejects
  non-ASCII/control/slash characters in enabled host payloads, and enforces the
  backend hostname limit. Queue number 0 is accepted consistently by CLI,
  procd, and LuCI.
- The APK initially failed to install because `nftables-nojson` conflicts with
  the router's required `nftables-json`. Depending on the `nftables` virtual
  package fixed the conflict without replacing the active firewall stack.
- `fakehttp-99.2-r6` and `luci-app-fakehttp-99.2-r3` were installed. Their
  router SHA-256 values were respectively
  `998891ee2cb799880bde9e846908d117a9198027f41413058d9be08d30fc02be`,
  `e0d57edde2a4c8fd60f13f94a615eb04c1fc350c2fb80fc86b2154d310a52fc0`
  for the LuCI view, and
  `08b320a56a30113abe85fed802d7a248895eb2175b4a4f7cd955919e7fb2e5d3`
  for its ACL.
- Real LuCI testing confirmed correct button state and process/queue behavior
  for Stop, Start, and Restart. A Unicode hostname that the prior character
  count accepted was rejected by the new validator, and no UCI write occurred.
- During a 900-second non-silent window, PID 13013 stayed unchanged. Queue 512
  backlog, kernel drops, and userspace drops were zero in all 15 one-minute
  samples; its sequence advanced from 9,172 to 19,067 between minute 1 and 15.
  The window produced 13,825 FakeHTTP lines with zero anomaly matches, and the
  related dmesg scan was empty.
- RSS warmed by 36 KiB from 888 to 924 KiB, then remained fixed from minute 9
  through minute 15. VmSize stayed 1,152 KiB, with no swap and no FD or thread
  growth. This rejects a fast leak in the observed load but does not rule out a
  very slow long-horizon leak.
- Fifteen fixed TUNA 64 MiB downloads completed in full from `101.6.15.130`.
  Median throughput was 210.98 Mbps, with an 86.86-328.43 Mbps range. The wide
  path variance is not evidence for either a throughput improvement or a
  regression.
- Linux release and ASan/LeakSan/UBSan runs, GCC `-fanalyzer`, unit/CLI tests,
  OpenWrt package smoke tests, LuCI and shell syntax, JSON parsing, and diff
  whitespace checks all passed.
- Final production state is r6/r3, silent mode, PID 30280, the original six
  enabled payloads and three WAN interfaces, `repeat=1`, and byte-identical
  config SHA-256
  `ce2442350bb96c5896025fb7081a4a85465624beae2f1b1cabbee27dd55a7c6c`.
  Queue 512 ended with zero backlog and zero kernel/userspace drops.

Remaining non-blocking coverage gaps are a multi-hour or 24-hour RSS/FD trend
and deliberate fault injection for NFQUEUE saturation, verdict/send failures,
and interface disappearance/recreation.

## 2026-07-30 r7 numeric parser consistency audit

LocalAI identified that r6 parsed every numeric CLI value with
`strtoull(..., 0)`. Consequently, a leading zero silently selected octal:
`010` became 8 and `018` was rejected. LuCI and ordinary UCI editing present
these fields as decimal values, so the same visible input had different
semantics across interfaces.

- r7 now treats only an explicit `0x` or `0X` prefix, after an optional `+`,
  as hexadecimal. Every other accepted numeric input is decimal. Negative
  values, whitespace, trailing data, range violations, and overflow remain
  rejected.
- This intentionally removes legacy octal interpretation: for example, `010`
  changes from 8 to 10 and `0377` is no longer a valid TTL. Existing
  configurations should use ordinary decimal or an explicit hexadecimal
  prefix.
- CLI regressions cover decimal `010` and `018`, explicit hexadecimal values,
  decimal-only upper-bound distinctions for mark, queue, repeat, TTL, and
  percentage, plus malformed sign and hexadecimal prefixes.
- Linux release tests, ASan/LeakSan/UBSan, GCC `-fanalyzer`, package smoke
  checks, LuCI and shell syntax, diff whitespace checks, and an OpenWrt 25.12.5
  x86_64 SDK build all passed.
- The resulting local test artifact is `fakehttp-99.2-r7.apk`, SHA-256
  `4a7bbec495f152ea563a3aa5517cce56bcf7e45ec7fa29a10b492a4e3c8cf02c`.

This was a local-only audit. The r7 APK was not installed on the production
router, and neither FakeHTTP queue 512 nor FakeSIP queue 513 was accessed or
changed.

## 2026-08-08 r8 China Speed Test protocol isolation

The Global Speed Test application was reported to stay near 150 Mbps download
while Debian's Ookla client (`speedtest -s 24447`) ran at line rate. A packet
capture with the released FakeHTTP behavior proved that the application's
custom TCP protocol was being contaminated: on port 65499, FakeHTTP injected
its generic `GET /` request before the real `/speed/...` request. That protocol
does not accept a generic HTTP request on the same connection.

- r8 adds repeatable TCP port bypasses. The bypass is installed before the
  NFQUEUE rules in both nftables and iptables paths and matches both `tcp dport`
  and `tcp sport`, so the request and reply directions are excluded.
- The OpenWrt service and LuCI page expose `bypass_port`; the router's active
  configuration contains `65499`. A new packet capture showed only the real
  `/speed/dovalid` and `/speed/File(1G).dl` requests, with no FakeHTTP `GET /`.
- The r8 package was built with the official OpenWrt 25.12.5 x86_64 SDK and
  installed on the router. Debian native release tests, CLI validation, core
  validation, packet validation, and signal validation all passed.
- With r8 enabled, Debian Ookla server 24447 measured 2290.13 Mbps download
  and 189.12 Mbps upload. This confirms that r8 does not impose a general
  download ceiling.
- The Global Speed Test result remained destination-dependent: Chongqing
  Telecom was about 102 Mbps down, Shanghai Telecom about 51 Mbps down, and
  Shanghai Unicom about 28 Mbps down. Turning FakeHTTP off for the same
  Shanghai Telecom selection measured about 29 Mbps down; temporarily pinning
  the Shanghai Unicom test to wan2 measured about 29 Mbps down as well. These
  controls do not support FakeHTTP or the original balanced policy as the
  remaining bottleneck.
- All temporary mwan3 marks, captures, and test processes were removed. The
  router ended with the original balanced configuration, FakeHTTP r8 running
  silently, and `/tmp` space restored.

Conclusion: r8 fixes the confirmed FakeHTTP protocol bug, but it cannot turn a
low-capacity or path-limited 65499 speed-test server into the Ookla server's
2290 Mbps path. Port 65499 should remain bypassed rather than being used as a
generic FakeHTTP payload target. The Global Speed Test node, server protocol,
or carrier classification needs separate investigation; promoting its host as
a FakeHTTP payload is not supported by these controls.
