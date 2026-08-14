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

Recipe sanitize_recipe(const nlohmann::json& raw);
Recipe recipe_from_json(const nlohmann::json& j);
nlohmann::json recipe_to_json(const Recipe& recipe);

} // namespace atm
