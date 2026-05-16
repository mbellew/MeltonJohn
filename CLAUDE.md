# MeltonJohn — Daisy project notes for Claude

## Project purpose

Controls 20 RGB lights reactively to audio (bass/mid/treble spectrum analysis).

## Repository layout

```
MeltonJohn/
  Teensy/   — read-only reference (original Teensy/Linux implementation, do not modify)
  Daisy/    — working port to Electro-Smith Daisy Patch hardware
    src/                  — shared platform-agnostic source (canonical)
    DaisyMelton/          — Daisy Patch Arduino sketch (build target)
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

## Platform layer (`DaisyMelton/daisy.cpp`)

- `AudioCallback` — DaisyDuino ISR, writes stereo floats into circular buffer
- `DaisySpectrumAnalyzer::next()` — snapshots circular buffer, applies Hann window, runs dual `rdft()`, fills `Spectrum`
- `setup_daisy()` / `loop_daisy()` — entry points called from `DaisyMelton.ino`

## Shared files (manual copy pattern)

Arduino IDE cannot include files from subdirectories. Files in `src/` are **manually copied** into each sketch directory. When `src/` changes, re-copy:

```sh
# DaisyMelton
cp Daisy/src/Patterns.{h,cpp} Daisy/src/Renderer.{h,cpp} \
   Daisy/src/MultiLayerPatterns.cpp \
   Daisy/src/fftsg.{hpp,cpp} \
   Daisy/DaisyMelton/

# TeensyMelton — see TeensyMelton/TEENSY.md for the sync command and caveats
```

## Build

- **DaisyMelton** — see `DaisyMelton/DAISY.md`
- **TeensyMelton** — see `TeensyMelton/TEENSY.md`

VS Code tasks: **Cmd+Shift+B** runs "Compile TeensyMelton" by default.
VS Code workspace root is `MeltonJohn/` — `.vscode/` lives there.
