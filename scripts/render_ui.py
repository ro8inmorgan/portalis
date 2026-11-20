#!/usr/bin/env python3
"""Render firmware UI previews: uv run --with pillow python scripts/render_ui.py.
Requires a C++17 compiler and M5GFX dependencies installed by the firmware build.
The host adapter uses drawUi() and the library's GLCD font, with illustrative data.
"""
import json
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/screenshots"
FONT = ROOT / ".pio/libdeps/cardputer/M5GFX/src/lgfx/Fonts"


def rgb(color):
    return ((color >> 11 & 31) * 255 // 31, (color >> 5 & 63) * 255 // 63, (color & 31) * 255 // 31)


def render(lines):
    name, picture, clip = None, None, None
    frames = []

    def save():
        if picture is None:
            return
        scaled = picture.resize((720, 405), Image.Resampling.NEAREST)
        if name == "animation":
            frames.append(scaled)
        else:
            scaled.save(OUT / f"{name}.png", optimize=True)

    for line in lines:
        kind, *args = json.loads(line)
        if kind == "frame":
            save()
            name = args[0]
            picture = Image.new("RGB", (240, 135))
            clip = None
        elif kind == "clip":
            x, y, w, h = args
            clip = (max(0, x), max(0, y), min(240, x+w), min(135, y+h))
        elif kind == "unclip":
            clip = None
        elif kind == "background":
            picture.paste(rgb(args[0]), (0, 0, 240, 135))
        else:
            layer = picture.copy() if clip else picture
            draw = ImageDraw.Draw(layer)
            color = rgb(args[-1])
            if kind in ("rect", "round"):
                x, y, w, h = args[:4]
                if w <= 0 or h <= 0:
                    continue
                box = (x, y, x+w-1, y+h-1)
                if kind == "round":
                    draw.rounded_rectangle(box, radius=args[4], fill=color)
                else:
                    draw.rectangle(box, fill=color)
            elif kind == "line":
                draw.line(args[:4], fill=color)
            elif kind in ("circle", "disc"):
                x, y, radius = args[:3]
                box = (x-radius, y-radius, x+radius, y+radius)
                draw.ellipse(box, fill=color if kind == "disc" else None, outline=color)
            if clip and clip[2] > clip[0] and clip[3] > clip[1]:
                picture.paste(layer.crop(clip), clip)
    save()
    frames[0].save(OUT / "menu.gif", save_all=True, append_images=frames[1:],
                   duration=80, loop=0, optimize=True, disposal=2)


def main():
    if not (FONT / "glcdfont.h").exists():
        raise SystemExit("Run the cardputer firmware build first to install M5GFX.")
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="portalis-preview-") as temp:
        binary = Path(temp) / "render"
        subprocess.run(["c++", "-std=c++17", "-I", str(ROOT / "scripts/preview"),
                        "-I", str(ROOT / "include"), "-I", str(FONT),
                        str(ROOT / "scripts/preview/screens.cpp"), str(ROOT / "src/ui.cpp"),
                        "-o", str(binary)], check=True)
        result = subprocess.run([str(binary)], check=True, text=True, stdout=subprocess.PIPE)
        render(result.stdout.splitlines())
    print(f"Rendered {len(list(OUT.glob('*.png')))} PNG previews and an animated menu to {OUT}")


if __name__ == "__main__":
    main()
