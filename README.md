# ⚡ Alif Ensemble Zephyr Hub

> **Battle-tested Zephyr RTOS runbooks, AI agent skills, and hands-on examples for Alif Semiconductor Ensemble (E8/E7) on WSL2.**

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Zephyr](https://img.shields.io/badge/RTOS-Zephyr-purple.svg)](https://zephyrproject.org/)
[![Alif Semiconductor](https://img.shields.io/badge/Silicon-Alif_Ensemble_E8%2FE7-orange.svg)](https://alifsemi.com/)
[![GitHub Sponsors](https://img.shields.io/badge/Sponsor-GitHub-ea4aaa?logo=github&logoColor=white)](#-support--donate)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-Donate-F16061.svg?logo=ko-fi&logoColor=white)](https://ko-fi.com/S7R228TRQX)
[![PayPal](https://img.shields.io/badge/Donate-PayPal-00457C.svg?logo=paypal&logoColor=white)](https://www.paypal.me/)

Welcome to **Alif Ensemble Zephyr Hub**! 

This repository provides production-ready, deterministic **AI Agent Skills (`SKILL.md`)**, **Agent Execution Guidelines (`AGENTS.md`)**, and hands-on **Zephyr RTOS example projects** migrated from legacy Keil/CMSIS workflows to modern Linux/WSL2 development. It equips developers and AI coding assistants (Google Antigravity, Claude Code, Cursor, Windsurf) with zero-hallucination hardware runbooks.

---

## 🎯 Practical Examples Catalog (Keil to Zephyr Migration)

| Example | Keil Equivalent | Description | Target Core | Status |
| :--- | :--- | :--- | :--- | :--- |
| **[`01_hello_world`](examples/01_hello_world)** | `hello` | UART2 console output, Secure Enclave (SES) boot verification table, and loop logging. | M55-HE / M55-HP | ✅ Verified |
| **[`02_blinky`](examples/02_blinky)** | `blinky` | Controls on-board Green LED (`LED1_G`, `GPIO6_4`) at 1 Hz with status logging. | M55-HE | ✅ Verified |
| **[`03_button`](examples/03_button)** | *(Hardware IRQ)* | SW2 5-way navigation joystick (`LPGPIO_0..4`) with hardware interrupts & active-low polling controlling LED. | M55-HE | ✅ Verified |
| **[`04_hello_rtt`](examples/04_hello_rtt)** | `hello_rtt` | SEGGER RTT real-time logging without toggling physical `SW4` UART switches. | M55-HP / M55-HE | ✅ Verified |
| **[`05_se_services`](examples/05_se_services)** | *(SES Security)* | Secure Enclave (SES) communication: reads OTP, LCS lifecycle, part UID & live hardware TRNG entropy over MHUv2. | M55-HP / M55-HE | ✅ Verified |
| **[`06_timer_pwm`](examples/06_timer_pwm)** | *(Hardware PWM)* | Hardware Universal Timer (UTIMER) & RGB LED smooth PWM color breathing. | M55-HP / M55-HE | ✅ Verified |
| **[`07_dual_core_ipc`](examples/07_dual_core_ipc)** | *(CMSIS-RTOS)* | Asymmetric Multiprocessing (AMP) with M55-HE & M55-HP full-duplex Mailbox/IPM communication & latency benchmark. | Dual M55 (HP+HE) | ✅ Verified |
| **[`08_sensor_imu`](examples/08_sensor_imu)** | *(I3C / IMU)* | Bosch BMI323 6-axis IMU over I3C, Cortex-M55 FPU 3D attitude estimation & SEGGER RTT horizon telemetry. | M55-HP | ✅ Verified |


---

## ⚡ Battle-Tested Firmware & Toolchain Fixes

This hub solves notoriously frustrating hardware quirks out-of-the-box:
1. **Automated Serial Conflict Resolver**: Automatically detects and terminates lingering background terminal sessions (`picocom`, `minicom`, or stale python instances) that cause the cryptic `[ERROR] /dev/ttyACM0 readSerial reporting disconnected` failure during `west flash`.
2. **Dual-Image MRAM Flash Fix**: Seamlessly links build artifacts and synchronizes `application_package.ds` so both `zephyr.bin` (at `0x80000000`) and `AppTocPackage.bin` (TOC) are burned in a single step without silent omission.
3. **Multi-Core Staging Collision Fix**: Automatically separates worker and master binaries to prevent flat-directory overwrite collisions in `alif_flash.py`.
4. **5-Way Navigation Joystick Support**: Full hardware mapping for Alif DevKit's `SW2` joystick across all 5 directions (`LPGPIO_0` to `LPGPIO_4`) with glitch-free active-low falling-edge IRQs.
5. **Universal Timer (UTIMER) 32-Bit PWM**: Zero CPU overhead RGB LED breathing & color cycling.
6. **I3C High-Speed Sensor Telemetry**: Autonomous dynamic address assignment (`0x0A`) and FPU attitude calculation for Bosch BMI323.

---

## 📂 Repository Layout

```text
├── .agents/
│   └── skills/
│       └── alif-ensemble-zephyr/
│           └── SKILL.md       # Complete setup, build, flash & debug runbook
├── .github/
│   └── FUNDING.yml            # GitHub Sponsor & Ko-fi config
├── docs/
│   └── HARDWARE_GPIO_REFERENCE.md # Verified DevKit Pinout, LEDs, SW2, PWM, Dual-Core & I3C rules
├── examples/
│   ├── 01_hello_world/        # Verified UART console output
│   ├── 02_blinky/             # Verified 1Hz GPIO LED toggle
│   ├── 03_button/             # Verified SW2 5-way joystick interrupt & LED control
│   ├── 04_hello_rtt/          # SEGGER RTT real-time logging (zero SW4 switch)
│   ├── 05_se_services/        # Secure Enclave services & hardware TRNG
│   ├── 06_timer_pwm/          # Hardware UTIMER 32-bit PWM & RGB Breathing
│   ├── 07_dual_core_ipc/      # Dual-core AMP IPC over ARM MHUv2 (HP Master + HE Worker)
│   └── 08_sensor_imu/         # Bosch BMI323 6-axis IMU over I3C & 3D Attitude Telemetry
├── scripts/
│   └── app-write-mram         # Automated port-unlocking & package sync wrapper
├── AGENTS.md                  # System prompt & instruction entry point for LLMs
├── LICENSE                    # Apache License 2.0
└── README.md                  # Documentation and catalog (this file)
```

---

## 🛠️ Contributing

Contributions are welcome! If you have validated a complex hardware setup, embedded RTOS pipeline, or toolchain workflow:

1. Fork this repository.
2. Create a new skill directory: `.agents/skills/<your-skill-name>/`.
3. Provide a standard `SKILL.md` with:
   - YAML frontmatter (`name`, `description`).
   - Hardware requirements and pin/switch settings.
   - Host environment preparation.
   - Build, flash, and verify commands.
   - Practical troubleshooting checklist.
4. Submit a Pull Request!

---

## ☕ Support & Donate

If these skills saved you hours of debugging, toolchain setup, and firmware headache, consider supporting the maintenance and acquisition of new hardware dev kits!

### Ways to Support:

<p align="center">
  <a href="https://ko-fi.com/S7R228TRQX">
    <img src="https://storage.ko-fi.com/cdn/kofi2.png?v=3" alt="Support Samuel (Shu-Ming) Liu on Ko-fi" height="42">
  </a>
  <br>
  <sub>☕ <b>Support Samuel (Shu-Ming) Liu on Ko-fi</b></sub>
</p>

| Platform | Link / Badge | Description |
| :--- | :--- | :--- |
| **Ko-fi** ⭐ | [![Ko-fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/S7R228TRQX) | **0% Platform Fee** for one-off tips. Supports Credit Card & PayPal. |
| **PayPal** | [![PayPal](https://img.shields.io/badge/PayPal-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://www.paypal.me/YOUR_PAYPAL_USERNAME) | Direct support via PayPal.Me link. |
| **GitHub Sponsors** | [![GitHub Sponsors](https://img.shields.io/badge/Sponsor-EA4AAA?style=for-the-badge&logo=github&logoColor=white)](https://github.com/sponsors/YOUR_GITHUB_USERNAME) | Native GitHub sponsorship (supports one-time and recurring). |

---

## 👤 Author & Maintainer

- **Samuel (Shu-Ming) Liu**
  - Ko-fi: [ko-fi.com/S7R228TRQX](https://ko-fi.com/S7R228TRQX)

---

## 📄 License

This repository is licensed under the **[Apache License 2.0](LICENSE)**.

```text
Copyright 2026 Samuel (Shu-Ming) Liu

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0
```
