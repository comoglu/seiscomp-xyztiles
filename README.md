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

The same source builds against both **SeisComP 7** (SC_API 17) and **SeisComP 8**
(SC_API 18); the Mercator-projection call is version-guarded. Plugins are
ABI-specific per SeisComP major version, so use the binary that matches yours.

### Prebuilt binaries

The [Releases](https://github.com/comoglu/seiscomp-xyztiles/releases) page has
prebuilt `.so` files for Ubuntu 24.04 (x86_64, Qt5):

- `xyztiles-<ver>-seiscomp7-...` — **all SeisComP 7.x** (7.0 through the latest
  7.3+; built against API 17.0 so it loads on every 7.x by SeisComP's plugin
  API rule).
- `xyztiles-<ver>-seiscomp8.0-...` — **SeisComP 8.x** (API 18).

Install by copying the matching file into `<SEISCOMP_ROOT>/share/plugins/` and
the description XML into `<SEISCOMP_ROOT>/etc/descriptions/`.

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
| `map.xyz.sources` | *(empty)* | Optional list of `minLevel:maxLevel:url` entries — one basemap per zoom band (overrides `map.location`) |
| `map.xyz.subdomains` | `a,b,c` | Comma-separated subdomain list for `{s}` rotation |
| `map.xyz.minLevel` | `0` | Minimum zoom level (single-source mode) |
| `map.xyz.maxLevel` | `19` | Maximum zoom level (single-source mode) |
| `map.xyz.tileSize` | `256` | Tile edge length in px — `256` (standard) or `512` (HiDPI/retina). Must match the server's actual tiles |
| `map.xyz.cacheDir` | *(empty)* | Directory for on-disk tile cache |
| `map.xyz.cacheDuration` | `86400` | Cache TTL in seconds (`-1` = forever, `0` = disabled) |
| `map.xyz.missingTTL` | `300` | Seconds to remember a failed (e.g. 404) tile before retrying (`-1` = session, `0` = always retry) |
| `map.xyz.userAgent` | `SeisComP-xyztiles/1.0` | HTTP User-Agent header |

> **Max zoom per provider:** OSM standard caps at **19**; third-party OSM
> (MapTiler, Thunderforest) reach 20–22; Google ~21–22; ESRI ArcGIS up to 23.
> Set `maxLevel` (or a source's band) to match your server. The hard ceiling is 29.

### Different basemaps at different zoom levels

Use `map.xyz.sources` to serve, say, a clean overview map when zoomed out and
satellite imagery when zoomed in. Each entry is `minLevel:maxLevel:urlTemplate`
(only the first two colons are separators, so `https://` is preserved):

```ini
plugins = xyztiles
map.type = xyz

map.xyz.sources = "0:9:https://{s}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}.png", \
                  "10:19:https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"
```

When `map.xyz.sources` is set it overrides `map.location`. The store's overall
zoom range is the union of all bands.

> **OSM policy:** If using OpenStreetMap tiles, set `map.xyz.userAgent` to
> something identifying your institution —
> [required by OSM's tile usage policy](https://operations.osmfoundation.org/policies/tiles/).

---

## Notes

- **Map looks sharp while dragging, softer once you release.** This is expected
  SeisComP behaviour, not the plugin: while panning, the canvas renders tiles
  with nearest-neighbour sampling (crisp); on release it switches to bilinear
  filtering (smoother). To keep the released view crisp too, set
  `scheme.map.bilinearFilter = false` — the trade-off is more aliasing on
  coastlines and labels.
- **`map.xyz.tileSize` is a `256`/`512` dropdown in scconfig.** It must match the
  pixel size the server actually serves; on the first tile fetch the plugin logs
  a warning if the served size differs, since a mismatch makes the map blurry.

---

## License

GNU Affero General Public License v3.0 — see [LICENSE](LICENSE).
