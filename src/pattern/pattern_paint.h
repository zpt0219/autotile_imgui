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

struct TextureSide {
    std::string algo = "none";
    double amount = 0.0;
    int shades = 2;
    int32_t seed = 0;
    int cellScale = 4;
    int rippleScale = 4;
    int geoScale = 1;
    std::optional<RGB> colour;
    std::optional<std::vector<std::optional<RGB>>> ramp;
};

struct TexturePaintOptions {
    TextureSide a;
    TextureSide b;
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

class TilePainter {
public:
    TilePainter(const std::string& pattern, const RoleColours& colours, const PaintOptions& opts);

    void paint_tile_into(int mask, uint8_t* out_rgba, int row_stride_bytes = 0) const;
    std::vector<uint8_t> paint_tile_rgba(int mask) const;

    const std::string& pattern() const { return pattern_; }
    const RoleColours& colours() const { return colours_; }
    const PaintOptions& options() const { return opts_; }

private:
    std::string pattern_;
    RoleColours colours_;
    PaintOptions opts_;
    std::vector<RGB> ramp_;
    std::vector<PatternLevelDef> level_defs_;
    FieldParams fp_;
    int solid_ = 0;
    int edge_level_ = -1;
    int rib_shades_ = 1;
    bool ribbon_on_ = false;
    float rib_width_ = 1.0f;
    int span_ = 0;
    std::optional<std::vector<RGB>> rib_ramp_;
    std::optional<std::vector<RGB>> texA_;
    std::optional<std::vector<RGB>> texB_;
    std::vector<bool> noise_targets_lut_;
};

// Paint a single tile (tile_size x tile_size x 4 RGBA bytes)
std::vector<uint8_t> paint_pattern_tile_rgba(
    const std::string& pattern,
    int mask,
    const RoleColours& colours,
    const PaintOptions& opts
);

} // namespace atm
