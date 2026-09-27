# FakeHTTP OpenWrt 99.3-r1

本專案採用獨立版本號；核心與 LuCI 同步從 `99.2-r16` 升級至 `99.3-r1`。
這是本專案的修正版，不代表上游版本或上游背書。

## 修正與限制

- IPv4／IPv6 iptables 模式將 NFQUEUE 掛鉤加在既有 mangle 規則之後，
  避免佇列放行時跳過既有路由標記。原生 nftables 是預設模式，不受這次
  已重現的規則順序問題影響。之後新增的 iptables 規則仍須位於掛鉤之前；
  前方的終止規則可能讓 FakeHTTP 不處理該流量。
- 距離估算納入初始 TTL／Hop Limit 32，修正把鄰近對端誤認為遠端、讓假
  資料抵達對端並干擾 TCP 連線的情況。這仍是啟發式估算，不保證任意
  初始值、非對稱路由或停用估算（`-g`）時假資料不會抵達目的地。
- 新增 IPv4／IPv6 隔離網路回歸，以及 255 個 TTL 邊界單元測試。
  發布流程依完整版本選取說明檔，避免重新從 r1 編號時引用舊文件。

## 驗證

- 一般與 ASan／LSan／UBSan 回歸、套件檢查及 LuCI 45 條翻譯檢查通過。
- 隔離網路：IPv4／IPv6、原生 nftables／iptables，PREROUTING 與
  POSTROUTING 既有標記均保留；TTL 32／64／128／255 的鄰接對端只收到正常資料。
- 2026-09-27 已在 OpenWrt 25.12.5 x86_64 實機安裝這兩個附件套件：
  設定及七個安裝檔案雜湊核對通過，255 個 TTL 單元測試通過；服務正常
  停止／重啟，原生 nftables 規則及 NFQUEUE 正常清理／重建。
- 三次 IPv4 HTTPS 請求均回應 200，實際封包包含 HTTP 與 TLS 假資料，
  並與實際 WAN 握手關聯；佇列丟包為零。這是短時間驗收，並非長期或
  全路由測試。IPv6 與 iptables 的新情境在本機隔離網路測試，沒有切換
  正式路由器的防火牆模式或注入故障。舊版與設定回退備份已保留。

## 安裝

附件適用 OpenWrt 25.12+ x86_64；安裝前備份設定並確認架構相符。

```sh
apk add --allow-untrusted ./fakehttp-99.3-r1.apk ./luci-app-fakehttp-99.3-r1.apk
```

未包含私人日誌、封包、憑證或實機拓撲。舊版本標籤與附件不變。

## English summary

Independent downstream release, upgrading both packages from 99.2-r16 to 99.3-r1.
Legacy IPv4/IPv6 queue hooks now follow existing mangle rules, and hop estimation
includes an initial TTL of 32. Tests reproduce and cover both fixes. Rule ordering
after subsequent firewall changes and arbitrary peer TTL/asymmetric paths remain
limitations; this is not a universal endpoint-safety guarantee. Both attached APKs
were installed on OpenWrt 25.12.5 x86_64 hardware on 2026-09-27. Configuration/file
hashes, 255 TTL unit cases, normal service lifecycle and three IPv4 HTTPS 200
responses passed. Capture confirmed HTTP/TLS decoys correlated to real WAN
handshakes, with zero NFQUEUE drops. This is bounded acceptance, not endurance or
all-route coverage. IPv6 and legacy iptables scenarios were tested in local network
namespaces, not by changing the production firewall mode. Rollback was retained.
