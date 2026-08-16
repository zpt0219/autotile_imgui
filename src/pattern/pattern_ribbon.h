#pragma once

#include "pattern_paint.h"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace atm {

int ribbon_shade_at(
    const std::string& id,
    double s,
    double depth,
    double width_px,
    int32_t seed,
    double amount,
    int shades,
    int period,
    bool invert
);

} // namespace atm
