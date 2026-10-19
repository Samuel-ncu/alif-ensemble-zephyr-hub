# Example 06: Hardware Universal Timer (UTIMER) PWM & RGB LED Breathing

This example demonstrates how to configure and control **Alif Semiconductor Ensemble E8's on-chip 32-bit Universal Timers (UTIMER)** in hardware PWM mode to drive the onboard RGB LED (`LEDRGB1`) with smooth perceptual gamma-corrected breathing animations and real-time SEGGER RTT telemetry.

---

## 🎯 Architecture & Hardware Pinout

On the **Alif Ensemble E8 Development Kit** (`AE822FA0E5597LS0`), the user RGB LEDs are wired to Port 6 pins which have alternate hardware multiplexing to Universal Timer channel outputs:

| Silk Screen | LED Function / Color | SoC GPIO Pin | Alternate Function | Timer Controller | Zephyr Node | PWM Channel |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **`LED1_R`** | Red LED | `P6_2` | `UT5_T0_B` | **UTIMER 5** | `&pwm_red` (`pwm5`) | Channel 0 (T0) |
| **`LED1_G`** | Green LED | `P6_4` | `UT6_T0_B` | **UTIMER 6** | `&pwm_green` (`pwm6`) | Channel 0 (T0) |
| **`LED1_B`** | Blue LED | `P6_6` | `UT7_T0_B` | **UTIMER 7** | `&pwm_blue` (`pwm7`) | Channel 0 (T0) |

### ⚡ Hardware Autonomous Generation (Zero CPU Load)
Once configured, the 32-bit UTIMER counter hardware automatically drives the PWM waveform continuously without CPU polling, context switches, or interrupts. The CPU only wakes up periodically to adjust compare reload values for smooth animations (~40 FPS).

---

## 🌈 Key Features

1. **Quadratic Gamma Correction**:
   Human visual brightness perception is non-linear (logarithmic). A quadratic gamma correction curve is applied to ensure smooth, natural breathing without harsh brightness jumps at low duty cycles.
2. **Animation Engine**:
   - **Mode 0: HSV 360° Rainbow Color Wheel**: Cycles smoothly through all hues with 100% saturation and brightness.
   - **Mode 1: Synchronous White Breathing**: Smooth fade in and out across all 3 channels in unison.
   - **Mode 2: Phase-Shifted RGB Chase Breathing**: 120-degree out-of-phase wave creating a rotating color glow.
   - **Mode 3: Static Color Hold**: Freeze or set pure Red, Green, Blue, Cyan, Magenta, Yellow, White.
3. **SEGGER RTT Real-Time Telemetry & Interactive Console**:
   Operates entirely through J-Link RTT (`Address: 0x20000840`), requiring **no physical SW4 UART switch changes**.

---

## 🛠️ Build & Flash Instructions

### 1. Build Firmware (WSL2)
```bash
cd ~/alif/sdk-alif/zephyr
source ~/alif_venv/bin/activate
export ALIF_SE_TOOLS_DIR=/home/user/alif/alif_security_toolkit/toolkit

west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp /mnt/c/Users/test/.gemini/antigravity/scratch/alif_vscode-template/examples/06_timer_pwm
```

### 2. Flash to MRAM
1. Switch `SW4` to **`SE` (Top)**.
2. In Windows PowerShell:
   ```powershell
   usbipd bind --busid 1-1 --force
   usbipd attach --wsl --busid 1-1
   ```
3. In WSL2:
   ```bash
   west flash
   ```
4. Release USB back to Windows host:
   ```powershell
   usbipd detach --busid 1-1
   usbipd unbind -b 1-1
   ```

---

## 📺 RTT Viewer Verification

1. Set `SW4` to **`UART2` (Center)** position.
2. Press **`RESET`** on the board.
3. Launch **SEGGER J-Link RTT Viewer**:
   - **Specify Target Device**: `AE722F80F55D5_HP`
   - **Target Interface & Speed**: `JTAG`, `4000 kHz`
   - **RTT Control Block**: Select `Address`, enter **`0x20000840`**
4. Watch the live PWM duty cycle log and type interactive commands into the Input field (`m`, `+`, `-`, `p`, `r`, `g`, `b`, `c`, `y`, `w`, `o`).

