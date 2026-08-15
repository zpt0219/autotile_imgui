#pragma once

#include <cstdint>

namespace atm {

// Ported verbatim from the reference implementation. Every texture, ribbon,
// grain and edge-jitter sampler in this project shares this one mixer — the
// entire corpus was baked with it. Do not touch the constants.
uint32_t hash_bits(int32_t x, int32_t y, int32_t seed);

// The 0..1 float form used by textures / ribbons / grain.
inline float hash01(int32_t x, int32_t y, int32_t seed) {
    return static_cast<float>(static_cast<double>(hash_bits(x, y, seed)) / 4294967296.0);
}

// 32-bit seed perturbation used by blue noise. Reference: renderSheet.ts.
uint32_t seed_bits(int32_t seed, int32_t salt);

} // namespace atm
