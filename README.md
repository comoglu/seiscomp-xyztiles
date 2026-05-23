# seiscomp-maptiles-community

A community [SeisComP](https://www.seiscomp.de/) plugin that adds support for
XYZ/slippy-map tile servers (OpenStreetMap, OpenTopoMap, ESRI World Imagery,
CartoDB, and any other standard XYZ tile source) to SeisComP GUI applications
such as **scolv** and **scmv**.

---

## Features

- Any XYZ/slippy-map tile URL template (`{z}/{x}/{y}`, `{s}` subdomain rotation)
- Disk tile cache with configurable TTL
- HTTP/2 and redirect support
- Fully configurable via SeisComP's standard `scconfig` interface
- No extra runtime dependencies beyond Qt Network (already required by SeisComP GUI)

---

## Requirements

- SeisComP ≥ 5.x (tested against nightly builds)
- Qt 5 (Core, Network, Gui)
- CMake ≥ 3.14
- C++17 compiler

---

## Build

```bash
git clone https://github.com/comoglu/seiscomp-maptiles-community.git
cd seiscomp-maptiles-community
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

Add `maptiles_community` to the plugin list in `global.cfg` (or via `scconfig`):

```ini
plugins = maptiles_community

map.type     = xyz
map.location = https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png
```

### Example tile sources

| Source | URL template |
|--------|-------------|
| OpenStreetMap | `https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png` |
| OpenTopoMap | `https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png` |
| ESRI World Imagery | `https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}` |
| CartoDB Positron | `https://{s}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}.png` |

### Optional parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `map.xyz.subdomains` | `a,b,c` | Comma-separated subdomain list for `{s}` |
| `map.xyz.maxLevel` | `18` | Maximum zoom level |
| `map.xyz.tileSize` | `256` | Tile edge length in pixels |
| `map.xyz.cacheDir` | *(empty)* | Directory for disk tile cache |
| `map.xyz.cacheDuration` | `86400` | Cache TTL in seconds (`-1` = forever, `0` = disabled) |
| `map.xyz.userAgent` | `SeisComP-maptiles-community/1.0` | HTTP User-Agent header |

> **Note:** If using OpenStreetMap tiles, please set `map.xyz.userAgent` to
> something identifying your institution — this is required by OSM's
> [tile usage policy](https://operations.osmfoundation.org/policies/tiles/).

---

## License

GNU Affero General Public License v3.0 — see [LICENSE](LICENSE).
