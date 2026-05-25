# DaisyMelton

DaisyMelton is an Arduino sketch for the Daisy Patch / Daisy Seed platform.

## Build

This project includes a VS Code task for building with `arduino-cli`.

- Run **Tasks: Run Task** in VS Code
- Choose **Arduino: Compile DaisyMelton**

The build command is:

```sh
arduino-cli compile \
  --fqbn STMicroelectronics:stm32:GenH7:pnum=DAISY_SEED,usb=CDCgen \
  --libraries ~/Documents/Arduino/libraries \
  .
```

## Upload

A second task is available for upload:

- Run **Tasks: Run Task**
- Choose **Arduino: Upload DaisyMelton (DFU)**

Put the Daisy in DFU mode first: hold **BOOT**, press **RESET**, release both.

Alternatively, upload manually with:

```sh
dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D build/DaisyMelton.ino.bin || true
```

Note: `arduino-cli upload` requires STM32CubeProgrammer on macOS and will fail without it.
Use `dfu-util` directly instead.

## Workspace configuration

This repo includes `arduino.json` so the Arduino extension and CLI know the sketch and board:

- `sketch`: `DaisyMelton.ino`
- `board`: `STMicroelectronics:stm32:GenH7:pnum=DAISY_SEED,usb=CDCgen`
- `output`: `build`

## Notes

- The ST board package must be installed: `STMicroelectronics:stm32`
- The Daisy library is used as an Arduino library and should be installed in the Arduino libraries folder.
- See `DAISY.md` for project-specific build notes and hardware details.
