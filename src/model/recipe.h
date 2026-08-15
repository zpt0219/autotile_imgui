#pragma once

#include "pattern/pattern_noise.h"
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace atm {

struct RoleHex {
    std::string terrainA = "#3a7fc9";
    std::string terrainB = "#5da832";
    std::string edge = "#e8d5a0";
};

struct Recipe {
    RoleHex roleHex;
    std::string patternId = "rounded";
    int edgeSeed = 0;
    int outlineWidth = 2;
    int bandSteps = 4;
    bool hardEdgeB = false;
    bool transparentB = false;
    double bandBias = 0.0;
    std::optional<std::vector<std::string>> customShadesHex;

    std::vector<NoiseId> patternNoise;
    int patternNoiseSeed = 1234;
    double patternNoiseStrength = 0.15;

    std::string ribbonAlgo = "none";
    double ribbonAmount = 0.25;
    int ribbonPeriod = 4;
    int ribbonShades = 2;
    bool ribbonInvert = false;
    std::optional<std::vector<std::optional<std::string>>> customRibbonHex;

    std::string textureAlgoA = "none";
    std::string textureAlgoB = "none";
    double textureAmountA = 0.35;
    double textureAmountB = 0.35;
    int textureShadesA = 2;
    int textureShadesB = 2;
    int textureSeedA = 1;
    int textureSeedB = 1;
    int cellScaleA = 4;
    int cellScaleB = 4;
    int rippleScaleA = 4;
    int rippleScaleB = 4;
    int geoScaleA = 1;
    int geoScaleB = 1;
    std::optional<std::vector<std::optional<std::string>>> customTexHexA;
    std::optional<std::vector<std::optional<std::string>>> customTexHexB;
};

const Recipe& get_default_recipe();

// --- colour-override length invariants -------------------------------------
//
// Each override array's length is pinned to a count some *other* field owns:
//
//   customShadesHex   == bandSteps + 2          (all-or-nothing, see recipe.ts)
//   customRibbonHex   == ribbonShades + 1       (per-slot optional)
//   customTexHexA     == textureShadesA + 1     (per-slot optional)
//   customTexHexB     == textureShadesB + 1     (per-slot optional)
//
// sanitizeRecipe drops any array whose length does not match, so moving one of
// those counts without resizing loses the user's hand-tuned colours the next
// time the recipe round-trips through JSON — silently, on save/load.
//
// Call the matching sync after changing a count, in the same edit. They are
// idempotent and a no-op on an absent (nullopt) array.
//
// Growing customShadesHex fills the new levels with the colour they would have
// had uncustomised, since that array cannot hold a "not overridden" entry; the
// sparse arrays grow with nullopt, which already means "use the computed one".

void sync_band_overrides(Recipe& r);
void sync_ribbon_overrides(Recipe& r);
void sync_texture_overrides(Recipe& r);

/** All four at once. */
void sync_all_overrides(Recipe& r);

Recipe sanitize_recipe(const nlohmann::json& raw);
Recipe recipe_from_json(const nlohmann::json& j);
nlohmann::json recipe_to_json(const Recipe& recipe);

} // namespace atm
