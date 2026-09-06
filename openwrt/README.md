# FakeHTTP 的 OpenWrt 套件

[English](README.en.md)

本目錄包含 FakeHTTP 的 OpenWrt 套件定義、procd 服務、UCI 預設值、LuCI
網頁控制介面與快速設定工具。

## 發佈範圍

GitHub Release 只提供經過實機安裝測試的 OpenWrt 25.12+ x86_64 APK。
OpenWrt 24.10 與更早版本不提供預先編譯的 IPK，使用者必須使用符合路由器
版本、目標平台與架構的 SDK 自行編譯。

| OpenWrt 版本 | 套件格式 | 預設防火牆 | 取得方式 |
| --- | --- | --- | --- |
| 25.12 與更新版本 | APK / `apk` | firewall4 / nftables | Release 提供已測試的 x86_64 APK |
| 24.10、23.05、22.03 | IPK / `opkg` | firewall4 / nftables | 使用對應 SDK 自行編譯 |
| 21.02 | IPK / `opkg` | firewall3 / iptables | 使用對應 SDK 自行編譯 |

目前 FakeHTTP 與 LuCI 套件版本均為 `99.2-r15`。19.07 與更早版本已停止
維護，本專案不宣告支援。

## 安裝 OpenWrt 25.12+

從 [99.2-r15 Release](https://github.com/alzpqm/FakeHTTP/releases/tag/openwrt-99.2-r15)
下載兩個 x86_64 APK，確認雜湊後傳到路由器：

```sh
scp fakehttp-99.2-r15.apk root@192.168.1.1:/tmp/
scp luci-app-fakehttp-99.2-r15.apk root@192.168.1.1:/tmp/
ssh root@192.168.1.1
apk add --allow-untrusted /tmp/fakehttp-99.2-r15.apk
apk add --allow-untrusted /tmp/luci-app-fakehttp-99.2-r15.apk
```

APK 必須與路由器版本和架構相符。Release 中的套件只驗證於 OpenWrt
25.12.5 x86_64，不可安裝到其他架構。

## 自行編譯 OpenWrt 25.12+ APK

使用與路由器版本及架構完全相符的 SDK：

```sh
./tools/build-openwrt-apk.sh /path/to/openwrt-sdk /tmp/fakehttp-apk
```

建置工具會使用 SDK 的編譯器、`apk` 與 `po2lmo`，並把正體中文翻譯直接
放入 LuCI APK。

## 自行編譯 OpenWrt 24.10 與更早版本 IPK

舊版套件不由 GitHub Release 預先編譯。請準備符合路由器版本與目標平台的
SDK，再執行：

```sh
./tools/build-openwrt-ipk.sh /path/to/openwrt-sdk /tmp/fakehttp-ipk
```

或在 SDK 內手動執行：

```sh
ln -s /path/to/FakeHTTP/openwrt/fakehttp package/fakehttp
ln -s /path/to/FakeHTTP/openwrt/luci-app-fakehttp package/luci-app-fakehttp
make defconfig
make package/fakehttp/compile V=s FAKEHTTP_SOURCE_DIR=/path/to/FakeHTTP
make package/luci-app-fakehttp/compile V=s
```

OpenWrt 22.03、23.05、24.10 使用預設的 nftables 後端。OpenWrt 21.02
需要安裝 iptables NFQUEUE 與 connbytes 擴充套件，並在啟動前切換後端：

```sh
uci set fakehttp.advanced.use_iptables='1'
uci commit fakehttp
/etc/init.d/fakehttp restart
```

使用 `opkg install` 安裝自行編譯的兩個 IPK。請勿混用不同 OpenWrt 版本或
架構的套件。

## 設定與操作

安裝完成後，前往 LuCI 的「服務 -> FakeHTTP」。正體中文介面已包含在
`luci-app-fakehttp` 中，並支援 Bootstrap 主題的淺色與夜間模式。

命令列快速設定：

```sh
fakehttp-setup www.example.com wan
fakehttp-setup --https www.example.com wan
```

進階 UCI 範例：

```sh
uci set fakehttp.globals='globals'
uci set fakehttp.globals.enabled='1'
uci add_list fakehttp.globals.interface='pppoe-wan'
uci add fakehttp payload
uci set fakehttp.@payload[-1].enabled='1'
uci set fakehttp.@payload[-1].type='http'
uci set fakehttp.@payload[-1].payload='www.example.com'
uci commit fakehttp
/etc/init.d/fakehttp enable
/etc/init.d/fakehttp restart
```

若特定服務使用自訂的 HTTP 類型連接埠，可讓 FakeHTTP 在雙向流量略過該
TCP 連接埠：

```sh
uci add_list fakehttp.advanced.bypass_port='65499'
uci commit fakehttp
/etc/init.d/fakehttp restart
```

檢查服務與日誌：

```sh
/etc/init.d/fakehttp status
logread -e fakehttp
```
