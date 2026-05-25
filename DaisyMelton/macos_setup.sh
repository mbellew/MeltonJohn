#!/usr/bin/env bash
# Setup script for DaisyMelton development on macOS.
# Installs arduino-cli, the STM32/DaisyDuino toolchain, and dfu-util.
# Run once on a fresh Mac; safe to re-run (all steps are idempotent).

set -euo pipefail

# --- Homebrew ---
if ! command -v brew &>/dev/null; then
    echo "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

# --- arduino-cli ---
if ! command -v arduino-cli &>/dev/null; then
    echo "Installing arduino-cli..."
    brew install arduino-cli
fi

# --- dfu-util (USB DFU flashing) ---
if ! command -v dfu-util &>/dev/null; then
    echo "Installing dfu-util..."
    brew install dfu-util
fi

# --- Board manager URLs ---
echo "Configuring arduino-cli board manager URLs..."
arduino-cli config add board_manager.additional_urls \
    https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json \
    2>/dev/null || true  # already present → non-fatal

arduino-cli config init --overwrite 2>/dev/null || true
# Ensure both URLs are present (config add is idempotent for duplicates in newer cli versions)
arduino-cli config add board_manager.additional_urls \
    https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json \
    2>/dev/null || true

# --- Update index ---
echo "Updating board index..."
arduino-cli core update-index

# --- STM32 board package (includes DaisyDuino FQBN: STMicroelectronics:stm32:GenH7:pnum=DAISY_SEED) ---
if ! arduino-cli core list | grep -q "STMicroelectronics:stm32"; then
    echo "Installing STM32 board package..."
    arduino-cli core install STMicroelectronics:stm32
fi

# --- DaisyDuino library ---
if ! arduino-cli lib list | grep -q DaisyDuino; then
    echo "Installing DaisyDuino library..."
    arduino-cli lib install DaisyDuino
fi

echo ""
echo "Setup complete. Verify with:"
echo "  arduino-cli core list   # should show STMicroelectronics:stm32"
echo "  arduino-cli lib list    # should show DaisyDuino"
echo "  dfu-util --version"
echo ""
echo "To compile:"
echo "  arduino-cli compile --fqbn STMicroelectronics:stm32:GenH7:pnum=DAISY_SEED,usb=CDCgen \\"
echo "    --libraries ~/Documents/Arduino/libraries \\"
echo "    --output-dir \$(pwd)/build \$(pwd)"
echo ""
echo "To flash (put Daisy in DFU mode first: hold BOOT, press RESET):"
echo "  dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D build/DaisyMelton.ino.bin"
