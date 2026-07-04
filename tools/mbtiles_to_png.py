#!/usr/bin/env python3
"""
Unpack an .mbtiles file (as produced by MOBAC's "MBTiles" atlas format) into
a plain <z>/<x>/<y>.png directory tree, ready for tools/png_to_bin.py.

MBTiles (https://github.com/mapbox/mbtiles-spec) is a SQLite database with a
`tiles` table of (zoom_level, tile_column, tile_row, tile_data) rows. Rows are
stored PNG/JPEG blobs already -- no decoding needed, just write them out.

One wrinkle: MBTiles stores tile_row in TMS order (row 0 = the SOUTHERNMOST
row), while the standard "slippy map" / XYZ scheme used everywhere else
(including this project's z/x/y.png convention and OSM itself) numbers rows
from the NORTH. This script flips that back to plain XYZ on the way out.

Usage:
    python3 tools/mbtiles_to_png.py <path/to/atlas.mbtiles> <dst_tile_dir>
"""
import pathlib
import sqlite3
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 1

    src_path = pathlib.Path(sys.argv[1])
    dst_root = pathlib.Path(sys.argv[2])
    if not src_path.is_file():
        print(f"no such file: {src_path}", file=sys.stderr)
        return 1

    con = sqlite3.connect(str(src_path))
    rows = con.execute(
        "SELECT zoom_level, tile_column, tile_row, tile_data FROM tiles"
    ).fetchall()
    con.close()

    if not rows:
        print("no rows in tiles table -- wrong file, or an empty atlas?", file=sys.stderr)
        return 1

    n_written = 0
    for z, x, tms_row, data in rows:
        y = (2 ** z) - 1 - tms_row   # TMS -> XYZ row flip
        dst = dst_root / str(z) / str(x) / f"{y}.png"
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(data)
        n_written += 1

    print(f"wrote {n_written} tile(s) to {dst_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
