# FakeHTTP

[正體中文](README.md)

FakeHTTP makes TCP connection setup look like HTTP/HTTPS traffic using Linux
Netfilter Queue (NFQUEUE). The project includes the command-line program plus
an OpenWrt UCI service and LuCI control page.

## Quick start

```sh
fakehttp -h www.example.com -i eth0
```

## OpenWrt

Use the [OpenWrt 99.2-r15 release](https://github.com/alzpqm/FakeHTTP/releases/tag/openwrt-99.2-r15).
Release assets contain only the router-tested OpenWrt 25.12+ x86_64 APKs:

```sh
apk add --allow-untrusted ./fakehttp-99.2-r15.apk
apk add --allow-untrusted ./luci-app-fakehttp-99.2-r15.apk
```

After installation, open `Services -> FakeHTTP`. The LuCI page supports
Traditional Chinese and both light and dark themes.

Prebuilt packages are not provided for OpenWrt 24.10 and older. Users of those
releases can build IPKs with an SDK matching their router release and target.
See [openwrt/README.en.md](openwrt/README.en.md) for details.

## Usage

```text
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
