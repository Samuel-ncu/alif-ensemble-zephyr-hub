# Example 01: Hello World (UART Console Output)

This example demonstrates the foundational bring-up workflow on the **Alif Semiconductor Ensemble E8 / E7** fusion processor family using **Zephyr RTOS**. It validates the Secure Bootloader (SES) verification chain, the UART console driver, and periodic timer loop logging.

---

## 🎯 Features

- **SoC Target**: Alif Ensemble E8 (`AE822FA0E5597LS0`) / E7 (`AE722F80F55D5LS`)
- **Core Target**: Cortex-M55 RTSS-HE (High-Efficiency)
- **Console Interface**: `UART2` (115,200 baud, 8N1)
- **RTOS**: Zephyr RTOS v4.1+ (Alif Semiconductor SDK)

---

## 🔌 Hardware Setup

1. **USB Cable**: Connect your host PC to the **`PRG`** USB Type-C port (on-board J-Link CDC UART bridge).
   > ⚠️ Do not use the `MCU` port for this example.
2. **Hardware Switch (`SW4` / `UART SEL`)**:
   - **During Flashing**: Set to **`SE` (Top position)**.
   - **During Execution / Serial Monitor**: Set to **`UART2` (Center position)**.

---

## 🚀 Build and Flash Instructions

### 1. Build the Binary
From your Zephyr workspace root:
```bash
cd ~/alif/sdk-alif/zephyr
source ~/alif_venv/bin/activate

# Build for Alif E8 DevKit RTSS-HE
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_he /path/to/examples/01_hello_world
```

### 2. Flash to MRAM
Ensure `SW4` is on **`SE` (Top)** and no terminal is occupying `/dev/ttyACM0`:
```bash
pkill picocom
west flash
```
Wait until the terminal outputs:
```text
Burn into MRAM: [100%]
Write finish!
```

### 3. View Serial Console Output
1. Slide **`SW4`** to the **Center position (`UART2`)**.
2. Open your terminal emulator:
   ```bash
   picocom /dev/ttyACM0 -b 115200
   ```
3. Press the physical **RESET** button on the board.
4. Expected output:
   ```text
   ========================================
   *** Alif Ensemble E8 - Hello World ***
   Target Board: alif_e8_dk
   RTOS: Zephyr OS 4.1.0
   ========================================
   Hello World from Alif Ensemble! (count=1)
   Hello World from Alif Ensemble! (count=2)
   Hello World from Alif Ensemble! (count=3)
   ```
*(To exit picocom: press `Ctrl+A`, then `Ctrl+X`)*

---

## 📄 File Structure

- [`src/main.c`](src/main.c): Application entry point with Zephyr kernel timer delay loop.
- [`prj.conf`](prj.conf): Project Kconfig options (`CONFIG_SERIAL=y`, `CONFIG_UART_CONSOLE=y`).
- [`CMakeLists.txt`](CMakeLists.txt): Zephyr CMake build recipe.

