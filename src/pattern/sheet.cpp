#include "sheet.h"
#include "pattern_data.h"
#include "pattern_texture.h"
#include "blob47.h"
#include <algorithm>
#include <cstring>

namespace atm {

static std::optional<std::vector<std::optional<RGB>>> parse_custom_ramp(
    const std::optional<std::vector<std::optional<std::string>>>& hexes,
    int shade_count
) {
    if (!hexes.has_value() || hexes->empty()) return std::nullopt;
    bool any_set = false;
    for (const auto& h : *hexes) {
        if (h.has_value() && !h->empty()) any_set = true;
    }
    if (!any_set) return std::nullopt;

    std::vector<std::optional<RGB>> sliced;
    size_t count = std::min(hexes->size(), static_cast<size_t>(shade_count + 1));
    bool sliced_any = false;
    for (size_t i = 0; i < count; ++i) {
        const auto& h = (*hexes)[i];
        if (h.has_value() && !h->empty()) {
            sliced.push_back(parse_hex_colour(*h));
            sliced_any = true;
        } else {
            sliced.push_back(std::nullopt);
        }
    }
    if (!sliced_any) return std::nullopt;
    return sliced;
}

PaintArgs recipe_to_paint_args(const Recipe& r, const PaintOverrides& overrides) {
    RoleColours role_colours = {
        parse_hex_colour(r.roleHex.terrainA),
        parse_hex_colour(r.roleHex.terrainB),
        parse_hex_colour(r.roleHex.edge)
    };

    auto r_range = pattern_data::get_pattern_offset_range(r.patternId);
    double lo = static_cast<double>(r_range.min_off);
    double hi = static_cast<double>(r_range.max_off);
    double offset_px = (r.bandBias < 0.0) ? -r.bandBias * lo : r.bandBias * hi;

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

    int shadesA = (r.textureAlgoA == "water") ? 2 : r.textureShadesA;
    int shadesB = (r.textureAlgoB == "water") ? 2 : r.textureShadesB;
    double amountA = texture_uses_amount(r.textureAlgoA) ? r.textureAmountA : 1.0;
    double amountB = texture_uses_amount(r.textureAlgoB) ? r.textureAmountB : 1.0;

    auto texture_ramp_for = [](
        const std::optional<std::vector<std::optional<std::string>>>& hexes,
        const std::string& algo,
        int shade_count
    ) -> std::optional<std::vector<std::optional<RGB>>> {
        auto custom = parse_custom_ramp(hexes, shade_count);
        if (algo != "water") return custom;
        std::vector<std::optional<RGB>> water_ramp = custom.has_value() ? *custom : std::vector<std::optional<RGB>>(3, std::nullopt);
        if (water_ramp.size() < 3) water_ramp.resize(3, std::nullopt);
        if (!water_ramp[2].has_value()) {
            water_ramp[2] = WATER_DOT_COLOUR;
        }
        return water_ramp;
    };

    TexturePaintOptions texture;
    texture.algoA = r.textureAlgoA;
    texture.algoB = r.textureAlgoB;
    texture.amountA = amountA;
    texture.amountB = amountB;
    texture.shadesA = shadesA;
    texture.shadesB = shadesB;
    texture.seedA = r.textureSeedA;
    texture.seedB = r.textureSeedB;
    texture.cellScaleA = r.cellScaleA;
    texture.cellScaleB = r.cellScaleB;
    texture.rippleScaleA = r.rippleScaleA;
    texture.rippleScaleB = r.rippleScaleB;
    texture.geoScaleA = r.geoScaleA;
    texture.geoScaleB = r.geoScaleB;
    texture.colourA = DEFAULT_TEXTURE_TERRAIN_A;
    texture.colourB = DEFAULT_TEXTURE_TERRAIN_B;
    texture.rampA = texture_ramp_for(r.customTexHexA, r.textureAlgoA, shadesA);
    texture.rampB = texture_ramp_for(r.customTexHexB, r.textureAlgoB, shadesB);

    RibbonPaintOptions ribbon;
    ribbon.algo = r.ribbonAlgo;
    ribbon.amount = r.ribbonAmount;
    ribbon.period = r.ribbonPeriod;
    ribbon.shades = r.ribbonShades;
    ribbon.seed = r.edgeSeed;
    ribbon.invert = r.ribbonInvert;
    if (r.customRibbonHex.has_value() && static_cast<int>(r.customRibbonHex->size()) == r.ribbonShades + 1) {
        std::vector<std::optional<RGB>> custom_rib;
        for (const auto& item : *r.customRibbonHex) {
            if (item.has_value() && !item->empty()) {
                custom_rib.push_back(parse_hex_colour(*item));
            } else {
                custom_rib.push_back(std::nullopt);
            }
        }
        ribbon.ramp = custom_rib;
    }

    PaintOptions opts;
    opts.tile_size = SHEET_TILE_SIZE;
    opts.offset_px = offset_px;
    opts.band_steps = r.bandSteps;
    opts.hard_edge_b = r.hardEdgeB;
    opts.edge_seed = r.edgeSeed;
    opts.outline_width = static_cast<float>(r.outlineWidth);
    opts.noises = r.patternNoise;
    opts.noise_seed = r.patternNoiseSeed;
    opts.noise_strength = r.patternNoiseStrength;

    if (overrides.noise_targets.has_value()) {
        opts.noise_targets = *overrides.noise_targets;
    } else {
        opts.noise_targets = { NoiseTargetId::Edge, NoiseTargetId::TerrainA, NoiseTargetId::TerrainB };
    }
    opts.noise_colours = overrides.noise_colours;
    opts.ribbon = ribbon;
    opts.texture = texture;
    opts.ramp = ramp;
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

    for (size_t i = 0; i < BLOB47_LAYOUT.size(); ++i) {
        int col = static_cast<int>(i % BLOB47_COLS);
        int row = static_cast<int>(i / BLOB47_COLS);
        std::string grid = pattern_levels_for_mask(
            a.pattern_id,
            BLOB47_LAYOUT[i],
            opts.offset_px,
            SHEET_TILE_SIZE,
            opts.band_steps,
            opts.hard_edge_b,
            opts.edge_seed,
            opts.outline_width
        );

        for (int y = 0; y < SHEET_TILE_SIZE; ++y) {
            int dst_y = row * SHEET_TILE_SIZE + y;
            int dst_x = col * SHEET_TILE_SIZE;
            std::memcpy(&rows[dst_y][dst_x], &grid[y * SHEET_TILE_SIZE], SHEET_TILE_SIZE);
        }
    }

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

    for (size_t i = 0; i < BLOB47_LAYOUT.size(); ++i) {
        int col = static_cast<int>(i % BLOB47_COLS);
        int row = static_cast<int>(i / BLOB47_COLS);
        auto tile = paint_pattern_tile_rgba(a.pattern_id, BLOB47_LAYOUT[i], a.role_colours, a.opts);
        int x0 = col * SHEET_TILE_SIZE;
        int y0 = row * SHEET_TILE_SIZE;

        for (int y = 0; y < SHEET_TILE_SIZE; ++y) {
            int src_offset = y * SHEET_TILE_SIZE * 4;
            int dst_offset = ((y0 + y) * SHEET_WIDTH + x0) * 4;
            std::memcpy(&out[dst_offset], &tile[src_offset], SHEET_TILE_SIZE * 4);
        }
    }
    return out;
}

} // namespace atm
