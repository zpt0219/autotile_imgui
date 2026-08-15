#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace atm {

enum class PatternRole {
    TerrainA,
    TerrainB,
    Edge
};

struct PatternLevelDef {
    PatternRole role;
    float shade;
};

constexpr float MIN_OUTLINE_WIDTH = 1.0f;
constexpr float MAX_OUTLINE_WIDTH = 6.0f;
constexpr float OUTLINE_WIDTH_STEP = 0.5f;
constexpr float DEFAULT_OUTLINE_WIDTH = 2.0f;

constexpr int MIN_BAND_STEPS = 3;
constexpr int MAX_BAND_STEPS = 5;
constexpr int DEFAULT_BAND_STEPS = 3;
constexpr float BAND_STEP_PX = 2.0f;

// Roles and shade fractions for a step count
std::vector<PatternLevelDef> pattern_levels_for(int steps = DEFAULT_BAND_STEPS);

// Level thresholds (distances in field pixels) for pattern configuration
std::vector<float> bands_for(
    const std::string& pattern,
    int steps = DEFAULT_BAND_STEPS,
    bool hard_edge_b = false,
    float outline_width = -1.0f // < 0 means default
);

// Outline width in pixels
float outline_width_px(
    const std::string& pattern,
    int steps = DEFAULT_BAND_STEPS,
    bool hard_edge_b = false,
    float outline_width = -1.0f,
    int tile_size = 32
);

// Grain displacement span (1 or 2 levels)
int band_noise_span(const std::string& pattern, int steps = DEFAULT_BAND_STEPS);

// Clamp offset into pattern's allowed range
float clamp_offset(const std::string& pattern, float offset_px);

// Edge jitter headroom for reseeded patterns
float edge_jitter_amplitude(const std::string& pattern, float offset_px = 0.0f);

// Flat distance field (1024 chars) for a canonical mask
const char* pattern_field_for_mask(const std::string& pattern, int mask);

struct FieldParams {
    float offset_px = 0.0f;
    int tile_size = 32;
    int band_steps = DEFAULT_BAND_STEPS;
    bool hard_edge_b = false;
    int edge_seed = 0;
    float outline_width = -1.0f;
};

// Generate 1024-char level grid string (digits 0..4) for a tile mask
std::string pattern_levels_for_mask(
    const std::string& pattern,
    int mask,
    const FieldParams& params = {}
);

struct BandCoords {
    std::vector<float> s;
    std::vector<float> depth;
};

// Generate tangent distance `s` and cross-band fraction `depth` for outline pixels
BandCoords pattern_band_coords(
    const std::string& pattern,
    int mask,
    const FieldParams& params = {}
);

} // namespace atm
