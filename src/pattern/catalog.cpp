#include "catalog.h"
#include "pattern_texture.h"
#include "pattern_ribbon.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace atm {

// --- the pattern registry ---------------------------------------------------
//
// One row per silhouette. Row order is the picker's display order. Adding a
// pattern means a row here plus its baked distance field in pattern_data.cpp.

enum class PatternGroup {
    Clean,
    Irregular
};

struct PatternDef {
    const char*  id;
    PatternGroup group;
    const char*  zh;
    const char*  en;
    PatternBands bands;
    PatternOffsetRange offset;
    bool         reseedable;    // does an edge seed jitter this silhouette
    const char*  field_source;  // nullptr = uses its own baked field
};

static const PatternDef PATTERN_DEFS[] = {
    { "square",  PatternGroup::Clean,     "纯直角 · 方角描边", "Square — 90° right angles",
      { 7.0f, 9.0f, 11.0f, 13.0f }, { -8.50f, 6.25f }, false, nullptr },
    { "rounded", PatternGroup::Clean,     "圆润 · 全四级过渡", "Rounded — soft corners, full ramp",
      { 7.0f, 9.0f, 11.0f, 13.0f }, { -3.75f, 2.75f }, false, nullptr },
    { "sharp",   PatternGroup::Clean,     "硬边 · 弧角描边", "Sharp — rounded corners, outline",
      { 7.0f, 9.0f, 11.0f, 13.0f }, { -8.50f, 6.25f }, false, nullptr },
    // wave has no field of its own; it modulates rounded's with a sine.
    { "wave",    PatternGroup::Clean,     "波浪 · 规则圆弧边", "Wave — regular circular arc edge",
      { 7.0f, 9.0f, 11.0f, 13.0f }, { -3.75f, 2.75f }, true,  "rounded" },

    { "jagged",  PatternGroup::Irregular, "粗糙 · 岩石碎边", "Jagged — rough rocky edge",
      { 7.0f, 9.0f, 11.0f, 13.0f }, { -5.50f, 1.00f }, true,  nullptr },
    { "gravel",  PatternGroup::Irregular, "砂砾 · 细碎颗粒边", "Gravel — fine crumbling edge",
      { 7.0f, 9.0f, 11.0f, 13.0f }, { -5.25f, 1.50f }, true,  nullptr },
    { "boulder", PatternGroup::Irregular, "巨砾 · 大块起伏", "Boulder — large rolling masses",
      { 8.0f, 10.0f, 12.0f, 14.0f }, { -4.00f, 1.25f }, true,  nullptr },
    { "billow",  PatternGroup::Irregular, "云絮 · 扇贝鼓边", "Billow — scalloped bulges",
      { 7.0f, 9.0f, 11.0f, 13.0f }, { -2.75f, 1.00f }, true,  nullptr },
    { "coast",   PatternGroup::Irregular, "海岸 · 多层碎屑", "Coast — multi-scale fractal edge",
      { 7.5f, 9.5f, 11.5f, 13.5f }, { -3.75f, 2.25f }, true,  nullptr },
    { "moss",    PatternGroup::Irregular, "苔藓 · 团簇细胞", "Moss — clustered cellular edge",
      { 7.0f, 9.0f, 11.0f, 13.0f }, { -5.75f, 1.00f }, true,  nullptr },
    { "thorn",   PatternGroup::Irregular, "荆棘 · 尖刺边", "Thorn — spiky ridged edge",
      { 7.5f, 9.0f, 10.0f, 12.0f }, { -4.50f, 1.25f }, true,  nullptr },
};

static const PatternDef* find_pattern_def(const std::string& pattern) {
    for (const auto& def : PATTERN_DEFS) {
        if (pattern == def.id) return &def;
    }
    return nullptr;
}

const std::vector<CatalogGroup>& pattern_groups() {
    struct GroupLabel { PatternGroup group; const char* zh; const char* en; };
    static const GroupLabel LABELS[] = {
        { PatternGroup::Clean,     "规整边缘", "Clean edges" },
        { PatternGroup::Irregular, "不规则边缘", "Irregular edges" },
    };

    static const std::vector<CatalogGroup> groups = [] {
        std::vector<CatalogGroup> out;
        out.reserve(sizeof(LABELS) / sizeof(LABELS[0]));
        for (const auto& label : LABELS) {
            CatalogGroup group{ label.zh, label.en, {} };
            for (const auto& def : PATTERN_DEFS) {
                if (def.group == label.group) {
                    group.items.push_back({ def.id, def.zh, def.en });
                }
            }
            out.push_back(std::move(group));
        }
        return out;
    }();
    return groups;
}

// The fallbacks below reproduce what the old if-chains in pattern_data.cpp
// returned for an id they did not recognise.
PatternBands pattern_bands(const std::string& pattern) {
    auto* def = find_pattern_def(pattern);
    return def ? def->bands : PatternBands{ 7.0f, 9.0f, 11.0f, 13.0f };
}

PatternOffsetRange pattern_offset_range(const std::string& pattern) {
    auto* def = find_pattern_def(pattern);
    return def ? def->offset : PatternOffsetRange{ -3.75f, 2.75f };
}

bool pattern_is_reseedable(const std::string& pattern) {
    auto* def = find_pattern_def(pattern);
    return def ? def->reseedable : false;
}

std::string pattern_field_source(const std::string& pattern) {
    auto* def = find_pattern_def(pattern);
    if (def && def->field_source) return def->field_source;
    return pattern;
}

// --- the texture registry --------------------------------------------------
//
// One row per texture, holding everything declarative about it. This table is
// the single source of truth for the picker (grouping + labels), the sanitiser
// whitelist, and the per-texture switches the renderer reads. Adding a texture
// means one row here and the algorithm itself in `pattern_texture.cpp` — two
// files, nothing else to keep in step.
//
// Row order is the picker's display order; it must stay grouped, because
// `texture_groups()` is built by walking this table in order.

enum class TextureGroup {
    None,
    Nature,
    Procedural,
    Masonry,
    Speckle
};

struct TextureDef {
    const char*  id;
    TextureGroup group;
    const char*  zh;
    const char*  en;
    int  period;            // sampling period in px: 16 or 32
    bool uses_amount;       // is the "amount" slider live for this texture
    bool joint_at_rank_0;   // baked table keeps the joint at rank 0, not BAKED_RANKS
    bool uses_geo_scale;
    int  natural_geo_scale;
    bool max_geo_scale_4;   // picker stops at 4 rather than 8
};

static const TextureDef TEXTURE_DEFS[] = {
    { "none",           TextureGroup::None,       "无纹理", "None",
      16, false, false, false, 1, false },

    { "field",          TextureGroup::Nature,     "草地颗粒 · Field", "Field — grassy ground",
      32, false, false, false, 1, false },
    { "rubble",         TextureGroup::Nature,     "碎石地面 · Rubble", "Rubble — broken stone",
      32, false, false, false, 1, false },
    { "ripple",         TextureGroup::Nature,     "水面波纹 · Ripples", "Ripples — short horizontal dashes",
      32, true,  false, false, 1, false },
    { "ripple_diag",    TextureGroup::Nature,     "斜向水波 · Diagonal Ripples", "Diagonal Ripples — 45° short dashes",
      32, true,  false, false, 1, false },
    { "water",          TextureGroup::Nature,     "水面边线 · Water", "Water — edge lines only",
      32, true,  false, false, 1, false },

    { "cells",          TextureGroup::Procedural, "多边形细胞 · Voronoi 细胞网格", "Polygonal Cells — Voronoi cell mesh",
      32, false, false, false, 1, false },
    { "square",         TextureGroup::Procedural, "正方形铺砖 · 可调尺寸", "Square — plain square paving, sizeable",
      32, false, false, true,  2, false },
    { "hexagon",        TextureGroup::Procedural, "规则六边形 · 可调尺寸", "Hexagon — regular hexagonal tiles, sizeable",
      32, false, false, true,  1, true  },
    { "isometric",      TextureGroup::Procedural, "等距菱形块 · 可调尺寸", "Isometric — diamond blocks, sizeable",
      32, false, false, true,  1, false },
    { "isometric_grid", TextureGroup::Procedural, "等距立体方块 · 可调尺寸", "Isometric Grid — 3D cube mesh, sizeable",
      32, false, false, true,  1, true  },
    { "octagonal",      TextureGroup::Procedural, "八边切角砖 (32px)", "Octagonal — chamfered square tiles (32)",
      32, false, false, true,  2, false },
    { "nonslip",        TextureGroup::Procedural, "交叉防滑纹 · 可调尺寸", "Non-slip — textured grip, sizeable",
      32, true,  false, true,  4, true  },

    { "brick_wall",     TextureGroup::Masonry,    "错缝砖墙 (32px)", "Brick Wall — running-bond masonry (32)",
      16, false, true,  false, 1, false },
    { "brick_bond",     TextureGroup::Masonry,    "程序化错缝砖 · 可调尺寸", "Running Bond — procedural offset bricks, sizeable",
      32, false, false, true,  2, true  },
    { "cobbles2",       TextureGroup::Masonry,    "细密错缝砖 (16px)", "Cobbles2 — fine running-bond bricks",
      16, false, true,  false, 1, false },
    { "brick_floor",    TextureGroup::Masonry,    "45° 斜铺砖 (16px)", "Brick Floor — diagonal 45° bond",
      16, false, true,  false, 1, false },
    { "weave",          TextureGroup::Masonry,    "菱格编织砖 (16px)", "Weave — diagonal interlocking bricks",
      16, false, false, false, 1, false },
    { "breeze_block",   TextureGroup::Masonry,    "镂空通风砖 (32px)", "Breeze Block — perforated masonry (32)",
      32, false, true,  false, 1, false },
    { "paving",         TextureGroup::Masonry,    "乱砌石板 (32px)", "Paving — random ashlar flags (32)",
      32, false, false, false, 1, false },
    { "paving3",        TextureGroup::Masonry,    "等距立体方块 (32px)", "Paving3 — isometric cubes (32)",
      32, false, false, false, 1, false },
    { "paving5",        TextureGroup::Masonry,    "曲边咬合铺砖 (32px)", "Paving5 — interlocking curved pavers (32)",
      32, false, false, false, 1, false },
    { "stone_floor",    TextureGroup::Masonry,    "不规则石板地面 (32px)", "Stone Floor — irregular stone slabs (32)",
      32, false, true,  false, 1, false },

    { "white",          TextureGroup::Speckle,    "白噪散点 · 随机沙粒", "White speckle — random sand",
      16, true,  false, false, 1, false },
    { "blue",           TextureGroup::Speckle,    "蓝噪散点 · 均匀细颗粒", "Blue speckle — even fine grain",
      16, true,  false, false, 1, false },
    { "ordered",        TextureGroup::Speckle,    "有序网点 · 规则半调", "Ordered — regular halftone",
      16, true,  false, false, 1, false },
};

static const TextureDef* find_texture_def(const std::string& tex) {
    for (const auto& def : TEXTURE_DEFS) {
        if (tex == def.id) return &def;
    }
    return nullptr;
}

const std::vector<CatalogGroup>& texture_groups() {
    struct GroupLabel { TextureGroup group; const char* zh; const char* en; };
    static const GroupLabel LABELS[] = {
        { TextureGroup::None,       "无纹理", "None" },
        { TextureGroup::Nature,     "自然与有机", "Nature & Organic" },
        { TextureGroup::Procedural, "程序与几何", "Procedural & Geometry" },
        { TextureGroup::Masonry,    "砖石与石板铺装", "Masonry & Paving" },
        { TextureGroup::Speckle,    "散点与半调噪声", "Speckle & Noise" },
    };

    static const std::vector<CatalogGroup> groups = [] {
        std::vector<CatalogGroup> out;
        out.reserve(sizeof(LABELS) / sizeof(LABELS[0]));
        for (const auto& label : LABELS) {
            CatalogGroup group{ label.zh, label.en, {} };
            for (const auto& def : TEXTURE_DEFS) {
                if (def.group == label.group) {
                    group.items.push_back({ def.id, def.zh, def.en });
                }
            }
            out.push_back(std::move(group));
        }
        return out;
    }();
    return groups;
}

// --- the ribbon registry ----------------------------------------------------
//
// One row per motif.

enum class RibbonGroup {
    Motif,
    AlongAxis
};

struct RibbonDef {
    const char* id;
    RibbonGroup group;
    const char* zh;
    const char* en;
    bool uses_period;      // is the "period" control live
    bool uses_invert;      // does the invert flag flip this motif's depth
    double min_width;      // ribbon narrower than this cannot show the motif
    const char* along_source;  // non-null: paint this texture along the band axis
};

static const RibbonDef RIBBON_DEFS[] = {
    { "none",    RibbonGroup::Motif, "纯色 · 不加花纹", "Flat — no motif",
      false, false, 1.0, nullptr },
    { "bevel",   RibbonGroup::Motif, "倒角 · 内亮外暗", "Bevel — lit inside, dark out",
      false, true,  2.0, nullptr },
    { "dashes",  RibbonGroup::Motif, "虚线 · 等距断口", "Dashes — evenly broken",
      true,  false, 1.0, nullptr },
    { "ticks",   RibbonGroup::Motif, "齿纹 · 垂直短划", "Ticks — perpendicular strokes",
      true,  false, 2.0, nullptr },
    { "beads",   RibbonGroup::Motif, "珠链 · 等距圆点", "Beads — dots along the edge",
      true,  false, 3.0, nullptr },
    { "rope",    RibbonGroup::Motif, "缆绳 · 斜向绞纹", "Rope — slanted twist",
      true,  true,  3.0, nullptr },
    { "wave",    RibbonGroup::Motif, "波浪 · 起伏高光", "Wave — undulating highlight",
      true,  true,  3.0, nullptr },
    { "grain",   RibbonGroup::Motif, "颗粒 · 带内碎点", "Grain — scatter in the ribbon",
      false, false, 1.0, nullptr },
    { "speckle", RibbonGroup::Motif, "沿边细点 · 均匀", "Speckle — even fine dots",
      false, false, 1.0, nullptr },

    { "along_brick_wall",   RibbonGroup::AlongAxis, "砖墙 · 沿边一列砖", "Brick wall — one course",
      false, false, 3.0, "brick_wall" },
    { "along_cobbles2",     RibbonGroup::AlongAxis, "细密砖 · 沿边小块", "Cobbles — fine bricks",
      false, false, 3.0, "cobbles2" },
    { "along_weave",        RibbonGroup::AlongAxis, "编织 · 沿边菱格", "Weave — diagonal braid",
      false, false, 3.0, "weave" },
    { "along_stone_floor",  RibbonGroup::AlongAxis, "石板 · 沿边不规则块", "Stone — irregular slabs",
      false, false, 3.0, "stone_floor" },
    { "along_breeze_block", RibbonGroup::AlongAxis, "通风砖 · 沿边细孔", "Breeze block — perforated",
      false, false, 3.0, "breeze_block" },
    { "along_octagonal",    RibbonGroup::AlongAxis, "八边形 · 沿边切角砖", "Octagonal — chamfered",
      false, false, 3.0, "octagonal" },
};

static const RibbonDef* find_ribbon_def(const std::string& id) {
    for (const auto& def : RIBBON_DEFS) {
        if (id == def.id) return &def;
    }
    return nullptr;
}

const std::vector<CatalogGroup>& ribbon_groups() {
    struct GroupLabel { RibbonGroup group; const char* zh; const char* en; };
    static const GroupLabel LABELS[] = {
        { RibbonGroup::Motif,     "带内花纹", "Ribbon motifs" },
        { RibbonGroup::AlongAxis, "沿轴纹理", "Textures laid along the axis" },
    };

    static const std::vector<CatalogGroup> groups = [] {
        std::vector<CatalogGroup> out;
        out.reserve(sizeof(LABELS) / sizeof(LABELS[0]));
        for (const auto& label : LABELS) {
            CatalogGroup group{ label.zh, label.en, {} };
            for (const auto& def : RIBBON_DEFS) {
                if (def.group == label.group) {
                    group.items.push_back({ def.id, def.zh, def.en });
                }
            }
            out.push_back(std::move(group));
        }
        return out;
    }();
    return groups;
}

bool ribbon_uses_invert(const std::string& id) {
    auto* def = find_ribbon_def(id);
    return def ? def->uses_invert : false;
}

std::string ribbon_along_source(const std::string& id) {
    auto* def = find_ribbon_def(id);
    return (def && def->along_source) ? def->along_source : std::string();
}

const std::vector<GeoScaleItem>& geo_scales() {
    static const std::vector<GeoScaleItem> scales = {
        { 1, "32px · 原尺寸", "32px — original" },
        { 2, "16px", "16px" },
        { 4, "8px", "8px" },
        { 8, "4px", "4px" },
    };
    return scales;
}

int texture_period(const std::string& texture) {
    auto* def = find_texture_def(texture);
    return def ? def->period : 16;
}

bool texture_uses_geo_scale(const std::string& texture) {
    auto* def = find_texture_def(texture);
    return def ? def->uses_geo_scale : false;
}

int natural_geo_scale(const std::string& texture) {
    auto* def = find_texture_def(texture);
    return def ? def->natural_geo_scale : 1;
}

bool texture_uses_amount(const std::string& texture) {
    // An id that is not in the registry keeps the old fallback: everything
    // except "none" was assumed to take an amount.
    auto* def = find_texture_def(texture);
    return def ? def->uses_amount : (texture != "none");
}

bool texture_joint_at_rank_0(const std::string& texture) {
    auto* def = find_texture_def(texture);
    return def ? def->joint_at_rank_0 : false;
}

bool is_known_texture(const std::string& texture) {
    return find_texture_def(texture) != nullptr;
}

static bool catalogue_contains(const std::vector<CatalogGroup>& groups, const std::string& id) {
    for (const auto& group : groups) {
        for (const auto& item : group.items) {
            if (id == item.id) return true;
        }
    }
    return false;
}

bool is_known_pattern(const std::string& pattern) {
    return catalogue_contains(pattern_groups(), pattern);
}

bool is_known_ribbon(const std::string& ribbon) {
    return catalogue_contains(ribbon_groups(), ribbon);
}

std::vector<GeoScaleItem> geo_scales_for(const std::string& texture) {
    const auto& all = geo_scales();
    auto* def = find_texture_def(texture);
    if (def && def->max_geo_scale_4) {
        std::vector<GeoScaleItem> filtered;
        for (const auto& item : all) {
            if (item.id <= 4) {
                filtered.push_back(item);
            }
        }
        return filtered;
    }
    return all;
}

std::set<int> used_texture_shades(
    const std::string& tex,
    double amount,
    int shades,
    int cell_scale,
    int ripple_scale,
    int geo_scale,
    int32_t seed
) {
    std::set<int> used;
    if (tex == "none") return used;
    int p = texture_period(tex);
    for (int y = 0; y < p; ++y) {
        for (int x = 0; x < p; ++x) {
            used.insert(texture_shade_at(tex, x, y, seed, amount, shades, cell_scale, ripple_scale, geo_scale));
        }
    }
    return used;
}

bool ribbon_uses_period(const std::string& id) {
    auto* def = find_ribbon_def(id);
    return def ? def->uses_period : false;
}

double ribbon_min_width(const std::string& id) {
    auto* def = find_ribbon_def(id);
    return def ? def->min_width : 1.0;
}


std::set<int> used_ribbon_shades(
    const std::string& id,
    double width_px,
    double amount,
    int shades,
    int period,
    bool invert
) {
    std::set<int> used;
    if (id == "none") return used;
    int w = std::max(1, static_cast<int>(std::round(width_px)));
    int span = std::max(32, static_cast<int>(std::ceil(period)) * 4);
    for (int i = 0; i < span; ++i) {
        for (int j = 0; j < w; ++j) {
            double depth = (j + 0.5) / w;
            used.insert(ribbon_shade_at(id, i, depth, w, 0, amount, shades, period, invert));
        }
    }
    return used;
}

} // namespace atm
