#include "js_math.h"
#include <cmath>
#include <limits>
#include <algorithm>

namespace atm {
namespace js_math {

// Forwarding, not reimplementing — see the note in js_math.h for why these
// three keep a seam they do not currently use.
double sin(double x) {
    return std::sin(x);
}

double cos(double x) {
    return std::cos(x);
}

double atan2(double y, double x) {
    return std::atan2(y, x);
}

// V8 Math.hypot(x, y) - compensated algorithm
double hypot(double x, double y) {
    double ax = std::abs(x);
    double ay = std::abs(y);
    if (std::isinf(ax) || std::isinf(ay)) return std::numeric_limits<double>::infinity();
    if (std::isnan(ax) || std::isnan(ay)) return std::numeric_limits<double>::quiet_NaN();
    double max_v = (ax > ay) ? ax : ay;
    double min_v = (ax > ay) ? ay : ax;
    if (min_v == 0.0) return max_v;
    double r = min_v / max_v;
    return max_v * std::sqrt(1.0 + r * r);
}

} // namespace js_math
} // namespace atm
