# 📋 Alif Ensemble E8 / E7 Hardware & GPIO Verification Reference

This document serves as the **definitive, hardware-verified reference** for GPIOs, onboard LEDs, switches, and peripheral drivers for the **Alif Semiconductor Ensemble E8 Development Kit** (`AE822FA0E5597LS0`, Rev A0) and **E7 DevKit** running Zephyr RTOS.

> [!IMPORTANT]
> **Zero-Hallucination Hardware Rules**: Both developers and AI coding agents must consult this reference before writing GPIO or hardware driver code for Alif Ensemble processors.

---

## 1. Onboard LEDs & GPIO Mapping

| Silk Screen | Function / Color | SoC GPIO Port & Pin | Devicetree Node | DTS Alias | Active Level / Circuit Logic |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`LED1_G`** | User Green LED | `GPIO6_4` (Port 6, Pin 4) | `&aled0` in `leds` | `led0` | **Active-Low**: `0` = ON (sinks current), `1` = OFF (cutoff) |
| **`LED1_R`** | User Red LED | `GPIO6_2` (Port 6, Pin 2) | Defined on HP/Dual core | `led1` | **Active-Low**: `0` = ON, `1` = OFF |
| **`LED1_B`** | User Blue LED | `GPIO6_3` (Port 6, Pin 3) | Optional overlay | `led2` | **Active-Low**: `0` = ON, `1` = OFF |

### ⚠️ LED Polarity Rule
- In Zephyr's DTS for `alif_e8_dk`, the LED flags are defined as `0` (`GPIO_ACTIVE_HIGH`).
- Because the physical circuit has the LED anode tied to 3.3V and cathode connected to the GPIO:
  - `gpio_pin_set_dt(&led, 0)` -> Outputs 0V -> **LED is ON**.
  - `gpio_pin_set_dt(&led, 1)` -> Outputs 3.3V -> **LED is OFF**.
  - Always use `gpio_pin_toggle_dt(&led)` for reliable state flipping!

---

## 2. Onboard Buttons & Joystick (SW2) Mapping

On the Alif E8 DevKit (Rev A0), user input is provided by a **5-Way Navigation Joystick / Push Switch** labeled **`SW2`**.

| Joystick Action | SoC GPIO Pin | Port Number | NVIC IRQ Line | DTS Node / Alias | Circuit State |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Center Push** | `LPGPIO_4` | Port 15, Pin 4 | IRQ 175 | `&button0` / `sw0` | **Active-Low** (Pressed = GND) |
| **Up** | `LPGPIO_0` | Port 15, Pin 0 | IRQ 171 | `&lpgpio 0` | **Active-Low** (Tilted Up = GND) |
| **Down** | `LPGPIO_1` | Port 15, Pin 1 | IRQ 172 | `&lpgpio 1` | **Active-Low** (Tilted Down = GND) |
| **Left** | `LPGPIO_2` | Port 15, Pin 2 | IRQ 173 | `&lpgpio 2` | **Active-Low** (Tilted Left = GND) |
| **Right** | `LPGPIO_3` | Port 15, Pin 3 | IRQ 174 | `&lpgpio 3` | **Active-Low** (Tilted Right = GND) |
| *Auxiliary* | `LPGPIO_5` | Port 15, Pin 5 | IRQ 176 | `&lpgpio 5` | Camera/Peripheral reset |
| *Auxiliary* | `LPGPIO_6` | Port 15, Pin 6 | IRQ 177 | `&lpgpio 6` | Auxiliary |
| *Auxiliary* | `LPGPIO_7` | Port 15, Pin 7 | IRQ 178 | `&lpgpio 7` | Auxiliary |

---

## 3. Critical Driver Rule: Synopsys DesignWare GPIO (`gpio_dw.c`)

Alif Ensemble Low-Power GPIO (`LPGPIO`) uses the **Synopsys DesignWare GPIO IP** (`compatible = "snps,designware-gpio"`).

### 🚫 The `GPIO_PULL_UP` Prohibition Bug
In Zephyr's DesignWare GPIO driver (`zephyr/drivers/gpio/gpio_dw.c` lines 301-304):
```c
/* Does not support pull-up/pull-down */
if ((flags & (GPIO_PULL_UP | GPIO_PULL_DOWN)) != 0U) {
    return -ENOTSUP; /* Error -134 */
}
```

> [!CAUTION]
> **NEVER pass `GPIO_PULL_UP` or `GPIO_PULL_DOWN` to `gpio_pin_configure()` on LPGPIO!**
> - If passed, `gpio_pin_configure()` returns `-ENOTSUP` (-134).
> - The pin direction register (`SWPORTA_DDR`) is **never configured**.
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
