# TeensyMelton — Teensy 4.0 build notes

## Compile command

```sh
arduino-cli compile \
  --fqbn teensy:avr:teensy40 \
  --libraries /Users/matthew/Documents/Arduino/libraries \
  /Users/matthew/Projects/MeltonJohn/Daisy/TeensyMelton
```

Upload (once port is known):
```sh
arduino-cli upload \
  --fqbn teensy:avr:teensy40 \
  --port /dev/cu.usbmodemXXXX \
  /Users/matthew/Projects/MeltonJohn/Daisy/TeensyMelton
```

VS Code tasks: **Cmd+Shift+B** runs "Compile TeensyMelton" by default.

## Board setup

- Board package: `teensy:avr` (PJRC, installed via `https://www.pjrc.com/teensy/package_teensy_index.json`)
- Version: `teensy:avr@1.60.0`
- FQBN: `teensy:avr:teensy40`

## Key config (`config.h`)

- `MELTONJOHN 1` — selects 20-light configuration (active; do not switch to BICYCLEPOLE)
- `PLATFORM_TEENSY 1` — set automatically when not building for ESP32
- `USE_MSGEQ7 1` — MSGEQ7 spectrum analyzer chip (under `#ifdef MELTONJOHN`)
- `OUTPUT_TEENSYDMX 1` — DMX output via TeensyDMX library on Serial1

## Required libraries

Beyond the Teensy bundle, install these before compiling:

| Library | Install command |
|---------|-----------------|
| TeensyDMX (Shawn Silverman) | `arduino-cli lib install "TeensyDMX"` |

## Platform layer (`teensy.cpp`)

- `SoundFFT::next()` — reads MSGEQ7 (or Teensy Audio FFT) and fills `Spectrum`
- `setup_teensy()` / `loop_teensy()` — entry points called from `TeensyMelton.ino`
- `mapToDisplay()` — converts normalized float buffer to `CRGB` with gamma and brightness

## Renderer note

`Renderer.cpp` in this directory is **Teensy-specific** — it uses time-modulo pattern cycling
instead of the beat-triggered cycling in `src/Renderer.cpp`. Do **not** overwrite it when
syncing shared files from `src/`.

## Shared source files

These files are copied manually from `Daisy/src/` (Arduino IDE cannot include subdirectories):

| File | Source |
|------|--------|
| `Patterns.h` / `Patterns.cpp` | `../src/` |
| `Renderer.h` | `../src/` |
| `MultiLayerPatterns.cpp` | `../src/` |
| `fftsg.hpp` / `fftsg.cpp` | `../src/` |

**Do not copy** `src/Renderer.cpp` — the local version must be kept.

Sync command:
```sh
cp Daisy/src/Patterns.{h,cpp} Daisy/src/Renderer.h \
   Daisy/src/MultiLayerPatterns.cpp \
   Daisy/src/fftsg.{hpp,cpp} \
   Daisy/TeensyMelton/
```

## Known compile issues (already fixed — do not re-introduce)

- **`extern int random(void)` in `config.h`**: Teensy provides `random()` as `int32_t`; stdlib provides it as `long int`. The declaration was replaced with a comment to avoid the ambiguity error.
- **`MAX` macro**: Was missing from `src/Patterns.h`. Added `#ifndef MAX / #define MAX(a,b)` guard there. This fix is in `src/` — preserve it when re-copying `Patterns.h`.
- **`BICYCLEPOLE` config**: If active, `output` is never defined for `PLATFORM_TEENSY` and the linker fails. Keep `MELTONJOHN 1` as the active application.

## Known warnings (pre-existing, not errors)

- `Patterns.h`: `memset` on `union Color` — non-trivial type; harmless but noisy
- `Renderer.cpp`: `beat_sensitivity` unused variable — dead code in the Teensy renderer
