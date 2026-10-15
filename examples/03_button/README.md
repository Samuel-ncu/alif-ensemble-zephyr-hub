# Example 03: Button & 5-Way Joystick Navigation (GPIO IRQ & LED Control)

This example demonstrates asynchronous hardware interrupt handling on the **Alif Semiconductor Ensemble E8 / E7** fusion processors using the **Zephyr RTOS GPIO callback and interrupt APIs**.

On the **Alif E8 DevKit (Rev A0)**, user input is provided by **`SW2`**, a physical 5-way navigation joystick/switch mapped to Low-Power GPIO pins (`LPGPIO_0..4`):
- **Up**: `LPGPIO_0`
- **Down**: `LPGPIO_1`
- **Left**: `LPGPIO_2`
- **Right**: `LPGPIO_3`
- **Center Push**: `LPGPIO_4` (Default alias `sw0`)

Tilting or pressing `SW2` in any direction generates an active-low edge interrupt that toggles the on-board green LED (`LED1_G`) and streams real-time IRQ telemetry over UART.

---

## 🎯 Features

- **SoC Target**: Alif Ensemble E8 (`AE822FA0E5597LS0`) / E7 (`AE722F80F55D5LS`)
- **Core Target**: Cortex-M55 RTSS-HE
- **Button / Joystick Input**: Physical 5-way navigation switch **`SW2`**, mapped to **`LPGPIO_0..4`** (Low-Power GPIO)
- **Interrupt Trigger**: `GPIO_INT_EDGE_FALLING` with internal pull-up (`GPIO_PULL_UP`)
- **Controlled LED**: On-board Green LED **`LED1_G`**, mapped to `GPIO6_4` via alias `led0`
- **Self-Test Indicator**: 3 rapid green LED blinks on system boot to verify GPIO functionality, then turns OFF awaiting input
- **Telemetry**: Real-time interrupt event logging with direction identification over `UART2` (115,200 baud)

---

## 🔌 Hardware Setup

1. **USB Cable**: Connect your host PC to the **`PRG`** USB Type-C port.
2. **Physical Controls on Board**:
   - **`SW2`**: The 5-way navigation joystick located on the board.
   - **`LED1_G`**: The green user LED.
3. **Hardware Switch (`SW4` / `UART SEL`)**:
   - **During Flashing**: Set to **`SE` (Top position)**.
   - **To View Serial Output**: Set to **`UART2` (Center position)**.
   *(Note: The physical LED will toggle on button press regardless of switch position!)*

---

## 🚀 Build and Flash Instructions

### 1. Build the Binary
From your Zephyr workspace root:
```bash
cd ~/alif/sdk-alif/zephyr
source ~/alif_venv/bin/activate

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
1. **Boot Self-Test**: Observe the green LED (**`LED1_G`**): it will blink rapidly 3 times on boot, then turn OFF.
2. **Joystick Control**: Tilt **`SW2`** (Up, Down, Left, Right) or press **Center Push**: **`LED1_G`** toggles state on every action (ON -> OFF -> ON).

#### Serial Console Verification (Optional)
1. Slide **`SW4`** to the **Center position (`UART2`)**.
2. Open the terminal:
   ```bash
   picocom /dev/ttyACM0 -b 115200
   ```
3. Press **RESET** on the board. The banner will print:
   ```text
   ========================================
   *** Alif Ensemble E8 - SW2 Joystick (Zephyr RTOS) ***
   Target Board: alif_e8_dk
   SW2 5-Way Joystick:
     - LPGPIO_0: Up
     - LPGPIO_1: Down
     - LPGPIO_2: Left
     - LPGPIO_3: Right
     - LPGPIO_4: Center Push (sw0 alias)
   LED   : LED1_G (gpio@49006000, Pin: 4)
   Instruction: Press/tilt SW2 in any direction to toggle LED!
   ========================================
   >> Hardware Interrupt configured successfully. Waiting for button press...
   ```
4. Now tilt or press **`SW2`**:
   ```text
   [IRQ Event #1] Direction [Center Push] Pressed! Green LED: ON (SysTick: 2314)
   [IRQ Event #2] Direction [Up] Pressed! Green LED: OFF (SysTick: 3840)
   [IRQ Event #3] Direction [Left] Pressed! Green LED: ON (SysTick: 4510)
   ```

---

## 💡 How It Works

1. **Devicetree Binding**:
   - `DT_ALIAS(sw0)` resolves to `&lpgpio 4 0` (Center Push).
   - Additional directions (`LPGPIO_0..3`) are dynamically acquired from the LPGPIO controller.
   - `DT_ALIAS(led0)` resolves to `&gpio6 4 0`.
2. **Edge-Triggered Interrupt**:
   - `gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_FALLING)` registers an asynchronous NVIC interrupt line for LPGPIO.
3. **Active-Low Pull-Up Architecture**:
   - The joystick pins are normally pulled HIGH. Tilting or pressing connects the pin to GND (Active Low).

---

## 📄 File Structure

- [`src/main.c`](src/main.c): 5-direction interrupt & polling callback registration, startup self-test, and LED toggle handler.
- [`prj.conf`](prj.conf): Project Kconfig options (`CONFIG_GPIO=y`, `CONFIG_SERIAL=y`).
- [`CMakeLists.txt`](CMakeLists.txt): Zephyr CMake build recipe.


