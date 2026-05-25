# DaisyMelton — native libDaisy build

Bare-metal firmware for the Electro-Smith Daisy Patch. No Arduino, no
DaisyDuino — straight `arm-none-eabi-gcc` + libDaisy via `make`.

## Prerequisites

| Tool | Location | Notes |
|------|----------|-------|
| `arm-none-eabi-gcc` | `~/Library/Arduino15/packages/STMicroelectronics/tools/xpack-arm-none-eabi-gcc/14.2.1-1.1/bin` | Reused from the STMicroelectronics Arduino package; the `Makefile` sets `GCC_PATH` to point at it. To use a different one (e.g. `brew install --cask gcc-arm-embedded`), override `GCC_PATH` on the make command line. |
| `dfu-util` | `brew install dfu-util` | Already present. |
| `make` | `/usr/bin/make` | System make works fine. |
| `libDaisy` | `~/Projects/electrosmith/libDaisy` | One clone shared across Daisy projects; override `LIBDAISY_DIR` if you keep it elsewhere. |

### One-time libDaisy install

```sh
mkdir -p ~/Projects/electrosmith && cd ~/Projects/electrosmith
git clone --recursive https://github.com/electro-smith/libDaisy.git
cd libDaisy
PATH="$HOME/Library/Arduino15/packages/STMicroelectronics/tools/xpack-arm-none-eabi-gcc/14.2.1-1.1/bin:$PATH" make
```

That builds `libDaisy/build/libdaisy.a`, which our `Makefile` links against.

## Build

```sh
cd Daisy/DaisyMelton
make            # → build/DaisyMelton.{elf,bin,hex}
```

Or VS Code: **Cmd+Shift+B** → "Compile DaisyMelton".

## Flash

We target `APP_TYPE = BOOT_SRAM`: the firmware lives in QSPI external
flash and gets copied into SRAM by the Electrosmith bootloader on every
power-up. This gives ~480 KB of code budget instead of the 128 KB internal
flash limit; the trade-off is a one-time bootloader install and a ~3 s
delay at boot while the bootloader waits for a DFU connection.

### One-time: install the bootloader

Hold **BOOT**, tap **RESET** to enter the STM32 ROM bootloader, then:

```sh
make program-boot
```

This flashes the Electrosmith bootloader into internal flash.

### Every time: flash the app

After the bootloader is installed, every power-on/RESET spends ~3 s in
the Electrosmith bootloader before jumping to the app. During that
window:

```sh
make program-dfu
```

flashes `build/DaisyMelton.bin` to QSPI. The bootloader then jumps into
it. If `dfu-util` can't find the device, tap **RESET** and re-run the
command quickly — you have ~3 s.

> Both the stock STM32 ROM bootloader and the Electrosmith bootloader
> enumerate as USB PID `0483:df11`, so you can't tell them apart from
> `lsusb`. The difference: the Electrosmith bootloader exposes an extra
> DFU alt-setting for QSPI flash. If `program-dfu` errors out with
> *"Last page at 0x9006xxxx is not writeable"*, you're talking to the
> stock ROM bootloader — run `program-boot` first.

## Architecture quick-reference

- **Audio**: 8 kHz stereo via the Patch's WM8731 codec (`SAI_8KHZ`).
  `AudioCallback` writes into a power-of-2 circular buffer at interrupt
  priority; the main loop snapshots and runs FFT at HOP_SIZE intervals.
- **FFT**: 256-point real DFT (Ooura `fftsg` — pure C, no CMSIS-DSP
  dependency). 32 ms window, ~31.25 Hz/bin, two channels processed
  independently and magnitudes summed (phase-safe).
- **Bands**: bass 31–250 Hz, mid 281–656 Hz, treb 688–2500 Hz.
- **Renderer**: shared `Renderer.cpp` (also drives the Linux build).
- **Output**: 20-channel DMX-512 on the MIDI TRS jack (`USART_1`,
  pins D13/D14) through an external MAX3485. The DMX BREAK is produced
  by re-initialising the UART at 100 kbaud 8E1 and sending a zero,
  which holds the line low for ~100 µs.
- **OLED**: built-in 128×64 SSD1309 (SPI), driven via `patch.display`.
  Shows a vertical raw-level VU bar plus three per-band horizontal bars.

## Files

| File | Role |
|------|------|
| `Makefile` | Top-level build; sets `APP_TYPE = BOOT_SRAM` and includes `$(LIBDAISY_DIR)/core/Makefile`. |
| `main.cpp` | `int main()`, audio callback, spectrum analyzer, DMX output, OLED VU meter. |
| `config.h` | Platform/feature flags. Sets `PLATFORM_DAISY`, `IMAGE_SIZE=20`, FFT/band parameters. |
| `pixeltypes.h` | Standalone `CRGB`/`CHSV` (FastLED is not available on Daisy). |
| `Patterns.{h,cpp}`, `Renderer.{h,cpp}`, `MultiLayerPatterns.cpp`, `fftsg.{hpp,cpp}`, `beat_data.{h,cpp}` | Live only in `Daisy/src/` and are compiled in-place via `vpath` (no copy step). |

Desktop-only sources (`BeatDetect`, `MidiMix`, `MidiPatterns`, `PCM`,
plus the desktop `main.cpp` and `config.h`) live in `Daisy/DesktopApp/`
— not visible to or referenced by this build at all.
