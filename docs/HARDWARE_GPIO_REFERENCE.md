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
| **Up** | `LPGPIO_1` | Port 15, Pin 1 | `JOY_SW_B` | `&lpgpio 1` | **Active-Low** (Tilted Up = 0V / GND) |
| **Down** | `LPGPIO_0` | Port 15, Pin 0 | `JOY_SW_A` | `&lpgpio 0` | **Active-Low** (Tilted Down = 0V / GND) |
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

---

## 6. Universal Timer (UTIMER) Hardware PWM Pin Mapping

On the Alif Ensemble E8 DevKit, the onboard RGB LEDs (`LEDRGB1`) feature direct hardware multiplexing to the 32-bit Universal Timer (UTIMER) subsystem (`compatible = "alif,pwm"`):

| Silk Screen | LED Color | Pin Number | Devicetree Node | Alternate Pinmux | Timer Subsystem | PWM Output Channel |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **`LED1_R`** | Red LED | `P6_2` | `&pwm_red` (`pwm5`) | `PIN_P6_2__UT5_T0_B` | **UTIMER 5** | Channel 0 (T0) |
| **`LED1_G`** | Green LED | `P6_4` | `&pwm_green` (`pwm6`) | `PIN_P6_4__UT6_T0_B` | **UTIMER 6** | Channel 0 (T0) |
| **`LED1_B`** | Blue LED | `P6_6` | `&pwm_blue` (`pwm7`) | `PIN_P6_6__UT7_T0_B` | **UTIMER 7** | Channel 0 (T0) |

> [!TIP]
> **Autonomous PWM Operation**: When using UTIMER hardware PWM, the waveform is generated autonomously by the 32-bit counter with 0% CPU overhead, avoiding any software timing jitter or interrupt latency.

---

## 7. Inter-Processor Communication (ARM MHUv2 & Hardware Semaphores)

The Alif Ensemble SoC provides dedicated ARM Message Handling Unit v2 (MHUv2) controllers and hardware semaphores for zero-copy, full-duplex inter-core communication between **Cortex-M55 HP** (`rtss_hp`) and **Cortex-M55 HE** (`rtss_he`):

| Peripheral Label | Memory Address | Direction / Role | IRQ (HP / HE) | Devicetree Node Alias |
| :--- | :--- | :--- | :--- | :--- |
| **`MHU0_R`** | `0x40080000` | Receiver (RX) | IRQ 41 / 41 | `rtsshemhu0r` |
| **`MHU0_S`** | `0x40090000` | Sender (TX) | IRQ 42 / 42 | `rtsshemhu0s` |
| **`MHU1_R`** | `0x400A0000` | Receiver (RX) | IRQ 43 / 43 | `rtsshemhu1r` |
| **`MHU1_S`** | `0x400B0000` | Sender (TX) | IRQ 44 / 44 | `rtsshemhu1s` |
| **`HWSEM0`** | `0x4902E000` | HW Mutex / Sem | IRQ 105 | `&hwsem0` |
| **`HWSEM1`** | `0x4902E010` | HW Mutex / Sem | IRQ 106 | `&hwsem1` |

---

## 8. CoreSight AP Topology & J-Link RTT Address Rules

| Core Target | CoreSight Access Port | J-Link V7.80c Device Target | DTCM Base Address | Default RTT Control Block (`_SEGGER_RTT`) |
| :--- | :--- | :--- | :--- | :--- |
| **Cortex-M55 HP** | **`AP[0]`** | **`AE722F80F55D5_HP`** | `0x20000000` (1024 KB) | **`0x20000410`** (Default 1KB UP / 16B DOWN)<br>**`0x20000840`** (2KB UP / 64B DOWN) |
| **Cortex-M55 HE** | **`AP[4]`** | `AE722F80F55D5_HE` *(Requires J-Link >= V8.86 for E8)* | `0x58000000` (256 KB) | `0x58000410` |

> [!IMPORTANT]
> **RTT Control Block Mismatch Warning**:
> If J-Link RTT Viewer connects with an address that does not match `_SEGGER_RTT` in the compiled ELF:
> 1. Terminal window will remain blank (no welcome banner).
> 2. Key input will fail with `WARNING: Sent 0 of 1 byte.`.
> Always verify the exact address via:
> ```bash
> nm build/zephyr/zephyr.elf | grep _SEGGER_RTT
> ```

---

## 9. Dual-Core Packaging & Multi-Binary Flashing

When running both **Cortex-M55 HP** and **Cortex-M55 HE** concurrently, the Alif Security Enclave (SES) bootloader extracts and executes both images from MRAM simultaneously using the Application Table of Contents (AToC):

| CPU Node | Flash Address | Boot Flag | Build Directory / Source | Default Staging Binary |
| :--- | :--- | :--- | :--- | :--- |
| **`HP_APP`** | `0x80200000` | `["boot"]` | `core_hp/build/zephyr/` | `zephyr.bin` |
| **`HE_APP`** | `0x80000000` | `["boot"]` | `core_he/build/zephyr/` | `zephyr_he.bin` *(Renamed)* |

### Critical Multi-Core Flashing Rule:
The Alif Security Toolkit (`alif_flash.py`) stages binaries in a single flat directory (`build/images/`). To prevent name collision errors (`HP_APP and HE_APP images have the same destination name zephyr.bin`), always copy or rename the worker binary to a unique name before invoking `west flash`:
```bash
cp -f core_he/build/zephyr/zephyr.bin core_he/build/zephyr/zephyr_he.bin
west flash -d core_hp/build -- --he-bin core_he/build/zephyr/zephyr_he.bin --he-mram-boot 1
```

---

## 10. Bosch BMI323 6-Axis IMU & I3C Bus Mapping

On the Alif E8 DevKit (Rev A0), motion sensing is provided by an onboard **Bosch BMI323** 6-axis IMU interfaced over the **Synopsys DesignWare I3C Bus**:

| Function | SoC Pin / Pad | Devicetree Node | Configuration & Notes |
| :--- | :--- | :--- | :--- |
| **I3C Clock (`SCL`)** | `P7_0` | `&i3c0` | Synopsys DW-I3C bus master clock |
| **I3C Data (`SDA`)** | `P7_1` | `&i3c0` | Synopsys DW-I3C bus master data |
| **IMU Interrupt (`INT1`)** | `P8_4` | `&gpio8` pin 4 | Data Ready / Motion interrupt (`GPIO_ACTIVE_HIGH`) |
| **I3C Bus Recovery** | `GPIO7` | `&gpio7` | Bus recovery GPIO fallback (`scl-gpios`, `sda-gpios`) |
| **Static / Dynamic Addr** | `0x69` / `0x0A` | `bmi323@...` | Static I2C address `0x69`, dynamic I3C address `0x0A` |

### Devicetree Node Pattern:
```dts
#include <zephyr/dt-bindings/pinctrl/ensemble-pinctrl.h>

&i3c0 {
	status = "okay";
	bmi323: bmi323@69000003b810431000 {
		compatible = "bosch,bmi323";
		reg = <0x69 0x03b8 0x10431000>;
		assigned-address = <0xA>;
		int-gpios = <&gpio8 4 GPIO_ACTIVE_HIGH>;
		status = "okay";
	};
};

&pinctrl_gpio8 {
	group0 {
		pinmux = <PIN_P8_4__GPIO>;
	};
};

&gpio8 {
	pinctrl-0 = <&pinctrl_gpio8>;
	pinctrl-names = "default";
	status = "okay";
};

&gpio7 { status = "okay"; };
```



