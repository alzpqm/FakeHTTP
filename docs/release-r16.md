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
本版未在正式路由器上執行升級，亦未進行長時間或頻寬測試。

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

## English

Fix subprocess redirection when standard descriptors are closed, and bound idle
NFQUEUE shutdown with timed polling and nonblocking receive. Adds 32 descriptor
cases and deterministic queue shutdown/readiness/dispatch regressions to CI.
Validated with sanitizers, GCC analyzer, isolated Linux start/stop, musl target
regressions and package/LuCI checks. No production-router upgrade, long-duration
or throughput test was performed for this release.
