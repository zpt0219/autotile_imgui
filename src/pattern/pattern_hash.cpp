#include "pattern_hash.h"
#include "js_math.h"

namespace atm {

uint32_t hash_bits(int32_t x, int32_t y, int32_t seed) {
    int32_t n = js_math::imul(x, 374761393) + js_math::imul(y, 668265263) + js_math::imul(seed, 1442695041);
    n = js_math::imul(n ^ static_cast<int32_t>(js_math::urshift(static_cast<uint32_t>(n), 13)), 1274126177);
    return static_cast<uint32_t>(n ^ static_cast<int32_t>(js_math::urshift(static_cast<uint32_t>(n), 16)));
}

uint32_t seed_bits(int32_t seed, int32_t salt) {
    if (seed == 0) return 0;
    int32_t n = js_math::imul(seed, 0x9e3779b1) ^ js_math::imul(salt, 0x85ebca6b);
    n = js_math::imul(n ^ static_cast<int32_t>(js_math::urshift(static_cast<uint32_t>(n), 15)), 0xc2b2ae35);
    return js_math::urshift(static_cast<uint32_t>(n ^ static_cast<int32_t>(js_math::urshift(static_cast<uint32_t>(n), 13))), 0);
}

} // namespace atm
