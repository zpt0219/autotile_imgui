#include "pattern_ribbon.h"
#include "pattern_texture.h"
#include "catalog.h"
#include "pattern_hash.h"
#include "js_math.h"
#include <algorithm>
#include <cmath>

namespace atm {

// Ribbon seed perturbation salt. Reference: renderSheet.ts ribbonShadeAt()
static const int32_t RIBBON_SALT = 0x2c9f;

int ribbon_shade_at(
    const std::string& id,
    double s,
    double depth,
    double width_px,
    int32_t seed,
    double amount,
    int shades,
    int period_in,
    bool invert
) {
    if (id == "none" || amount <= 0.0 || shades < 1) return 0;

    uint32_t sd = js_math::urshift(static_cast<uint32_t>(seed ^ RIBBON_SALT), 0);
    bool is_flipped = (id == "bevel") ? !invert : invert;
    double dp = (is_flipped && ribbon_uses_invert(id)) ? 1.0 - depth : depth;
    double T = static_cast<double>(std::max(1, period_in));
    double sp = s + static_cast<double>(sd % 32);
    auto cap = [shades](int k) { return std::max(0, std::min(shades, k)); };

    // The along_* motifs are a texture painted along the band axis rather than a
    // motif of their own; the registry says which texture.
    const std::string along = ribbon_along_source(id);
    if (!along.empty()) {
        int ty = std::min(static_cast<int>(width_px - 1.0), static_cast<int>(std::floor(dp * width_px)));
        return texture_shade_at(along, static_cast<int>(std::floor(sp)), ty, seed, static_cast<float>(amount), shades);
    }

    if (id == "bevel") {
        int k = static_cast<int>(std::floor(dp * static_cast<double>(shades + 1)));
        return cap(static_cast<int>(js_math::round(static_cast<double>(std::min(shades, k)) * amount)));
    }
    if (id == "dashes") {
        return (js_math::wrap(sp, T) < T * amount) ? shades : 0;
    }
    if (id == "ticks") {
        return (js_math::wrap(sp, T) < 1.0) ? cap(static_cast<int>(js_math::round(static_cast<double>(shades) * amount))) : 0;
    }
    if (id == "beads") {
        double ds = js_math::wrap(sp + T / 2.0, T) - T / 2.0;
        double dd = (dp - 0.5) * width_px;
        double r = std::max(1.0, amount * std::min(T / 2.0, width_px / 2.0));
        double q = std::sqrt(ds * ds + dd * dd);
        if (q > r) return 0;
        return (q > r - 1.0) ? cap(shades - 1) : shades;
    }
    if (id == "rope") {
        double u = js_math::wrap(sp + dp * width_px, T) / T;
        if (u >= amount) return 0;
        double t = (amount <= 0.0) ? 0.0 : u / amount;
        return cap(1 + static_cast<int>(std::floor(static_cast<double>(shades - 1) * (1.0 - std::abs(2.0 * t - 1.0)))));
    }
    if (id == "wave") {
        double split = 0.5 + 0.35 * js_math::sin((2.0 * js_math::PI * sp) / T);
        return (dp > split) ? cap(static_cast<int>(js_math::round(static_cast<double>(shades) * amount))) : 0;
    }
    if (id == "grain") {
        float n = hash01(static_cast<int32_t>(std::floor(sp)), static_cast<int32_t>(std::floor(dp * width_px)), sd);
        double cut = 1.0 - std::min(1.0, amount);
        if (static_cast<double>(n) < cut) return 0;
        double u = (cut >= 1.0) ? 1.0 : (static_cast<double>(n) - cut) / (1.0 - cut);
        return std::min(shades, 1 + static_cast<int>(std::floor(static_cast<double>(shades) * u * u)));
    }
    if (id == "speckle") {
        int ix = static_cast<int>(std::floor(sp));
        int iy = static_cast<int>(std::floor(dp * width_px));
        if (((ix + iy) & 1) == 1) return 0;
        // Speckle noise salt (0x51). Reference: renderSheet.ts speckle()
        return (static_cast<double>(hash01(ix, iy, sd ^ 0x51)) < amount * 2.0) ? shades : 0;
    }

    return 0;
}

} // namespace atm
