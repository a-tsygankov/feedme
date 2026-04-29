"""Resize cat silhouette PNGs from firmware/design/cats4/ down to the two
display sizes (130 px and 88 px) and write them to firmware/data/cats/
where PlatformIO's `pio run --target uploadfs` picks them up and packs
them into the LittleFS image partition.

LVGL reads them at runtime via the `lv_fs` adapter
(adapters/LvglLittleFs registered as drive 'L:'); the PNG decoder
(LV_USE_PNG=1 in lv_conf.h) decompresses on first display and the LVGL
image cache (LV_IMG_CACHE_DEF_SIZE=24 in lv_conf.h) keeps decoded
buffers in PSRAM so subsequent renders are instant.

This replaces an earlier pipeline that produced lv_img_dsc_t C arrays
under firmware/src/assets/cats/. The migration cuts ~870 KB out of the
firmware binary (now lives on LittleFS instead).

Usage (from repo root):
    python firmware/scripts/convert_cats.py
    cd firmware && pio run --target uploadfs
"""

from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Iterable

try:
    from PIL import Image
except ImportError:
    sys.stderr.write(
        "ERROR: Pillow is required. Install with: pip install pillow\n"
    )
    sys.exit(1)


# ── Configuration ─────────────────────────────────────────────────────────
HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent  # repo root
SRC_DIR = REPO / "firmware" / "design" / "cats4"
# pio's uploadfs default — files under <projdir>/data are packed into
# the LittleFS image. Naming follows L:/cats/<slug>_<size>.png on device
# (lowercase to match the slug normalization in CatSlug.h).
DST_DIR = REPO / "firmware" / "data" / "cats"

# All 12 cat poses from the design's cats4/ asset set. (The first
# pass shipped just the 5 mood cats — B1/B2/B3/C2/C4 — per the
# locked mood mapping in firmware/design/handoff.md §1. The full 12
# unlocks the cat-edit slug picker and any future per-cat appearance
# work; flash impact is modest, ~520 KB more on top of the original
# 370 KB.)
MOOD_CATS = [
    "A1", "A2", "A3", "A4",
    "B1", "B2", "B3", "B4",
    "C1", "C2", "C3", "C4",
]

# Sizes used by the screens. 130 px = Idle hero, 88 px = Feed Confirm.
SIZES = [130, 88]

# Use the white-on-transparent variant — they render against the dark
# Aubergine background. Pixels are essentially (255,255,255,A); we
# preserve A to keep anti-aliased edges crisp.
VARIANT = "white"


# ── Source loading & resizing ─────────────────────────────────────────────
def load_and_resize(path: Path, size: int) -> Image.Image:
    img = Image.open(path).convert("RGBA")
    return img.resize((size, size), Image.LANCZOS)


# ── Driver ────────────────────────────────────────────────────────────────
def main() -> int:
    if not SRC_DIR.is_dir():
        sys.stderr.write(f"ERROR: source dir not found: {SRC_DIR}\n")
        sys.stderr.write(
            "Did you copy firmware/design/cats4/ from the handoff bundle?\n"
        )
        return 1

    print(f"[cats] source : {SRC_DIR}")
    print(f"[cats] dest   : {DST_DIR}")
    print(f"[cats] cats   : {', '.join(MOOD_CATS)}")
    print(f"[cats] sizes  : {SIZES}")

    DST_DIR.mkdir(parents=True, exist_ok=True)
    total_bytes = 0
    for slug in MOOD_CATS:
        png = SRC_DIR / f"{slug}-{VARIANT}.png"
        if not png.is_file():
            sys.stderr.write(f"WARN: missing source {png}\n")
            continue
        for size in SIZES:
            img = load_and_resize(png, size)
            out = DST_DIR / f"{slug.lower()}_{size}.png"
            img.save(out, format="PNG", optimize=True)
            n = out.stat().st_size
            total_bytes += n
            print(f"  {slug} {size:3d}px  ->  "
                  f"{out.relative_to(REPO)}  ({n:>6d} B)")

    print(f"[cats] total LittleFS payload: {total_bytes/1024:.1f} KB")
    print(f"[cats] flash with: cd firmware && pio run --target uploadfs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
