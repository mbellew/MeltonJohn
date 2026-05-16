# DaisyMelton — Daisy Patch build notes

## Compile command

```sh
arduino-cli compile \
  --fqbn STMicroelectronics:stm32:GenH7:pnum=DAISY_SEED \
  --libraries /Users/matthew/Documents/Arduino/libraries \
  /Users/matthew/Projects/MeltonJohn/Daisy/DaisyMelton
```

Upload (once port is known):
```sh
arduino-cli upload \
  --fqbn STMicroelectronics:stm32:GenH7:pnum=DAISY_SEED \
  --port /dev/cu.usbmodemXXXX \
  /Users/matthew/Projects/MeltonJohn/Daisy/DaisyMelton
```

## Board setup

- Board package: `STMicroelectronics:stm32` (installed via `https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json`)
- DaisyDuino is a **library** (not a board package) — installed via Library Manager
- FQBN: `STMicroelectronics:stm32:GenH7:pnum=DAISY_SEED`

## Audio / FFT pipeline

- Sample rate: 8000 Hz (`AUDIO_SR_8K`)
- FFT size: 256 points → 32 ms window, ~31.25 Hz/bin
- Hop size: 128 samples → ~62 fps update rate
- Library: `fftsg.cpp` (pure C rdft, already in project — avoids CMSIS-DSP linker issues)
- Two channels processed independently, magnitudes summed (phase-safe)
- Frequency bands:
  - Bass: bins 1–8 → 31–250 Hz
  - Mid: bins 9–21 → 281–656 Hz
  - Treb: bins 22–80 → 688–2500 Hz

## Hardware TODOs (validate on real hardware before enabling)

- Confirm `DAISY_PATCH` is the correct enum for `DAISY.init()` — alternatives are `DAISY_PATCH_SM`, `DAISY_PATCH_INIT`
- Confirm OLED is SSD1309 (not SSD1306) and pin numbers match schematic (currently: clk=8, data=10, cs=7, dc=9)
- Confirm Serial1 maps to the MIDI TRS UART for DMX output
- DMX requires external MAX3485 (3.3V RS-485 driver) on the MIDI TRS jack
- Uncomment OLED init block in `setup_daisy()` once wiring confirmed

## Shared source files

These files are copied manually from `Daisy/src/` into this directory (Arduino IDE limitation — no subdirectory includes):

| File | Source |
|------|--------|
| `Patterns.h` / `Patterns.cpp` | `../src/` |
| `Renderer.h` / `Renderer.cpp` | `../src/` |
| `MultiLayerPatterns.cpp` | `../src/` |
| `fftsg.hpp` / `fftsg.cpp` | `../src/` |

When updating shared files, copy them again from `src/`. The Teensy copy in `/Teensy/` is read-only reference.
