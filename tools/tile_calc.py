import math


def latlon_to_tile(lat, lon, z):
    n = 2**z
    xtile = int((lon + 180.0) / 360.0 * n)
    lat_rad = math.radians(lat)
    ytile = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)
    return xtile, ytile


coords = [
    (59.85753, 17.64911),
    (59.8765, 17.7388),
    (59.8534, 17.8318),
    (59.84262, 17.87810),
    (59.86680, 17.94394),
    (59.88170, 17.99200),
    (59.87653, 18.04573),
    (59.92053, 18.11594),
]

tiles = []

for coord in coords:
    tile = latlon_to_tile(*coord, 14)
    print(f"{coord}: {tile}")
    tiles.append(tile)

print(f"min_x: {min(v[0] for v in tiles)} min_y: {min(v[1] for v in tiles)}")
print(f"max_x: {max(v[0] for v in tiles)} max_y: {max(v[1] for v in tiles)}")
print(f"min_lat: {min(v[0] for v in coords)} min_lon: {min(v[1] for v in coords)}")
print(f"min_lat: {max(v[0] for v in coords)} max_lon: {max(v[1] for v in coords)}")
