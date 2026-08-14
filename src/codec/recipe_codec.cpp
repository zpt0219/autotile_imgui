#include "recipe_codec.h"
#include "pattern/pattern_paint.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace atm {

static const std::vector<std::string> PATTERNS = {
    "square", "sharp", "rounded", "wave", "jagged", "gravel",
    "boulder", "thorn", "coast", "moss", "billow"
};

static const std::vector<std::string> RIBBONS = {
    "none", "bevel", "dashes", "ticks", "beads", "rope", "wave", "grain", "speckle",
    "along_brick_wall", "along_cobbles2", "along_weave", "along_stone_floor",
    "along_breeze_block", "along_octagonal"
};

static const std::vector<std::string> TEXTURES = {
    "none", "white", "blue", "ordered", "ripple", "ripple_diag", "cells",
    "breeze_block", "brick_wall", "cobbles2", "brick_floor", "hexagon",
    "isometric", "isometric_grid", "octagonal", "square", "weave",
    "paving", "paving3", "paving5", "stone_floor", "water", "brick_bond",
    "field", "rubble", "nonslip"
};

static inline int find_index(const std::vector<std::string>& list, const std::string& item) {
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i] == item) return static_cast<int>(i);
    }
    return 0;
}

static inline void write24(std::vector<uint8_t>& buf, size_t offset, uint32_t val) {
    val = std::min(val, 0xffffffu);
    buf[offset] = (val >> 16) & 0xff;
    buf[offset + 1] = (val >> 8) & 0xff;
    buf[offset + 2] = val & 0xff;
}

static inline uint32_t read24(const std::vector<uint8_t>& buf, size_t offset) {
    return (static_cast<uint32_t>(buf[offset]) << 16) |
           (static_cast<uint32_t>(buf[offset + 1]) << 8) |
           static_cast<uint32_t>(buf[offset + 2]);
}

static inline void write_rgb(std::vector<uint8_t>& buf, size_t offset, const std::string& hex) {
    RGB c = parse_hex_colour(hex);
    buf[offset] = c.r;
    buf[offset + 1] = c.g;
    buf[offset + 2] = c.b;
}

static inline std::string read_rgb(const std::vector<uint8_t>& buf, size_t offset) {
    RGB c{ buf[offset], buf[offset + 1], buf[offset + 2] };
    return to_hex_colour(c);
}

// Base64URL
static const char BASE64URL_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static std::string base64url_encode(const std::vector<uint8_t>& data) {
    std::string out;
    size_t i = 0;
    while (i < data.size()) {
        size_t rem = data.size() - i;
        uint32_t b0 = data[i++];
        uint32_t b1 = (rem > 1) ? data[i++] : 0;
        uint32_t b2 = (rem > 2) ? data[i++] : 0;
        uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

        out.push_back(BASE64URL_CHARS[(triple >> 18) & 0x3f]);
        out.push_back(BASE64URL_CHARS[(triple >> 12) & 0x3f]);
        if (rem > 1) out.push_back(BASE64URL_CHARS[(triple >> 6) & 0x3f]);
        if (rem > 2) out.push_back(BASE64URL_CHARS[triple & 0x3f]);
    }
    return out;
}

static std::vector<uint8_t> base64url_decode(const std::string& str) {
    std::vector<uint8_t> out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[static_cast<uint8_t>(BASE64URL_CHARS[i])] = i;
    // Also allow standard base64 '+' and '/'
    T['+'] = 62;
    T['/'] = 63;

    uint32_t val = 0;
    int valb = -8;
    for (char c : str) {
        if (c == '=') break;
        int d = T[static_cast<uint8_t>(c)];
        if (d < 0) continue;
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xff));
            valb -= 8;
        }
    }
    return out;
}

std::string encode_recipe(const Recipe& recipe) {
    // Dynamic bytes calculation
    size_t dynamic_bytes = 0;
    bool has_custom_shades = recipe.customShadesHex.has_value();
    if (has_custom_shades) {
        dynamic_bytes += recipe.customShadesHex->size() * 3;
    }

    bool has_custom_ribbon = recipe.customRibbonHex.has_value();
    if (has_custom_ribbon) {
        dynamic_bytes += 1 + recipe.customRibbonHex->size() * 4;
    }

    bool has_custom_texA = recipe.customTexHexA.has_value();
    if (has_custom_texA) {
        dynamic_bytes += 1 + recipe.customTexHexA->size() * 4;
    }

    bool has_custom_texB = recipe.customTexHexB.has_value();
    if (has_custom_texB) {
        dynamic_bytes += 1 + recipe.customTexHexB->size() * 4;
    }

    size_t total_bytes = 45 + dynamic_bytes;
    std::vector<uint8_t> buf(total_bytes, 0);

    // Byte 0: version (4b = 1) | pattern (4b)
    int pattern_idx = find_index(PATTERNS, recipe.patternId);
    buf[0] = static_cast<uint8_t>((1 << 4) | (pattern_idx & 0x0f));

    // Bytes 1..9: RGB
    write_rgb(buf, 1, recipe.roleHex.terrainA);
    write_rgb(buf, 4, recipe.roleHex.terrainB);
    write_rgb(buf, 7, recipe.roleHex.edge);

    // Bytes 10..12: edgeSeed (24b)
    write24(buf, 10, static_cast<uint32_t>(recipe.edgeSeed));

    // Byte 13: outlineWidth(2b) | bandSteps(2b) | hardEdgeB(1b) | transparentB(1b) | (1 << 1)
    int outline_idx = std::max(0, std::min(3, recipe.outlineWidth - 1));
    int steps_idx = std::max(0, std::min(2, recipe.bandSteps - 3));
    uint8_t b13 = (outline_idx << 6) |
                  (steps_idx << 4) |
                  ((recipe.hardEdgeB ? 1 : 0) << 3) |
                  ((recipe.transparentB ? 1 : 0) << 2) |
                  (1 << 1);
    buf[13] = b13;

    // Byte 14: bandBias (Int8: -100..100)
    int bias_val = static_cast<int>(std::round(recipe.bandBias * 100.0));
    buf[14] = static_cast<uint8_t>(static_cast<int8_t>(bias_val));

    // Byte 15: noiseMask(3b) | ribbonAlgo(3b) | ribbonInvert(1b) | hasCustomShades(1b)
    uint8_t noise_mask = 0;
    for (auto nid : recipe.patternNoise) {
        if (nid == NoiseId::White) noise_mask |= 1;
        if (nid == NoiseId::Blue) noise_mask |= 2;
        if (nid == NoiseId::Ordered) noise_mask |= 4;
    }
    int ribbon_idx = find_index(RIBBONS, recipe.ribbonAlgo);
    uint8_t b15 = (noise_mask << 5) |
                  ((ribbon_idx & 7) << 2) |
                  ((recipe.ribbonInvert ? 1 : 0) << 1) |
                  (has_custom_shades ? 1 : 0);
    buf[15] = b15;

    // Bytes 16..18: patternNoiseSeed (24b)
    write24(buf, 16, static_cast<uint32_t>(recipe.patternNoiseSeed));

    // Byte 19: patternNoiseStrength (Uint8: 0..200)
    buf[19] = static_cast<uint8_t>(std::max(0, std::min(255, static_cast<int>(std::round(recipe.patternNoiseStrength * 100.0)))));

    // Bytes 20..21: ribbonAmount(8b) | ribbonPeriod(4b) | ribbonShades(4b)
    buf[20] = static_cast<uint8_t>(std::max(0, std::min(255, static_cast<int>(std::round(recipe.ribbonAmount * 200.0)))));
    int period_idx = std::max(0, std::min(7, recipe.ribbonPeriod - 1));
    int shades_idx = std::max(0, std::min(3, recipe.ribbonShades - 1));
    buf[21] = static_cast<uint8_t>((period_idx << 4) | shades_idx);

    // Texture A (Bytes 22..29)
    int tex_a_idx = find_index(TEXTURES, recipe.textureAlgoA);
    int tex_a_shades_idx = std::max(0, std::min(3, recipe.textureShadesA - 1));
    buf[22] = static_cast<uint8_t>(((tex_a_idx & 0x1f) << 3) | ((tex_a_shades_idx & 0x03) << 1));
    buf[23] = static_cast<uint8_t>(std::max(0, std::min(255, static_cast<int>(std::round(recipe.textureAmountA * 100.0)))));
    write24(buf, 24, static_cast<uint32_t>(recipe.textureSeedA));
    write_rgb(buf, 27, "#ffffff");

    // Texture B (Bytes 30..37)
    int tex_b_idx = find_index(TEXTURES, recipe.textureAlgoB);
    int tex_b_shades_idx = std::max(0, std::min(3, recipe.textureShadesB - 1));
    buf[30] = static_cast<uint8_t>(((tex_b_idx & 0x1f) << 3) | ((tex_b_shades_idx & 0x03) << 1));
    buf[31] = static_cast<uint8_t>(std::max(0, std::min(255, static_cast<int>(std::round(recipe.textureAmountB * 100.0)))));
    write24(buf, 32, static_cast<uint32_t>(recipe.textureSeedB));
    write_rgb(buf, 35, "#ffffff");

    // Fine Scales (Bytes 38..43)
    auto enc_scale = [](int v) -> uint8_t {
        return static_cast<uint8_t>(std::max(1, std::min(255, v)));
    };
    buf[38] = enc_scale(recipe.cellScaleA);
    buf[39] = enc_scale(recipe.cellScaleB);
    buf[40] = enc_scale(recipe.rippleScaleA);
    buf[41] = enc_scale(recipe.rippleScaleB);
    buf[42] = enc_scale(recipe.geoScaleA);
    buf[43] = enc_scale(recipe.geoScaleB);

    // Byte 44: customTexMask(2b) | hasCustomRibbon(1b)
    uint8_t b44 = ((has_custom_texA ? 1 : 0) << 7) |
                  ((has_custom_texB ? 1 : 0) << 6) |
                  ((has_custom_ribbon ? 1 : 0) << 5);
    buf[44] = b44;

    // Dynamic Writing (Byte 45+)
    size_t cur = 45;
    if (has_custom_shades && recipe.customShadesHex.has_value()) {
        for (const auto& hex : *recipe.customShadesHex) {
            write_rgb(buf, cur, hex);
            cur += 3;
        }
    }

    if (has_custom_ribbon && recipe.customRibbonHex.has_value()) {
        buf[cur++] = static_cast<uint8_t>(recipe.customRibbonHex->size());
        for (const auto& hex_opt : *recipe.customRibbonHex) {
            if (hex_opt.has_value()) {
                buf[cur++] = 1;
                write_rgb(buf, cur, *hex_opt);
                cur += 3;
            } else {
                buf[cur++] = 0;
            }
        }
    }

    if (has_custom_texA && recipe.customTexHexA.has_value()) {
        buf[cur++] = static_cast<uint8_t>(recipe.customTexHexA->size());
        for (const auto& hex_opt : *recipe.customTexHexA) {
            if (hex_opt.has_value()) {
                buf[cur++] = 1;
                write_rgb(buf, cur, *hex_opt);
                cur += 3;
            } else {
                buf[cur++] = 0;
            }
        }
    }

    if (has_custom_texB && recipe.customTexHexB.has_value()) {
        buf[cur++] = static_cast<uint8_t>(recipe.customTexHexB->size());
        for (const auto& hex_opt : *recipe.customTexHexB) {
            if (hex_opt.has_value()) {
                buf[cur++] = 1;
                write_rgb(buf, cur, *hex_opt);
                cur += 3;
            } else {
                buf[cur++] = 0;
            }
        }
    }

    return base64url_encode(buf);
}

std::optional<Recipe> decode_recipe(const std::string& hash) {
    if (hash.empty()) return std::nullopt;

    auto bytes = base64url_decode(hash);
    if (bytes.size() < 45) return std::nullopt;

    uint8_t b0 = bytes[0];
    int version = (b0 >> 4) & 0x0f;
    if (version != 1) return std::nullopt;

    Recipe r;
    int pattern_idx = b0 & 0x0f;
    if (pattern_idx >= 0 && pattern_idx < static_cast<int>(PATTERNS.size())) {
        r.patternId = PATTERNS[pattern_idx];
    }

    r.roleHex.terrainA = read_rgb(bytes, 1);
    r.roleHex.terrainB = read_rgb(bytes, 4);
    r.roleHex.edge = read_rgb(bytes, 7);

    r.edgeSeed = static_cast<int>(read24(bytes, 10));

    uint8_t b13 = bytes[13];
    r.outlineWidth = ((b13 >> 6) & 0x03) + 1;
    r.bandSteps = ((b13 >> 4) & 0x03) + 3;
    r.hardEdgeB = ((b13 >> 3) & 0x01) == 1;
    r.transparentB = ((b13 >> 2) & 0x01) == 1;

    int8_t bias_i8 = static_cast<int8_t>(bytes[14]);
    r.bandBias = static_cast<double>(bias_i8) / 100.0;

    uint8_t b15 = bytes[15];
    uint8_t noise_mask = (b15 >> 5) & 0x07;
    r.patternNoise.clear();
    if (noise_mask & 1) r.patternNoise.push_back(NoiseId::White);
    if (noise_mask & 2) r.patternNoise.push_back(NoiseId::Blue);
    if (noise_mask & 4) r.patternNoise.push_back(NoiseId::Ordered);

    int ribbon_idx = (b15 >> 2) & 0x07;
    if (ribbon_idx >= 0 && ribbon_idx < static_cast<int>(RIBBONS.size())) {
        r.ribbonAlgo = RIBBONS[ribbon_idx];
    }
    r.ribbonInvert = ((b15 >> 1) & 0x01) == 1;
    bool has_custom_shades = (b15 & 0x01) == 1;

    r.patternNoiseSeed = static_cast<int>(read24(bytes, 16));
    r.patternNoiseStrength = static_cast<double>(bytes[19]) / 100.0;

    r.ribbonAmount = static_cast<double>(bytes[20]) / 200.0;
    uint8_t b21 = bytes[21];
    r.ribbonPeriod = ((b21 >> 4) & 0x0f) + 1;
    r.ribbonShades = (b21 & 0x0f) + 1;

    uint8_t b22 = bytes[22];
    int tex_a_idx = (b22 >> 3) & 0x1f;
    if (tex_a_idx >= 0 && tex_a_idx < static_cast<int>(TEXTURES.size())) {
        r.textureAlgoA = TEXTURES[tex_a_idx];
    }
    r.textureShadesA = ((b22 >> 1) & 0x03) + 1;
    r.textureAmountA = static_cast<double>(bytes[23]) / 100.0;
    r.textureSeedA = static_cast<int>(read24(bytes, 24));

    uint8_t b30 = bytes[30];
    int tex_b_idx = (b30 >> 3) & 0x1f;
    if (tex_b_idx >= 0 && tex_b_idx < static_cast<int>(TEXTURES.size())) {
        r.textureAlgoB = TEXTURES[tex_b_idx];
    }
    r.textureShadesB = ((b30 >> 1) & 0x03) + 1;
    r.textureAmountB = static_cast<double>(bytes[31]) / 100.0;
    r.textureSeedB = static_cast<int>(read24(bytes, 32));

    auto dec_scale = [](uint8_t v) -> int {
        return static_cast<int>(v);
    };
    r.cellScaleA = dec_scale(bytes[38]);
    r.cellScaleB = dec_scale(bytes[39]);
    r.rippleScaleA = dec_scale(bytes[40]);
    r.rippleScaleB = dec_scale(bytes[41]);
    r.geoScaleA = dec_scale(bytes[42]);
    r.geoScaleB = dec_scale(bytes[43]);

    uint8_t b44 = bytes[44];
    bool has_custom_texA = ((b44 >> 7) & 0x01) == 1;
    bool has_custom_texB = ((b44 >> 6) & 0x01) == 1;
    bool has_custom_ribbon = ((b44 >> 5) & 0x01) == 1;

    size_t cur = 45;
    if (has_custom_shades) {
        int shade_count = r.bandSteps + 2;
        std::vector<std::string> shades;
        for (int i = 0; i < shade_count; ++i) {
            if (cur + 3 <= bytes.size()) {
                shades.push_back(read_rgb(bytes, cur));
                cur += 3;
            }
        }
        r.customShadesHex = shades;
    }

    if (has_custom_ribbon && cur < bytes.size()) {
        uint8_t len = bytes[cur++];
        std::vector<std::optional<std::string>> custom;
        for (uint8_t i = 0; i < len; ++i) {
            if (cur < bytes.size()) {
                uint8_t flag = bytes[cur++];
                if (flag == 1 && cur + 3 <= bytes.size()) {
                    custom.push_back(read_rgb(bytes, cur));
                    cur += 3;
                } else {
                    custom.push_back(std::nullopt);
                }
            }
        }
        r.customRibbonHex = custom;
    }

    if (has_custom_texA && cur < bytes.size()) {
        uint8_t len = bytes[cur++];
        std::vector<std::optional<std::string>> custom;
        for (uint8_t i = 0; i < len; ++i) {
            if (cur < bytes.size()) {
                uint8_t flag = bytes[cur++];
                if (flag == 1 && cur + 3 <= bytes.size()) {
                    custom.push_back(read_rgb(bytes, cur));
                    cur += 3;
                } else {
                    custom.push_back(std::nullopt);
                }
            }
        }
        r.customTexHexA = custom;
    }

    if (has_custom_texB && cur < bytes.size()) {
        uint8_t len = bytes[cur++];
        std::vector<std::optional<std::string>> custom;
        for (uint8_t i = 0; i < len; ++i) {
            if (cur < bytes.size()) {
                uint8_t flag = bytes[cur++];
                if (flag == 1 && cur + 3 <= bytes.size()) {
                    custom.push_back(read_rgb(bytes, cur));
                    cur += 3;
                } else {
                    custom.push_back(std::nullopt);
                }
            }
        }
        r.customTexHexB = custom;
    }

    return r;
}

} // namespace atm
