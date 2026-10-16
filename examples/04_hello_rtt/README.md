# Example 04: SEGGER RTT Real-Time Logging (Zero-Switch Debugging)

This example demonstrates **SEGGER Real-Time Transfer (RTT)** logging and console communication on the **Alif Semiconductor Ensemble E8 / E7** fusion processors using Zephyr RTOS.

---

## ⚡ The Zero-Switch Breakthrough for Alif DevKits

On the **Alif E8 DevKit**, the physical 3-position slide switch **`SW4`** routes the onboard J-Link CDC virtual COM port:
- **`SE` (Top position)**: Required for bootloader ISP flashing (`west flash`).
- **`UART2` (Center position)**: Required for viewing standard UART `printf()` / `printk()` serial logs.

In standard workflows, developers must repeatedly flip `SW4` to `SE` to flash firmware, then flip it down to `UART2` to see output, and flip it back up to flash again.

### 🚀 Why SEGGER RTT Changes Everything:
1. **Zero Switch Toggling**: RTT transfers data bi-directionally directly through the SWD/JTAG debug probe and an in-memory ring buffer (`_SEGGER_RTT` in SRAM). **`SW4` can remain on `SE` (Top) at all times!**
2. **Zero Baud Rate Bottleneck**: Eliminates standard 115,200 baud serialization delays; messages transfer in microseconds with negligible CPU cycles.
3. **Dual Output Support**: Zephyr routes console logs to **both** SEGGER RTT and physical `UART2`, allowing simultaneous viewing.
4. **Bi-Directional Interactive Echo**: Host can send keystrokes into the target via RTT down-buffers (`SEGGER_RTT_GetKey()`).

---

## 🎯 Features

- **SoC Target**: Alif Ensemble E8 (`AE822FA0E5597LS0`) / E7 (`AE722F80F55D5LS`)
- **Core Target**: Cortex-M55 RTSS-HP (`alif_e8_dk/ae822fa0e5597xx0/rtss_hp`)
  > Note: For J-Link V7.80c, `rtss_hp` maps to native AP[0] (`AE722F80F55D5_HP`). For J-Link >= V8.86, both `AE822FA0E5597BS0_M55_HP` and `AE822FA0E5597LS0_M55_HE` are supported.
- **Console Backend**: SEGGER RTT (`CONFIG_RTT_CONSOLE=y`, `CONFIG_USE_SEGGER_RTT=y`, `CONFIG_UART_CONSOLE=n`)
- **Visual Heartbeat**: On-board RGB LED toggles Cyan (`LED1_G` + `LED1_B`) at 1 Hz
- **Interactive Feedback**: On-board LED flashes Purple (`LED1_R` + `LED1_B`) when keystrokes are received via RTT
- **RTT Buffer Location**: Control Block `_SEGGER_RTT` located in DTCM at `0x20000410`

---

## 🔌 Hardware Setup

1. **USB Cable**: Connect host PC to the **`PRG`** USB Type-C connector on the Alif DevKit.
2. **Switch `SW4`**:
   - Set to **`SE` (Right position)** during `west flash`.
   - Set to **`UART2` (Center position)** to boot firmware from MRAM, then press **`RESET`**.

---

## 🚀 Build and Flash Instructions

### 1. Build the Binary
From your Zephyr workspace:
```bash
cd ~/alif/sdk-alif/zephyr
source ~/alif_venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR=/home/user/zephyr-sdk-0.16.8
export ALIF_SE_TOOLS_DIR=/home/user/alif/alif_security_toolkit/toolkit

# Build for Alif E8 DevKit RTSS-HP
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp examples/04_hello_rtt
```

### 2. Flash to MRAM
With `SW4` in the **`SE` (Right)** position and USB attached to WSL:
```bash
west flash
```

---

## 🖥️ How to View RTT Real-Time Logs

### Method 1: J-Link RTT Viewer (Windows GUI - Verified)
1. Launch **J-Link RTT Viewer** (from Start Menu or `C:\Program Files\SEGGER\JLink\JLinkRTTViewer.exe`).
2. Configure Connection:
   - **Connection to J-Link**: `USB`
   - **Specify Target Device**: `AE722F80F55D5_HP`
   - **Target Interface & Speed**: `JTAG`, `4000 kHz`
   - **RTT Control Block**: Select `Address`, enter **`0x20000410`**
3. Click **OK**.
4. The terminal will immediately display formatted ANSI color logs from the Alif E8!
5. In the bottom input bar of RTT Viewer, type any character (e.g., `hello`) and press Enter to observe interactive echo and purple LED flashes!

---

### Method 2: J-Link RTT Client / Telnet
If J-Link GDB Server or JLinkExe is running:
```bash
# Connect to J-Link RTT Telnet Port
telnet localhost 19021
```

---

## 🔍 Expected Console Output

```text
===============================================================
⚡ Alif Ensemble E8 - SEGGER RTT High-Speed Logging Active ⚡
 Target Board : alif_e8_dk/ae822fa0e5597xx0/rtss_hp
 Architecture : ARM Cortex-M55 (RTSS-HP)
 RTT Buffer   : Local 0x20000410
===============================================================

[RTT #0000] Uptime: 512 ms | Heartbeat LED: ON  | System OK
[RTT #0001] Uptime: 1512 ms | Heartbeat LED: OFF | System OK
[RTT #0002] Uptime: 2512 ms | Heartbeat LED: ON  | System OK
>>> [RTT RX] Received key from host: 'h' (0x68) <<<
[RTT #0003] Uptime: 3512 ms | Heartbeat LED: OFF | System OK
```

