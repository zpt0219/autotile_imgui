#include "recipe.h"
#include "pattern/js_math.h"
#include "pattern/pattern_paint.h"
#include <unordered_set>
#include <regex>
#include <cmath>

namespace atm {

static const Recipe DEFAULT_RECIPE_INSTANCE;

namespace {

/** Grow or shrink a sparse override array; new slots mean "not overridden". */
void resize_sparse(std::optional<std::vector<std::optional<std::string>>>& arr, size_t want) {
    if (!arr.has_value()) return;
    arr->resize(want);
}

} // namespace

void sync_band_overrides(Recipe& r) {
    if (!r.customShadesHex.has_value()) return;

    const size_t want = static_cast<size_t>(r.bandSteps) + 2;
    auto& arr = *r.customShadesHex;
    if (arr.size() == want) return;

    if (arr.size() > want) {
        arr.resize(want);
        return;
    }

    // customShadesHex is all-or-nothing: an empty entry invalidates the whole
    // array, so a grown level cannot be left blank. Fill it with the shade the
    // level would have had with no override at all.
    RoleColours colours{
        parse_hex_colour(r.roleHex.terrainA),
        parse_hex_colour(r.roleHex.terrainB),
        parse_hex_colour(r.roleHex.edge)
    };
    const std::vector<RGB> computed = pattern_ramp(colours, r.bandSteps);

    const size_t first_new = arr.size();
    arr.resize(want);
    for (size_t i = first_new; i < want; ++i) {
        arr[i] = (i < computed.size()) ? to_hex_colour(computed[i])
                                       : to_hex_colour(colours.terrainA);
    }
}

void sync_ribbon_overrides(Recipe& r) {
    resize_sparse(r.customRibbonHex, static_cast<size_t>(r.ribbonShades) + 1);
}

void sync_texture_overrides(Recipe& r) {
    resize_sparse(r.customTexHexA, static_cast<size_t>(r.textureShadesA) + 1);
    resize_sparse(r.customTexHexB, static_cast<size_t>(r.textureShadesB) + 1);
}

void sync_all_overrides(Recipe& r) {
    sync_band_overrides(r);
    sync_ribbon_overrides(r);
    sync_texture_overrides(r);
}

const Recipe& get_default_recipe() {
    return DEFAULT_RECIPE_INSTANCE;
}

static bool is_hex_color(const std::string& str) {
    if (str.length() != 7 || str[0] != '#') return false;
    for (int i = 1; i < 7; ++i) {
        char c = str[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

static int clamp_int(int val, int min_val, int max_val, int fallback) {
    if (std::isnan(static_cast<float>(val))) return fallback;
    return std::max(min_val, std::min(max_val, val));
}

static double clamp_double(double val, double min_val, double max_val, double fallback) {
    if (!std::isfinite(val)) return fallback;
    return std::max(min_val, std::min(max_val, val));
}

Recipe sanitize_recipe(const nlohmann::json& raw) {
    if (!raw.is_object()) return DEFAULT_RECIPE_INSTANCE;

    Recipe r;

    if (raw.contains("roleHex") && raw["roleHex"].is_object()) {
        const auto& rh = raw["roleHex"];
        if (rh.contains("terrainA") && rh["terrainA"].is_string() && is_hex_color(rh["terrainA"].get<std::string>())) {
            r.roleHex.terrainA = rh["terrainA"].get<std::string>();
        }
        if (rh.contains("terrainB") && rh["terrainB"].is_string() && is_hex_color(rh["terrainB"].get<std::string>())) {
            r.roleHex.terrainB = rh["terrainB"].get<std::string>();
        }
        if (rh.contains("edge") && rh["edge"].is_string() && is_hex_color(rh["edge"].get<std::string>())) {
            r.roleHex.edge = rh["edge"].get<std::string>();
        }
    }

    static const std::unordered_set<std::string> VALID_PATTERNS = {
        "square", "sharp", "rounded", "wave", "jagged", "gravel",
        "boulder", "thorn", "coast", "moss", "billow"
    };

    if (raw.contains("patternId") && raw["patternId"].is_string()) {
        std::string pid = raw["patternId"].get<std::string>();
        if (VALID_PATTERNS.count(pid)) {
            r.patternId = pid;
        }
    }

    if (raw.contains("edgeSeed") && raw["edgeSeed"].is_number()) {
        r.edgeSeed = clamp_int(raw["edgeSeed"].get<int>(), 0, 99999, DEFAULT_RECIPE_INSTANCE.edgeSeed);
    }
    if (raw.contains("outlineWidth") && raw["outlineWidth"].is_number()) {
        r.outlineWidth = clamp_int(raw["outlineWidth"].get<int>(), 1, 4, DEFAULT_RECIPE_INSTANCE.outlineWidth);
    }
    if (raw.contains("bandSteps") && raw["bandSteps"].is_number()) {
        r.bandSteps = clamp_int(raw["bandSteps"].get<int>(), 3, 5, DEFAULT_RECIPE_INSTANCE.bandSteps);
    }
    if (raw.contains("hardEdgeB") && raw["hardEdgeB"].is_boolean()) {
        r.hardEdgeB = raw["hardEdgeB"].get<bool>();
    }
    if (raw.contains("transparentB") && raw["transparentB"].is_boolean()) {
        r.transparentB = raw["transparentB"].get<bool>();
    }
    if (raw.contains("bandBias") && raw["bandBias"].is_number()) {
        r.bandBias = clamp_double(raw["bandBias"].get<double>(), -1.0, 1.0, DEFAULT_RECIPE_INSTANCE.bandBias);
    }

    if (raw.contains("customShadesHex") && raw["customShadesHex"].is_array()) {
        const auto& arr = raw["customShadesHex"];
        if (static_cast<int>(arr.size()) == r.bandSteps + 2) {
            std::vector<std::string> custom;
            bool valid = true;
            for (const auto& item : arr) {
                if (item.is_string() && is_hex_color(item.get<std::string>())) {
                    custom.push_back(item.get<std::string>());
                } else {
                    valid = false;
                    break;
                }
            }
            if (valid) r.customShadesHex = custom;
        }
    }

    if (raw.contains("patternNoise") && raw["patternNoise"].is_array()) {
        r.patternNoise.clear();
        for (const auto& item : raw["patternNoise"]) {
            if (item.is_string()) {
                std::string s = item.get<std::string>();
                if (s == "white" || s == "blue" || s == "ordered") {
                    r.patternNoise.push_back(parse_noise_id(s));
                }
            }
        }
    }
    if (raw.contains("patternNoiseSeed") && raw["patternNoiseSeed"].is_number()) {
        r.patternNoiseSeed = clamp_int(raw["patternNoiseSeed"].get<int>(), 0, 99999, DEFAULT_RECIPE_INSTANCE.patternNoiseSeed);
    }
    if (raw.contains("patternNoiseStrength") && raw["patternNoiseStrength"].is_number()) {
        r.patternNoiseStrength = clamp_double(raw["patternNoiseStrength"].get<double>(), 0.0, 2.0, DEFAULT_RECIPE_INSTANCE.patternNoiseStrength);
    }

    static const std::unordered_set<std::string> VALID_RIBBONS = {
        "none", "bevel", "dashes", "ticks", "beads", "rope", "wave", "grain", "speckle",
        "along_brick_wall", "along_cobbles2", "along_weave", "along_stone_floor",
        "along_breeze_block", "along_octagonal"
    };

    if (raw.contains("ribbonAlgo") && raw["ribbonAlgo"].is_string()) {
        std::string ral = raw["ribbonAlgo"].get<std::string>();
        if (VALID_RIBBONS.count(ral)) r.ribbonAlgo = ral;
    }
    if (raw.contains("ribbonAmount") && raw["ribbonAmount"].is_number()) {
        r.ribbonAmount = clamp_double(raw["ribbonAmount"].get<double>(), 0.0, 1.0, DEFAULT_RECIPE_INSTANCE.ribbonAmount);
    }
    if (raw.contains("ribbonPeriod") && raw["ribbonPeriod"].is_number()) {
        r.ribbonPeriod = clamp_int(raw["ribbonPeriod"].get<int>(), 1, 8, DEFAULT_RECIPE_INSTANCE.ribbonPeriod);
    }
    if (raw.contains("ribbonShades") && raw["ribbonShades"].is_number()) {
        r.ribbonShades = clamp_int(raw["ribbonShades"].get<int>(), 1, 4, DEFAULT_RECIPE_INSTANCE.ribbonShades);
    }
    if (raw.contains("ribbonInvert") && raw["ribbonInvert"].is_boolean()) {
        r.ribbonInvert = raw["ribbonInvert"].get<bool>();
    }

    if (raw.contains("customRibbonHex") && raw["customRibbonHex"].is_array()) {
        const auto& arr = raw["customRibbonHex"];
        if (static_cast<int>(arr.size()) == r.ribbonShades + 1) {
            std::vector<std::optional<std::string>> custom;
            for (const auto& item : arr) {
                if (item.is_string() && is_hex_color(item.get<std::string>())) {
                    custom.push_back(item.get<std::string>());
                } else {
                    custom.push_back(std::nullopt);
                }
            }
            r.customRibbonHex = custom;
        }
    }

    static const std::unordered_set<std::string> VALID_TEXTURES = {
        "none", "white", "blue", "ordered", "ripple", "ripple_diag", "cells",
        "breeze_block", "brick_wall", "cobbles2", "brick_floor", "hexagon",
        "isometric", "isometric_grid", "octagonal", "square", "weave",
        "paving", "paving3", "paving5", "stone_floor", "water", "brick_bond",
        "field", "rubble", "nonslip"
    };

    if (raw.contains("textureAlgoA") && raw["textureAlgoA"].is_string()) {
        std::string tal = raw["textureAlgoA"].get<std::string>();
        if (VALID_TEXTURES.count(tal)) r.textureAlgoA = tal;
    }
    if (raw.contains("textureAlgoB") && raw["textureAlgoB"].is_string()) {
        std::string tal = raw["textureAlgoB"].get<std::string>();
        if (VALID_TEXTURES.count(tal)) r.textureAlgoB = tal;
    }

    if (raw.contains("textureAmountA") && raw["textureAmountA"].is_number()) {
        r.textureAmountA = clamp_double(raw["textureAmountA"].get<double>(), 0.0, 1.0, DEFAULT_RECIPE_INSTANCE.textureAmountA);
    }
    if (raw.contains("textureAmountB") && raw["textureAmountB"].is_number()) {
        r.textureAmountB = clamp_double(raw["textureAmountB"].get<double>(), 0.0, 1.0, DEFAULT_RECIPE_INSTANCE.textureAmountB);
    }
    if (raw.contains("textureShadesA") && raw["textureShadesA"].is_number()) {
        r.textureShadesA = clamp_int(raw["textureShadesA"].get<int>(), 1, 4, DEFAULT_RECIPE_INSTANCE.textureShadesA);
    }
    if (raw.contains("textureShadesB") && raw["textureShadesB"].is_number()) {
        r.textureShadesB = clamp_int(raw["textureShadesB"].get<int>(), 1, 4, DEFAULT_RECIPE_INSTANCE.textureShadesB);
    }
    if (raw.contains("textureSeedA") && raw["textureSeedA"].is_number()) {
        r.textureSeedA = clamp_int(raw["textureSeedA"].get<int>(), 0, 99999, DEFAULT_RECIPE_INSTANCE.textureSeedA);
    }
    if (raw.contains("textureSeedB") && raw["textureSeedB"].is_number()) {
        r.textureSeedB = clamp_int(raw["textureSeedB"].get<int>(), 0, 99999, DEFAULT_RECIPE_INSTANCE.textureSeedB);
    }

    if (raw.contains("cellScaleA") && raw["cellScaleA"].is_number()) {
        r.cellScaleA = clamp_int(raw["cellScaleA"].get<int>(), 2, 8, DEFAULT_RECIPE_INSTANCE.cellScaleA);
    }
    if (raw.contains("cellScaleB") && raw["cellScaleB"].is_number()) {
        r.cellScaleB = clamp_int(raw["cellScaleB"].get<int>(), 2, 8, DEFAULT_RECIPE_INSTANCE.cellScaleB);
    }
    if (raw.contains("rippleScaleA") && raw["rippleScaleA"].is_number()) {
        r.rippleScaleA = clamp_int(raw["rippleScaleA"].get<int>(), 2, 8, DEFAULT_RECIPE_INSTANCE.rippleScaleA);
    }
    if (raw.contains("rippleScaleB") && raw["rippleScaleB"].is_number()) {
        r.rippleScaleB = clamp_int(raw["rippleScaleB"].get<int>(), 2, 8, DEFAULT_RECIPE_INSTANCE.rippleScaleB);
    }
    if (raw.contains("geoScaleA") && raw["geoScaleA"].is_number()) {
        r.geoScaleA = clamp_int(raw["geoScaleA"].get<int>(), 1, 8, DEFAULT_RECIPE_INSTANCE.geoScaleA);
    }
    if (raw.contains("geoScaleB") && raw["geoScaleB"].is_number()) {
        r.geoScaleB = clamp_int(raw["geoScaleB"].get<int>(), 1, 8, DEFAULT_RECIPE_INSTANCE.geoScaleB);
    }

    if (raw.contains("customTexHex") && raw["customTexHex"].is_object()) {
        const auto& ctex = raw["customTexHex"];
        if (ctex.contains("terrainA") && ctex["terrainA"].is_array()) {
            const auto& arr = ctex["terrainA"];
            if (static_cast<int>(arr.size()) == r.textureShadesA + 1) {
                std::vector<std::optional<std::string>> custom;
                for (const auto& item : arr) {
                    if (item.is_string() && is_hex_color(item.get<std::string>())) {
                        custom.push_back(item.get<std::string>());
                    } else {
                        custom.push_back(std::nullopt);
                    }
                }
                r.customTexHexA = custom;
            }
        }
        if (ctex.contains("terrainB") && ctex["terrainB"].is_array()) {
            const auto& arr = ctex["terrainB"];
            if (static_cast<int>(arr.size()) == r.textureShadesB + 1) {
                std::vector<std::optional<std::string>> custom;
                for (const auto& item : arr) {
                    if (item.is_string() && is_hex_color(item.get<std::string>())) {
                        custom.push_back(item.get<std::string>());
                    } else {
                        custom.push_back(std::nullopt);
                    }
                }
                r.customTexHexB = custom;
            }
        }
    }

    return r;
}

Recipe recipe_from_json(const nlohmann::json& j) {
    return sanitize_recipe(j);
}

nlohmann::json recipe_to_json(const Recipe& r) {
    nlohmann::json j;
    j["roleHex"]["terrainA"] = r.roleHex.terrainA;
    j["roleHex"]["terrainB"] = r.roleHex.terrainB;
    j["roleHex"]["edge"] = r.roleHex.edge;

    j["patternId"] = r.patternId;
    j["edgeSeed"] = r.edgeSeed;
    j["outlineWidth"] = r.outlineWidth;
    j["bandSteps"] = r.bandSteps;
    j["hardEdgeB"] = r.hardEdgeB;
    j["transparentB"] = r.transparentB;
    j["bandBias"] = r.bandBias;

    if (r.customShadesHex.has_value()) {
        j["customShadesHex"] = *r.customShadesHex;
    } else {
        j["customShadesHex"] = nullptr;
    }

    nlohmann::json noiseArr = nlohmann::json::array();
    for (NoiseId nid : r.patternNoise) {
        noiseArr.push_back(noise_id_to_string(nid));
    }
    j["patternNoise"] = noiseArr;
    j["patternNoiseSeed"] = r.patternNoiseSeed;
    j["patternNoiseStrength"] = r.patternNoiseStrength;

    j["ribbonAlgo"] = r.ribbonAlgo;
    j["ribbonAmount"] = r.ribbonAmount;
    j["ribbonPeriod"] = r.ribbonPeriod;
    j["ribbonShades"] = r.ribbonShades;
    j["ribbonInvert"] = r.ribbonInvert;

    if (r.customRibbonHex.has_value()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& item : *r.customRibbonHex) {
            if (item.has_value()) arr.push_back(*item);
            else arr.push_back(nullptr);
        }
        j["customRibbonHex"] = arr;
    } else {
        j["customRibbonHex"] = nullptr;
    }

    j["textureAlgoA"] = r.textureAlgoA;
    j["textureAlgoB"] = r.textureAlgoB;
    j["textureAmountA"] = r.textureAmountA;
    j["textureAmountB"] = r.textureAmountB;
    j["textureShadesA"] = r.textureShadesA;
    j["textureShadesB"] = r.textureShadesB;
    j["textureSeedA"] = r.textureSeedA;
    j["textureSeedB"] = r.textureSeedB;
    j["cellScaleA"] = r.cellScaleA;
    j["cellScaleB"] = r.cellScaleB;
    j["rippleScaleA"] = r.rippleScaleA;
    j["rippleScaleB"] = r.rippleScaleB;
    j["geoScaleA"] = r.geoScaleA;
    j["geoScaleB"] = r.geoScaleB;

    nlohmann::json ctex;
    if (r.customTexHexA.has_value()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& item : *r.customTexHexA) {
            if (item.has_value()) arr.push_back(*item);
            else arr.push_back(nullptr);
        }
        ctex["terrainA"] = arr;
    } else {
        ctex["terrainA"] = nullptr;
    }

    if (r.customTexHexB.has_value()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& item : *r.customTexHexB) {
            if (item.has_value()) arr.push_back(*item);
            else arr.push_back(nullptr);
        }
        ctex["terrainB"] = arr;
    } else {
        ctex["terrainB"] = nullptr;
    }
    j["customTexHex"] = ctex;

    return j;
}

} // namespace atm
