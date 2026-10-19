# Example 10: On-Chip 12-Bit ADC & Internal Die Temperature Sensor

This example demonstrates analog data acquisition on the **Alif Semiconductor Ensemble E8 Development Kit** (`AE822FA0E5597LS0`), utilizing the on-chip **12-bit Successive Approximation Register (SAR) ADC** (`adc0` @ `0x49028000`) to sample the internal **Die Temperature Sensor (TSENS, Channel 6)**, calculating silicon temperature in Celsius and Fahrenheit, tracking running thermal statistics, and rendering live ASCII thermometer gauges and thermal RGB LED alerts over **SEGGER RTT**.

---

## 🌟 Key Features

1. **Integrated 12-Bit SAR ADC**:
   - Master peripheral `adc0` at memory address `0x49028000`.
   - Single-ended 12-bit conversion ($0 \sim 4095$ quantization levels).
   - High-precision 1.2V internal analog bandgap voltage reference.
2. **On-Chip Silicon Temperature Sensor (TSENS)**:
   - Dedicated internal analog routing via **Channel 6**.
   - Direct measurement of on-chip processor die temperature without external components.
   - Microvolt-level resolution with calibrated conversion table and linear interpolation.
3. **Statistical Thermal Tracking**:
   - Continuous 2 Hz telemetry monitoring.
   - Dynamic tracking of **Minimum Temperature**, **Maximum Temperature**, and **Cumulative Running Average Temperature**.
4. **Visual RGB Thermal Alert Indicators**:
   - **Normal Die Temperature ($< 35^\circ\text{C}$)**: Solid Green (`LED1_G`).
   - **Warm Die Temperature ($35^\circ\text{C} \sim 50^\circ\text{C}$)**: Solid Yellow (`LED1_R` + `LED1_G`).
   - **Hot Thermal Alert ($\ge 50^\circ\text{C}$)**: Solid Red (`LED1_R`).
5. **Zero-Latency SEGGER RTT Real-Time Console**:
   - High-throughput terminal telemetry without physical switch toggling.
   - `_SEGGER_RTT` control block verified at DTCM address **`0x20000410`**.
   - Dual visualization modes: Compact live data stream and Visual ASCII Thermometer Gauge.
6. **Interactive RTT Host Commands**:
   - `'s'` / `'S'`: Trigger immediate manual ADC sample reading.
   - `'r'` / `'R'`: Reset Min/Max/Avg temperature tracking statistics.
   - `'m'` / `'M'`: Toggle between Compact Telemetry and ASCII Thermometer Gauge modes.
   - `'h'` / `'H'`: Display interactive help menu.

---

## 🛠️ Hardware & Architecture Topology

| Component | Target / Signal | Specification |
| :--- | :--- | :--- |
| **ADC Peripheral** | `adc0` | 12-bit SAR ADC @ `0x49028000` |
| **Internal Channel** | Channel 6 | On-chip Silicon Die Temperature Sensor (`TSENS`) |
| **Reference Voltage** | $V_{\text{ref}}$ | 1.2V internal analog reference |
| **Resolution** | 12-bit | $1.2\text{V} / 4095 \approx 0.293\text{ mV/count}$ |
| **LED Green** | `P6_4` | `LED1_G` (Normal temperature indicator) |
| **LED Red** | `P6_2` | `LED1_R` (Warm / Hot temperature indicator) |
| **LED Blue** | `P6_6` | `LED1_B` (Sampling pulse indicator) |

---

## 📐 Mathematical Formulation

Voltage across the internal TSENS channel is calculated as:

$$V_{\text{mV}} = \frac{\text{raw\_code} \times 1200.0\text{ mV}}{4095.0}$$

Temperature in Fahrenheit is converted from Celsius via:

$$T_{\text{F}} = T_{\text{C}} \times 1.8 + 32.0$$

---

## 🚀 Build & Flash Instructions

### 1. Compile Firmware (WSL2)
```bash
cd ~/alif/sdk-alif/zephyr
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp \
  /mnt/c/Users/test/.gemini/antigravity/scratch/alif_vscode-template/examples/10_adc_temp
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
4. The terminal will immediately stream real-time die temperature telemetry and ASCII thermometer gauges!
