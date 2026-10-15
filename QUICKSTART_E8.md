# Alif E8 DevKit (Cortex-M55) VS Code 開發指南

本專案已為您完整配置好 **Alif Semiconductor Ensemble E8 DevKit (Cortex-M55)** 的開發環境與編譯管線。

---

## 📂 專案結構說明

* `blinky/`：**LED 閃爍範例**（透過 SysTick 與 GPIO 控制 DevKit 上的 RGB LED）
* `hello/`：**UART 串口通訊範例**（將標準 `printf` 重定向至 DevKit 的 UART 埠）
* `hello_rtt/`：**SEGGER RTT 極速日誌範例**（無需佔用 UART，直接透過 J-Link 讀取日誌）
* `board/devkit-e8/`：Alif E8 專屬引腳與板級定義檔 (`board_defs.h`, `pins.h`, `gpios.h`)
* `.alif/`：J-Link 設備配置檔與 Secure Enclave 啟動配置
* `.vscode/`：已配置好的 VS Code 建置任務 (`tasks.json`)、J-Link 偵錯 (`launch.json`) 與環境設定

---

## 🚀 常用操作指令

### 1. 快速編譯 (Terminal)
在終端機中可直接執行以下指令進行編譯：

```bash
# 編譯 E8 Cortex-M55 HP Blinky
cbuild alif.csolution.yml --context blinky.debug+DevKit-E8-HP

# 編譯 E8 Cortex-M55 HP Hello (UART Printf)
cbuild alif.csolution.yml --context hello.debug+DevKit-E8-HP

# 編譯 E8 Cortex-M55 HP Hello (J-Link RTT)
cbuild alif.csolution.yml --context hello_rtt.debug+DevKit-E8-HP

# 編譯 E8 Cortex-M55 HE (超低功耗核心) Blinky
cbuild alif.csolution.yml --context blinky.debug+DevKit-E8-HE
```

### 2. 在 VS Code 內快速建置 (GUI)
* 按下快速鍵 **`Ctrl + Shift + B`**。
* 選單中可直接選擇：
  * `Build DevKit-E8-HP Blinky`
  * `Build DevKit-E8-HP Hello (UART)`
  * `Build DevKit-E8-HP Hello (RTT)`
  * `Build DevKit-E8-HE Blinky`

### 3. 編譯產物路徑 (Output Artifacts)
編譯完成後的二進位檔會產生於：
* **ELF 檔**（含除錯符號）：`out/<project>/DevKit-E8-HP/debug/<project>.elf`
* **BIN 檔**（純二進位檔）：`out/<project>/DevKit-E8-HP/debug/<project>.bin`

### 4. 硬體除錯 (J-Link Debug)
1. 將 Alif E8 DevKit 連接至電腦的 USB 埠。
2. 在 VS Code 左側點擊 **Run & Debug (執行與偵錯)**。
3. 選擇 **`Alif Ensemble Debug (Cortex-Debug)`** 或 **`M55_HE JLink (launch)`**。
4. 按下 **`F5`** 即可自動透過 J-Link 下載韌體並中斷在 `main()` 進入單步除錯！

