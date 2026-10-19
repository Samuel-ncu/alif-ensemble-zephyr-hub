#!/usr/bin/env bash
#
# Copyright (c) 2026 Alif Semiconductor / Antigravity
# SPDX-License-Identifier: Apache-2.0
#
# Dual-Core IPC (M55-HP + M55-HE) Automated Build & Flash Script
# Target: Alif Ensemble E8 DevKit (AE822FA0E5597LS0)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_DIR="/home/user/alif/sdk-alif"
VENV_DIR="/home/user/alif_venv"

if [ -f "$VENV_DIR/bin/activate" ]; then
    source "$VENV_DIR/bin/activate"
fi

cd "$SDK_DIR"

echo "=========================================================="
echo " [1/3] Building Cortex-M55 HE Worker (core_he)..."
echo "=========================================================="
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_he \
    "$SCRIPT_DIR/core_he" \
    -d "$SCRIPT_DIR/core_he/build"

# Rename to unique filename to prevent flat-directory collision in SE tools (Golden Rule #8)
cp -f "$SCRIPT_DIR/core_he/build/zephyr/zephyr.bin" "$SCRIPT_DIR/core_he/build/zephyr/zephyr_he.bin"

echo "=========================================================="
echo " [2/3] Building Cortex-M55 HP Master (core_hp)..."
echo "=========================================================="
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp \
    "$SCRIPT_DIR/core_hp" \
    -d "$SCRIPT_DIR/core_hp/build"

echo "=========================================================="
echo " [3/3] Packaging & Flashing Dual-Core Application to MRAM..."
echo "=========================================================="
west flash -d "$SCRIPT_DIR/core_hp/build" -- \
    --he-bin "$SCRIPT_DIR/core_he/build/zephyr/zephyr_he.bin" \
    --he-mram-boot 1

echo ""
echo "=========================================================="
echo " [SUCCESS] Dual-core firmware successfully flashed to MRAM!"
echo " Next Steps:"
echo " 1. Flip physical switch SW4 to UART2 (Center position)."
echo " 2. Press RESET button on DevKit."
echo " 3. Launch J-Link RTT Viewer connecting to AE722F80F55D5_HP @ 0x20000410"
echo "=========================================================="

