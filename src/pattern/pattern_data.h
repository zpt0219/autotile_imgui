#pragma once

// DO NOT EDIT MANUALLY.
// Distance field string data and decoding tables are verbatim transcriptions
// from reference/generated.ts. See CLAUDE.md Rule #2.

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

} // namespace pattern_data
} // namespace atm
