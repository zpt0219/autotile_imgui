#pragma once

#include <cstdint>
#include <cmath>

namespace atm {
namespace js_math {

constexpr double PI = 3.14159265358979323846;

// JS Math.imul(a, b) -> 32-bit integer multiplication
inline int32_t imul(int32_t a, int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

// JS >>> (unsigned right shift)
inline uint32_t urshift(uint32_t value, uint32_t shift) {
    return value >> (shift & 31);
}

// JS Math.round(x) - round half away from zero / towards +inf
inline double round(double x) {
    if (x < 0.0 && x >= -0.5) return -0.0;
    if (x == 0.0 && std::signbit(x)) return -0.0;
    return std::floor(x + 0.5);
}

inline float round(float x) {
    if (x < 0.0f && x >= -0.5f) return -0.0f;
    if (x == 0.0f && std::signbit(x)) return -0.0f;
    return std::floor(x + 0.5f);
}

// JS Math.sqrt(x) - IEEE 754 sqrt
inline double sqrt(double x) {
    return std::sqrt(x);
}

inline float sqrt(float x) {
    return std::sqrt(x);
}

// V8's Math.sin / cos / atan2 / hypot.
//
// Only `hypot` needed a real reimplementation — V8 uses a compensated form that
// `std::hypot` does not match. The other three currently forward straight to
// `std::`, which agreed with V8 on every one of the 1161 corpus sheets and on
// the 500-row conformance vectors in tests/data/js_math_vectors.json.
//
// They stay behind this seam anyway: `std::sin` and friends are not required to
// be correctly rounded, so a different libm could diverge in the last ulp — and
// in this project a last-ulp difference is not a small error, it flips a
// quantiser boundary and repaints a pixel. If that ever happens, drop an fdlibm
// implementation in here rather than chasing it through the callers.
double sin(double x);
double cos(double x);
double atan2(double y, double x);
double hypot(double x, double y);

// float overloads
inline float sin(float x) { return static_cast<float>(sin(static_cast<double>(x))); }
inline float cos(float x) { return static_cast<float>(cos(static_cast<double>(x))); }
inline float atan2(float y, float x) { return static_cast<float>(atan2(static_cast<double>(y), static_cast<double>(x))); }
inline float hypot(float x, float y) { return static_cast<float>(hypot(static_cast<double>(x), static_cast<double>(y))); }

} // namespace js_math
} // namespace atm
