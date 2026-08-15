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

// fdlibm / V8 exact Math.sin, Math.cos, Math.atan2, Math.hypot
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
