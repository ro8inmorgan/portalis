Import("env")
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import zipfile

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
                "target": "Cardputer / Cardputer ADV", "version": "1.0.0"}
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
