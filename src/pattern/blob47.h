#pragma once

#include <cstdint>
#include <array>

namespace atm {

// Direction bit layout
constexpr uint8_t N  = 1;
constexpr uint8_t E  = 2;
constexpr uint8_t S  = 4;
constexpr uint8_t W  = 8;
constexpr uint8_t NE = 16;
constexpr uint8_t SE = 32;
constexpr uint8_t SW = 64;
constexpr uint8_t NW = 128;

constexpr int BLOB47_COLS = 8;
constexpr int BLOB47_ROWS = 6;
constexpr int BLOB47_SLOTS = 48;
constexpr int BLOB47_CANONICAL_COUNT = 47;

// 48-slot sheet layout
extern const std::array<uint8_t, BLOB47_SLOTS> BLOB47_LAYOUT;

// 47 canonical masks, ascending
extern const std::array<uint8_t, BLOB47_CANONICAL_COUNT> BLOB47_MASKS;

// Drop corner bits whose two adjacent edges are not both set
uint8_t canonicalize_blob_mask(uint8_t mask);

// 0..46 index for canonical mask
int blob_index_for_mask(uint8_t mask);

// 0..47 slot on sheet for raw neighbourhood mask
int blob_slot_for_mask(uint8_t mask);

} // namespace atm
