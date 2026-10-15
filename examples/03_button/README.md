# Example 03: Button & 5-Way Joystick Navigation (GPIO IRQ & LED Control)

This example demonstrates asynchronous hardware interrupt handling on the **Alif Semiconductor Ensemble E8 / E7** fusion processors using the **Zephyr RTOS GPIO callback and interrupt APIs**.

On the **Alif E8 DevKit (Rev A0)**, user input is provided by **`SW2`**, a physical 5-way navigation joystick/switch mapped to Low-Power GPIO pins (`LPGPIO_0..4`):
- **Up**: `LPGPIO_1` (`JOY_SW_B`)
- **Down**: `LPGPIO_0` (`JOY_SW_A`)
- **Left**: `LPGPIO_2` (`JOY_SW_C`)
- **Right**: `LPGPIO_3` (`JOY_SW_D`)
- **Center Push**: `LPGPIO_4` (`JOY_SW_CENTER`, aliased to `sw0`)

Tilting or pressing `SW2` generates active-low falling-edge interrupts and state changes to control the on-board green LED (`LED1_G`, `GPIO6_4`):
- **Push Center**: Toggles Green LED (ON <-> OFF).
- **Push Up**: Turns Green LED **ON**.
- **Push Down**: Turns Green LED **OFF (熄滅)**.
- **Push Left / Right**: Toggles Green LED.

---

## 🎯 Features

- **SoC Target**: Alif Ensemble E8 (`AE822FA0E5597LS0`) / E7 (`AE722F80F55D5LS`)
- **Core Target**: Cortex-M55 RTSS-HE
- **Button / Joystick Input**: Physical 5-way navigation switch **`SW2`**, mapped to **`LPGPIO_0..4`** (Low-Power GPIO)
- **Hardware Pull-Up Control**: Activates SoC Pad Control (`0x42007000`) with `0x2B` (`READ_ENABLE | SCHMITT | PULL_UP | 4MA`)
- **Interrupt Trigger**: `GPIO_INT_EDGE_FALLING` with edge debounce
- **Controlled LED**: On-board Green LED **`LED1_G`** (`GPIO6_4`) with RGB self-test on boot
- **Telemetry**: Real-time interrupt event logging with direction identification over `UART2` (115,200 baud)

---

## 🔌 Hardware Setup

1. **USB Cable**: Connect your host PC to the **`PRG`** USB Type-C port.
2. **Physical Controls on Board**:
   - **`SW2`**: The 5-way navigation joystick located on the board.
   - **`LED1_G`**: The green user LED (`GPIO6_4`).
3. **Hardware Switch (`SW4` / `UART SEL`)**:
   - **During Flashing**: Set to **`SE` (Top position)**.
   - **To View Serial Output**: Set to **`UART2` (Center position)**.
   *(Note: The physical LED will react to joystick presses regardless of switch position!)*

---

## 🚀 Build and Flash Instructions

### 1. Build the Binary
From your Zephyr workspace root:
```bash
cd ~/alif/sdk-alif/zephyr
source ~/alif_venv/bin/activate
export ALIF_SE_TOOLS_DIR=/home/user/alif/alif_security_toolkit/toolkit

# Build for Alif E8 DevKit RTSS-HE
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_he /path/to/examples/03_button
```

### 2. Flash to MRAM
Ensure `SW4` is on **`SE` (Top)**:
```bash
west flash
```
*(Note: If a serial monitor like `picocom` was left open, the enhanced `scripts/app-write-mram` wrapper automatically detects and terminates conflicting handles to avoid `readSerial reporting disconnected` errors).*

Wait until the terminal outputs:
```text
Burn into MRAM: [100%]
Write finish!
```

### 3. Verify Operation

#### Visual Verification (Hardware Joystick & LED)
1. **Boot Self-Test**: Observe the RGB LED: it will sequence **Red (250ms) -> Green (250ms) -> Blue (250ms)** on boot, then turn completely OFF.
2. **Turn ON**: Push **`SW2` UP** -> Green LED turns **ON**.
3. **Turn OFF / Extinguish (熄滅)**: Push **`SW2` DOWN** -> Green LED turns **OFF**.
4. **Toggle**: Press **`SW2` CENTER PUSH** -> Green LED **toggles state** (ON <-> OFF).

#### Serial Console Verification (Optional)
1. Slide **`SW4`** to the **Center position (`UART2`)**.
2. Open the terminal:
   ```bash
   picocom /dev/ttyACM0 -b 115200
   ```
3. Press **RESET** on the board. The banner will print:
   ```text
   =======================================================
   *** Alif Ensemble E8 - SW2 Joystick & LED Control ***
   Target Board: alif_e8_dk (RTSS-HE)
   SW2 Physical Mapping (Hardware-Verified):
     - LPGPIO_1: UP        -> Turn ON Green LED
     - LPGPIO_0: DOWN      -> Turn OFF Green LED (熄滅)
     - LPGPIO_4: CENTER    -> Toggle Green LED (切換)
     - LPGPIO_2: LEFT      -> Toggle Green LED (切換)
     - LPGPIO_3: RIGHT     -> Toggle Green LED (切換)
   =======================================================
   >> Ready! Press or tilt SW2:
   ```
4. Now tilt or press **`SW2`**:
   ```text
   [SW2 Event #1] Action: UP (Turn ON) -> Green LED: ON (Time: 2314 ms)
   [SW2 Event #2] Action: DOWN (Turn OFF / 熄滅) -> Green LED: OFF (Time: 3840 ms)
   [SW2 Event #3] Action: CENTER PUSH (Toggle) -> Green LED: ON (Time: 4510 ms)
   ```

---

## 💡 How It Works

1. **Pad Control Hardware Pull-Up**:
   - The board has no external pull-up resistors on `SW2`.
   - The firmware writes `0x2B` (`PADCTRL_READ_ENABLE | PADCTRL_SCHMITT_TRIGGER_ENABLE | PADCTRL_DRIVER_DISABLED_PULL_UP | PADCTRL_OUTPUT_DRIVE_STRENGTH_4MA`) directly to the SoC Pad Control block at `0x42007000`.
2. **Devicetree & DesignWare GPIO**:
   - Pins are configured with `GPIO_INPUT` without software pull flags (which DesignWare IP rejects).
3. **Dual Edge IRQ + State Polling**:
   - Asynchronous NVIC falling-edge interrupts work alongside main loop state polling with 180ms debouncing.

---

## 📄 File Structure

- [`src/main.c`](src/main.c): 5-direction interrupt & polling callback registration, startup self-test, and LED toggle handler.
- [`prj.conf`](prj.conf): Project Kconfig options (`CONFIG_GPIO=y`, `CONFIG_SERIAL=y`).
- [`CMakeLists.txt`](CMakeLists.txt): Zephyr CMake build recipe.


