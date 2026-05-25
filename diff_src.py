#!/usr/bin/env python3
"""Diff C++ files shared between DaisyMelton/ and src/."""

import difflib
import os
import sys

SKETCH_DIR = os.path.join(os.path.dirname(__file__), "DaisyMelton")
SRC_DIR    = os.path.join(os.path.dirname(__file__), "src")
EXTS       = {".cpp", ".h", ".hpp"}

sketch_files = {f for f in os.listdir(SKETCH_DIR) if os.path.splitext(f)[1] in EXTS}
src_files    = {f for f in os.listdir(SRC_DIR)    if os.path.splitext(f)[1] in EXTS}
common       = sorted(sketch_files & src_files)

if not common:
    print("No files in common.")
    sys.exit(0)

any_diff = False
for name in common:
    a_path = os.path.join(SRC_DIR,    name)
    b_path = os.path.join(SKETCH_DIR, name)
    a_lines = open(a_path).readlines()
    b_lines = open(b_path).readlines()
    diff = list(difflib.unified_diff(a_lines, b_lines, fromfile=f"src/{name}", tofile=f"DaisyMelton/{name}"))
    if diff:
        any_diff = True
        print("".join(diff))
    else:
        print(f"{name}: identical")

sys.exit(1 if any_diff else 0)
