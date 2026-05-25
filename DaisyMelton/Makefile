# DaisyMelton — native libDaisy build.
#
# Compiles a bare-metal firmware for the Daisy Patch using libDaisy
# (no Arduino, no DaisyDuino).  See DAISY.md for setup notes.

TARGET = DaisyMelton

# The STMicroelectronics Arduino package ships a recent arm-none-eabi
# toolchain; reuse it so we don't need a separate `brew install --cask
# gcc-arm-embedded`.  Override on the make command line if you install
# a different one.
GCC_PATH ?= $(HOME)/Library/Arduino15/packages/STMicroelectronics/tools/xpack-arm-none-eabi-gcc/14.2.1-1.1/bin

# Sources.  The platform-agnostic files live in ../src/; libDaisy's core
# Makefile uses `vpath` so listing them with their relative path here is
# all that's needed — the .o files still land flat in ./build/.
CPP_SOURCES  = main.cpp
CPP_SOURCES += ../src/Renderer.cpp
CPP_SOURCES += ../src/Patterns.cpp
CPP_SOURCES += ../src/MultiLayerPatterns.cpp
CPP_SOURCES += ../src/fftsg.cpp
CPP_SOURCES += ../src/beat_data.cpp     # pre-recorded band magnitudes for the silence fallback

# Include paths.  `-I.` lets shared sources in ../src/ pick up
# DaisyMelton/config.h via the standard include search (src/config.h
# was moved to ../DesktopApp/, so there's no longer a conflicting
# config.h in the source-file's own directory to shadow this one).
# `-I../src` covers main.cpp's references to the shared headers.
C_INCLUDES += -I.
C_INCLUDES += -I../src

# Run from SRAM via the Electrosmith bootloader: gives ~480 KB code
# budget vs. 128 KB internal flash.  Requires the bootloader to be
# installed on the Daisy first (one-time `make program-boot`); then
# `make program-dfu` flashes the app to QSPI and the bootloader copies
# it to SRAM on boot.  See DAISY.md.
APP_TYPE = BOOT_SRAM

# Optimise for size — both fits more in flash AND keeps QSPI→SRAM copy
# at boot quick (matters with APP_TYPE = BOOT_SRAM).
OPT = -Os

# libDaisy lives outside this repo (one clone shared across Daisy
# projects).  Override LIBDAISY_DIR if you keep it elsewhere.
LIBDAISY_DIR ?= $(HOME)/Projects/electrosmith/libDaisy

# Core Makefile (toolchain rules, linker script, libdaisy.a link).
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Override the core Makefile's `all: elf + hex + bin` default — we only
# need the .bin (that's what `make program-dfu` flashes).
.DEFAULT_GOAL := $(BUILD_DIR)/$(TARGET).bin
