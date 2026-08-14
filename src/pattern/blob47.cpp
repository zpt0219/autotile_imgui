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

static inline float box_dist(float px, float py, float x0, float y0, float x1, float y1) {
    float dx = std::max({ x0 - px, 0.0f, px - x1 });
    float dy = std::max({ y0 - py, 0.0f, py - y1 });
    return js_math::hypot(dx, dy);
}

struct NeighborOffset {
    float dx, dy;
    uint8_t bit;
};

static const NeighborOffset ORTHO[4] = {
    {  0.0f, -1.0f, N },
    {  1.0f,  0.0f, E },
    {  0.0f,  1.0f, S },
    { -1.0f,  0.0f, W }
};

static const NeighborOffset DIAGONAL[4] = {
    {  1.0f, -1.0f, NE },
    {  1.0f,  1.0f, SE },
    { -1.0f,  1.0f, SW },
    { -1.0f, -1.0f, NW }
};

static const int CONVEX_PAIRS[4][2] = {
    { 0, 1 }, // N + E
    { 1, 2 }, // E + S
    { 2, 3 }, // S + W
    { 3, 0 }  // W + N
};

float blob_weight_at(float tx, float ty, uint8_t mask, float radius, float corner_rounding) {
    if (radius <= 0.0f || radius >= 1.0f || !std::isfinite(radius)) {
        return 1.0f;
    }

    float ortho_dist[4] = {
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity()
    };

    for (int i = 0; i < 4; ++i) {
        if (mask & ORTHO[i].bit) continue;
        ortho_dist[i] = box_dist(tx, ty, ORTHO[i].dx, ORTHO[i].dy, ORTHO[i].dx + 1.0f, ORTHO[i].dy + 1.0f);
    }

    float d = std::min({ ortho_dist[0], ortho_dist[1], ortho_dist[2], ortho_dist[3] });

    for (int i = 0; i < 4; ++i) {
        if (mask & DIAGONAL[i].bit) continue;
        d = std::min(d, box_dist(tx, ty, DIAGONAL[i].dx, DIAGONAL[i].dy, DIAGONAL[i].dx + 1.0f, DIAGONAL[i].dy + 1.0f));
    }

    if (corner_rounding > 0.0f) {
        float r = corner_rounding;
        for (int i = 0; i < 4; ++i) {
            float da = ortho_dist[CONVEX_PAIRS[i][0]];
            float db = ortho_dist[CONVEX_PAIRS[i][1]];
            if (da < r && db < r) {
                float d_corner = r - js_math::hypot(r - da, r - db);
                d = std::min(d, d_corner);
            }
        }
    }

    if (!std::isfinite(d)) return 1.0f;
    return std::max(0.0f, std::min(1.0f, d / radius));
}

} // namespace atm
