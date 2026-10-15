# 📋 Alif Ensemble E8 / E7 Hardware & GPIO Verification Reference

This document serves as the **definitive, hardware-verified reference** for GPIOs, onboard LEDs, switches, and peripheral drivers for the **Alif Semiconductor Ensemble E8 Development Kit** (`AE822FA0E5597LS0`, Rev A0) and **E7 DevKit** running Zephyr RTOS.

> [!IMPORTANT]
> **Zero-Hallucination Hardware Rules**: Both developers and AI coding agents must consult this reference before writing GPIO or hardware driver code for Alif Ensemble processors.

---

## 1. Onboard LEDs & GPIO Mapping

On the Alif E8 DevKit (Rev A0), the primary user RGB LEDs are located on **GPIO Port 6** (`LEDRGB1`):

| Silk Screen | Function / Color | SoC GPIO Port & Pin | Devicetree Node | DTS Alias | Active Level / Logic |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`LED1_G`** | User Green LED | `GPIO6_4` (Port 6, Pin 4) | `&aled0` in `leds` | `led0` | **Active-High**: `1` = ON, `0` = OFF |
| **`LED1_R`** | User Red LED | `GPIO6_2` (Port 6, Pin 2) | `&gpio6 2` | `led1` | **Active-High**: `1` = ON, `0` = OFF |
| **`LED1_B`** | User Blue LED | `GPIO6_6` (Port 6, Pin 6) | `&gpio6 6` | `led2` | **Active-High**: `1` = ON, `0` = OFF |

### 💡 LED Polarity Rule
- Physical hardware validation confirms: **`1` = LED ON, `0` = LED OFF**.
- Writing `1` to all three pins produces **White Light** (Red + Green + Blue).
- Writing `1` to Red + Green produces **Yellow Light**.
- Writing `1` to Red + Blue produces **Purple/Magenta Light**.
- Writing `1` to Green + Blue produces **Cyan/Teal Light**.

---

## 2. Onboard Buttons & Joystick (SW2) Mapping

On the Alif E8 DevKit (Rev A0), user input is provided by a **5-Way Navigation Joystick / Push Switch** labeled **`SW2`**.

| Joystick Action | SoC GPIO Pin | Port Number | Keil Conductor Signal | DTS Node / Alias | Circuit State |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Center Push** | `LPGPIO_4` | Port 15, Pin 4 | `JOY_SW_CENTER` | `&button0` / `sw0` | **Active-Low** (Pressed = 0V / GND) |
| **Up** | `LPGPIO_0` | Port 15, Pin 0 | `JOY_SW_A` | `&lpgpio 0` | **Active-Low** (Tilted Up = 0V / GND) |
| **Down** | `LPGPIO_1` | Port 15, Pin 1 | `JOY_SW_B` | `&lpgpio 1` | **Active-Low** (Tilted Down = 0V / GND) |
| **Left** | `LPGPIO_2` | Port 15, Pin 2 | `JOY_SW_C` | `&lpgpio 2` | **Active-Low** (Tilted Left = 0V / GND) |
| **Right** | `LPGPIO_3` | Port 15, Pin 3 | `JOY_SW_D` | `&lpgpio 3` | **Active-Low** (Tilted Right = 0V / GND) |

---

## 3. Critical Driver Rule: Synopsys DesignWare GPIO & Hardware Pad Control

Alif Ensemble Low-Power GPIO (`LPGPIO`) uses the **Synopsys DesignWare GPIO IP** (`compatible = "snps,designware-gpio"`).

### 🚫 The `GPIO_PULL_UP` Prohibition & The Hardware Pull-Up Solution
1. **Driver Limitation**: In Zephyr's DesignWare GPIO driver (`gpio_dw.c`), passing `GPIO_PULL_UP` or `GPIO_PULL_DOWN` strictly returns `-ENOTSUP` (-134).
2. **Hardware Reality**: The board has NO external pull-up resistors on `SW2`. Without internal pull-ups, the pins float, resulting in random or stuck readings.
3. **Hardware Activation**: To activate the internal pull-up resistor on `LPGPIO` pins, write directly to the SoC Pad Control block:
   ```c
   #define LPGPIO_PADCTRL_BASE 0x42007000
   /* READ_ENABLE (0x01) | SCHMITT (0x02) | PULL_UP (0x08) | 4MA (0x20) = 0x2B */
   #define PADCTRL_VAL_PULL_UP (0x01U | 0x02U | 0x08U | 0x20U)

   volatile uint32_t *pad_ctrl = (volatile uint32_t *)LPGPIO_PADCTRL_BASE;
   for (int i = 0; i < 5; i++) {
       pad_ctrl[i] = PADCTRL_VAL_PULL_UP;
   }
   ```
4. **Zephyr Pin Configuration**: Configure Zephyr pins as standard `GPIO_INPUT` (without software pull flags):
   ```c
   gpio_pin_configure(port, pin, GPIO_INPUT);
   ```
> - Subsequent `gpio_pin_interrupt_configure()` fails with `-EINVAL` (-22).
> - The interrupt callback is never called, and polling returns floating/stuck values!

### ✅ Correct Initialization Pattern
```c
/* 1. Configure as standard input (pull-ups are handled in hardware/pinctrl) */
int ret = gpio_pin_configure(button.port, pin, GPIO_INPUT);
if (ret != 0) {
    printk("Config error: %d\n", ret);
}

/* 2. Configure edge-triggered interrupt */
ret = gpio_pin_interrupt_configure(button.port, pin, GPIO_INT_EDGE_BOTH);

/* 3. Register callback */
gpio_init_callback(&button_cb_data, button_isr, BIT(pin));
gpio_add_callback(button.port, &button_cb_data);
```

---

## 4. Hardware Switch `SW4` (UART Routing) Truth Table

The physical 3-position slide switch **`SW4`** routes the onboard J-Link CDC virtual COM port (`/dev/ttyACM0`):

| Switch Position | Label / Mode | Target Connection | Purpose / Usage |
| :--- | :--- | :--- | :--- |
| **Top Position** | **`SE`** | Secure Enclave ISP ROM (`UART4` @ 57,600 baud) | **Mandatory for Flashing (`west flash`)**. Bootloader commands, MRAM burning, and Security Toolkit. |
| **Center Position** | **`UART2`** (AP) | Application Processor Console (`UART2` @ 115,200 baud) | **Application Logging & Telemetry**. View Zephyr `printk()` / `printf()` output for RTSS-HE. |
| **Bottom Position** | **`MCU`** / Aux | Auxiliary Cortex-M MCU | External debugger / auxiliary debug channel. |

---

## 5. Standard Build & Flash Quick-Commands

### Build for RTSS-HE Core:
```bash
cd ~/alif/sdk-alif/zephyr
source ~/alif_venv/bin/activate
export ALIF_SE_TOOLS_DIR=/home/user/alif/alif_security_toolkit/toolkit

# Compile target example:
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_he /mnt/c/Users/test/.gemini/antigravity/scratch/alif_vscode-template/examples/03_button
```

### Flash to MRAM:
1. Ensure `SW4` is in the **`SE` (Top)** position.
2. Run:
```bash
west flash
```
*(The repository's automated `scripts/app-write-mram` wrapper will automatically terminate any conflicting terminal sessions and verify 100% burn).*

