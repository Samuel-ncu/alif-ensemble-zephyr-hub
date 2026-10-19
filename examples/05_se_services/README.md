# Example 05: Alif Ensemble E8 - Secure Enclave Services & Hardware Security Telemetry

This example demonstrates direct inter-processor service calls between the **Cortex-M55** application core and the on-chip **Secure Enclave (SES)** on the **Alif Semiconductor Ensemble E8 / E7** fusion processors using Zephyr RTOS.

---

## 🛡️ What are Secure Enclave (SES) Services?

The Alif Ensemble architecture features an isolated, hardware-enforced **Secure Enclave (SES)** processor. The Secure Enclave oversees:
1. **Root-of-Trust & Secure Boot**: Verifies MRAM image certificates and Table of Contents (TOC).
2. **Cryptographic Identity**: Maintains factory-burned OTP keys, Hash of Boot Keys (`HBK0`, `HBK1`, `HBK_FW`), and Device Lifecycle State (`LCS`).
3. **Hardware True Random Number Generation (TRNG)**: Quantum/thermal physical entropy generator.
4. **Power & Reset Management**: Controls SoC-wide power domains and reset lines for individual CPU cores.

The Cortex-M55 application processor communicates with the Secure Enclave over the **ARM Message Handling Unit v2 (MHUv2)** mailbox protocol using Alif's `se_service` driver.

---

## 🎯 Features

- **SoC Target**: Alif Ensemble E8 (`AE822FA0E5597LS0`) / E7 (`AE722F80F55D5LS`)
- **Core Target**: Cortex-M55 RTSS-HP (`alif_e8_dk/ae822fa0e5597xx0/rtss_hp`)
  > Note: Also compatible with `rtss_he`.
- **Console Backend**: SEGGER RTT (`CONFIG_RTT_CONSOLE=y`, `CONFIG_USE_SEGGER_RTT=y`, `CONFIG_UART_CONSOLE=n`)
- **Secure Telemetry Queried**:
  - **SE Firmware Revision**: Version string and build timestamp
  - **Silicon Part Number**: e.g., `AE822FA0E5597LS0`
  - **Hardware Serial Number**: Unique 64-bit factory UID
  - **Lifecycle State (LCS)**: `0x01` (CM), `0x02` (DM), `0x05` (Secure SE), etc.
  - **Cryptographic Hashes**: `HBK0`, `HBK1`, `HBK_FW`
  - **DCU Permissions**: Debug Certificate Unit configuration bitmask
  - **Table of Contents (TOC)**: Number of MRAM application images & TOC version
- **Live Hardware TRNG Stream**: Generates true random 64-bit entropy words every 2 seconds
- **Interactive Commands (over RTT Viewer)**:
  - Press `'r'` / `'R'`: Request immediate 128-bit hardware TRNG entropy
  - Press `'h'` / `'H'`: Ping Secure Enclave heartbeat
  - Press `'t'` / `'T'`: Re-query TOC firmware status

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
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp examples/05_se_services
```

### 2. Flash to MRAM
With `SW4` in the **`SE` (Right)** position and USB attached to WSL:
```bash
west flash
```

---

## 🖥️ How to View Real-Time Output

1. Slide `SW4` switch back to **`UART2` (Center)** and press **`RESET`** on the DevKit.
2. Launch **J-Link RTT Viewer** on Windows:
   - **Connection to J-Link**: `USB`
   - **Specify Target Device**: `AE722F80F55D5_HP`
   - **Target Interface & Speed**: `JTAG`, `4000 kHz`
   - **RTT Control Block**: Select `Address`, enter **`0x20000410`**
3. Click **OK** to connect.

---

## 🔍 Expected Console Output

```text
===============================================================
🛡️  Alif Ensemble E8 - Secure Enclave (SES) Services Active 🛡️
 Target Core : ARM Cortex-M55 (RTSS-HP)
 Communication: Inter-Processor Message Handling Unit (MHU)
===============================================================

[SE-SVC] Synchronizing with Secure Enclave...
[SE-SVC] Secure Enclave Handshake: SUCCESS (SE is Ready!)

  📌 SE Firmware Rev : SES A0 v1.110.0 Mar  4 2026 19:05:34
  📌 Part Number ID  : 672 (0x02A0)
  📌 TOC Image Count : 4 entries (TOC Version: 0x00010000)
  📌 Silicon Part#   : AE822FA0E5597LS0
  📌 Revision ID     : 672 (0x000002A0)
  📌 Serial Number   : 0x00000000
  📌 Lifecycle State : 0x01 -> DM (Device Manufacture)
  🔑 Root Hash (HBK0): 0xCD74AC70972C3E27B8B8F73AF34EDE
  🔑 App Hash (HBK1) : 0x0000000000000000
  🔒 DCU Permissions : 0x0000000000000000
  🏭 Mfg MetaData    : 0x51176004E0011D000000118041A0DAA4000900007C

===============================================================
>> Interactive Commands (Type in RTT Viewer and hit Enter):
   'r' : Generate 16 bytes of hardware TRNG entropy
   'h' : Send ping heartbeat to Secure Enclave
   't' : Re-query Table of Contents (TOC) status
===============================================================

[SE-TRNG #0000] Uptime:   2005 ms | Hardware Entropy: 0x3F8A1B29C4E05712 | OK
[SE-TRNG #0001] Uptime:   4005 ms | Hardware Entropy: 0x9D0E74C1A882FB34 | OK
[SE-TRNG #0002] Uptime:   6005 ms | Hardware Entropy: 0x55E29140B37CD6AA | OK
>>> [USER CMD: TRNG 128-bit] 0x4B9E2180FA1094E738B012EF90D7816A <<<
[SE-TRNG #0003] Uptime:   8005 ms | Hardware Entropy: 0x7E31D9A508FF4431 | OK
```

