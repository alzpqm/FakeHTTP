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
