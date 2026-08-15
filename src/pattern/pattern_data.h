#pragma once

#include <string>
#include <array>
#include <cstdint>

namespace atm {
namespace pattern_data {

constexpr float FIELD_STEP = 0.25f;
constexpr int PATTERN_TILE_SIZE = 32;

// Decodes a character to its integer index 0..89
uint8_t char_to_value(char c);

// Get stored raw field string (1024 chars) for a given pattern and mask
// Returns nullptr if not found
const char* get_field_string(const std::string& pattern, int mask);

// Pattern band definitions (distances in field units)
struct PatternBands {
    float b0, b1, b2, b3;
};

PatternBands get_pattern_bands(const std::string& pattern);

// Pattern offset ranges [min, max]
struct PatternOffsetRange {
    float min_off, max_off;
};

PatternOffsetRange get_pattern_offset_range(const std::string& pattern);

} // namespace pattern_data
} // namespace atm
