#include "pattern_paint.h"
#include "pattern_texture.h"
#include "pattern_ribbon.h"
#include "catalog.h"
#include "js_math.h"
#include <algorithm>
#include <cmath>
#include <string_view>

namespace atm {

const RoleColours DEFAULT_ROLE_COLOURS = {
    {  58, 127, 201 }, // #3a7fc9 water
    {  93, 168,  50 }, // #5da832 grass
    { 232, 213, 160 }  // #e8d5a0 sand
};

static inline int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

RGB parse_hex_colour(const std::string& hex) {
    std::string_view s = hex;
    if (!s.empty() && s[0] == '#') s.remove_prefix(1);
    if (s.length() == 3) {
        int r = nibble(s[0]);
        int g = nibble(s[1]);
        int b = nibble(s[2]);
        return { static_cast<uint8_t>(r * 17), static_cast<uint8_t>(g * 17), static_cast<uint8_t>(b * 17) };
    }
    if (s.length() >= 6) {
        int r = (nibble(s[0]) << 4) | nibble(s[1]);
        int g = (nibble(s[2]) << 4) | nibble(s[3]);
        int b = (nibble(s[4]) << 4) | nibble(s[5]);
        return { static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b) };
    }
    return { 0, 0, 0 };
}

std::string to_hex_colour(RGB c) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    return { '#',
             HEX_DIGITS[(c.r >> 4) & 0xf], HEX_DIGITS[c.r & 0xf],
             HEX_DIGITS[(c.g >> 4) & 0xf], HEX_DIGITS[c.g & 0xf],
             HEX_DIGITS[(c.b >> 4) & 0xf], HEX_DIGITS[c.b & 0xf] };
}

std::tuple<double, double, double> rgb_to_hsv(RGB c) {
    double R = static_cast<double>(c.r) / 255.0;
    double G = static_cast<double>(c.g) / 255.0;
    double B = static_cast<double>(c.b) / 255.0;
    double mx = std::max({ R, G, B });
    double mn = std::min({ R, G, B });
    double range = mx - mn;
    if (range == 0.0) return { 0.0, 0.0, mx };
    double rc = (mx - R) / range;
    double gc = (mx - G) / range;
    double bc = (mx - B) / range;
    double h;
    if (R == mx) h = bc - gc;
    else if (G == mx) h = 2.0 + rc - bc;
    else h = 4.0 + gc - rc;
    h = std::fmod(h / 6.0, 1.0);
    return { h < 0.0 ? h + 1.0 : h, range / mx, mx };
}

RGB hsv_to_rgb(double h, double s, double v) {
    auto cl = [](double x) {
        return static_cast<uint8_t>(std::max(0.0, std::min(255.0, std::floor(x + 0.5))));
    };
    if (s == 0.0) {
        uint8_t val = cl(v * 255.0);
        return { val, val, val };
    }
    double i_f = std::floor(h * 6.0);
    int i = static_cast<int>(i_f);
    double f = h * 6.0 - i_f;
    double p = v * (1.0 - s);
    double q = v * (1.0 - s * f);
    double t = v * (1.0 - s * (1.0 - f));
    double table[6][3] = {
        { v, t, p }, { q, v, p }, { p, v, t }, { p, q, v }, { t, p, v }, { v, p, q }
    };
    int idx = ((i % 6) + 6) % 6;
    return { cl(table[idx][0] * 255.0), cl(table[idx][1] * 255.0), cl(table[idx][2] * 255.0) };
}

RGB shade_colour(RGB c, PatternRole role, float t_f) {
    if (t_f <= 0.0f) return c;
    double t = static_cast<double>(t_f);
    struct ShadeRecipe {
        double hue, greyHue, sat, val;
    };
    ShadeRecipe recipe;
    if (role == PatternRole::TerrainA) {
        recipe = { 0.0, 0.541667, 0.129032, 1.000000 };
    } else if (role == PatternRole::TerrainB) {
        recipe = { 0.012037, 0.012037, 0.166667, 0.888889 };
    } else {
        recipe = { 0.0, 0.0, 0.0, 1.0 };
    }

    auto [h0, s0, v0] = rgb_to_hsv(c);
    double h = (s0 < 1e-6) ? recipe.greyHue : std::fmod(h0 + recipe.hue * t + 1.0, 1.0);
    return hsv_to_rgb(
        h,
        std::max(0.0, std::min(1.0, s0 + recipe.sat * t)),
        std::max(0.0, std::min(1.0, v0 * (1.0 + (recipe.val - 1.0) * t)))
    );
}

std::vector<RGB> pattern_ramp(const RoleColours& colours, int band_steps) {
    auto levels = pattern_levels_for(band_steps);
    std::vector<RGB> out;
    out.reserve(levels.size());
    for (const auto& l : levels) {
        RGB base = (l.role == PatternRole::TerrainA) ? colours.terrainA
                 : (l.role == PatternRole::TerrainB) ? colours.terrainB
                 : colours.edge;
        if (l.shade > 0.0f) {
            out.push_back(shade_colour(base, l.role, l.shade));
        } else {
            out.push_back(base);
        }
    }
    return out;
}

struct GrainResult {
    RGB rgb;
    int final_level;
    bool grained;
};

template <typename TargetMatcher>
static GrainResult apply_grain(
    int level,
    int x,
    int y,
    const PaintOptions& opts,
    const std::vector<RGB>& ramp,
    const std::vector<PatternLevelDef>& level_defs,
    int solid,
    int span,
    const TargetMatcher& target_matches
) {
    if (level <= 0 || level >= solid || opts.noises.empty()) {
        return { ramp[level], level, false };
    }

    int step = noise_step(opts.noises, x, y, opts.noise_seed, static_cast<float>(opts.noise_strength)) * span;
    if (step == 0) {
        return { ramp[level], level, false };
    }

    int next_lvl = std::max(0, std::min(solid, level + step));
    PatternRole from_role = level_defs[level].role;
    PatternRole next_role = level_defs[next_lvl].role;

    bool keep_noise = target_matches(from_role) &&
        (next_role != PatternRole::Edge || target_matches(PatternRole::Edge));

    if (!keep_noise) {
        return { ramp[level], level, false };
    }

    RGB rgb;
    if (next_role == PatternRole::Edge && opts.noise_colours.edge.has_value()) {
        rgb = *opts.noise_colours.edge;
    } else if (step < 0 && opts.noise_colours.b.has_value()) {
        rgb = *opts.noise_colours.b;
    } else if (step > 0 && opts.noise_colours.a.has_value()) {
        rgb = *opts.noise_colours.a;
    } else {
        rgb = ramp[next_lvl];
    }
    return { rgb, next_lvl, true };
}

static std::optional<RGB> pick_overlay(
    int level,
    int x,
    int y,
    int p,
    int solid,
    int edge_level,
    const PaintOptions& opts,
    const std::optional<std::vector<RGB>>& rib_ramp,
    const std::optional<BandCoords>& coords,
    float rib_width,
    int rib_shades,
    const std::optional<std::vector<RGB>>& texA,
    const std::optional<std::vector<RGB>>& texB
) {
    if (rib_ramp.has_value() && coords.has_value() && level == edge_level) {
        int k = ribbon_shade_at(
            opts.ribbon.algo, coords->s[p], coords->depth[p], rib_width,
            opts.ribbon.seed, opts.ribbon.amount, rib_shades, opts.ribbon.period, opts.ribbon.invert
        );
        if (k > 0 && k < static_cast<int>(rib_ramp->size())) {
            return (*rib_ramp)[k];
        }
    } else if (texA.has_value() && level == solid) {
        int k = texture_shade_at(
            opts.texture.a.algo, x, y, opts.texture.a.seed, static_cast<float>(opts.texture.a.amount),
            std::max(1, opts.texture.a.shades),
            opts.texture.a.cellScale, opts.texture.a.rippleScale, opts.texture.a.geoScale
        );
        if (k > 0 && k < static_cast<int>(texA->size())) {
            return (*texA)[k];
        }
    } else if (texB.has_value() && level == 0) {
        int k = texture_shade_at(
            opts.texture.b.algo, x, y, opts.texture.b.seed, static_cast<float>(opts.texture.b.amount),
            std::max(1, opts.texture.b.shades),
            opts.texture.b.cellScale, opts.texture.b.rippleScale, opts.texture.b.geoScale
        );
        if (k > 0 && k < static_cast<int>(texB->size())) {
            return (*texB)[k];
        }
    }
    return std::nullopt;
}

TilePainter::TilePainter(const std::string& pattern, const RoleColours& colours, const PaintOptions& opts)
    : pattern_(pattern), colours_(colours), opts_(opts)
{
    auto derived = pattern_ramp(colours_, opts_.band_steps);
    ramp_ = (opts_.ramp.has_value() && opts_.ramp->size() == derived.size()) ? *opts_.ramp : derived;
    level_defs_ = pattern_levels_for(opts_.band_steps);
    fp_ = FieldParams{
        static_cast<float>(opts_.offset_px),
        opts_.tile_size,
        opts_.band_steps,
        opts_.hard_edge_b,
        opts_.edge_seed,
        static_cast<float>(opts_.outline_width)
    };
    solid_ = static_cast<int>(ramp_.size()) - 1;

    if (opts_.texture.a.algo != "none" && opts_.texture.a.amount > 0.0) {
        texA_ = texture_ramp(colours_.terrainA, opts_.texture.a.colour, std::max(1, opts_.texture.a.shades), opts_.texture.a.ramp);
    }

    if (!opts_.transparent_b && opts_.texture.b.algo != "none" && opts_.texture.b.amount > 0.0) {
        texB_ = texture_ramp(colours_.terrainB, opts_.texture.b.colour, std::max(1, opts_.texture.b.shades), opts_.texture.b.ramp);
    }

    edge_level_ = -1;
    for (size_t l = 0; l < level_defs_.size(); ++l) {
        if (level_defs_[l].role == PatternRole::Edge) {
            edge_level_ = static_cast<int>(l);
            break;
        }
    }

    rib_shades_ = std::max(1, opts_.ribbon.shades);
    ribbon_on_ = (opts_.ribbon.algo != "none" && opts_.ribbon.amount > 0.0);

    if (ribbon_on_ && edge_level_ >= 0) {
        rib_ramp_ = texture_ramp(ramp_[edge_level_], opts_.ribbon.colour, rib_shades_, opts_.ribbon.ramp);
    }

    rib_width_ = ribbon_on_ ? std::max(1.0f, outline_width_px(pattern_, opts_.band_steps, opts_.hard_edge_b, static_cast<float>(opts_.outline_width), opts_.tile_size)) : 1.0f;
    span_ = band_noise_span(pattern_, opts_.band_steps);

    static_assert(static_cast<int>(PatternRole::Edge) == 2,
                  "noise_targets_lut_ is indexed by PatternRole; keep it dense and sized to match");
    noise_targets_lut_.fill(false);
    for (auto tid : opts_.noise_targets) {
        if (tid == NoiseTargetId::TerrainA) noise_targets_lut_[static_cast<int>(PatternRole::TerrainA)] = true;
        if (tid == NoiseTargetId::TerrainB) noise_targets_lut_[static_cast<int>(PatternRole::TerrainB)] = true;
        if (tid == NoiseTargetId::Edge)     noise_targets_lut_[static_cast<int>(PatternRole::Edge)] = true;
    }
}

void TilePainter::paint_tile_into(int mask, uint8_t* out_rgba, int row_stride_bytes) const {
    int tile_size = opts_.tile_size;
    int stride = (row_stride_bytes > 0) ? row_stride_bytes : (tile_size * 4);

    std::string grid = pattern_levels_for_mask(pattern_, mask, fp_);

    std::optional<BandCoords> coords;
    if (ribbon_on_ && mask >= 0) {
        coords = pattern_band_coords(pattern_, mask, fp_);
    }

    auto target_matches = [this](PatternRole r) {
        return noise_targets_lut_[static_cast<size_t>(r)];
    };

    for (int y = 0; y < tile_size; ++y) {
        uint8_t* row_ptr = out_rgba + y * stride;
        for (int x = 0; x < tile_size; ++x) {
            int p = y * tile_size + x;
            int level = grid[p] - '0';

            auto grain = apply_grain(level, x, y, opts_, ramp_, level_defs_, solid_, span_, target_matches);
            RGB rgb = grain.rgb;
            if (!grain.grained) {
                if (auto overlay = pick_overlay(level, x, y, p, solid_, edge_level_, opts_, rib_ramp_, coords, rib_width_, rib_shades_, texA_, texB_)) {
                    rgb = *overlay;
                }
            }

            int i = x * 4;
            row_ptr[i]     = rgb.r;
            row_ptr[i + 1] = rgb.g;
            row_ptr[i + 2] = rgb.b;
            row_ptr[i + 3] = (opts_.transparent_b && level_defs_[grain.final_level].role == PatternRole::TerrainB) ? 0 : 255;
        }
    }
}

std::vector<uint8_t> TilePainter::paint_tile_rgba(int mask) const {
    std::vector<uint8_t> out(opts_.tile_size * opts_.tile_size * 4, 0);
    paint_tile_into(mask, out.data(), opts_.tile_size * 4);
    return out;
}

std::vector<uint8_t> paint_pattern_tile_rgba(
    const std::string& pattern,
    int mask,
    const RoleColours& colours,
    const PaintOptions& opts
) {
    return TilePainter(pattern, colours, opts).paint_tile_rgba(mask);
}

} // namespace atm
