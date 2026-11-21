#!/usr/bin/env python3
"""Regenerate the vendored, offline-buildable MicroPython embed package.
Usage: python3 scripts/vendor_micropython.py /path/to/micropython-v1.27.0
"""
from pathlib import Path
import json
import shutil
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path(sys.argv[1]).resolve()
REVISION='78ff170de9e32c79db6e64d3e33d2bd60002bdcd'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=SOURCE,text=True).strip()==REVISION
DEST=ROOT/'lib/PortalisMicroPython'
PORT=ROOT/'scripts/micropython_port'
with tempfile.TemporaryDirectory(prefix='portalis-mpy-') as directory:
    work=Path(directory)
    for name in ('mpconfigport.h','portalis_python.h','portalis_runtime.c'):
        shutil.copyfile(PORT/name,work/name)
    (work/'Makefile').write_text(f'MICROPYTHON_TOP = {SOURCE}\nSRC_QSTR += portalis_runtime.c\ninclude $(MICROPYTHON_TOP)/ports/embed/embed.mk\n')
    subprocess.run(['make','-j2','PYTHON='+sys.executable],cwd=work,check=True)
    DEST.mkdir(parents=True,exist_ok=True)
    if (DEST/'src').exists(): shutil.rmtree(DEST/'src')
    shutil.copytree(work/'micropython_embed',DEST/'src')
    for name in ('mpconfigport.h','portalis_python.h','portalis_runtime.c'):
        shutil.copyfile(PORT/name,DEST/'src'/name)
    for name in ('embed_util.c','mphalport.c'): (DEST/'src/port'/name).unlink()
    (DEST/'src/shared/readline').mkdir()
    shutil.copyfile(SOURCE/'shared/readline/readline.h',DEST/'src/shared/readline/readline.h')
    with (DEST/'src/port/mphalport.h').open('a') as out:
        out.write('\n#include "py/misc.h"\nint portalis_mp_readline(vstr_t*, const char*);\n')
shutil.copyfile(SOURCE/'LICENSE',DEST/'LICENSE')
(DEST/'library.json').write_text(json.dumps({'name':'PortalisMicroPython','version':'1.27.0','build':{'flags':['-DNDEBUG']}},indent=2)+'\n')
(DEST/'README.md').write_text(f'''MicroPython v1.27.0 embed port, upstream commit `{REVISION}`.
Source: https://github.com/micropython/micropython/tree/{REVISION}
MIT license: see LICENSE and individual source headers.

Generated with `python3 scripts/vendor_micropython.py /path/to/micropython`.
Portalis configuration and C bridge sources live in scripts/micropython_port.
Generated sources are committed so ordinary firmware builds need no Python-source download or code generation.
''')
