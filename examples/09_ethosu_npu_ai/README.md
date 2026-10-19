# Example 09: Arm Ethos-U55 NPU Edge AI Hardware Acceleration & TFLM Benchmark

This example demonstrates edge machine learning hardware acceleration on the **Alif Semiconductor Ensemble E8 Development Kit** (`AE822FA0E5597LS0`), executing quantized convolutional neural network (CNN) inferences on the integrated **Arm Ethos-U55 microNPU (256 MACs @ 400 MHz)** using **TensorFlow Lite for Microcontrollers (TFLM)** with real-time microsecond latency benchmarking, softmax probability telemetry, and RGB LED classification feedback over **SEGGER RTT**.

---

## 🌟 Key Features

1. **Hardware Neural Processing Unit (NPU)**:
   - **Arm Ethos-U55 microNPU** configured with **256 MACs** directly coupled to the Cortex-M55 High Performance (`rtss_hp`) core.
   - TFLM custom operator resolver with native Ethos-U delegate (`resolver.AddEthosU()`).
2. **Pre-compiled Vela Edge AI Model**:
   - Model: **Keyword Spotting (KWS)** CNN small INT8 (`keyword_spotting_cnn_small_int8`).
   - Quantized INT8 weights and activations optimized with the Arm Vela compiler.
   - 12 speech classification classes (`down`, `go`, `left`, `no`, `off`, `on`, `right`, `stop`, `up`, `yes`, `_silence_`, `_unknown_`).
3. **Hardware Latency & Throughput Benchmark**:
   - Microsecond-accurate latency measurement via Zephyr high-precision kernel timers.
   - Hardware CPU cycle count measurement via Cortex-M55 DWT cycle counters.
   - 50-run statistical benchmark calculating Minimum, Maximum, Average Latency, and Inferences Per Second (FPS).
4. **Dynamic Visual RGB Classification Feedback**:
   - **High-Confidence Keyword Match ($\ge 70\%$)**: Solid Green (`LED1_G`).
   - **Moderate Confidence / Uncertain ($40\% \sim 70\%$)**: Solid Yellow (`LED1_R` + `LED1_G`).
   - **Inference in Progress**: Blue pulse (`LED1_B`).
   - **Low Confidence / Error**: Red (`LED1_R`).
5. **Zero-Latency SEGGER RTT Real-Time Console**:
   - Full telemetry output without toggling physical switches.
   - `_SEGGER_RTT` control block verified at address **`0x20000410`**.
6. **Interactive RTT Host Commands**:
   - `'s'` / `'S'`: Run single inference with detailed 12-class probability breakdown.
   - `'b'` / `'B'`: Run automated 50-inference benchmark (measures min/avg/max latency & throughput).
   - `'t'` / `'T'`: Toggle continuous periodic inference mode (2 Hz).
   - `'h'` / `'H'`: Display interactive help menu.

---

## 🛠️ Hardware & Architecture Topology

| Component | Target / Signal | Specification |
| :--- | :--- | :--- |
| **CPU Core** | `rtss_hp` | ARM Cortex-M55 @ 400 MHz (Helium Vector MVE + FPU) |
| **NPU Engine** | `ethosu0` | Arm Ethos-U55 (256 MACs/cycle, AXI bus interface) |
| **Framework** | TFLM | TensorFlow Lite for Microcontrollers (C++17) |
| **Model Arena** | `.bss.tflm_arena` | 50,000 Bytes allocated in 0-wait-state DTCM RAM |
| **LED Green** | `P6_4` | `LED1_G` (High confidence match indicator) |
| **LED Blue** | `P6_6` | `LED1_B` (NPU actively calculating) |
| **LED Red** | `P6_2` | `LED1_R` (Low confidence / Error indicator) |

---

## 🚀 Build & Flash Instructions

### 1. Compile Firmware (WSL2)
```bash
cd ~/alif/sdk-alif/zephyr
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp \
  -S ethos-u55-enable \
  /mnt/c/Users/test/.gemini/antigravity/scratch/alif_vscode-template/examples/09_ethosu_npu_ai -- \
  -DETHOSU_TARGET_NPU_CONFIG=ethos-u55-256
```

Verify the RTT Control Block address:
```bash
nm build/zephyr/zephyr.elf | grep _SEGGER_RTT
# Output: 20000410 B _SEGGER_RTT
```

### 2. Flash via MRAM ISP Bootloader
1. Set physical switch **`SW4`** to **`SE` (Top Position)**.
2. Press physical reset button **`SW3`** (or `RESET`).
3. Attach USB device to WSL2:
   ```powershell
   usbipd bind --busid 1-1 --force
   usbipd attach --wsl --busid 1-1
   ```
4. Flash via `west`:
   ```bash
   west flash
   ```
5. Detach and restore Windows USB control:
   ```powershell
   usbipd detach --busid 1-1
   Start-Sleep -Seconds 1
   usbipd unbind -b 1-1
   ```

---

## 🖥️ Live Telemetry & Verification (J-Link RTT Viewer)

1. Set physical switch **`SW4`** to **`UART2` (Center Position)**.
2. Press physical reset button **`SW3`**.
3. Open **J-Link RTT Viewer** on Windows:
   - **Connection to J-Link**: USB
   - **Specify Target Device**: `AE722F80F55D5_HP`
   - **Target Interface & Speed**: `JTAG`, `4000 kHz`
   - **RTT Control Block**: Select **`Address`** and enter **`0x20000410`**.
4. The terminal will immediately stream NPU startup banner, initial inference classification, and interactive prompt!

