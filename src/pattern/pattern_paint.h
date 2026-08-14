#pragma once

#include "blob47_pattern.h"
#include "pattern_noise.h"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <tuple>

namespace atm {

struct RGB {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    bool operator==(const RGB& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
    bool operator!=(const RGB& other) const {
        return !(*this == other);
    }
};

struct RoleColours {
    RGB terrainA;
    RGB terrainB;
    RGB edge;
};

extern const RoleColours REFERENCE_ROLE_COLOURS;
extern const RoleColours DEFAULT_ROLE_COLOURS;

RGB parse_hex_colour(const std::string& hex);
std::string to_hex_colour(RGB c);

std::tuple<double, double, double> rgb_to_hsv(RGB c);
RGB hsv_to_rgb(double h, double s, double v);

RGB shade_colour(RGB c, PatternRole role, float t = 1.0f);
std::vector<RGB> pattern_ramp(const RoleColours& colours, int band_steps = DEFAULT_BAND_STEPS);

struct NoiseColours {
    std::optional<RGB> b;
    std::optional<RGB> edge;
    std::optional<RGB> a;
};

struct RibbonPaintOptions {
    std::string algo = "none";
    double amount = 0.0;
    int period = 8;
    int shades = 2;
    int32_t seed = 0;
    bool invert = false;
    std::optional<RGB> colour;
    std::optional<std::vector<std::optional<RGB>>> ramp;
};

struct TexturePaintOptions {
    std::string algoA = "none";
    std::string algoB = "none";
    double amountA = 0.0;
    double amountB = 0.0;
    int shadesA = 2;
    int shadesB = 2;
    int32_t seedA = 0;
    int32_t seedB = 0;
    int cellScaleA = 4;
    int cellScaleB = 4;
    int rippleScaleA = 4;
    int rippleScaleB = 4;
    int geoScaleA = 1;
    int geoScaleB = 1;
    std::optional<RGB> colourA;
    std::optional<RGB> colourB;
    std::optional<std::vector<std::optional<RGB>>> rampA;
    std::optional<std::vector<std::optional<RGB>>> rampB;
};

struct PaintOptions {
    int tile_size = 32;
    double offset_px = 0.0;
    int band_steps = DEFAULT_BAND_STEPS;
    bool hard_edge_b = false;
    int edge_seed = 0;
    double outline_width = -1.0; // < 0 means default

    std::vector<NoiseId> noises;
    int32_t noise_seed = 0;
    double noise_strength = 1.0;
    std::vector<NoiseTargetId> noise_targets;
    NoiseColours noise_colours;

    RibbonPaintOptions ribbon;
    TexturePaintOptions texture;

    std::optional<std::vector<RGB>> ramp;
    bool transparent_b = false;
};

// Paint a single tile (tile_size x tile_size x 4 RGBA bytes)
std::vector<uint8_t> paint_pattern_tile_rgba(
    const std::string& pattern,
    int mask,
    const RoleColours& colours,
    const PaintOptions& opts
);

} // namespace atm
