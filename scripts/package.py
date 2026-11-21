Import("env")
from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import zipfile

def check_ui_stack(disassembly, limit=2048):
    # Native layout tests have desktop-sized stacks. Check the actual Xtensa
    # frame as well: drawUi runs on every screen in an 8192-byte Arduino task.
    match = re.search(
        r"<portalis::drawUi\(m5gfx::M5Canvas&, portalis::ViewModel const&\)>:\s*"
        r"[0-9a-f]+:\s+[0-9a-f]+\s+entry\s+a1,\s*(0x[0-9a-f]+|[0-9]+)",
        disassembly,
    )
    if not match:
        raise RuntimeError("Cannot verify drawUi stack frame in the Cardputer object file")
    frame = int(match.group(1), 0)
    if frame > limit:
        raise RuntimeError(f"drawUi stack frame is {frame} bytes (limit {limit}); move large snapshots off the stack")
    print(f"Portalis: drawUi stack frame {frame} bytes (limit {limit})")
    return frame

def write_archive(path, entries):
    temporary = path.with_suffix(".zip.tmp")
    with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED) as archive:
        for source, name in entries:
            archive.write(source, name)
    with zipfile.ZipFile(temporary) as archive:
        assert archive.testzip() is None
        for source, name in entries:
            assert archive.read(name) == source.read_bytes(), name
    temporary.replace(path)

def package(source, target, env):
    root = Path(env.subst("$PROJECT_DIR"))
    build = Path(env.subst("$BUILD_DIR"))
    dist = root / "dist"
    dist.mkdir(exist_ok=True)
    platform = env.PioPlatform()
    objdump = Path(platform.get_package_dir("toolchain-xtensa-esp32s3")) / "bin/xtensa-esp32s3-elf-objdump"
    check_ui_stack(subprocess.check_output([str(objdump), "-Cd", str(build / "src/ui.cpp.o")], text=True))
    esptool = Path(platform.get_package_dir("tool-esptoolpy")) / "esptool.py"
    framework = Path(platform.get_package_dir("framework-arduinoespressif32"))
    parts = [(0, build / "bootloader.bin"), (0x8000, build / "partitions.bin"),
             (0xe000, framework / "tools/partitions/boot_app0.bin"),
             (0x10000, build / "firmware.bin")]
    output = dist / "portalis.bin"
    cmd = [env.subst("$PYTHONEXE"), str(esptool), "--chip", "esp32s3", "merge_bin", "-o", str(output)]
    for offset, path in parts:
        cmd.extend([hex(offset), str(path)])
    subprocess.run(cmd, check=True)
    merged = output.read_bytes()
    for offset, path in parts:
        data = path.read_bytes()
        assert merged[offset:offset + len(data)] == data, str(path)
    assert parts[-1][1].stat().st_size <= 0x300000
    shutil.copyfile(build / "firmware.bin", dist / "Portalis-app.bin")
    shutil.copytree(root / "sd-card", dist / "sd-card", dirs_exist_ok=True)
    manifest = {"image": output.name, "flash_offset": "0x0", "size": len(merged),
                "sha256": hashlib.sha256(merged).hexdigest(),
                "target": "Cardputer / Cardputer ADV", "version": "2.3.2"}
    (dist / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    shutil.copyfile(root / "README.txt", dist / "README.txt")
    examples = [(path, path.relative_to(root / "sd-card").as_posix())
                for path in sorted((root / "sd-card").rglob("*")) if path.is_file()]
    write_archive(dist / "Portalis-SD-examples.zip", examples)
    entries = [(dist / name, name) for name in
               (output.name, "Portalis-app.bin", "manifest.json", "README.txt")]
    entries += [(path, "sd-card/" + name) for path, name in examples]
    write_archive(dist / "Portalis-package.zip", entries)
    print("Portalis: merged image verified and packaged in dist/")

env.AddPostAction("$BUILD_DIR/firmware.bin", package)
