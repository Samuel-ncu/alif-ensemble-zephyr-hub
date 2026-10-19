# AI Agent & Assistant Guidelines

This repository serves as a knowledge and execution hub for AI Coding Agents (such as Google Antigravity, Claude Code, Cursor, Windsurf, and GitHub Copilot).

---

## 📂 Repository Architecture

```text
.
├── .agents/
│   └── skills/                  # Modular AI Skills & Runbooks
│       └── alif-ensemble-zephyr/
│           └── SKILL.md         # Alif Ensemble E8/E7 Zephyr RTOS complete runbook
├── .github/
│   └── FUNDING.yml              # Sponsor and donation configuration
├── docs/
│   └── HARDWARE_GPIO_REFERENCE.md # Verified DevKit Pinout, LEDs, SW2, PadCtrl, & UTIMER PWM
├── examples/
│   ├── 01_hello_world/          # Verified 01: UART Console Output (115200 baud)
│   ├── 02_blinky/               # Verified 02: GPIO LED1_G 1Hz Toggle
│   ├── 03_button/               # Verified 03: SW2 5-Way Joystick Hardware Interrupts
│   ├── 04_hello_rtt/            # Verified 04: SEGGER RTT Real-Time Logging (Zero SW4 Switch)
│   ├── 05_se_services/          # Verified 05: Secure Enclave (SES) Services & Hardware TRNG
│   ├── 06_timer_pwm/            # Verified 06: Hardware UTIMER 32-bit PWM & RGB Breathing
│   ├── 07_dual_core_ipc/        # Verified 07: Dual-core AMP IPC over ARM MHUv2 (HP Master + HE Worker)
│   ├── 08_sensor_imu/           # Verified 08: Bosch BMI323 6-Axis IMU over I3C & 3D Attitude Telemetry
│   ├── 09_ethosu_npu_ai/        # Verified 09: Arm Ethos-U55 NPU Edge AI Hardware Acceleration (TFLM)
│   └── 10_adc_temp/             # Verified 10: On-Chip 12-Bit SAR ADC & Internal Die Temp Sensor (TSENS)
├── scripts/
│   └── app-write-mram           # Automated serial unlocking flash wrapper
├── LICENSE                      # Apache License 2.0
├── AGENTS.md                    # Agent instructions & guidelines (this file)
└── README.md                    # Project overview & documentation
```

---

## ⚡ Golden Rules for Automated Alif Hardware Operations

When an AI Agent is tasked with creating, building, flashing, or debugging applications on Alif Ensemble processors, the following rules MUST be followed strictly:

### 1. Target Core Selection (M55-HP vs M55-HE)
* **The CoreSight AP Quirk**: On Alif Ensemble E8 silicon, J-Link V7.80c AP[4] has no ROM table (`Could not find core in Coresight setup`), causing J-Link to fail when connecting to `_HE`.
* **The Rule**: Always compile real-time logging and debug targets for **`alif_e8_dk/ae822fa0e5597xx0/rtss_hp`** and connect J-Link to **`AE722F80F55D5_HP`** (CoreSight `AP[0]`).

### 2. SEGGER RTT Control Block (`_SEGGER_RTT`) Address Calculation
* J-Link RTT Viewer requires the exact memory address of `_SEGGER_RTT` in DTCM RAM:
  * **Default Buffer Size (1024 UP, 16 DOWN)**: Address is **`0x20000410`**.
  * **Custom Buffer Size (e.g. 2048 UP, 64 DOWN)**: Address shifts to **`0x20000840`**.
* **Verification Command**:
  ```bash
  nm build/zephyr/zephyr.elf | grep _SEGGER_RTT
  ```
* **Failure Symptom**: If RTT Viewer connects to the wrong address, terminal receives 0 bytes, and typing keys results in:
  `WARNING: Sent 0 of 1 byte.`
* **Settings File**: J-Link RTT Viewer saves its address in `C:\Users\<user>\AppData\Roaming\SEGGER\JLinkRTTViewerSettings.ini` under `[JLink] RTTAddr`.

### 3. Automated USBIPD & Windows J-Link Driver Lifecycle
* **Exclusive Lock Rule**: Windows J-Link RTT Viewer holds an exclusive lock on the J-Link USB device. Before attaching USB to WSL2 for flashing, you MUST terminate or disconnect RTT Viewer:
  ```powershell
  Stop-Process -Name JLinkRTTViewer -Force -ErrorAction SilentlyContinue
  ```
* **WSL2 Persistence**: Ensure WSL2 distribution remains active while attaching USB (use a background keep-alive or `IsDaemon: true`).
* **Attachment Sequence**:
  ```powershell
  usbipd bind --busid 1-1 --force
  usbipd attach --wsl --busid 1-1
  ```
* **Mandatory Unbind Sequence**: Immediately after `west flash` finishes, release USB back to Windows native control with a 1-second delay for Windows PnP re-enumeration:
  ```powershell
  usbipd detach --busid 1-1
  Start-Sleep -Seconds 1
  usbipd unbind -b 1-1
  ```
  *(If `unbind` is called too quickly without sleep, Windows throws `ERROR_NO_SUCH_DEVINST` and leaves the driver stuck as `USBIP Shared Device`, causing J-Link to report: "No emulators connected via USB". If this occurs, run `pnputil /restart-device "USB\VID_1366&PID_1024*"` or `usbipd unbind -b 1-1`)*.

### 4. Synopsys DesignWare GPIO (`gpio_dw.c`) & Internal Pull-Up Rule
* **Driver Constraint**: `gpio_dw.c` rejects `GPIO_PULL_UP` and `GPIO_PULL_DOWN` with `-ENOTSUP` (-134).
* **Hardware Solution**: Configure internal pull-ups directly in the SoC Pad Control block:
  ```c
  volatile uint32_t *pad_ctrl = (volatile uint32_t *)0x42007000;
  for (int i = 0; i < 5; i++) {
      pad_ctrl[i] = 0x2B; /* READ_ENABLE | SCHMITT | PULL_UP | 4MA */
  }
  ```
* Always configure Zephyr pins with `GPIO_INPUT` only.

### 5. Hardware Universal Timer (UTIMER) PWM Pin Mapping
* On the E8 DevKit, `LEDRGB1` pins map directly to 32-bit hardware Universal Timers:
  * **`LED1_R` (P6_2)** ➔ **`UT5_T0_B`** (UTIMER5 Channel 0)
  * **`LED1_G` (P6_4)** ➔ **`UT6_T0_B`** (UTIMER6 Channel 0)
  * **`LED1_B` (P6_6)** ➔ **`UT7_T0_B`** (UTIMER7 Channel 0)
* **Devicetree Overlay Pattern**: Label the timer's child PWM nodes and disable `&aled0` in `gpio-leds`:
  ```dts
  &aled0 { status = "disabled"; };
  &utimer5 { status = "okay"; pwm_red: pwm5 { status = "okay"; }; };
  &utimer6 { status = "okay"; pwm_green: pwm6 { status = "okay"; }; };
  &utimer7 { status = "okay"; pwm_blue: pwm7 { status = "okay"; }; };
  ```

### 6. Zephyr Kconfig Rules for Alif Applications
* **SEGGER RTT Declaration**: Must include `config HAS_SEGGER_RTT default y` in the application's `Kconfig` to unlock RTT driver modules.
* **Console Dependencies**: When using `CONFIG_USE_SEGGER_RTT=y` and `CONFIG_RTT_CONSOLE=y`:
  * Always enable `CONFIG_PRINTK=y`, `CONFIG_SERIAL=y`, `CONFIG_CONSOLE=y`, and `CONFIG_STDOUT_CONSOLE=y`.
  * **Mandatory**: Must explicitly set `CONFIG_UART_CONSOLE=n` in `prj.conf`. If omitted, Zephyr defaults `CONFIG_UART_CONSOLE=y`, redirecting all stdout/printk to UART4 and leaving J-Link RTT Viewer completely silent with 0 bytes!
* **Ethos-U Guard**: If Ethos-U NPU is not used, set `CONFIG_ARM_ETHOS_U=n` in `prj.conf` to prevent CMake build errors regarding undefined log levels.

### 7. Physical Switch `SW4` State Machine
* **`SE` (Top Position)**: Mandatory for flashing (`west flash`). Bootloader ISP ROM on UART4.
* **`UART2` (Center Position)**: Application run mode, UART console, and J-Link RTT monitoring.

### 8. Multi-Core Flashing Flat-Directory Collision Rule
* **The Root Cause**: `alif_flash.py` copies all binaries (`zephyr.bin`, `--he-bin`, `--a32-bin`) into a single flat directory `toolkit/build/images/`.
* **The Constraint**: If both master and worker builds produce the default name `zephyr.bin`, `_validate_unique_image_names()` rejects the command:
  `HP_APP and HE_APP images have the same destination name zephyr.bin`
* **The Rule**: Always copy/rename the worker binary to a unique filename before passing to `west flash`:
  ```bash
  cp -f build_he/zephyr/zephyr.bin build_he/zephyr/zephyr_he.bin
  west flash -d build_hp -- --he-bin build_he/zephyr/zephyr_he.bin --he-mram-boot 1
  ```

### 9. ARM MHUv2 Driver Kconfig Symbol (`CONFIG_ARM_MHUV2`)
* In Zephyr RTOS on Alif Ensemble, the ARM Message Handling Unit driver symbol is:
  * `CONFIG_IPM=y`
  * `CONFIG_ARM_MHUV2=y` *(Do NOT use `CONFIG_IPM_MHUV2=y`, which is an undefined symbol)*.
* Hardware Mailbox addresses:
  * **Sender Register** (`rtsshe_rtsshp_mhu0_s`): `0x40090000` (IRQ 42 TX completion).
  * **Receiver Register** (`rtsshp_rtsshe_mhu0_r`): `0x40080000` (IRQ 41 RX interrupt).

### 10. Bosch BMI323 6-Axis IMU & Synopsys DesignWare I3C Bus
* **Devicetree Binding**: The onboard Bosch BMI323 connects to `i3c0` at `0x49034000`:
  * Static I2C address: `0x69`, PID: `0x03b8 0x10431000`.
  * Dynamic I3C assigned address: **`0xA`**.
  * Hardware interrupt: `int-gpios = <&gpio8 4 GPIO_ACTIVE_HIGH>;`.
  * Mandatory: `&gpio7 { status = "okay"; };` must be enabled in DTS for I3C bus recovery.
* **Driver Symbols**:
  * `CONFIG_I3C=y`, `CONFIG_I3C_INIT_RSTACT=y`, `CONFIG_I3C_USE_IBI=n`.
  * `CONFIG_SENSOR=y`, `CONFIG_BMI323=y` (automatically selects `CONFIG_BMI323_BUS_I3C=y`).

### 11. Arm Ethos-U55 NPU & TFLM Configuration Rules
* **Target Core vs NPU MAC Allocation**:
  * **Cortex-M55 HP (`rtss_hp`)**: Supports **256 MACs** (`-DETHOSU_TARGET_NPU_CONFIG=ethos-u55-256`, `CONFIG_ARM_ETHOS_U55_256=y`).
  * **Cortex-M55 HE (`rtss_he`)**: Restricted to **128 MACs** (`-DETHOSU_TARGET_NPU_CONFIG=ethos-u55-128`, `CONFIG_ARM_ETHOS_U55_128=y`).
* **Devicetree & Snippet**: Must apply `-S ethos-u55-enable` or overlay `&ethosu0 { status = "okay"; };`.
* **West Manifest Module**: `tflite-micro` belongs to Zephyr's `optional` group. In workspaces without all optional modules pre-cloned, pull it into `optional/modules/lib/tflite-micro` (revision `8d404de7`).

### 12. On-Chip SAR ADC & Internal Die Temperature Sensor (`TSENS`)
* **Controller**: `adc0` at `0x49028000` (`compatible = "alif,adc"`).
* **Required Kconfig Symbols**:
  * `CONFIG_ADC=y`
  * `CONFIG_USE_ALIF_HAL_ANALOG=y`
* **Internal Routing**:
  * Internal Silicon Die Temperature Sensor (`TSENS`) is connected to **Channel 6** (`ADC_CHANNEL_6`).
  * Resolution: 12-bit SAR ($0 \sim 4095$ counts) with 1.2V internal analog reference ($V_{\text{ref}} = 1200\text{ mV}$).
  * Digital-to-temperature conversion requires the calibrated lookup table (`tempData[402][2]` from Alif HAL `temperature.h`).



