#include "pattern_ribbon.h"
#include "pattern_texture.h"
#include "js_math.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace atm {

static const int32_t RIBBON_SALT = 0x2c9f;

static inline double modp(double v, double m) {
    return std::fmod(std::fmod(v, m) + m, m);
}

static float hash01(int32_t ix, int32_t iy, int32_t seed) {
    int32_t n = js_math::imul(ix, 374761393) + js_math::imul(iy, 668265263) + js_math::imul(seed, 1442695041);
    n = js_math::imul(n ^ static_cast<int32_t>(js_math::urshift(static_cast<uint32_t>(n), 13)), 1274126177);
    uint32_t uval = static_cast<uint32_t>(n ^ static_cast<int32_t>(js_math::urshift(static_cast<uint32_t>(n), 16)));
    return static_cast<float>(static_cast<double>(uval) / 4294967296.0);
}

static bool ribbon_uses_invert(const std::string& id) {
    return id == "bevel" || id == "wave" || id == "rope";
}

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

    static const std::unordered_map<std::string, std::string> ALONG_SOURCE = {
        { "along_brick_wall", "brick_wall" },
        { "along_cobbles2", "cobbles2" },
        { "along_weave", "weave" },
        { "along_stone_floor", "stone_floor" },
        { "along_breeze_block", "breeze_block" },
        { "along_octagonal", "octagonal" },
    };

    auto it = ALONG_SOURCE.find(id);
    if (it != ALONG_SOURCE.end()) {
        int ty = std::min(static_cast<int>(width_px - 1.0), static_cast<int>(std::floor(dp * width_px)));
        return texture_shade_at(it->second, static_cast<int>(std::floor(sp)), ty, seed, static_cast<float>(amount), shades);
    }

    if (id == "bevel") {
        int k = static_cast<int>(std::floor(dp * static_cast<double>(shades + 1)));
        return cap(static_cast<int>(js_math::round(static_cast<double>(std::min(shades, k)) * amount)));
    }
    if (id == "dashes") {
        return (modp(sp, T) < T * amount) ? shades : 0;
    }
    if (id == "ticks") {
        return (modp(sp, T) < 1.0) ? cap(static_cast<int>(js_math::round(static_cast<double>(shades) * amount))) : 0;
    }
    if (id == "beads") {
        double ds = modp(sp + T / 2.0, T) - T / 2.0;
        double dd = (dp - 0.5) * width_px;
        double r = std::max(1.0, amount * std::min(T / 2.0, width_px / 2.0));
        double q = std::sqrt(ds * ds + dd * dd);
        if (q > r) return 0;
        return (q > r - 1.0) ? cap(shades - 1) : shades;
    }
    if (id == "rope") {
        double u = modp(sp + dp * width_px, T) / T;
        if (u >= amount) return 0;
        double t = (amount <= 0.0) ? 0.0 : u / amount;
        return cap(1 + static_cast<int>(std::floor(static_cast<double>(shades - 1) * (1.0 - std::abs(2.0 * t - 1.0)))));
    }
    if (id == "wave") {
        const double PI_D = 3.14159265358979323846;
        double split = 0.5 + 0.35 * js_math::sin((2.0 * PI_D * sp) / T);
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
        return (static_cast<double>(hash01(ix, iy, sd ^ 0x51)) < amount * 2.0) ? shades : 0;
    }

    return 0;
}

} // namespace atm
