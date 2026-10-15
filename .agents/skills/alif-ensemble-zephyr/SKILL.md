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
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_he samples/hello_world
```

#### Option B: Example 2 - Blinky (LED1_G Green LED Toggle)
```bash
cd ~/alif/sdk-alif/zephyr
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_he samples/basic/blinky
```

#### Option C: Example 3 - Button (SW2 5-Way Joystick Navigation & LED Control)
*Note: On the Alif E8 DevKit (Rev A0), user input is provided by `SW2` (a 5-way joystick connected to `LPGPIO_0..4`: Up, Down, Left, Right, Center Push), which acts as active-low input.*
```bash
cd ~/alif/sdk-alif/zephyr
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_he samples/basic/button
```
*(Or point to `examples/03_button` in your repository)*

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

---

## 6. Troubleshooting Checklist

| Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `[ERROR] /dev/ttyACM0 readSerial reporting disconnected` | **Multiple Access Conflict**: `picocom`, `minicom`, or another terminal emulator is running in background holding `/dev/ttyACM0`. Linux PySerial `select()` sees data but `read()` returns 0 bytes (stolen by picocom), throwing `SerialException` which the toolkit logs as "disconnected". | **1.** Use the enhanced `app-write-mram` wrapper (auto-runs `fuser -k -9 /dev/ttyACM0`).<br>**2.** Or manually execute `sudo fuser -k -9 /dev/ttyACM0` or `pkill -9 picocom`.<br>**3.** If USB dropped, ensure `usbipd attach --wsl Ubuntu --busid <BUSID> -a` is active. |
| `[ERROR] Target did not respond` | `SW4` switch is not in the **`SE` (Top)** position, or board is hung executing user code. | 1. Slide `SW4` to **`SE` (Top)**.<br>2. Press the hardware **`RESET`** button once to force SE ROM ISP mode.<br>3. Retry `west flash`. |
| `FATAL: cannot open /dev/ttyACM0` | Device disconnected, missing permissions, or bus reset. | Check Windows `usbipd list` and reattach with `usbipd attach --wsl Ubuntu --busid <BUSID> -a`. Check `sudo chmod 666 /dev/ttyACM0`. |
| Serial terminal outputs garbage (`3f0 3f ? ...`) | Baud rate mismatch. | SES bootloader uses `57600 baud`. Zephyr default console uses `115200 baud`. Adjust `picocom -b <baud>` to match the active transmitter. |
| Terminal prints SES table, then halts with no `Hello World!` | `SW4` is still in `SE` position. | Slide `SW4` to the **Center (UART2)** position. |
| `west flash` fails to connect or timeouts | Terminal (`picocom`) is holding open `/dev/ttyACM0`, or `SW4` is in `UART` position. | Run `pkill -9 picocom`, slide `SW4` back to **`SE`**, and retry `west flash`. |
| Flashing succeeds (100%), but SES reports `Bad Image` | `zephyr.bin` was not included in MRAM burn. | Ensure the `application_package.ds` fix in Section 4.4 is applied so `zephyr.bin` is burned at `0x80000000`. |
| Windows shows USB Device Descriptor Request Failed | Cable is plugged into the `MCU` port. | Move the cable to the **`PRG`** port. |


