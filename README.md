# xyztiles — SeisComP XYZ tile store plugin

A [SeisComP](https://www.seiscomp.de/) plugin that adds support for any
XYZ/slippy-map tile server to SeisComP GUI applications (scolv, scmv, etc.).

Registers as tile store type `xyz`. Works with OpenStreetMap, OpenTopoMap,
ESRI World Imagery, CartoDB, and any other server that serves standard
`{z}/{x}/{y}` tiles.

---

## Requirements

- SeisComP ≥ 5.x (Qt5 or Qt6 build)
- CMake ≥ 3.14
- C++17 compiler

No additional runtime dependencies — Qt Network is already required by SeisComP GUI.

---

## Build and install

```bash
git clone https://github.com/comoglu/seiscomp-xyztiles.git
cd seiscomp-xyztiles
cmake -S . -B build
cmake --build build -j$(nproc)
cmake --install build        # installs into /opt/seiscomp by default
```

To target a different SeisComP installation:

```bash
cmake -S . -B build -DSEISCOMP_ROOT=/path/to/seiscomp
```

---

## Configuration

Add `xyztiles` to the plugin list in `global.cfg` (or via `scconfig`):

```ini
plugins = xyztiles

map.type     = xyz
map.location = https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png
```

### Example tile sources

| Source | URL template |
|--------|-------------|
| OpenStreetMap | `https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png` |
| OpenTopoMap | `https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png` |
| ESRI NatGeo | `https://server.arcgisonline.com/ArcGIS/rest/services/NatGeo_World_Map/MapServer/tile/{z}/{y}/{x}` |
| ESRI World Imagery | `https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}` |
| CartoDB Positron | `https://{s}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}.png` |

> **Note:** ESRI tile URLs use `{z}/{y}/{x}` (row before column) — the opposite
> of the OSM convention. Both orderings work; just match the URL template to the
> server's expectation.

### Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `map.xyz.subdomains` | `a,b,c` | Comma-separated subdomain list for `{s}` rotation |
| `map.xyz.maxLevel` | `18` | Maximum zoom level |
| `map.xyz.tileSize` | `256` | Tile edge length in pixels |
| `map.xyz.cacheDir` | *(empty)* | Directory for on-disk tile cache |
| `map.xyz.cacheDuration` | `86400` | Cache TTL in seconds (`-1` = forever, `0` = disabled) |
| `map.xyz.userAgent` | `SeisComP-xyztiles/1.0` | HTTP User-Agent header |

> **OSM policy:** If using OpenStreetMap tiles, set `map.xyz.userAgent` to
> something identifying your institution —
> [required by OSM's tile usage policy](https://operations.osmfoundation.org/policies/tiles/).

---

## License

GNU Affero General Public License v3.0 — see [LICENSE](LICENSE).
