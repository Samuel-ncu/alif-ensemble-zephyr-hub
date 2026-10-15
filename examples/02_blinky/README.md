# Example 02: Blinky (GPIO LED Control)

This example demonstrates hardware peripheral control on the **Alif Semiconductor Ensemble E8 / E7** fusion processors using the **Zephyr RTOS GPIO driver API** and Devicetree abstractions. It drives the on-board green LED (`LED1_G`) at 1 Hz while printing toggle telemetry to the UART console.

---

## 🎯 Features

- **SoC Target**: Alif Ensemble E8 (`AE822FA0E5597LS0`) / E7 (`AE722F80F55D5LS`)
- **Core Target**: Cortex-M55 RTSS-HE
- **Target LED**: On-board Green LED (`LED1_G`), mapped to `GPIO6_4` via Devicetree alias `led0`
- **Toggle Frequency**: 1 Hz (500 ms ON / 500 ms OFF)
- **Telemetry**: Real-time state logging over `UART2` (115,200 baud)

---

## 🔌 Hardware Setup

1. **USB Cable**: Connect your host PC to the **`PRG`** USB Type-C port.
2. **Hardware Switch (`SW4` / `UART SEL`)**:
   - **During Flashing**: Set to **`SE` (Top position)**.
   - **To View Serial Output**: Set to **`UART2` (Center position)**.
   *(Note: The green LED will blink regardless of switch position once flashed!)*

---

## 🚀 Build and Flash Instructions

### 1. Build the Binary
From your Zephyr workspace root:
```bash
cd ~/alif/sdk-alif/zephyr
source ~/alif_venv/bin/activate

# Build for Alif E8 DevKit RTSS-HE
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_he /path/to/examples/02_blinky
```

### 2. Flash to MRAM
Ensure `SW4` is on **`SE` (Top)** and serial port is free:
```bash
pkill picocom
west flash
```
Wait until the terminal outputs:
```text
Burn into MRAM: [100%]
Write finish!
```

### 3. Verify Operation

#### Visual Verification (Hardware LED)
Look directly at the development board near the SoC / LEDs:
- **`LED1_G` (Green LED)** will immediately start blinking at 1 Hz.

#### Serial Console Verification (Optional)
1. Slide **`SW4`** to the **Center position (`UART2`)**.
2. Open the terminal:
   ```bash
   picocom /dev/ttyACM0 -b 115200
   ```
3. Press **RESET** to see the initialization banner and live toggle counts:
   ```text
   ========================================
   *** Alif Ensemble E8 - Blinky (Zephyr RTOS) ***
   Target Board: alif_e8_dk
   Target LED: LED1_G (Port: gpio@49006000, Pin: 4)
   Blink Interval: 500 ms
   ========================================
   [Blinky #1] Green LED: ON
   [Blinky #2] Green LED: OFF
   [Blinky #3] Green LED: ON
   ```

---

## 💡 Keil CMSIS vs. Zephyr Comparison

| Item | Legacy Keil / CMSIS | Modern Zephyr RTOS |
| :--- | :--- | :--- |
| **Pin Configuration** | Manual register manipulation, `pinconf.h`, `board_pins_config()` | Declared in Devicetree (`dts`), resolved automatically |
| **GPIO Abstraction** | CMSIS `ARM_DRIVER_GPIO` pointer arrays | Zephyr `gpio_dt_spec` type-safe struct |
| **Portability** | Hard-coded board headers | Runs unchanged on any board defining `aliases { led0 = ...; }` |
| **License** | Proprietary IDE license required | 100% Open Source (Apache 2.0) |

---

## 📄 File Structure

- [`src/main.c`](src/main.c): GPIO toggle logic using `gpio_pin_toggle_dt()`.
- [`prj.conf`](prj.conf): Project Kconfig options (`CONFIG_GPIO=y`, `CONFIG_SERIAL=y`).
- [`CMakeLists.txt`](CMakeLists.txt): Zephyr CMake build recipe.

