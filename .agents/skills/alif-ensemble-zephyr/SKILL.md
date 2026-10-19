---
name: alif-ensemble-zephyr
description: >-
  Complete runbook and setup guide for configuring, building, flashing, and debugging
  Zephyr RTOS applications on Alif Semiconductor Ensemble (E8, E7, B1) development kits
  and AppKits using WSL2, USBIPD, and Alif Security Toolkit.
---

# Alif Semiconductor Ensemble Zephyr RTOS Development Guide

This skill provides a complete, battle-tested procedure for setting up an Alif Semiconductor Ensemble development environment (E8 / E7 / B1 SoC families) on Windows 11 with WSL2, building Zephyr RTOS applications, flashing them into MRAM via the Alif Security Toolkit, and debugging output over serial console.

---

## 1. Hardware Architecture & Switch Configurations

Understanding the hardware routing is critical to avoid missing serial output or flashing failures.

### Board Ports
- **`PRG` Port (USB Type-C)**: Connects to the on-board SEGGER J-Link debugger and CDC UART bridge. **Always connect your PC to this port** for both flashing and console monitoring.
- **`MCU` Port (USB Type-C)**: Direct connection to the SoC USB 2.0 PHY. Triggers USB descriptor errors on the host PC unless the flashed firmware includes a USB device driver. Do not use for flashing.

### Critical Hardware Switch: `SW4` (`UART SEL`)
On the Alif evaluation boards (such as the E8 AppKit/DevKit with `AE822FA0E5597LS0`), the J-Link CDC UART bridge (`/dev/ttyACM0`) is multiplexed via a **3-position slide switch `SW4`**:

| Position | Target Interface | Baud Rate | Description & Usage |
| :--- | :--- | :--- | :--- |
| **Top / `SE`** | **SEUART** | 57,600 | Connects to the Secure Enclave (SES). **Required for `west flash`** and displays the Secure Boot verification table. |
| **Center / `UART2`** | **UART2** | 115,200 | Connects to the **RTSS-HE (M55-HE)** or **APSS (A32)** application core. **Required to see Zephyr application output (`Hello World!`)**. |
| **Bottom / `UART4`** | **UART4** | 115,200 | Connects to the **RTSS-HP (M55-HP)** high-performance core. |

> [!IMPORTANT]
> - **During Flashing (`west flash`)**: `SW4` **MUST** be set to **`SE` (Top)**.
> - **During Application Execution (`picocom`)**: `SW4` **MUST** be switched to **`UART2` (Center)**. If left on `SE`, the terminal stops updating after the bootloader table finishes.

---

## 2. Windows 11 Host Setup (USB Passthrough)

### 2.1 Install USBIPD
In an Administrator PowerShell:
```powershell
winget install --interactive --exact dorssel.usbipd-win
```
*(Restart the PowerShell terminal after installation)*

### 2.2 Bind and Auto-Attach the Board to WSL2
1. Plug the USB cable into the **`PRG`** port.
2. List USB devices:
   ```powershell
   usbipd list
   ```
   Locate the device with `1366:1024 JLink CDC UART Port (COMx)`.
3. Bind the device (one-time setup):
   ```powershell
   usbipd bind --busid <YOUR_BUSID>
   ```
4. Attach with the auto-attach flag `-a` (ensures reconnects survive board resets):
   ```powershell
   usbipd attach --wsl Ubuntu --busid <YOUR_BUSID> -a
   ```

---

## 3. WSL2 Linux Environment Setup

### 3.1 Install System Dependencies
In Ubuntu (WSL2):
```bash
sudo apt update -y
sudo apt install -y --no-install-recommends \
    python3-dev python3-pip python3-venv \
    git wget xz-utils file make \
    python3-setuptools python3-wheel \
    ninja-build build-essential cmake libmagic1 picocom
```

### 3.2 User Serial Permissions
Add your user to the `dialout` group to access serial ports without `sudo`:
```bash
sudo usermod -a -G dialout $USER
```
*(Verify `/dev/ttyACM0` exists with `ls -l /dev/ttyACM*`)*

### 3.3 Python Virtual Environment & West Tools
```bash
python3 -m venv ~/alif_venv
source ~/alif_venv/bin/activate
pip install --upgrade pip
pip install west pyelftools fdt
```
Add auto-activation to `~/.bashrc`:
```bash
echo "source ~/alif_venv/bin/activate" >> ~/.bashrc
```

### 3.4 Alif Zephyr SDK & ARM Toolchain
```bash
mkdir -p ~/alif && cd ~/alif
west init -m https://github.com/alifsemi/sdk-alif.git --mr main sdk-alif
cd sdk-alif
west update
west zephyr-export
pip install -r zephyr/scripts/requirements.txt
west sdk install -t arm-zephyr-eabi
```

---

## 4. Alif Security Toolkit (SETOOLS) Setup & Patches

The Alif Security Toolkit prepares Table of Contents (TOC) packages and handles MRAM flashing.

### 4.1 Clone Repository
```bash
cd ~/alif
git clone https://github.com/alifsemi/alif_security_toolkit.git
cd alif_security_toolkit/toolkit
```

### 4.2 Set Environment Variables
Add to `~/.bashrc`:
```bash
export ALIF_SE_TOOLS_DIR=~/alif/alif_security_toolkit/toolkit
export PATH=$ALIF_SE_TOOLS_DIR:$PATH
```
Reload: `source ~/.bashrc`

### 4.3 Configure Target SoC
Run the configuration utility:
```bash
cd $ALIF_SE_TOOLS_DIR
python3 tools-config.py
```
Select the corresponding IC family (e.g., `AE822FA0E5597LS0`, Rev `A0`, COM Port `/dev/ttyACM0`).

### 4.4 Critical Wrapper Fix (`app-write-mram`)
Ensure executable wrapper scripts exist in `$ALIF_SE_TOOLS_DIR`:
1. `app-gen-toc`
2. `tools-config`
3. `app-write-mram`

> [!WARNING]
> **TOC vs Application Binary Omission Bug**:
> `app-gen-toc` generates `./application_package.ds` in the current build directory containing both `zephyr.bin` and `AppTocPackage.bin`. However, standard toolkit scripts may read `$ALIF_SE_TOOLS_DIR/bin/application_package.ds`, which defaults to flashing only the TOC package and omits `zephyr.bin`!
> 
> Ensure `app-write-mram` links build images, copies the active package descriptor, and auto-releases any locked serial port prior to invocation:
```bash
#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PYTHONPATH="$DIR:$DIR/utils:$DIR/isp:$DIR/alif:$DIR/cert:$PYTHONPATH"
cd "$DIR"

# Link image binaries so they can be read by app-write-mram
if [ -d "$DIR/build/images" ]; then
    for f in "$DIR/build/images"/*; do
        if [ -f "$f" ]; then
            base=$(basename "$f")
            ln -sf "$f" "$DIR/$base"
        fi
    done
fi

# Sync newly generated application_package.ds into bin/
if [ -f "$DIR/application_package.ds" ]; then
    cp -f "$DIR/application_package.ds" "$DIR/bin/application_package.ds"
fi

# Detect configured serial port
TARGET_PORT="/dev/ttyACM0"
if [ -f "$DIR/isp_config_data.cfg" ]; then
    CFG_PORT=$(awk '/^comport[[:space:]]+/ {print $2}' "$DIR/isp_config_data.cfg" | tr -d '\r')
    if [ -n "$CFG_PORT" ]; then
        TARGET_PORT="$CFG_PORT"
    fi
fi

# Check if any background process (e.g. picocom, minicom) is locking the serial port
if [ -e "$TARGET_PORT" ]; then
    LOCKED_PIDS=$(fuser "$TARGET_PORT" 2>/dev/null)
    if [ -n "$LOCKED_PIDS" ]; then
        echo "[INFO] Detected conflicting process(es) holding $TARGET_PORT: $LOCKED_PIDS"
        echo "[INFO] Automatically releasing $TARGET_PORT to prevent 'readSerial reporting disconnected'..."
        fuser -k -9 "$TARGET_PORT" 2>/dev/null || true
        sleep 0.5
    fi
else
    echo "[WARNING] $TARGET_PORT not found. Ensure USB device is attached via: usbipd attach --wsl Ubuntu --busid <BUSID> -a"
fi

exec python3 "$DIR/app-write-mram.py" "$@"
```
Make executable: `chmod +x app-gen-toc app-write-mram tools-config`

### 4.5 Synopsys DesignWare GPIO (`LPGPIO`) & Peripheral Constraints

> [!CAUTION]
> **Synopsys DesignWare GPIO Driver (`gpio_dw.c`) Software Constraint**:
> The `LPGPIO` driver strictly prohibits software pull-up/pull-down flags (`flags & (GPIO_PULL_UP | GPIO_PULL_DOWN)` returns `-ENOTSUP` (-134)).
> 1. **Rule**: ALWAYS configure input pins using `GPIO_INPUT` only:
>    ```c
>    /* Correct: Standard input. Hardware pull-ups are set in pinctrl */
>    ret = gpio_pin_configure(button.port, pin, GPIO_INPUT);
>    /* Forbidden: Passing GPIO_PULL_UP returns -ENOTSUP and breaks interrupts */
>    ```
> 2. **Interrupt Configuration**: Set `GPIO_INT_EDGE_BOTH` or `GPIO_INT_EDGE_FALLING` AFTER the pin has been successfully set to `GPIO_INPUT`.
> 3. **LED Polarity**: `LED1_G` (`GPIO6_4`), `LED1_R` (`GPIO6_2`), and `LED1_B` (`GPIO6_6`) are verified **Active-High** circuits (`1` = ON, `0` = OFF).
> 
> See the complete verified hardware mappings in [HARDWARE_GPIO_REFERENCE.md](references/HARDWARE_GPIO_REFERENCE.md).

---

## 5. Build, Flash, and Verify Workflow

### 5.1 Compile the Application
Target board qualifiers:
- **E8 RTSS-HE**: `alif_e8_dk/ae822fa0e5597xx0/rtss_he`
- **E8 RTSS-HP**: `alif_e8_dk/ae822fa0e5597xx0/rtss_hp`
- **E7 RTSS-HE**: `alif_e7_dk/ae722f80f55d5xx/rtss_he`

#### Option A: Example 1 - Hello World (UART Console Output)
```bash
cd ~/alif/sdk-alif/zephyr
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_he examples/01_hello_world
```

#### Option B: Example 2 - Blinky (LED1_G Green LED Toggle)
```bash
cd ~/alif/sdk-alif/zephyr
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_he examples/02_blinky
```

#### Option C: Example 3 - Button (SW2 5-Way Joystick Navigation & LED Control)
*Note: On the Alif E8 DevKit (Rev A0), user input is provided by `SW2` (a 5-way joystick connected to `LPGPIO_0..4`: Up, Down, Left, Right, Center Push), which acts as active-low input.*
```bash
cd ~/alif/sdk-alif/zephyr
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_he examples/03_button
```

#### Option D: Example 4 - SEGGER RTT (High-Speed Debug Logging)
*Note: RTT streams real-time logs directly through J-Link RAM ring buffers (`_SEGGER_RTT` in DTCM).*
```bash
cd ~/alif/sdk-alif/zephyr
# Build for RTSS-HP (Cortex-M55 HP) for native J-Link V7.80c AP[0] support
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp examples/04_hello_rtt
```

#### Option E: Example 5 - Secure Enclave (SES) Services & Hardware TRNG
*Note: Communicates with on-chip Secure Enclave over MHUv2 mailbox to query chip UID, OTP, LCS, and live hardware TRNG entropy.*
```bash
cd ~/alif/sdk-alif/zephyr
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp examples/05_se_services
```

#### Option F: Example 6 - Hardware Universal Timer (UTIMER) PWM & RGB Breathing
*Note: 32-bit hardware PWM driving onboard RGB LEDs (P6_2, P6_4, P6_6) via UTIMER 5, 6, 7 with gamma correction and RTT telemetry.*
```bash
cd ~/alif/sdk-alif/zephyr
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp /mnt/c/Users/test/.gemini/antigravity/scratch/alif_vscode-template/examples/06_timer_pwm
```

### 5.2 Flash to MRAM
1. Verify `SW4` switch is in the **`SE` (Top)** position.
2. Ensure no serial terminals are open (`pkill picocom`). *(The enhanced `app-write-mram` wrapper will also auto-release any lingering locks).*
3. Run:
   ```bash
   west flash
   ```
4. Confirm terminal reports:
   ```text
   ...
   Burn into MRAM: [100%]
   Write finish!
   ```

### 5.3 Monitor and Verify Output

1. **Verify Blinky (LED)**:
   - Immediately after flashing finishes, look at the development board.
   - The on-board Green LED (**`LED1_G`**, mapped to `GPIO6_4`) will begin blinking at 1 Hz (500 ms ON / 500 ms OFF).

2. **Verify Button / Joystick (`SW2`)**:
   - On boot, the LED performs a 3-blink startup self-test, then stays OFF.
   - Tilt `SW2` in any direction (Up, Down, Left, Right) or press Center Push to toggle the Green LED.
   - Slide `SW4` to **`UART2` (Center)** and open `picocom /dev/ttyACM0 -b 115200` to see real-time button IRQ events.

3. **Verify Serial Console (Optional)**:
   - Slide `SW4` switch to the **`UART2` (Center)** position.
   - Open terminal:
     ```bash
     picocom /dev/ttyACM0 -b 115200
     ```
     *(Note: If `app.overlay` was used to override baud rate to 57600, use `-b 57600`)*
   - Press the **RESET** button on the board.
   - Confirm output stream:
     - For **Hello World**: `*** Hello World! alif_e8_dk ***`
     - For **Blinky**: `[Blinky #1] Green LED: ON`, `[Blinky #2] Green LED: OFF`
     - For **Button**: Press `SW2` directions to see `[SW2 IRQ] Direction triggered! LED: ON`
   - *(To exit picocom: press `Ctrl+A`, then `Ctrl+X`)*

4. **Verify SEGGER RTT Real-Time Logs**:
   - Slide `SW4` to **`UART2` (Center)** and press **`RESET`** on the board.
   - On boot, the RGB LED will flash Blue 3 times, then begin a 1 Hz Cyan heartbeat (`LED1_G` + `LED1_B`).
   - Open **J-Link RTT Viewer** on Windows:
     - **Specify Target Device**: `AE722F80F55D5_HP`
     - **Target Interface & Speed**: `JTAG`, `4000 kHz`
     - **RTT Control Block**: Select `Address`, enter **`0x20000410`**
   - Confirm immediate ANSI formatted startup banner and 1 Hz uptime heartbeat telemetry.
   - Type input in the bottom bar and hit Enter to observe instant key echo and purple LED flashes.

5. **Verify Example 05 (Secure Enclave Services & Hardware TRNG)**:
   - Slide `SW4` to **`UART2` (Center)** and press **`RESET`**.
   - Open J-Link RTT Viewer with Address **`0x20000410`**.
   - Verify SE handshake (`SUCCESS`), SE firmware revision, Silicon Part Number (`AE822FA0E5597LS0`), Lifecycle State (LCS `0x01` / `DM`), and live 2s TRNG hardware entropy stream.
   - Test interactive key commands in Input bar: `'r'` (TRNG), `'h'` (Heartbeat), `'t'` (TOC).

6. **Verify Example 06 (Hardware Universal Timer PWM & RGB Breathing)**:
   - Slide `SW4` to **`UART2` (Center)** and press **`RESET`**.
   - Observe onboard `LEDRGB1` execute startup 3-color self-test (Red -> Green -> Blue), then enter smooth 360° HSV Rainbow Color Wheel breathing with 0% CPU load.
   - Open J-Link RTT Viewer with Address **`0x20000840`** (or address from `nm zephyr.elf | grep _SEGGER_RTT`).
   - Observe live PWM duty telemetry (`R: %`, `G: %`, `B: %`).
   - Test interactive commands: `'m'` (Cycle animation mode), `'+'` / `'-'` (Speed), `' '` (Pause), `'r'`, `'g'`, `'b'`, `'c'`, `'y'`, `'w'`, `'o'` (Direct color override).

7. **Verify Example 07 (Dual-Core AMP IPC over ARM MHUv2)**:
   - Slide `SW4` to **`UART2` (Center)** and press **`RESET`**.
   - Observe onboard `LEDRGB1`:
     - Blue LED flashes when HP Master sends Ping packets.
     - Green LED pulses when HE Worker receives and replies with Pong.
   - Open J-Link RTT Viewer with Address **`0x20000410`**:
     - Live 3-second heartbeat telemetry: `[Heartbeat #001] Pong OK | RTT Latency: 3 us | Remote RX Count: 1`.
     - Test interactive commands in Input bar:
       - `'p'` : Single Ping with round-trip latency ($\mu$s).
       - `'l'` : Remote toggle M55-HE Green LED (`P6_4`).
       - `'b'` : 100-packet Burst Benchmark (measures min/avg/max latency & throughput >250k trans/sec).
       - `'s'` : Query remote worker metrics.
       - `'a'` : Pause/Resume periodic auto-heartbeat.

---

## 6. Troubleshooting Checklist

| Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `HP_APP and HE_APP images have the same destination name zephyr.bin` | `alif_flash.py` copies all images into flat `toolkit/build/images/` directory; both builds default to `zephyr.bin`. | Copy or rename the worker binary to a unique name (e.g. `zephyr_he.bin`) before passing to `west flash --he-bin ...`. |
| `warning: attempt to assign the value 'y' to undefined symbol IPM_MHUV2` | In Zephyr RTOS on Alif, the MHUv2 driver symbol is `CONFIG_ARM_MHUV2=y`. | Enable `CONFIG_IPM=y` and `CONFIG_ARM_MHUV2=y` in `prj.conf` (not `IPM_MHUV2`). |
| `gpio_pin_configure()` returns `-ENOTSUP` (-134), or button IRQ fails with `-EINVAL` (-22) | `GPIO_PULL_UP` or `GPIO_PULL_DOWN` was requested on Synopsys DesignWare `LPGPIO`. | Pass `GPIO_INPUT` only without pull flags. Pull-ups are hardware-managed via `pinctrl_lpgpio` or SoC Pad Control at `0x42007000` (`0x2B`). See [HARDWARE_GPIO_REFERENCE.md](references/HARDWARE_GPIO_REFERENCE.md). |
| `[ERROR] /dev/ttyACM0 readSerial reporting disconnected` | **Multiple Access Conflict**: `picocom`, `minicom`, or another terminal emulator is running in background holding `/dev/ttyACM0`. Linux PySerial `select()` sees data but `read()` returns 0 bytes (stolen by picocom), throwing `SerialException` which the toolkit logs as "disconnected". | **1.** Use the enhanced `app-write-mram` wrapper (auto-runs `fuser -k -9 /dev/ttyACM0`).<br>**2.** Or manually execute `sudo fuser -k -9 /dev/ttyACM0` or `pkill -9 picocom`.<br>**3.** If USB dropped, ensure `usbipd attach --wsl Ubuntu --busid <BUSID> -a` is active. |
| `[ERROR] Target did not respond` | `SW4` switch is not in the **`SE` (Top)** position, or board is hung executing user code. | 1. Slide `SW4` to **`SE` (Top)**.<br>2. Press the hardware **`RESET`** button once to force SE ROM ISP mode.<br>3. Retry `west flash`. |
| `FATAL: cannot open /dev/ttyACM0` | Device disconnected, missing permissions, or bus reset. | Check Windows `usbipd list` and reattach with `usbipd attach --wsl Ubuntu --busid <BUSID> -a`. Check `sudo chmod 666 /dev/ttyACM0`. |
| Serial terminal outputs garbage (`3f0 3f ? ...`) | Baud rate mismatch. | SES bootloader uses `57600 baud`. Zephyr default console uses `115200 baud`. Adjust `picocom -b <baud>` to match the active transmitter. |
| Terminal prints SES table, then halts with no `Hello World!` | `SW4` is still in `SE` position. | Slide `SW4` to the **Center (UART2)** position. |
| `west flash` fails to connect or timeouts | Terminal (`picocom`) is holding open `/dev/ttyACM0`, or `SW4` is in `UART` position. | Run `pkill -9 picocom`, slide `SW4` back to **`SE`**, and retry `west flash`. |
| Flashing succeeds (100%), but SES reports `Bad Image` | `zephyr.bin` was not included in MRAM burn. | Ensure the `application_package.ds` fix in Section 4.4 is applied so `zephyr.bin` is burned at `0x80000000`. |
| Windows shows USB Device Descriptor Request Failed | Cable is plugged into the `MCU` port. | Move the cable to the **`PRG`** port. |
| RTT Viewer shows `Could not find core in Coresight setup` (`AP[4]: Skipped. No ROM table`) | On Alif E8 (`AE822`), J-Link V7.80c profile `AE722F80F55D5_HE` maps to `AP[4]` which has no ROM table. | Target `rtss_hp` and select `AE722F80F55D5_HP` in J-Link (maps to CoreSight `AP[0]`). If using J-Link >= V8.86, select `AE822FA0E5597LS0_M55_HE`. |
| RTT Viewer outputs high-ASCII garbage & `Terminal change character (IAC)` | RTT control block address was entered as system global AXI address (`0x58800410`). | Use CPU DTCM local address **`0x20000410`**. J-Link connects through CoreSight AHB-AP to the core's private memory map. |
| RTT Viewer shows 0 Bytes / `Connection lost` / No logs | `CONFIG_UART_CONSOLE=y` overrode `stdout` hook in Zephyr console driver. | Set `CONFIG_UART_CONSOLE=n` and `CONFIG_RTT_CONSOLE=y` in `prj.conf` so `printf()` outputs directly to RTT ring buffer. |
| RTT Viewer shows blank / Keypress shows `WARNING: Sent 0 of 1 byte.` | Address mismatch. RTT Viewer is connected to `0x20000410` but the binary placed `_SEGGER_RTT` at `0x20000840` (due to custom buffer sizes), or vice versa. | Run `nm zephyr.elf \| grep _SEGGER_RTT` in WSL to check the exact symbol address. In RTT Viewer, click `File` -> `Disconnect`, then `File` -> `Connect`, and update the Address. Settings are saved in `%APPDATA%\SEGGER\JLinkRTTViewerSettings.ini`. |
| `usbipd: Attach Request failed - Device busy (exported)` | J-Link RTT Viewer is running in Windows holding an exclusive USB handle. | Close/disconnect J-Link RTT Viewer (`Stop-Process -Name JLinkRTTViewer -Force`) before attaching to WSL. |
| `usbipd: error: There is no WSL 2 distribution running` | WSL2 VM automatically shut down when idle. | Run a persistent background process in WSL2 (e.g. `wsl -d Ubuntu sleep infinity` as daemon) before attaching USB. |
| `CMake Error: if given arguments: "EQUAL" "0" Unknown arguments` in `hal_ethos_u` | `CONFIG_ARM_ETHOS_U_LOG_LEVEL` evaluated to empty string because `STDOUT_CONSOLE` was disabled. | Set `CONFIG_ARM_ETHOS_U=n` in `prj.conf` if NPU is not used, and ensure `CONFIG_SERIAL=y` and `CONFIG_CONSOLE=y` are present. |
| `Kconfig warning: USE_SEGGER_RTT assigned 'y' but got 'n'` | `HAS_SEGGER_RTT` dependency was unsatisfied because the board does not select it by default. | Add `config HAS_SEGGER_RTT default y` in the application's `Kconfig`. |
| `devicetree error: undefined node label 'pwm5'` | In `e1.dtsi`, `pwm5` is a node name under `utimer5`, not a label. | Label it in `app.overlay`: `&utimer5 { pwm_red: pwm5 { status = "okay"; }; };`. |
| Windows J-Link reports `No emulators connected via USB` after flashing | USB device `1-1` was left bound or attached to WSL2. | Run `usbipd detach --busid 1-1; usbipd unbind -b 1-1; usbipd list` to restore device state to `Not shared`. |




