#include "recipe.h"
#include "pattern/js_math.h"
#include "pattern/pattern_paint.h"
#include "pattern/catalog.h"
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

template <class T>
static void read_clamped(const nlohmann::json& raw, const char* key, T lo, T hi, T fallback, T& out) {
    if (!raw.contains(key) || !raw[key].is_number()) return;
    if constexpr (std::is_integral_v<T>) {
        out = std::max(lo, std::min(hi, raw[key].template get<T>()));
    } else {
        double val = raw[key].template get<double>();
        if (!std::isfinite(val)) out = fallback;
        else out = std::max(lo, std::min(hi, static_cast<T>(val)));
    }
}

static void read_bool(const nlohmann::json& raw, const char* key, bool& out) {
    if (raw.contains(key) && raw[key].is_boolean()) {
        out = raw[key].get<bool>();
    }
}

// `is_known` is one of the catalogue predicates from pattern/catalog.h, so the
// whitelist cannot drift away from what the pickers actually offer.
static void read_enum(const nlohmann::json& raw, const char* key,
                      bool (*is_known)(const std::string&), std::string& out) {
    if (raw.contains(key) && raw[key].is_string()) {
        std::string s = raw[key].get<std::string>();
        if (is_known(s)) out = s;
    }
}

static std::optional<std::vector<std::string>>
read_all_or_nothing_hex_array(const nlohmann::json& raw, const char* key, int expected_len) {
    if (!raw.contains(key) || !raw[key].is_array()) return std::nullopt;
    const auto& arr = raw[key];
    if (static_cast<int>(arr.size()) != expected_len) return std::nullopt;
    std::vector<std::string> custom;
    custom.reserve(arr.size());
    for (const auto& item : arr) {
        if (item.is_string() && is_hex_color(item.get<std::string>())) {
            custom.push_back(item.get<std::string>());
        } else {
            return std::nullopt;
        }
    }
    return custom;
}

static std::optional<std::vector<std::optional<std::string>>>
read_sparse_hex_array(const nlohmann::json& raw, const char* key, int expected_len) {
    if (!raw.contains(key) || !raw[key].is_array()) return std::nullopt;
    const auto& arr = raw[key];
    if (static_cast<int>(arr.size()) != expected_len) return std::nullopt;
    std::vector<std::optional<std::string>> custom;
    custom.reserve(arr.size());
    for (const auto& item : arr) {
        if (item.is_string() && is_hex_color(item.get<std::string>())) {
            custom.push_back(item.get<std::string>());
        } else {
            custom.push_back(std::nullopt);
        }
    }
    return custom;
}

Recipe sanitize_recipe(const nlohmann::json& raw) {
    if (!raw.is_object()) return DEFAULT_RECIPE_INSTANCE;

    Recipe r;

    if (raw.contains("roleHex") && raw["roleHex"].is_object()) {
        const auto& rh = raw["roleHex"];
        auto read_hex = [&](const char* k, std::string& out) {
            if (rh.contains(k) && rh[k].is_string() && is_hex_color(rh[k].get<std::string>())) {
                out = rh[k].get<std::string>();
            }
        };
        read_hex("terrainA", r.roleHex.terrainA);
        read_hex("terrainB", r.roleHex.terrainB);
        read_hex("edge",     r.roleHex.edge);
    }

    read_enum(raw, "patternId", is_known_pattern, r.patternId);
    read_clamped(raw, "edgeSeed", 0, 99999, DEFAULT_RECIPE_INSTANCE.edgeSeed, r.edgeSeed);
    read_clamped(raw, "outlineWidth", 1, 4, DEFAULT_RECIPE_INSTANCE.outlineWidth, r.outlineWidth);
    read_clamped(raw, "bandSteps", 3, 5, DEFAULT_RECIPE_INSTANCE.bandSteps, r.bandSteps);
    read_bool(raw, "hardEdgeB", r.hardEdgeB);
    read_bool(raw, "transparentB", r.transparentB);
    read_clamped(raw, "bandBias", -1.0, 1.0, DEFAULT_RECIPE_INSTANCE.bandBias, r.bandBias);

    if (auto custom = read_all_or_nothing_hex_array(raw, "customShadesHex", r.bandSteps + 2)) {
        r.customShadesHex = *custom;
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
    read_clamped(raw, "patternNoiseSeed", 0, 99999, DEFAULT_RECIPE_INSTANCE.patternNoiseSeed, r.patternNoiseSeed);
    read_clamped(raw, "patternNoiseStrength", 0.0, 2.0, DEFAULT_RECIPE_INSTANCE.patternNoiseStrength, r.patternNoiseStrength);

    read_enum(raw, "ribbonAlgo", is_known_ribbon, r.ribbonAlgo);
    read_clamped(raw, "ribbonAmount", 0.0, 1.0, DEFAULT_RECIPE_INSTANCE.ribbonAmount, r.ribbonAmount);
    read_clamped(raw, "ribbonPeriod", 1, 8, DEFAULT_RECIPE_INSTANCE.ribbonPeriod, r.ribbonPeriod);
    read_clamped(raw, "ribbonShades", 1, 4, DEFAULT_RECIPE_INSTANCE.ribbonShades, r.ribbonShades);
    read_bool(raw, "ribbonInvert", r.ribbonInvert);

    if (auto custom = read_sparse_hex_array(raw, "customRibbonHex", r.ribbonShades + 1)) {
        r.customRibbonHex = *custom;
    }

    read_enum(raw, "textureAlgoA", is_known_texture, r.textureAlgoA);
    read_enum(raw, "textureAlgoB", is_known_texture, r.textureAlgoB);
    read_clamped(raw, "textureAmountA", 0.0, 1.0, DEFAULT_RECIPE_INSTANCE.textureAmountA, r.textureAmountA);
    read_clamped(raw, "textureAmountB", 0.0, 1.0, DEFAULT_RECIPE_INSTANCE.textureAmountB, r.textureAmountB);
    read_clamped(raw, "textureShadesA", 1, 4, DEFAULT_RECIPE_INSTANCE.textureShadesA, r.textureShadesA);
    read_clamped(raw, "textureShadesB", 1, 4, DEFAULT_RECIPE_INSTANCE.textureShadesB, r.textureShadesB);
    read_clamped(raw, "textureSeedA", 0, 99999, DEFAULT_RECIPE_INSTANCE.textureSeedA, r.textureSeedA);
    read_clamped(raw, "textureSeedB", 0, 99999, DEFAULT_RECIPE_INSTANCE.textureSeedB, r.textureSeedB);
    read_clamped(raw, "cellScaleA", 2, 8, DEFAULT_RECIPE_INSTANCE.cellScaleA, r.cellScaleA);
    read_clamped(raw, "cellScaleB", 2, 8, DEFAULT_RECIPE_INSTANCE.cellScaleB, r.cellScaleB);
    read_clamped(raw, "rippleScaleA", 2, 8, DEFAULT_RECIPE_INSTANCE.rippleScaleA, r.rippleScaleA);
    read_clamped(raw, "rippleScaleB", 2, 8, DEFAULT_RECIPE_INSTANCE.rippleScaleB, r.rippleScaleB);
    read_clamped(raw, "geoScaleA", 1, 8, DEFAULT_RECIPE_INSTANCE.geoScaleA, r.geoScaleA);
    read_clamped(raw, "geoScaleB", 1, 8, DEFAULT_RECIPE_INSTANCE.geoScaleB, r.geoScaleB);

    if (raw.contains("customTexHex") && raw["customTexHex"].is_object()) {
        const auto& ctex = raw["customTexHex"];
        if (auto customA = read_sparse_hex_array(ctex, "terrainA", r.textureShadesA + 1)) {
            r.customTexHexA = *customA;
        }
        if (auto customB = read_sparse_hex_array(ctex, "terrainB", r.textureShadesB + 1)) {
            r.customTexHexB = *customB;
        }
    }

    return r;
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
