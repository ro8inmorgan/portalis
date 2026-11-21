#!/usr/bin/env python3
"""Compile and exercise the real embedded VM on the host, without hardware mocks."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/'lib/PortalisMicroPython/src'
with tempfile.TemporaryDirectory(prefix='portalis-python-test-') as directory:
    program=Path(directory)/'test'
    subprocess.run(['cc','-std=gnu99','-O1','-DNDEBUG','-I'+str(SRC),'-I'+str(SRC/'port'),
        *map(str,sorted(SRC.rglob('*.c'))),'-UNDEBUG',str(ROOT/'test/runtime_python/main.c'),'-lm','-o',str(program)],check=True)
    subprocess.run([str(program),*map(str,sorted((ROOT/"sd-card/scripts").glob("*.py")))],check=True,timeout=30)
