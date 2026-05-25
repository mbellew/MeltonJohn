# MeltonJohn — Daisy project notes for Claude

## Project purpose

Controls 20 RGB lights reactively to audio (bass/mid/treble spectrum analysis).

## Repository layout

```
MeltonJohn/
  Teensy/   — read-only reference (original Teensy/Linux implementation, do not modify)
  Daisy/    — Daisy Patch firmware, plus Linux/macOS visualiser
    src/                  — shared platform-agnostic source (canonical)
    DaisyMelton/          — Daisy Patch firmware, native libDaisy + Makefile (build target)
    DesktopApp/           — Linux/macOS PulseAudio build (build target); produces ./MeltonJohn
    TeensyMelton/         — Teensy 4.0 Arduino sketch (build target)
  .vscode/
    tasks.json            — build tasks for DaisyMelton and TeensyMelton
    c_cpp_properties.json — IntelliSense configs for both targets
```

## Architecture

- `Renderer::renderFrame(float time, Spectrum*, float buffer[], size_t)` — main interface, writes normalized floats
- `Spectrum` struct: `bass, mid, treb, bass_att, mid_att, treb_att, vol` (floats ~0–100, auto-scaled)
- `IMAGE_SIZE = 20` lights, 3 floats per light (RGB) → 60 floats total
- `CRGB` struct: from FastLED on Teensy; on Daisy, defined in `pixeltypes.h` shim (FastLED not available)

## Key config (`DaisyMelton/config.h`)

- `MELTONJOHN 1` — selects 20-light configuration
- `PLATFORM_DAISY 1` — selects Daisy Patch platform
- `OUTPUT_MYDMX 1` — DMX output via Serial1 + MAX3485
- `IMAGE_SIZE 20`, `FFT_SIZE 256`, `DAISY_SAMPLE_RATE_HZ 8000`

## Platform layer (`DaisyMelton/main.cpp`)

- `AudioCallback` — libDaisy DMA callback, writes stereo floats into circular buffer
- `DaisySpectrumAnalyzer::next()` — snapshots circular buffer, applies Hann window, runs dual `rdft()`, fills `Spectrum`
- `int main()` — initialises `DaisyPatch`, audio, DMX UART; runs the render loop
- DMX-512 output on USART_1 (MIDI TRS pins D13/D14) via libDaisy `UartHandler` + external MAX3485
- VU meter on the built-in OLED via `patch.display`

## Shared files

`src/` contains only platform-agnostic sources: `Patterns.{cpp,h}`, `Renderer.{cpp,h}`, `MultiLayerPatterns.cpp`, `fftsg.{cpp,hpp}`, `beat_data.{cpp,h}`. Everything platform-specific lives in the target dir alongside its `Makefile`.

Each platform Makefile pulls the shared sources in directly:
- `DaisyMelton/Makefile` lists `../src/Patterns.cpp` etc. (libDaisy's core Makefile uses `vpath`, so `.o` files land flat in `DaisyMelton/build/`)
- `DesktopApp/Makefile` builds `*.cpp ../src/*.cpp` together
- Both pass `-I. -I../src` so `#include "config.h"` resolves to the *local* (platform-specific) `config.h`, and shared headers from `../src/` are findable

TeensyMelton still uses the Arduino-CLI manual-copy pattern (see `TeensyMelton/TEENSY.md`).

Desktop-only sources (`BeatDetect`, `MidiMix`, `MidiPatterns`, `PCM`, `main.cpp`, `config.h`) live in `DesktopApp/`, not `src/`.

## Build

- **DaisyMelton** (Daisy Patch firmware) — see `DaisyMelton/DAISY.md`
- **DesktopApp** (Linux/macOS visualiser) — `cd DesktopApp && make` → produces `MeltonJohn` and `MeltonJohn_debug`
- **TeensyMelton** — see `TeensyMelton/TEENSY.md`

VS Code tasks: **Cmd+Shift+B** runs "Compile TeensyMelton" by default.
VS Code workspace root is `MeltonJohn/` — `.vscode/` lives there.
