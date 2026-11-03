#!/usr/bin/env python3
"""Add the SnakeMod native library as a dependency of Unity's libmain.so."""
import shutil
import subprocess
import sys
from pathlib import Path

libmain = Path(sys.argv[1])
patchelf = shutil.which("patchelf")
if not patchelf:
    raise SystemExit("patchelf is required")
needed = subprocess.check_output([patchelf, "--print-needed", str(libmain)], text=True).splitlines()
if "libsnakemod.so" not in needed:
    subprocess.run([patchelf, "--add-needed", "libsnakemod.so", str(libmain)], check=True)
subprocess.run([patchelf, "--print-needed", str(libmain)], check=True)
