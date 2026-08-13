# FakeHTTP

Obfuscate all your TCP connections into HTTP protocol, using Netfilter Queue (NFQUEUE).

[[ 中文文档 ]](https://github.com/MikeWang000000/FakeHTTP/wiki)


## Quick Start

```
fakehttp -h www.example.com -i eth0
```


## OpenWrt

The current x86_64 packages and LuCI control page are available in the
[OpenWrt 99.2-r11 release](https://github.com/alzpqm/FakeHTTP/releases/tag/openwrt-99.2-r11).
See [openwrt/README.md](openwrt/README.md) for installation and setup.


## Usage

```
Usage: fakehttp [options]

Interface Options:
  -a                 work on all network interfaces (ignores -i)
  -i <interface>     work on specified network interface

Payload Options:
  -b <file>          use TCP payload from binary file
  -e <hostname>      hostname for HTTPS obfuscation
  -h <hostname>      hostname for HTTP obfuscation

General Options:
  -0                 process inbound connections
  -1                 process outbound connections
  -4                 process IPv4 connections
  -6                 process IPv6 connections
  -d                 run as a daemon
  -k                 kill the running process
  -s                 enable silent mode
  -w <file>          write log to <file> instead of stderr

Advanced Options:
  -f                 skip firewall rules
  -g                 disable hop count estimation
  -m <mark>          fwmark for bypassing the queue
  -n <number>        netfilter queue number (0-65535)
  -p <port>          bypass destination TCP port (repeatable)
  -r <repeat>        duplicate generated packets for <repeat> times
  -t <ttl>           TTL for generated packets
  -x <mask>          set the mask for fwmark
  -y <pct>           raise TTL dynamically to <pct>% of estimated hops
  -z                 use iptables commands instead of nft

```

Numeric option values are decimal unless prefixed with `0x` or `0X`. Legacy
leading-zero octal notation is not supported; remove the leading zero for
decimal or use an explicit hexadecimal prefix.

## License

GNU General Public License v3.0
