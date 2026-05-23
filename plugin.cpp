#include <seiscomp/core/plugin.h>

// XYZTileStore self-registers via REGISTER_TILESTORE_INTERFACE in xyztilestore.cpp.
// This entry point just satisfies the SeisComP plugin loader contract.

ADD_SC_PLUGIN(
    "Community XYZ tile store — supports any XYZ/slippy tile server (OSM, OTM, ESRI, ...)",
    "maptiles-community contributors",
    1, 0, 0
)
