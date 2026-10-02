#!/usr/bin/env python3
"""Generate one complete horizontal atlas per Ephraim attack, plus idle.

Requires ffmpeg. Source row crops remain explicit; matching windup, strike and
recovery cels are assembled here so runtime playback never changes atlases.
"""

from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "properties/ephraim/Ephraim_Great_Lord_RGBA.png"
OUTPUT = ROOT / "properties/ephraim/atlases"
CELL_W = 96
CELL_H = 96

# Source row bounds and per-frame x bounds, measured against the 786x524 sheet.
ROWS = {
    "lance_windup": (18, 82, [(8, 53), (63, 108), (118, 163), (172, 216), (224, 269), (278, 322),
                              (333, 378), (388, 434), (444, 490), (500, 546), (555, 601), (611, 657), (668, 726)]),
    "lance_attack": (82, 155, [(8, 74), (83, 153), (163, 230), (240, 303), (311, 392),
                               (402, 469), (479, 545), (557, 627), (637, 690), (701, 777)]),
    "lance_followthrough": (151, 225, [(5, 60), (68, 120), (132, 175), (180, 228)]),
    "critical_lance": (235, 330, [(13, 58), (69, 112), (123, 169), (181, 230), (239, 288), (300, 349), (360, 418)]),
    "javelin": (350, 430, [(12, 57), (67, 115), (123, 174), (183, 235), (242, 311), (322, 366), (376, 421)]),
    "critical_javelin": (445, 524, [(7, 60), (63, 114), (122, 173), (182, 234), (246, 307),
                                     (318, 380), (391, 448), (461, 524), (533, 602)]),
}


def row_frames(name: str, indices=None):
    y0, y1, frames = ROWS[name]
    return [(x0, x1, y0, y1) for idx, (x0, x1) in enumerate(frames)
            if indices is None or idx in indices]


# The second half of the first source row repeats ready poses. Keep the six
# distinct intro poses and its final charge; retain every strike/recovery cel.
NORMAL_WINDUP = row_frames("lance_windup", {0, 1, 2, 3, 4, 5, 12})
STRIKE_RECOVERY = row_frames("lance_attack") + row_frames("lance_followthrough")
ANIMATIONS = {
    "idle": row_frames("lance_windup", {0, 1, 2, 3, 4, 5}),
    "lance": NORMAL_WINDUP + STRIKE_RECOVERY,
    "critical_lance_complete": row_frames("critical_lance") + STRIKE_RECOVERY,
    "javelin_complete": row_frames("javelin"),
    "critical_javelin_complete": row_frames("critical_javelin") + row_frames("javelin", {5, 6}),
}


def build_atlas(name: str, frames) -> None:
    filters = []
    labels = []
    for idx, (x0, x1, y0, y1) in enumerate(frames):
        width = x1 - x0
        filters.append(
            f"[0:v]crop={width}:{y1-y0}:{x0}:{y0},"
            f"pad={CELL_W}:{CELL_H}:(ow-iw)/2:oh-ih:color=0x00000000,"
            f"setsar=1[f{idx}]"
        )
        labels.append(f"[f{idx}]")
    filters.append(f"{''.join(labels)}hstack=inputs={len(labels)}:shortest=1[out]")
    target = OUTPUT / f"{name}.png"
    subprocess.run([
        "ffmpeg", "-v", "error", "-y", "-i", str(SOURCE),
        "-filter_complex", ";".join(filters), "-map", "[out]",
        "-frames:v", "1", str(target),
    ], check=True)
    print(f"{target.relative_to(ROOT)}: {len(frames)} complete sequence frames")


def main() -> None:
    if not SOURCE.is_file():
        raise SystemExit(f"Missing source sheet: {SOURCE}")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, frames in ANIMATIONS.items():
        build_atlas(name, frames)
    # Separate horizontal flight sprite beside the javelin row, facing left.
    subprocess.run([
        "ffmpeg", "-v", "error", "-y", "-i", str(SOURCE),
        "-vf", "crop=62:8:434:369,scale=136:18:flags=neighbor",
        "-frames:v", "1", str(OUTPUT / "javelin_projectile.png"),
    ], check=True)


if __name__ == "__main__":
    main()
