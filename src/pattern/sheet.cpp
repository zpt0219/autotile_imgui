#include "sheet.h"
#include "pattern_data.h"
#include "pattern_texture.h"
#include "catalog.h"   // texture_uses_amount() and the rest of the registry
#include "blob47.h"
#include <algorithm>
#include <cstring>

namespace atm {

// A sparse override array carries "" and nullopt alike to mean "not
// overridden"; both collapse to nullopt here so the paint layer only ever sees
// a colour or nothing.
static std::vector<std::optional<RGB>> parse_sparse_hexes(
    const std::vector<std::optional<std::string>>& hexes,
    size_t count
) {
    std::vector<std::optional<RGB>> out;
    out.reserve(count);
    for (size_t i = 0; i < count && i < hexes.size(); ++i) {
        const auto& h = hexes[i];
        if (h.has_value() && !h->empty()) {
            out.push_back(parse_hex_colour(*h));
        } else {
            out.push_back(std::nullopt);
        }
    }
    return out;
}

// Texture ramps are sliced to the shade count in play and dropped entirely when
// no slot is actually set — an all-empty array must not shadow the computed ramp.
static std::optional<std::vector<std::optional<RGB>>> parse_custom_ramp(
    const std::optional<std::vector<std::optional<std::string>>>& hexes,
    int shade_count
) {
    if (!hexes.has_value() || hexes->empty()) return std::nullopt;

    const size_t count = std::min(hexes->size(), static_cast<size_t>(shade_count + 1));
    std::vector<std::optional<RGB>> sliced = parse_sparse_hexes(*hexes, count);

    for (const auto& c : sliced) {
        if (c.has_value()) return sliced;
    }
    return std::nullopt;
}

static double band_offset_px(const Recipe& r) {
    auto r_range = pattern_offset_range(r.patternId);
    double lo = static_cast<double>(r_range.min_off);
    double hi = static_cast<double>(r_range.max_off);
    return (r.bandBias < 0.0) ? -r.bandBias * lo : r.bandBias * hi;
}

static std::vector<RGB> build_ramp(const Recipe& r, const RoleColours& role_colours) {
    auto derived_ramp = pattern_ramp(role_colours, r.bandSteps);
    std::vector<RGB> ramp = derived_ramp;
    if (r.customShadesHex.has_value() && r.customShadesHex->size() == derived_ramp.size()) {
        for (size_t i = 0; i < derived_ramp.size(); ++i) {
            const auto& hex_str = (*r.customShadesHex)[i];
            if (!hex_str.empty()) {
                ramp[i] = parse_hex_colour(hex_str);
            }
        }
    }
    return ramp;
}

static TexturePaintOptions build_texture_options(const Recipe& r) {
    auto texture_ramp_for = [](
        const std::optional<std::vector<std::optional<std::string>>>& hexes,
        const std::string& algo,
        int shade_count
    ) -> std::optional<std::vector<std::optional<RGB>>> {
        auto custom = parse_custom_ramp(hexes, shade_count);
        if (algo != "water") return custom;
        // Water texture special case: level 3 defaults to WATER_DOT_COLOUR if uncustomised. Reference: renderSheet.ts
        std::vector<std::optional<RGB>> water_ramp = custom.has_value() ? *custom : std::vector<std::optional<RGB>>(3, std::nullopt);
        if (water_ramp.size() < 3) water_ramp.resize(3, std::nullopt);
        if (!water_ramp[2].has_value()) {
            water_ramp[2] = WATER_DOT_COLOUR;
        }
        return water_ramp;
    };

    // Water texture special case: forced to 2 shades. Reference: renderSheet.ts
    int shadesA = (r.textureAlgoA == "water") ? 2 : r.textureShadesA;
    int shadesB = (r.textureAlgoB == "water") ? 2 : r.textureShadesB;

    TexturePaintOptions texture;
    texture.a = {
        r.textureAlgoA,
        texture_uses_amount(r.textureAlgoA) ? r.textureAmountA : 1.0,
        shadesA,
        r.textureSeedA,
        r.cellScaleA,
        r.rippleScaleA,
        r.geoScaleA,
        DEFAULT_TEXTURE_TERRAIN_A,
        texture_ramp_for(r.customTexHexA, r.textureAlgoA, shadesA)
    };
    texture.b = {
        r.textureAlgoB,
        texture_uses_amount(r.textureAlgoB) ? r.textureAmountB : 1.0,
        shadesB,
        r.textureSeedB,
        r.cellScaleB,
        r.rippleScaleB,
        r.geoScaleB,
        DEFAULT_TEXTURE_TERRAIN_B,
        texture_ramp_for(r.customTexHexB, r.textureAlgoB, shadesB)
    };
    return texture;
}

static RibbonPaintOptions build_ribbon_options(const Recipe& r) {
    RibbonPaintOptions ribbon;
    ribbon.algo = r.ribbonAlgo;
    ribbon.amount = r.ribbonAmount;
    ribbon.period = r.ribbonPeriod;
    ribbon.shades = r.ribbonShades;
    ribbon.seed = r.edgeSeed;
    ribbon.invert = r.ribbonInvert;
    // Unlike the texture ramps, this one is taken whole or not at all: the
    // length must match exactly, and an all-empty array is still honoured.
    if (r.customRibbonHex.has_value() && static_cast<int>(r.customRibbonHex->size()) == r.ribbonShades + 1) {
        ribbon.ramp = parse_sparse_hexes(*r.customRibbonHex, r.customRibbonHex->size());
    }
    return ribbon;
}

PaintArgs recipe_to_paint_args(const Recipe& r, const PaintOverrides& overrides) {
    RoleColours role_colours = {
        parse_hex_colour(r.roleHex.terrainA),
        parse_hex_colour(r.roleHex.terrainB),
        parse_hex_colour(r.roleHex.edge)
    };

    PaintOptions opts;
    opts.tile_size = SHEET_TILE_SIZE;
    opts.offset_px = band_offset_px(r);
    opts.band_steps = r.bandSteps;
    opts.hard_edge_b = r.hardEdgeB;
    opts.edge_seed = r.edgeSeed;
    opts.outline_width = static_cast<float>(r.outlineWidth);
    opts.noises = r.patternNoise;
    opts.noise_seed = r.patternNoiseSeed;
    opts.noise_strength = r.patternNoiseStrength;
    opts.noise_targets = overrides.noise_targets.value_or(
        std::vector<NoiseTargetId>{ NoiseTargetId::Edge, NoiseTargetId::TerrainA, NoiseTargetId::TerrainB }
    );
    opts.noise_colours = overrides.noise_colours;
    opts.ribbon = build_ribbon_options(r);
    opts.texture = build_texture_options(r);
    opts.ramp = build_ramp(r, role_colours);
    opts.transparent_b = r.transparentB;

    return {
        r.patternId,
        role_colours,
        opts
    };
}

std::string render_level_grid(const Recipe& recipe, const PaintOverrides& overrides) {
    PaintArgs a = recipe_to_paint_args(recipe, overrides);
    const auto& opts = a.opts;

    std::vector<std::string> rows(SHEET_HEIGHT, std::string(SHEET_WIDTH, '0'));

    FieldParams fp{
        static_cast<float>(opts.offset_px),
        SHEET_TILE_SIZE,
        opts.band_steps,
        opts.hard_edge_b,
        opts.edge_seed,
        static_cast<float>(opts.outline_width)
    };

    for_each_blob47_tile([&](size_t /*i*/, int col, int row, uint8_t mask) {
        std::string grid = pattern_levels_for_mask(a.pattern_id, mask, fp);

        for (int y = 0; y < SHEET_TILE_SIZE; ++y) {
            int dst_y = row * SHEET_TILE_SIZE + y;
            int dst_x = col * SHEET_TILE_SIZE;
            std::memcpy(&rows[dst_y][dst_x], &grid[y * SHEET_TILE_SIZE], SHEET_TILE_SIZE);
        }
    });

    std::string out;
    out.reserve(SHEET_WIDTH * SHEET_HEIGHT);
    for (const auto& r_str : rows) {
        out += r_str;
    }
    return out;
}

std::vector<uint8_t> render_sheet_rgba(const Recipe& recipe, const PaintOverrides& overrides) {
    PaintArgs a = recipe_to_paint_args(recipe, overrides);
    std::vector<uint8_t> out(SHEET_WIDTH * SHEET_HEIGHT * 4, 0);

    TilePainter painter(a.pattern_id, a.role_colours, a.opts);

    for_each_blob47_tile([&](size_t /*i*/, int col, int row, uint8_t mask) {
        int x0 = col * SHEET_TILE_SIZE;
        int y0 = row * SHEET_TILE_SIZE;
        uint8_t* dst = &out[(y0 * SHEET_WIDTH + x0) * 4];
        painter.paint_tile_into(mask, dst, SHEET_WIDTH * 4);
    });

    return out;
}

} // namespace atm
