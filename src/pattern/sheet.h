#pragma once

#include "model/recipe.h"
#include "pattern/blob47.h"
#include "pattern/pattern_paint.h"
#include <string>
#include <vector>
#include <optional>

namespace atm {

constexpr int SHEET_TILE_SIZE = 32;
constexpr int SHEET_WIDTH = BLOB47_COLS * SHEET_TILE_SIZE;   // 256
constexpr int SHEET_HEIGHT = BLOB47_ROWS * SHEET_TILE_SIZE; // 192

struct PaintOverrides {
    std::optional<std::vector<NoiseTargetId>> noise_targets;
    NoiseColours noise_colours;
};

struct PaintArgs {
    std::string pattern_id;
    RoleColours role_colours;
    PaintOptions opts;
};

PaintArgs recipe_to_paint_args(const Recipe& recipe, const PaintOverrides& overrides = {});

// Render the 256x192 ASCII digit level grid (row-major)
std::string render_level_grid(const Recipe& recipe, const PaintOverrides& overrides = {});

// Render the full 256x192 RGBA sheet (row-major, 4 bytes per pixel)
std::vector<uint8_t> render_sheet_rgba(const Recipe& recipe, const PaintOverrides& overrides = {});

} // namespace atm
