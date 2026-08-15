#pragma once

#include "pattern_paint.h"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace atm {

constexpr int DEFAULT_TEXTURE_SHADES = 4;
constexpr int DEFAULT_CELL_SCALE = 3;
constexpr int MIN_CELL_SCALE = 2;
constexpr int MAX_CELL_SCALE = 6;
constexpr int DEFAULT_RIPPLE_SCALE = 4;
constexpr int MIN_RIPPLE_SCALE = 2;
constexpr int MAX_RIPPLE_SCALE = 8;
constexpr int DEFAULT_GEO_SCALE = 1;

extern const RGB WATER_DOT_COLOUR;
extern const RGB DEFAULT_TEXTURE_TERRAIN_A;
extern const RGB DEFAULT_TEXTURE_TERRAIN_B;

// texture_uses_amount() and the rest of the per-texture switches now live on
// the registry in catalog.h.

RGB texture_colour(RGB c, float t);

std::vector<RGB> texture_ramp(
    RGB base,
    std::optional<RGB> target,
    int shades = DEFAULT_TEXTURE_SHADES,
    const std::optional<std::vector<std::optional<RGB>>>& overrides = std::nullopt
);

int texture_shade_at(
    const std::string& texture,
    int x,
    int y,
    int32_t seed,
    float amount,
    int shades = DEFAULT_TEXTURE_SHADES,
    int cell_scale = DEFAULT_CELL_SCALE,
    int ripple_scale = DEFAULT_RIPPLE_SCALE,
    int geo_scale = DEFAULT_GEO_SCALE
);

} // namespace atm
