#include "pattern_paint.h"
#include "pattern_texture.h"
#include "pattern_ribbon.h"
#include "js_math.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace atm {

const RoleColours REFERENCE_ROLE_COLOURS = {
    { 248, 248, 248 }, // terrainA
    { 176, 216,  72 }, // terrainB
    { 175, 198, 255 }  // edge
};

const RoleColours DEFAULT_ROLE_COLOURS = {
    {  58, 127, 201 }, // #3a7fc9 water
    {  93, 168,  50 }, // #5da832 grass
    { 232, 213, 160 }  // #e8d5a0 sand
};

RGB parse_hex_colour(const std::string& hex) {
    std::string s = hex;
    if (!s.empty() && s[0] == '#') s = s.substr(1);
    if (s.length() == 3) {
        std::string full;
        for (char c : s) {
            full += c;
            full += c;
        }
        s = full;
    }
    if (s.length() < 6) return { 0, 0, 0 };

    auto parse_byte = [](const std::string& sub) -> uint8_t {
        try {
            return static_cast<uint8_t>(std::stoul(sub, nullptr, 16));
        } catch (...) {
            return 0;
        }
    };

    return {
        parse_byte(s.substr(0, 2)),
        parse_byte(s.substr(2, 2)),
        parse_byte(s.substr(4, 2))
    };
}

static inline uint8_t clamp255(float v) {
    return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, std::floor(v + 0.5f))));
}

std::string to_hex_colour(RGB c) {
    std::ostringstream ss;
    ss << '#' << std::hex << std::setfill('0')
       << std::setw(2) << static_cast<int>(c.r)
       << std::setw(2) << static_cast<int>(c.g)
       << std::setw(2) << static_cast<int>(c.b);
    return ss.str();
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

std::vector<uint8_t> paint_pattern_tile_rgba(
    const std::string& pattern,
    int mask,
    const RoleColours& colours,
    const PaintOptions& opts
) {
    int tile_size = opts.tile_size;
    auto derived = pattern_ramp(colours, opts.band_steps);
    const auto& ramp = (opts.ramp.has_value() && opts.ramp->size() == derived.size()) ? *opts.ramp : derived;
    auto level_defs = pattern_levels_for(opts.band_steps);
    std::string grid = pattern_levels_for_mask(
        pattern, mask, opts.offset_px, tile_size, opts.band_steps,
        opts.hard_edge_b, opts.edge_seed, opts.outline_width
    );
    int solid = static_cast<int>(ramp.size()) - 1;

    int shadesA = std::max(1, opts.texture.shadesA);
    int shadesB = std::max(1, opts.texture.shadesB);

    std::optional<std::vector<RGB>> texA;
    if (opts.texture.algoA != "none" && opts.texture.amountA > 0.0f) {
        texA = texture_ramp(colours.terrainA, opts.texture.colourA, shadesA, opts.texture.rampA);
    }

    std::optional<std::vector<RGB>> texB;
    if (!opts.transparent_b && opts.texture.algoB != "none" && opts.texture.amountB > 0.0f) {
        texB = texture_ramp(colours.terrainB, opts.texture.colourB, shadesB, opts.texture.rampB);
    }

    int edge_level = -1;
    for (size_t l = 0; l < level_defs.size(); ++l) {
        if (level_defs[l].role == PatternRole::Edge) {
            edge_level = static_cast<int>(l);
            break;
        }
    }

    int rib_shades = std::max(1, opts.ribbon.shades);
    bool ribbon_on = (opts.ribbon.algo != "none" && opts.ribbon.amount > 0.0f && mask >= 0);

    std::optional<std::vector<RGB>> rib_ramp;
    if (ribbon_on && edge_level >= 0) {
        rib_ramp = texture_ramp(ramp[edge_level], opts.ribbon.colour, rib_shades, opts.ribbon.ramp);
    }

    std::optional<BandCoords> coords;
    if (ribbon_on) {
        coords = pattern_band_coords(
            pattern, mask, opts.offset_px, tile_size, opts.band_steps,
            opts.hard_edge_b, opts.edge_seed, opts.outline_width
        );
    }

    float rib_width = ribbon_on ? std::max(1.0f, outline_width_px(pattern, opts.band_steps, opts.hard_edge_b, opts.outline_width, tile_size)) : 1.0f;

    int span = band_noise_span(pattern, opts.band_steps);

    std::vector<uint8_t> out(tile_size * tile_size * 4, 0);

    for (int y = 0; y < tile_size; ++y) {
        for (int x = 0; x < tile_size; ++x) {
            int p = y * tile_size + x;
            int level = grid[p] - '0';
            RGB rgb = ramp[level];
            int final_level = level;
            bool grained = false;

            if (level > 0 && level < solid && !opts.noises.empty()) {
                int step = noise_step(opts.noises, x, y, opts.noise_seed, opts.noise_strength) * span;
                if (step != 0) {
                    int next_lvl = std::max(0, std::min(solid, level + step));
                    PatternRole from_role = level_defs[level].role;
                    PatternRole next_role = level_defs[next_lvl].role;

                    auto target_matches = [&](PatternRole r) {
                        for (auto tid : opts.noise_targets) {
                            if (tid == NoiseTargetId::TerrainA && r == PatternRole::TerrainA) return true;
                            if (tid == NoiseTargetId::TerrainB && r == PatternRole::TerrainB) return true;
                            if (tid == NoiseTargetId::Edge && r == PatternRole::Edge) return true;
                        }
                        return false;
                    };

                    bool keep_noise = target_matches(from_role) &&
                        (next_role != PatternRole::Edge || target_matches(PatternRole::Edge));

                    if (keep_noise) {
                        grained = true;
                        final_level = next_lvl;
                        if (next_role == PatternRole::Edge && opts.noise_colours.edge.has_value()) {
                            rgb = *opts.noise_colours.edge;
                        } else if (step < 0 && opts.noise_colours.b.has_value()) {
                            rgb = *opts.noise_colours.b;
                        } else if (step > 0 && opts.noise_colours.a.has_value()) {
                            rgb = *opts.noise_colours.a;
                        } else {
                            rgb = ramp[next_lvl];
                        }
                    }
                }
            }

            if (!grained && rib_ramp.has_value() && coords.has_value() && level == edge_level) {
                int k = ribbon_shade_at(
                    opts.ribbon.algo, coords->s[p], coords->depth[p], rib_width,
                    opts.ribbon.seed, opts.ribbon.amount, rib_shades, opts.ribbon.period, opts.ribbon.invert
                );
                if (k > 0 && k < static_cast<int>(rib_ramp->size())) {
                    rgb = (*rib_ramp)[k];
                }
            } else if (texA.has_value() && level == solid) {
                int k = texture_shade_at(
                    opts.texture.algoA, x, y, opts.texture.seedA, opts.texture.amountA, shadesA,
                    opts.texture.cellScaleA, opts.texture.rippleScaleA, opts.texture.geoScaleA
                );
                if (k > 0 && k < static_cast<int>(texA->size())) {
                    rgb = (*texA)[k];
                }
            } else if (texB.has_value() && level == 0) {
                int k = texture_shade_at(
                    opts.texture.algoB, x, y, opts.texture.seedB, opts.texture.amountB, shadesB,
                    opts.texture.cellScaleB, opts.texture.rippleScaleB, opts.texture.geoScaleB
                );
                if (k > 0 && k < static_cast<int>(texB->size())) {
                    rgb = (*texB)[k];
                }
            }

            int i = p * 4;
            out[i] = rgb.r;
            out[i + 1] = rgb.g;
            out[i + 2] = rgb.b;
            out[i + 3] = (opts.transparent_b && level_defs[final_level].role == PatternRole::TerrainB) ? 0 : 255;
        }
    }
    return out;
}

} // namespace atm
