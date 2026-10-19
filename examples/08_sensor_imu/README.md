# Example 08: Bosch BMI323 6-Axis IMU over I3C & Real-Time Attitude Telemetry

This example demonstrates high-speed sensor interfacing on the **Alif Semiconductor Ensemble E8 Development Kit** (`AE822FA0E5597LS0`), acquiring 3-axis acceleration and 3-axis angular velocity from the onboard **Bosch BMI323 IMU** via the **Synopsys DesignWare I3C Bus Controller** (`i3c0`), computing real-time 3D attitude (Pitch & Roll) on the ARM Cortex-M55 hardware FPU, and rendering live ASCII horizon telemetry and RGB LED indicators over **SEGGER RTT**.

---

## 🌟 Key Features

1. **High-Speed I3C Bus Protocol**:
   - Master controller `i3c0` at `0x49034000` assigns dynamic address `0x0A` to Bosch BMI323 (`0x69 0x03b8 0x10431000`).
   - Hardware interrupt line on `P8_4` (`gpio8` Pin 4) with bus recovery on `gpio7`.
2. **Cortex-M55 FPU 3D Attitude Estimation**:
   - Accelerometer configured to $\pm 2g$, 100 Hz, High Performance mode.
   - Gyroscope configured to $\pm 500^\circ/\text{s}$, 100 Hz, High Performance mode.
   - Die temperature monitoring in degrees Celsius.
   - Hardware floating-point calculation of Pitch and Roll angles.
3. **Dynamic Visual Attitude Feedback**:
   - **Horizon Level ($|\text{Pitch}| < 6^\circ, |\text{Roll}| < 6^\circ$)**: Solid Green (`LED1_G`).
   - **Forward/Backward Tilt ($|\text{Pitch}| \ge 6^\circ$)**: Solid Red (`LED1_R`).
   - **Left/Right Roll ($|\text{Roll}| \ge 6^\circ$)**: Solid Blue (`LED1_B`).
   - **High Angular Velocity ($|\vec{\omega}| > 40^\circ/\text{s}$)**: White strobe pulse.
4. **Zero-Latency SEGGER RTT Real-Time Console**:
   - High-throughput terminal telemetry without physical switch toggling.
   - `_SEGGER_RTT` control block verified at DTCM address **`0x20000410`**.
   - Dual visualization modes: Compact live data stream and Visual ASCII Horizon Bar.
5. **Interactive RTT Host Commands**:
   - `'c'` / `'C'`: Tare/Calibrate current physical attitude as reference level $(0.0^\circ, 0.0^\circ)$.
   - `'r'` / `'R'`: Reset calibration offsets to factory zero.
   - `'m'` / `'M'`: Toggle between Compact Telemetry and ASCII Horizon Bar modes.
   - `'h'` / `'H'`: Display interactive help menu.

---

## 🛠️ Hardware Interconnects

| Function | Signal / Pin | Devicetree Node | Description |
| :--- | :--- | :--- | :--- |
| **I3C SCL** | `P7_0` | `&i3c0` | I3C Bus Serial Clock line |
| **I3C SDA** | `P7_1` | `&i3c0` | I3C Bus Serial Data line |
| **IMU INT1** | `P8_4` | `&gpio8` pin 4 | BMI323 Hardware Data Ready interrupt |
| **LED Red** | `P6_2` | `&gpio6` pin 2 | Onboard RGB LED Red component |
| **LED Green** | `P6_4` | `&gpio6` pin 4 | Onboard RGB LED Green component |
| **LED Blue** | `P6_6` | `&gpio6` pin 6 | Onboard RGB LED Blue component |

---

## 📐 Mathematical Formulation

Euler attitude angles are calculated using Cortex-M55 single-precision hardware FPU instructions:

$$\text{Pitch } (\theta) = \text{atan2}\left(a_x, \sqrt{a_y^2 + a_z^2}\right) \times \frac{180}{\pi} - \theta_{\text{offset}}$$

$$\text{Roll } (\phi) = \text{atan2}\left(a_y, a_z\right) \times \frac{180}{\pi} - \phi_{\text{offset}}$$

$$\text{Angular Velocity Magnitude } (|\vec{\omega}|) = \sqrt{\omega_x^2 + \omega_y^2 + \omega_z^2}$$

---

## 🚀 Build & Flash Instructions

### 1. Compile Firmware (WSL2)
```bash
cd ~/alif/sdk-alif/zephyr
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp \
  /mnt/c/Users/test/.gemini/antigravity/scratch/alif_vscode-template/examples/08_sensor_imu
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
4. The terminal will immediately stream real-time IMU attitude data and ASCII horizon meters!

