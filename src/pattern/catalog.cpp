#include "catalog.h"
#include "pattern_texture.h"
#include "pattern_ribbon.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace atm {

const std::vector<CatalogGroup>& pattern_groups() {
    static const std::vector<CatalogGroup> groups = {
        {
            "规整边缘", "Clean edges",
            {
                { "square", "纯直角 · 方角描边", "Square — 90° right angles" },
                { "rounded", "圆润 · 全四级过渡", "Rounded — soft corners, full ramp" },
                { "sharp", "硬边 · 弧角描边", "Sharp — rounded corners, outline" },
                { "wave", "波浪 · 规则圆弧边", "Wave — regular circular arc edge" },
            }
        },
        {
            "不规则边缘", "Irregular edges",
            {
                { "jagged", "粗糙 · 岩石碎边", "Jagged — rough rocky edge" },
                { "gravel", "砂砾 · 细碎颗粒边", "Gravel — fine crumbling edge" },
                { "boulder", "巨砾 · 大块起伏", "Boulder — large rolling masses" },
                { "billow", "云絮 · 扇贝鼓边", "Billow — scalloped bulges" },
                { "coast", "海岸 · 多层碎屑", "Coast — multi-scale fractal edge" },
                { "moss", "苔藓 · 团簇细胞", "Moss — clustered cellular edge" },
                { "thorn", "荆棘 · 尖刺边", "Thorn — spiky ridged edge" },
            }
        }
    };
    return groups;
}

const std::vector<CatalogGroup>& texture_groups() {
    static const std::vector<CatalogGroup> groups = {
        {
            "无纹理", "None",
            {
                { "none", "无纹理", "None" },
            }
        },
        {
            "自然与有机", "Nature & Organic",
            {
                { "field", "草地颗粒 · Field", "Field — grassy ground" },
                { "rubble", "碎石地面 · Rubble", "Rubble — broken stone" },
                { "ripple", "水面波纹 · Ripples", "Ripples — short horizontal dashes" },
                { "ripple_diag", "斜向水波 · Diagonal Ripples", "Diagonal Ripples — 45° short dashes" },
                { "water", "水面边线 · Water", "Water — edge lines only" },
            }
        },
        {
            "程序与几何", "Procedural & Geometry",
            {
                { "cells", "多边形细胞 · Voronoi 细胞网格", "Polygonal Cells — Voronoi cell mesh" },
                { "square", "正方形铺砖 · 可调尺寸", "Square — plain square paving, sizeable" },
                { "hexagon", "规则六边形 · 可调尺寸", "Hexagon — regular hexagonal tiles, sizeable" },
                { "isometric", "等距菱形块 · 可调尺寸", "Isometric — diamond blocks, sizeable" },
                { "isometric_grid", "等距立体方块 · 可调尺寸", "Isometric Grid — 3D cube mesh, sizeable" },
                { "octagonal", "八边切角砖 (32px)", "Octagonal — chamfered square tiles (32)" },
                { "nonslip", "交叉防滑纹 · 可调尺寸", "Non-slip — textured grip, sizeable" },
            }
        },
        {
            "砖石与石板铺装", "Masonry & Paving",
            {
                { "brick_wall", "错缝砖墙 (32px)", "Brick Wall — running-bond masonry (32)" },
                { "brick_bond", "程序化错缝砖 · 可调尺寸", "Running Bond — procedural offset bricks, sizeable" },
                { "cobbles2", "细密错缝砖 (16px)", "Cobbles2 — fine running-bond bricks" },
                { "brick_floor", "45° 斜铺砖 (16px)", "Brick Floor — diagonal 45° bond" },
                { "weave", "菱格编织砖 (16px)", "Weave — diagonal interlocking bricks" },
                { "breeze_block", "镂空通风砖 (32px)", "Breeze Block — perforated masonry (32)" },
                { "paving", "乱砌石板 (32px)", "Paving — random ashlar flags (32)" },
                { "paving3", "等距立体方块 (32px)", "Paving3 — isometric cubes (32)" },
                { "paving5", "曲边咬合铺砖 (32px)", "Paving5 — interlocking curved pavers (32)" },
                { "stone_floor", "不规则石板地面 (32px)", "Stone Floor — irregular stone slabs (32)" },
            }
        },
        {
            "散点与半调噪声", "Speckle & Noise",
            {
                { "white", "白噪散点 · 随机沙粒", "White speckle — random sand" },
                { "blue", "蓝噪散点 · 均匀细颗粒", "Blue speckle — even fine grain" },
                { "ordered", "有序网点 · 规则半调", "Ordered — regular halftone" },
            }
        }
    };
    return groups;
}

const std::vector<CatalogGroup>& ribbon_groups() {
    static const std::vector<CatalogGroup> groups = {
        {
            "带内花纹", "Ribbon motifs",
            {
                { "none", "纯色 · 不加花纹", "Flat — no motif" },
                { "bevel", "倒角 · 内亮外暗", "Bevel — lit inside, dark out" },
                { "dashes", "虚线 · 等距断口", "Dashes — evenly broken" },
                { "ticks", "齿纹 · 垂直短划", "Ticks — perpendicular strokes" },
                { "beads", "珠链 · 等距圆点", "Beads — dots along the edge" },
                { "rope", "缆绳 · 斜向绞纹", "Rope — slanted twist" },
                { "wave", "波浪 · 起伏高光", "Wave — undulating highlight" },
                { "grain", "颗粒 · 带内碎点", "Grain — scatter in the ribbon" },
                { "speckle", "沿边细点 · 均匀", "Speckle — even fine dots" },
            }
        },
        {
            "沿轴纹理", "Textures laid along the axis",
            {
                { "along_brick_wall", "砖墙 · 沿边一列砖", "Brick wall — one course" },
                { "along_cobbles2", "细密砖 · 沿边小块", "Cobbles — fine bricks" },
                { "along_weave", "编织 · 沿边菱格", "Weave — diagonal braid" },
                { "along_stone_floor", "石板 · 沿边不规则块", "Stone — irregular slabs" },
                { "along_breeze_block", "通风砖 · 沿边细孔", "Breeze block — perforated" },
                { "along_octagonal", "八边形 · 沿边切角砖", "Octagonal — chamfered" },
            }
        }
    };
    return groups;
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

struct TextureDef {
    const char* id;
    int period;
    bool uses_geo_scale;
    int natural_geo_scale;
    bool max_geo_scale_4;
};

static const TextureDef TEXTURE_DEFS[] = {
    // id, period, uses_geo_scale, natural_geo_scale, max_geo_scale_4
    { "none",           16, false, 1, false },
    { "field",          32, false, 1, false },
    { "rubble",         32, false, 1, false },
    { "ripple",         32, false, 1, false },
    { "ripple_diag",    32, false, 1, false },
    { "water",          32, false, 1, false },
    { "cells",          32, false, 1, false },
    { "square",         32, true,  2, false },
    { "hexagon",        32, true,  1, true  },
    { "isometric",      32, true,  1, false },
    { "isometric_grid", 32, true,  1, true  },
    { "octagonal",      32, true,  2, false },
    { "nonslip",        32, true,  4, true  },
    { "brick_wall",     16, false, 1, false },
    { "brick_bond",     32, true,  2, true  },
    { "cobbles2",       16, false, 1, false },
    { "brick_floor",    16, false, 1, false },
    { "weave",          16, false, 1, false },
    { "breeze_block",   32, false, 1, false },
    { "paving",         32, false, 1, false },
    { "paving3",        32, false, 1, false },
    { "paving5",        32, false, 1, false },
    { "stone_floor",    32, false, 1, false },
    { "white",          16, false, 1, false },
    { "blue",           16, false, 1, false },
    { "ordered",        16, false, 1, false },
};

static const TextureDef* find_texture_def(const std::string& tex) {
    for (const auto& def : TEXTURE_DEFS) {
        if (tex == def.id) return &def;
    }
    return nullptr;
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
    static const std::vector<std::string> APERIODIC = {
        "none", "bevel", "grain", "speckle",
        "along_brick_wall", "along_cobbles2", "along_weave",
        "along_stone_floor", "along_breeze_block", "along_octagonal"
    };
    return std::find(APERIODIC.begin(), APERIODIC.end(), id) == APERIODIC.end();
}

double ribbon_min_width(const std::string& id) {
    static const std::unordered_map<std::string, double> MIN_WIDTH = {
        { "none", 1.0 },
        { "bevel", 2.0 }, { "dashes", 1.0 }, { "ticks", 2.0 },
        { "beads", 3.0 }, { "rope", 3.0 }, { "wave", 3.0 },
        { "grain", 1.0 }, { "speckle", 1.0 },
        { "along_brick_wall", 3.0 }, { "along_cobbles2", 3.0 },
        { "along_weave", 3.0 }, { "along_stone_floor", 3.0 },
        { "along_breeze_block", 3.0 }, { "along_octagonal", 3.0 }
    };
    auto it = MIN_WIDTH.find(id);
    if (it != MIN_WIDTH.end()) return it->second;
    return 1.0;
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
