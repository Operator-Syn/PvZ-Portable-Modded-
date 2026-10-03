#!/usr/bin/env python3
"""Extract row-local sniper atlases from the local sheet; requires ffmpeg.

Labels, credits, alternate palettes and the loose arrow are excluded. Each
source row stays a separate atlas, with complete bow attacks assembled explicitly.
Cells are transparent, 64x64, centered
horizontally and anchored at the feet; only transparent margins are trimmed.
"""
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "properties/sniper-female/Sniper_female_RGBA.png"
OUTPUT = SOURCE.parent / "atlases"
CELL = 64
WIDTH, HEIGHT = 300, 638
# Explicit bounds (left, right), top and bottom, in original sheet pixels.
ROWS = {
    "bow_windup": (35, 101, [(4, 32), (40, 71), (75, 112), (112, 143), (148, 194), (204, 239), (243, 273)]),
    "bow_release": (101, 164, [(9, 39), (45, 76), (83, 115), (117, 152), (160, 194), (202, 236), (239, 293)]),
    "bow_recovery": (164, 220, [(46, 82), (92, 125), (132, 164)]),
    "critical_windup": (239, 305, [(11, 40), (49, 80), (88, 123), (131, 162), (172, 215), (231, 265)]),
    "critical_spin": (305, 370, [(9, 47), (57, 96), (105, 142), (149, 189), (193, 233), (244, 276)]),
    "critical_release": (370, 432, [(11, 41), (48, 82), (86, 120), (125, 159), (166, 199), (200, 254)]),
    "dodge": (454, 505, [(0, 45), (51, 96)]),
    "map_idle": (449, 467, [(101, 122), (128, 146), (151, 172)]),
    "map_down": (471, 491, [(100, 126), (127, 151), (155, 178), (179, 203)]),
    "map_up": (495, 515, [(101, 125), (130, 152), (158, 182), (182, 205)]),
    "map_side": (518, 536, [(106, 132), (136, 159), (159, 184)]),
    "map_attack": (540, 563, [(105, 134), (135, 161), (162, 188)]),
}


def main():
    if not SOURCE.is_file():
        raise SystemExit(f"Missing source sheet: {SOURCE}")
    rgba = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(SOURCE),
                                    "-f", "rawvideo", "-pix_fmt", "rgba", "-"])
    if len(rgba) != WIDTH * HEIGHT * 4:
        raise SystemExit("Expected a 300x638 RGBA source sheet")
    animations = {"idle": (35, 101, [(4, 32)]), **ROWS}
    # Assemble complete attacks once; runtime never switches rows mid-shot.
    sequences = {}
    for name, groups in {
        "bow_complete": ("bow_windup", "bow_release", "bow_recovery"),
        "critical_complete": ("critical_windup", "critical_spin", "critical_release", "bow_recovery"),
    }.items():
        sequences[name] = [(left, right, top, bottom) for group in groups
                           for top, bottom, frames in [ROWS[group]] for left, right in frames]
    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {}
    sequences = {**{name: [(left, right, top, bottom) for left, right in frames]
                     for name, (top, bottom, frames) in animations.items()}, **sequences}
    sequences["idle"] += [(left, right, top, bottom)
                          for top, bottom, frames in [ROWS["bow_recovery"]]
                          for left, right in frames[1:]]
    for name, frames in sequences.items():
        atlas_width = CELL * len(frames)
        atlas = bytearray(atlas_width * CELL * 4)
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
            if w > CELL or h > CELL:
                raise SystemExit(f"Frame exceeds cell: {name}/{frame}")
            dest_x = frame * CELL + (CELL - w) // 2
            dest_y = CELL - h
            for y in range(h):
                src = ((y0 + y) * WIDTH + x0) * 4
                dst = ((dest_y + y) * atlas_width + dest_x) * 4
                atlas[dst:dst + w * 4] = rgba[src:src + w * 4]
            bounds.append([x0, y0, w, h])
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo",
                        "-pix_fmt", "rgba", "-s", f"{atlas_width}x{CELL}",
                        "-i", "-", "-frames:v", "1", str(OUTPUT / f"{name}.png")],
                       input=atlas, check=True)
        manifest[name] = {"columns": len(frames), "rows": 1, "cell": CELL,
                          "source_bounds": bounds}
        print(f"{name}: {len(frames)} frames")
    # The loose horizontal arrow between bow rows faces left in the source.
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(SOURCE),
                    "-vf", "crop=19:6:10:187,scale=126:12:flags=neighbor",
                    "-frames:v", "1", str(OUTPUT / "arrow.png")], check=True)
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
