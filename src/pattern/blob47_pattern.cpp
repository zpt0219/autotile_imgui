#include "blob47_pattern.h"
#include "pattern_data.h"
#include "catalog.h"
#include "pattern_noise.h"
#include "pattern_hash.h"
#include "js_math.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace atm {

std::vector<PatternLevelDef> pattern_levels_for(int steps) {
    int inner = std::max(1, steps - 2);
    std::vector<PatternLevelDef> out = {
        { PatternRole::TerrainB, 0.0f },
        { PatternRole::TerrainB, 1.0f },
        { PatternRole::Edge,     0.0f }
    };
    for (int k = inner; k >= 1; --k) {
        out.push_back({ PatternRole::TerrainA, static_cast<float>(k) / static_cast<float>(inner) });
    }
    out.push_back({ PatternRole::TerrainA, 0.0f });
    return out;
}

std::vector<float> bands_for(
    const std::string& pattern,
    int steps,
    bool hard_edge_b,
    float outline_width
) {
    auto base = pattern_bands(pattern);
    float adjusted[4] = { base.b0, base.b1, base.b2, base.b3 };
    if (outline_width > 0.0f) {
        float mid = (base.b1 + base.b2) / 2.0f;
        float w_shade_b = base.b1 - base.b0;
        float w_shade_a = base.b3 - base.b2;
        adjusted[1] = std::max(base.b0, mid - outline_width / 2.0f);
        adjusted[2] = adjusted[1] + outline_width;
        adjusted[0] = std::max(base.b0, adjusted[1] - w_shade_b);
        adjusted[3] = adjusted[2] + w_shade_a;
    }
    int extra = std::max(0, std::min(MAX_BAND_STEPS, steps) - MIN_BAND_STEPS);
    std::vector<float> out = { adjusted[0], adjusted[1], adjusted[2], adjusted[3] };
    for (int k = 1; k <= extra; ++k) {
        out.push_back(adjusted[3] + BAND_STEP_PX * static_cast<float>(k));
    }
    if (!hard_edge_b) return out;
    float w = out[1] - out[0];
    for (size_t i = 1; i < out.size(); ++i) {
        out[i] -= w;
    }
    return out;
}

float outline_width_px(
    const std::string& pattern,
    int steps,
    bool hard_edge_b,
    float outline_width,
    int tile_size
) {
    auto b = bands_for(pattern, steps, hard_edge_b, outline_width);
    return ((b[2] - b[1]) * static_cast<float>(tile_size)) / static_cast<float>(pattern_data::PATTERN_TILE_SIZE);
}

int band_noise_span(const std::string& pattern, int steps) {
    auto width_fn = [&pattern](int n) -> float {
        auto b = bands_for(pattern, n);
        return b.back() - b.front();
    };
    float base = width_fn(MIN_BAND_STEPS);
    if (base <= 0.0f) return 1;
    float ratio = width_fn(steps) / base;
    return std::max(1, std::min(2, static_cast<int>(js_math::round(ratio))));
}

float clamp_offset(const std::string& pattern, float offset_px) {
    auto r = pattern_offset_range(pattern);
    return std::max(r.min_off, std::min(r.max_off, offset_px));
}

float edge_jitter_amplitude(const std::string& pattern, float offset_px) {
    if (!pattern_is_reseedable(pattern)) return 0.0f;
    auto r = pattern_offset_range(pattern);
    return std::max(0.0f, r.max_off - std::max(0.0f, clamp_offset(pattern, offset_px)));
}

const char* pattern_field_for_mask(const std::string& pattern, int mask) {
    return pattern_data::get_field_string(pattern_field_source(pattern), mask);
}

static double edge_noise(double u, double v, int32_t seed) {
    const double per = 4.0;
    double fx = (u / static_cast<double>(pattern_data::PATTERN_TILE_SIZE)) * per;
    double fy = (v / static_cast<double>(pattern_data::PATTERN_TILE_SIZE)) * per;
    int32_t x0 = static_cast<int32_t>(std::floor(fx));
    int32_t y0 = static_cast<int32_t>(std::floor(fy));
    auto fade = [](double t) { return t * t * (3.0 - 2.0 * t); };
    double tx = fade(fx - static_cast<double>(x0));
    double ty = fade(fy - static_cast<double>(y0));
    auto h = [seed, per](int32_t ix, int32_t iy) -> double {
        int32_t iper = static_cast<int32_t>(per);
        int32_t wx = ((ix % iper) + iper) % iper;
        int32_t wy = ((iy % iper) + iper) % iper;
        return (static_cast<double>(hash_bits(wx, wy, seed)) / 4294967296.0) * 2.0 - 1.0;
    };
    double a = h(x0, y0) * (1.0 - tx) + h(x0 + 1, y0) * tx;
    double b = h(x0, y0 + 1) * (1.0 - tx) + h(x0 + 1, y0 + 1) * tx;
    return a * (1.0 - ty) + b * ty;
}

static double sample_field(const char* field, int N, double u, double v) {
    double scale = static_cast<double>(N) / static_cast<double>(pattern_data::PATTERN_TILE_SIZE);
    double fu = u * scale;
    double fv = v * scale;
    int x0 = static_cast<int>(std::floor(fu));
    int y0 = static_cast<int>(std::floor(fv));
    double tx = fu - static_cast<double>(x0);
    double ty = fv - static_cast<double>(y0);
    auto cl = [N](int n) { return (n < 0) ? 0 : (n > N - 1 ? N - 1 : n); };
    int gx0 = cl(x0), gx1 = cl(x0 + 1), gy0 = cl(y0), gy1 = cl(y0 + 1);
    auto g = [field, N](int x, int y) -> double {
        return static_cast<double>(pattern_data::char_to_value(field[y * N + x]));
    };
    double a = g(gx0, gy0) * (1.0 - tx) + g(gx1, gy0) * tx;
    double b = g(gx0, gy1) * (1.0 - tx) + g(gx1, gy1) * tx;
    return (a * (1.0 - ty) + b * ty) * static_cast<double>(pattern_data::FIELD_STEP);
}

static double wave_offset_at(
    const char* field,
    double u,
    double v,
    double d_base,
    int edge_seed,
    double off
) {
    double wavelength = 16.0;
    double preset_amp = 1.4;
    double phase = 0.0;
    if (edge_seed != 0) {
        // Phase hash for the wave pattern (salt: 0x1f3b2a). Reference: renderSheet.ts wave()
        int32_t n1 = js_math::imul(edge_seed, 374761393) ^ 0x1f3b2a;
        n1 = js_math::imul(n1 ^ (static_cast<int32_t>(js_math::urshift(static_cast<uint32_t>(n1), 13))), 1274126177);
        int32_t hash = std::abs(n1 ^ (static_cast<int32_t>(js_math::urshift(static_cast<uint32_t>(n1), 16))));
        wavelength = ((hash & 1) == 0) ? 16.0 : 32.0;
        preset_amp = 1.3 + static_cast<double>(hash % 8) * 0.1;
        phase = static_cast<double>(hash % 13);
    }

    double headroom = static_cast<double>(edge_jitter_amplitude("wave", static_cast<float>(off)));
    double wave_amp = std::max(0.0, std::min(preset_amp, headroom));

    double gx = sample_field(field, pattern_data::PATTERN_TILE_SIZE, u + 0.5, v) - sample_field(field, pattern_data::PATTERN_TILE_SIZE, u - 0.5, v);
    double gy = sample_field(field, pattern_data::PATTERN_TILE_SIZE, u, v + 0.5) - sample_field(field, pattern_data::PATTERN_TILE_SIZE, u, v - 0.5);
    double len_sq = gx * gx + gy * gy;
    double wy2 = 1.0;
    double wx2 = 0.0;
    if (len_sq > 1e-4) {
        wy2 = (gy * gy) / len_sq;
        wx2 = (gx * gx) / len_sq;
    }

    double wu = js_math::sin((2.0 * js_math::PI * (u + phase)) / wavelength);
    double wv = js_math::sin((2.0 * js_math::PI * (v + phase)) / wavelength);
    double wave_val = wy2 * wu + wx2 * wv;

    double border_fade = std::max(0.0, std::min(1.0, (d_base - 2.5) / 2.0));
    return wave_amp * border_fade * wave_val;
}

std::string pattern_levels_for_mask(
    const std::string& pattern,
    int mask,
    const FieldParams& params
) {
    int tile_size = params.tile_size;
    if (mask < 0) {
        return std::string(tile_size * tile_size, '0');
    }

    const char* field = pattern_field_for_mask(pattern, mask);
    if (!field) {
        return std::string(tile_size * tile_size, '0');
    }

    double off = static_cast<double>(clamp_offset(pattern, params.offset_px));
    double amp = (params.edge_seed == 0) ? 0.0 : static_cast<double>(edge_jitter_amplitude(pattern, static_cast<float>(off)));
    auto bands = bands_for(pattern, params.band_steps, params.hard_edge_b, params.outline_width);
    double scale = static_cast<double>(pattern_data::PATTERN_TILE_SIZE) / static_cast<double>(tile_size);
    bool is_wave = (pattern == "wave");

    std::string out;
    out.reserve(tile_size * tile_size);

    for (int y = 0; y < tile_size; ++y) {
        double v = (static_cast<double>(y) + 0.5) * scale - 0.5;
        for (int x = 0; x < tile_size; ++x) {
            double u = (static_cast<double>(x) + 0.5) * scale - 0.5;
            double d_base = sample_field(field, pattern_data::PATTERN_TILE_SIZE, u, v);
            double wave_offset = is_wave ? wave_offset_at(field, u, v, d_base, params.edge_seed, off) : 0.0;
            double jitter = (amp > 0.0 && !is_wave) ? amp * edge_noise(u, v, params.edge_seed) : 0.0;
            double d = d_base + off + wave_offset + jitter;

            size_t level = 0;
            while (level < bands.size() && d >= static_cast<double>(bands[level])) {
                level++;
            }
            out.push_back(static_cast<char>('0' + level));
        }
    }
    return out;
}

static double radius_at(double c, int n) {
    const double GRAD_RADIUS = 3.0;
    return std::max(1.0, std::min(GRAD_RADIUS, std::min(c, static_cast<double>(n - 1) - c)));
}

static double derivative(const char* field, int N, double u, double v, bool horizontal) {
    auto at = [field, N, horizontal, u, v](double d, double k) {
        return horizontal ? sample_field(field, N, u + d, v + k) : sample_field(field, N, u + k, v + d);
    };
    double c = horizontal ? u : v;
    double r = radius_at(c, N);
    double lo = std::max(-r, -c);
    double hi = std::min(r, static_cast<double>(N - 1) - c);
    double span = hi - lo;
    if (span <= 0.0) return 0.0;
    double total = 0.0;
    int n = 0;
    int r_int = static_cast<int>(r);
    for (int k = -r_int; k <= r_int; ++k) {
        total += at(hi, static_cast<double>(k)) - at(lo, static_cast<double>(k));
        n++;
    }
    return total / (static_cast<double>(n) * span);
}

BandCoords pattern_band_coords(
    const std::string& pattern,
    int mask,
    const FieldParams& params
) {
    int tile_size = params.tile_size;
    int n = tile_size * tile_size;
    BandCoords coords;
    coords.s.resize(n, 0.0f);
    coords.depth.resize(n, 0.0f);

    if (mask < 0) return coords;
    const char* field = pattern_field_for_mask(pattern, mask);
    if (!field) return coords;

    double off = static_cast<double>(clamp_offset(pattern, params.offset_px));
    double amp = (params.edge_seed == 0) ? 0.0 : static_cast<double>(edge_jitter_amplitude(pattern, static_cast<float>(off)));
    auto bands = bands_for(pattern, params.band_steps, params.hard_edge_b, params.outline_width);
    double inner = static_cast<double>(bands[1]);
    double width = std::max(1e-6, static_cast<double>(bands[2] - bands[1]));
    double scale = static_cast<double>(pattern_data::PATTERN_TILE_SIZE) / static_cast<double>(tile_size);

    for (int y = 0; y < tile_size; ++y) {
        double v = (static_cast<double>(y) + 0.5) * scale - 0.5;
        for (int x = 0; x < tile_size; ++x) {
            double u = (static_cast<double>(x) + 0.5) * scale - 0.5;
            double jitter = (amp > 0.0) ? amp * edge_noise(u, v, params.edge_seed) : 0.0;
            double d = sample_field(field, pattern_data::PATTERN_TILE_SIZE, u, v) + off + jitter;
            int i = y * tile_size + x;
            coords.depth[i] = static_cast<float>(std::max(0.0, std::min(1.0, (d - inner) / width)));

            if (d < inner || d >= static_cast<double>(bands[2])) continue;

            double gx = derivative(field, pattern_data::PATTERN_TILE_SIZE, u, v, true);
            double gy = derivative(field, pattern_data::PATTERN_TILE_SIZE, u, v, false);
            double ang = js_math::atan2(gx, -gy);
            if (ang < 0.0) ang += js_math::PI;
            if (ang >= js_math::PI) ang -= js_math::PI;
            int bucket = static_cast<int>(std::floor((ang + (js_math::PI / 8.0)) / (js_math::PI / 4.0))) % 4;

            coords.s[i] = (bucket == 0) ? static_cast<float>(x)
                        : (bucket == 1) ? static_cast<float>(x + y)
                        : (bucket == 2) ? static_cast<float>(y)
                        : static_cast<float>(x - y);
        }
    }
    return coords;
}

} // namespace atm
