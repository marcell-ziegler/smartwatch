#!/usr/bin/env python3
"""
Convert OSM slippy-map PNG tiles to raw little-endian RGB565 .bin files.

Both build targets read tiles as raw 256x256 RGB565 (131072 bytes) at
<z>/<x>/<y>.bin -- no PNG decoder needed on the ESP32 or in the simulator.

Usage:
    python3 tools/png_to_bin.py <src_tile_dir> <dst_tile_dir>

<src_tile_dir> is expected to already be laid out as <z>/<x>/<y>.png (the
standard slippy-map tile tree, e.g. as downloaded/rendered for the thin
ribbon of tiles your traced route crosses at your chosen zoom -- not a
whole region). <dst_tile_dir> is created with the same <z>/<x>/<y>.bin tree,
ready to copy onto the SD card (hardware) or point FolderTileStore at
(simulator's assets/tiles/).
"""
import pathlib
import sys

TILE_PX = 256


def convert_one(src_png: pathlib.Path, dst_bin: pathlib.Path) -> None:
    from PIL import Image  # pillow; only needed when this script runs

    img = Image.open(src_png).convert("RGB")
    if img.size != (TILE_PX, TILE_PX):
        img = img.resize((TILE_PX, TILE_PX))

    dst_bin.parent.mkdir(parents=True, exist_ok=True)
    with open(dst_bin, "wb") as out:
        for y in range(TILE_PX):
            row = bytearray()
            for x in range(TILE_PX):
                r, g, b = img.getpixel((x, y))
                rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                row += rgb565.to_bytes(2, "little")
            out.write(row)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 1

    src_root = pathlib.Path(sys.argv[1])
    dst_root = pathlib.Path(sys.argv[2])
    if not src_root.is_dir():
        print(f"no such directory: {src_root}", file=sys.stderr)
        return 1

    pngs = sorted(src_root.glob("*/*/*.png"))
    if not pngs:
        print(f"no <z>/<x>/<y>.png files found under {src_root}", file=sys.stderr)
        return 1

    for src_png in pngs:
        z, x, y_png = src_png.parts[-3:]
        dst_bin = dst_root / z / x / (pathlib.Path(y_png).stem + ".bin")
        convert_one(src_png, dst_bin)
        print(f"{src_png} -> {dst_bin}")

    print(f"converted {len(pngs)} tile(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
