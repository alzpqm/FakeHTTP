# FakeHTTP OpenWrt 99.2-r16

修正兩個程序穩定性問題，FakeHTTP 與 LuCI 套件版本同步為 `99.2-r16`。

- 修正標準輸入／輸出被關閉時，子程序管線與輸出重導向失敗。
  管線描述符會避開 stdin/stdout/stderr，重導向完成後保留有效的標準描述符。
- 修正停止訊號剛好落在退出檢查與封包等待之間時，程序仍可能阻塞的問題。
  NFQUEUE 使用最長 100 毫秒的輪詢等待及非阻塞接收，避免遺失停止通知。
- 新增 32 種描述符狀態回歸測試，以及停止競態、就緒狀態消失與封包分派測試。
  原版本會失敗，修正版本全部通過；CI 也執行這些測試。
- 固定 APK 中的目錄與 LuCI 資料檔權限，避免非 POSIX 工作目錄的寬鬆權限
  被複製到套件中。

驗證包含 Linux ASan/LSan/UBSan、GCC 靜態分析、OpenWrt 套件與 LuCI 測試、
隔離網路命名空間啟停，以及 SDK 編譯的 musl 目標回歸測試。
初次發佈時尚未在正式路由器上升級；2026-09-24 已補完下列實機驗收。
未進行長時間或頻寬測試，不代表已完成 48 小時實機耐久測試。

隔離啟停測試可在 Linux 以 `sudo unshare -n python3 scripts/test-runtime-shutdown.py build/fakehttp`
重跑，測試只在新網路命名空間內建立與清除防火牆規則。

附件僅包含 OpenWrt 25.12+ x86_64 的核心 APK、LuCI APK 與 `SHA256SUMS`。
使用 OpenWrt 25.12.5 SDK 建置；安裝前請核對版本、架構與雜湊。
套件未簽章，安裝方式：

```sh
apk add --allow-untrusted ./fakehttp-99.2-r16.apk ./luci-app-fakehttp-99.2-r16.apk
```

OpenWrt 24.10 與更早版本、其他架構請使用對應 SDK 自行編譯。
發佈資料已掃描匿名化；歷史 Git 物件未重寫。

來源說明：`openwrt-99.2-r16` 標籤包含本版核心修正。附件已套用檔案權限正規化，
但對應的三行打包腳本修正於標籤之後補交至 `main`；重建附件時請使用該修正版
`tools/build-openwrt-apk.sh`，見[打包修正提交](https://github.com/alzpqm/FakeHTTP/commit/8b2688ac6a8682b8214357a1ed14ae57d25450f0)。
此差異僅影響打包檔案權限，不涉及核心程式，既有標籤與附件未被替換。

## 2026-09-24 實機驗收

- 使用既有發佈附件，在 OpenWrt 25.12.5 x86_64 同步升級核心與 LuCI 至 r16。
  安裝前驗證套件、備份原設定與檔案，並保留 r15 回復套件。
- 七個已安裝的核心／服務／LuCI 檔案均與發佈套件雜湊一致；原設定保持不變。
- 實機執行 32 種描述符測試及四種佇列迴圈測試，全部通過。
- 停止測試確認程序正常退出、佇列與規則清除，重新啟動後恢復。
  一秒取樣的停止觀測值約 1.01 秒，不是精確的 100 毫秒退出量測。
- 三次 HTTPS 請求皆回應 200；實際 WAN 擷取確認 HTTP 與 TLS 偽裝封包。
  以擷取到的握手連線判讀，未將透明代理前的本機連接埠誤當作 WAN 連接埠。
  佇列積壓及核心／使用者丟包皆為零，其他佇列的持有程序不變。
- 初次驗收腳本使用裝置不支援的小數秒等待，已自動回復 r15 且保留設定。
  修正測試腳本後重新安裝並通過上述驗收；不是產品缺陷修正或新的套件版本。

此補充只更新驗收紀錄，不替換 r16 標籤或附件；私人日誌、封包及登入資訊未公開。

## English

Fix subprocess redirection when standard descriptors are closed, and bound idle
NFQUEUE shutdown with timed polling and nonblocking receive. Adds 32 descriptor
cases and deterministic queue shutdown/readiness/dispatch regressions to CI.
Validated with sanitizers, GCC analyzer, isolated Linux start/stop, musl target
regressions and package/LuCI checks. The initial publication did not include a
production installation; real-device acceptance was completed on 2026-09-24 below.
No long-duration, 48-hour endurance or throughput claim is made.

Source provenance: the r16 tag contains the runtime fixes. The shipped APKs also
use permission normalization whose three-line builder change was committed to
main after tagging. Use that corrected builder when reproducing the APKs; the
release page links the exact follow-up commit. Runtime sources, the published tag
and the uploaded artifacts are unchanged by this packaging-only correction.

## Real-device acceptance, 2026-09-24

The published core/LuCI APKs were installed together on OpenWrt 25.12.5 x86_64.
All seven installed program/service/UI file hashes match the artifacts; the saved
configuration is unchanged. The 32 descriptor and four queue regressions pass on
the device. A normal stop removes the process, queue and rules, and startup
recreates them; one-second sampling observed about 1.01 seconds for shutdown.
Three HTTPS requests returned 200, and captured WAN handshakes contained HTTP/TLS
decoys. Flow attribution uses actual WAN ports because a transparent proxy can
replace client-side ports. Queue backlog/drops remain zero; other queue ownership
is unchanged. These are bounded functional checks, not performance/endurance tests.

The first acceptance harness used unsupported fractional sleep and automatically
rolled back to r15 while preserving configuration. After correcting the harness,
installation and acceptance passed. No product rebuild, tag move or asset
replacement was needed. Private backups, packet captures and access details are
not published.
