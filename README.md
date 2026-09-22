# FakeHTTP

[English](README.en.md)

FakeHTTP 透過 Linux Netfilter Queue（NFQUEUE），在 TCP 連線建立時送出
HTTP/HTTPS 形式的偽裝封包。專案包含命令列程式，以及適合一般 OpenWrt
使用者的 UCI 服務與 LuCI 網頁控制介面。

## 快速開始

```sh
fakehttp -h www.example.com -i eth0
```

## OpenWrt

建議使用 [OpenWrt 99.2-r16 正式版](https://github.com/alzpqm/FakeHTTP/releases/tag/openwrt-99.2-r16)。
Release 頁面提供 OpenWrt 25.12+ x86_64 APK；本版驗證範圍請見發佈說明：

```sh
apk add --allow-untrusted ./fakehttp-99.2-r16.apk
apk add --allow-untrusted ./luci-app-fakehttp-99.2-r16.apk
```

安裝後前往「服務 -> FakeHTTP」。LuCI 介面支援正體中文、淺色與夜間模式。

OpenWrt 24.10 與更早版本不提供預先編譯套件。這些版本仍可使用符合路由器
版本與架構的 SDK 自行編譯 IPK。完整步驟請參閱
[OpenWrt 安裝與編譯說明](openwrt/README.md)。

## 使用方式

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

數值參數預設使用十進位，只有 `0x` 或 `0X` 前綴表示十六進位。舊式前導零
八進位寫法不受支援；十進位請移除前導零，或改用明確的十六進位前綴。

## 授權

GNU General Public License v3.0
