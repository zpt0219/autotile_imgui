#include "blob47.h"
#include "js_math.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace atm {

const std::array<uint8_t, BLOB47_SLOTS> BLOB47_LAYOUT = {
    6,  10,  46,  76,  38, 110,  78,  12,
    7,  14,  31, 175, 127, 255, 205,   5,
   39,  79,  15,  63, 223, 159, 141,   1,
   23, 143,  13,  55, 239, 111,  77,   4,
    3,  11,  47,  95, 191, 255, 207,   9,
    0,   2,  27, 137,  19, 155, 139,   8,
};

const std::array<uint8_t, BLOB47_CANONICAL_COUNT> BLOB47_MASKS = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    19, 23, 27, 31, 38, 39, 46, 47, 55, 63, 76, 77, 78, 79,
    95, 110, 111, 127, 137, 139, 141, 143, 155, 159, 175,
    191, 205, 207, 223, 239, 255
};

uint8_t canonicalize_blob_mask(uint8_t mask) {
    uint8_t m = mask;
    if ((m & (N | E)) != (N | E)) m &= ~NE;
    if ((m & (S | E)) != (S | E)) m &= ~SE;
    if ((m & (S | W)) != (S | W)) m &= ~SW;
    if ((m & (N | W)) != (N | W)) m &= ~NW;
    return m;
}

namespace {
struct Tables {
    int16_t mask_to_index[256];
    int16_t mask_to_slot[256];

    Tables() {
        std::fill(std::begin(mask_to_index), std::end(mask_to_index), -1);
        std::fill(std::begin(mask_to_slot), std::end(mask_to_slot), -1);

        for (int m = 0; m < 256; ++m) {
            uint8_t canon = canonicalize_blob_mask(static_cast<uint8_t>(m));
            auto it = std::find(BLOB47_MASKS.begin(), BLOB47_MASKS.end(), canon);
            if (it != BLOB47_MASKS.end()) {
                mask_to_index[m] = static_cast<int16_t>(std::distance(BLOB47_MASKS.begin(), it));
            }

            auto it_slot = std::find(BLOB47_LAYOUT.begin(), BLOB47_LAYOUT.end(), canon);
            if (it_slot != BLOB47_LAYOUT.end()) {
                mask_to_slot[m] = static_cast<int16_t>(std::distance(BLOB47_LAYOUT.begin(), it_slot));
            }
        }
    }
};

const Tables& get_tables() {
    static const Tables tables;
    return tables;
}
} // namespace

int blob_index_for_mask(uint8_t mask) {
    return get_tables().mask_to_index[mask];
}

int blob_slot_for_mask(uint8_t mask) {
    return get_tables().mask_to_slot[mask];
}

} // namespace atm
