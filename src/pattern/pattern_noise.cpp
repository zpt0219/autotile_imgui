#include "pattern_noise.h"
#include "pattern_hash.h"
#include "js_math.h"
#include <algorithm>

namespace atm {

NoiseId parse_noise_id(const std::string& name) {
    if (name == "blue") return NoiseId::Blue;
    if (name == "ordered") return NoiseId::Ordered;
    return NoiseId::White;
}

std::string noise_id_to_string(NoiseId id) {
    switch (id) {
        case NoiseId::Blue: return "blue";
        case NoiseId::Ordered: return "ordered";
        default: return "white";
    }
}

NoiseTargetId parse_noise_target_id(const std::string& name) {
    if (name == "terrainA") return NoiseTargetId::TerrainA;
    if (name == "terrainB") return NoiseTargetId::TerrainB;
    return NoiseTargetId::Edge;
}

std::string noise_target_id_to_string(NoiseTargetId id) {
    switch (id) {
        case NoiseTargetId::TerrainA: return "terrainA";
        case NoiseTargetId::TerrainB: return "terrainB";
        default: return "edge";
    }
}

static const float MAX_SHARE = 0.5f;

static float get_noise_amount(NoiseId id) {
    switch (id) {
        case NoiseId::White: return 0.22f;
        case NoiseId::Blue: return 0.24f;
        case NoiseId::Ordered: return 0.19f;
        default: return 0.0f;
    }
}

static const uint8_t BLUE[256] = {
  228, 182, 246, 42, 98, 29, 127, 44, 211, 8, 83, 164, 217, 2, 93, 123,
  85, 26, 135, 74, 163, 238, 194, 89, 175, 117, 225, 31, 106, 151, 252, 40,
  174, 109, 196, 218, 3, 141, 60, 229, 25, 156, 49, 199, 66, 189, 55, 210,
  9, 236, 53, 91, 179, 111, 19, 149, 76, 250, 100, 140, 239, 16, 130, 158,
  65, 147, 125, 36, 247, 70, 221, 185, 124, 193, 4, 171, 84, 114, 227, 97,
  249, 192, 17, 201, 134, 167, 46, 102, 33, 64, 208, 45, 219, 27, 178, 38,
  79, 169, 103, 231, 82, 10, 204, 233, 159, 242, 92, 126, 157, 63, 139, 213,
  119, 52, 32, 155, 59, 118, 144, 86, 14, 136, 181, 18, 253, 195, 96, 1,
  202, 244, 132, 214, 180, 254, 41, 190, 113, 54, 222, 75, 108, 48, 226, 146,
  23, 183, 87, 5, 105, 24, 209, 68, 232, 165, 34, 148, 188, 12, 168, 69,
  110, 62, 153, 230, 72, 173, 131, 152, 0, 99, 207, 121, 235, 90, 129, 237,
  160, 43, 206, 116, 35, 241, 94, 50, 248, 184, 73, 21, 56, 212, 39, 197,
  95, 251, 20, 138, 162, 198, 22, 220, 122, 37, 137, 240, 172, 150, 81, 6,
  133, 216, 77, 186, 88, 57, 112, 177, 80, 161, 215, 101, 13, 115, 245, 187,
  166, 107, 51, 224, 7, 234, 143, 15, 200, 61, 28, 191, 78, 223, 58, 30,
  67, 11, 154, 120, 203, 170, 71, 255, 104, 145, 243, 128, 47, 176, 142, 205,
};

static const uint8_t BAYER8[64] = {
  0, 32, 8, 40, 2, 34, 10, 42,
  48, 16, 56, 24, 50, 18, 58, 26,
  12, 44, 4, 36, 14, 46, 6, 38,
  60, 28, 52, 20, 62, 30, 54, 22,
  3, 35, 11, 43, 1, 33, 9, 41,
  51, 19, 59, 27, 49, 17, 57, 25,
  15, 47, 7, 39, 13, 45, 5, 37,
  63, 31, 55, 23, 61, 29, 53, 21,
};

static inline int wrap16(int v) {
    return ((v % 16) + 16) % 16;
}

float sample_noise(NoiseId noise, int x, int y, int32_t seed) {
    int px = wrap16(x);
    int py = wrap16(y);
    switch (noise) {
        case NoiseId::Blue: {
            uint32_t s = seed_bits(seed, 1);
            int ax = (px + (s & 15)) & 15;
            int ay = (py + ((s >> 4) & 15)) & 15;
            uint32_t t = (s >> 8) & 7;
            if (t & 1) { int k = ax; ax = ay; ay = k; }
            if (t & 2) ax = 15 - ax;
            if (t & 4) ay = 15 - ay;
            return static_cast<float>(BLUE[ay * 16 + ax]) / 256.0f;
        }
        case NoiseId::Ordered: {
            uint32_t s = seed_bits(seed, 2);
            int ax = (px + (s & 7)) & 7;
            int ay = (py + ((s >> 3) & 7)) & 7;
            return static_cast<float>(BAYER8[ay * 8 + ax]) / 64.0f;
        }
        case NoiseId::White:
            return hash01(px, py, seed);
        default:
            return 0.5f;
    }
}

static int step_of(NoiseId noise, int x, int y, int32_t seed, float strength) {
    float p = std::min(MAX_SHARE, get_noise_amount(noise) * strength);
    if (p <= 0.0f) return 0;
    float n = sample_noise(noise, x, y, seed);
    if (n < p) return -1;
    if (n >= 1.0f - p) return 1;
    return 0;
}

int noise_step(
    const std::vector<NoiseId>& noises,
    int x,
    int y,
    int32_t seed,
    float strength
) {
    if (strength <= 0.0f) return 0;
    int total = 0;
    for (NoiseId id : noises) {
        total += step_of(id, x, y, seed, strength);
    }
    return (total < -1) ? -1 : (total > 1) ? 1 : total;
}

} // namespace atm
