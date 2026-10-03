#!/usr/bin/env python3
"""Extract Serra Bishop idle, staff and critical sequences; requires ffmpeg.

The explicit first-row crop excludes labels and adjacent magic frames.
Source pixels are preserved, centered and bottom-aligned in 64x64 cells
(96x96 for the complete critical halo).
"""
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "properties/serra-bishop/Serra_Bishop_RGBA.png"
OUTPUT = SOURCE.parent / "atlases"
BLESSING_SOURCE = SOURCE.parent / "Divine_Battle_Effects_RGBA.png"
CELL = 64
WIDTH, HEIGHT = 376, 469
# Explicit (left, right, top, bottom) bounds in original sheet pixels.
IDLE_CROP = (3, 31, 38, 77)
# Keep the two critical rows in their drawn order, including the magic effects.
STAFF_CROPS = [(left, right, 273, 315) for left, right in
               [(6, 39), (43, 79), (82, 119), (123, 159), (162, 191)]]
CRITICAL_CROPS = [(left, right, top, 155) for left, right, top in
                  [(7, 35, 110), (42, 70, 110), (77, 105, 110),
                   (110, 138, 110), (144, 175, 110), (177, 205, 110),
                   (211, 242, 110), (245, 294, 98), (303, 339, 98)]]
CRITICAL_CROPS += [(left, right, 160, 244) for left, right in
                   [(3, 84), (96, 129), (134, 165), (169, 205),
                    (209, 249), (255, 299), (309, 337), (343, 371)]]


def main():
    if not SOURCE.is_file():
        raise SystemExit(f"Missing source sheet: {SOURCE}")
    rgba = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(SOURCE),
                                    "-f", "rawvideo", "-pix_fmt", "rgba", "-"])
    if len(rgba) != WIDTH * HEIGHT * 4:
        raise SystemExit("Expected a 376x469 RGBA source sheet")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {}
    sequences = {"idle": [IDLE_CROP], "staff": STAFF_CROPS, "critical": CRITICAL_CROPS}
    for name, frames in sequences.items():
        cell = 96 if name == "critical" else CELL
        atlas_width = cell * len(frames)
        atlas = bytearray(atlas_width * cell * 4)
        bounds = []
        for frame, (left, right, top, bottom) in enumerate(frames):
            pixels = [(x, y) for y in range(top, bottom) for x in range(left, right)
                      if rgba[(y * WIDTH + x) * 4 + 3]]
            if not pixels:
                raise SystemExit(f"Empty frame: {name}/{frame}")
            x0 = min(x for x, y in pixels)
            x1 = max(x for x, y in pixels) + 1
            y0 = min(y for x, y in pixels)
            y1 = max(y for x, y in pixels) + 1
            w, h = x1 - x0, y1 - y0
            if w > cell or h > cell:
                raise SystemExit(f"Frame exceeds cell: {name}/{frame}")
            dest_x = frame * cell + (cell - w) // 2
            dest_y = cell - h
            for y in range(h):
                src = ((y0 + y) * WIDTH + x0) * 4
                dst = ((dest_y + y) * atlas_width + dest_x) * 4
                atlas[dst:dst + w * 4] = rgba[src:src + w * 4]
            bounds.append([x0, y0, w, h])
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo",
                        "-pix_fmt", "rgba", "-s", f"{atlas_width}x{cell}",
                        "-i", "-", "-frames:v", "1", str(OUTPUT / f"{name}.png")],
                       input=atlas, check=True)
        manifest[name] = {"columns": len(frames), "rows": 1, "cell": cell,
                          "source_bounds": bounds}
        print(f"{name}: {len(frames)} frames")
    # The lower starburst sequence is one continuous Divine effect, across four
    # rows. Keep that grid and its common center; exclude green rules and credits.
    effect = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(BLESSING_SOURCE),
                                      "-f", "rawvideo", "-pix_fmt", "rgba", "-"])
    effect_width, effect_height = 1208, 1613
    if len(effect) != effect_width * effect_height * 4:
        raise SystemExit("Expected a 1208x1613 RGBA Divine effect sheet")
    cell, columns, rows = 192, 5, 4
    atlas_width = cell * columns
    atlas = bytearray(atlas_width * cell * rows * 4)
    bounds = []
    for frame in range(18):
        col, row = frame % columns, frame // columns
        x0 = [1, 243, 484, 725, 967][col]
        y0 = [968, 1129, 1290, 1451][row]
        bounds.append([x0, y0, cell, 160])
        for y in range(160):
            src = ((y0 + y) * effect_width + x0) * 4
            dst = ((row * cell + 16 + y) * atlas_width + col * cell) * 4
            atlas[dst:dst + cell * 4] = effect[src:src + cell * 4]
    for pixel in range(0, len(atlas), 4):
        if atlas[pixel:pixel + 3] == bytes((34, 177, 76)):
            atlas[pixel:pixel + 4] = bytes(4)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo",
                    "-pix_fmt", "rgba", "-s", f"{atlas_width}x{cell * rows}",
                    "-i", "-", "-frames:v", "1", str(OUTPUT / "divine_blessing.png")],
                   input=atlas, check=True)
    manifest["divine_blessing"] = {"columns": columns, "rows": rows, "cell": cell,
                                    "source_bounds": bounds}
    print("divine_blessing: 18 frames, original row order")
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
