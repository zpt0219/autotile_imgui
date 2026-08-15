#pragma once

#include "pattern_ribbon.h"
#include <string>
#include <vector>
#include <set>
#include <cstdint>

namespace atm {

struct CatalogItem {
    const char* id;
    const char* zh;
    const char* en;
};

struct CatalogGroup {
    const char* zh;
    const char* en;
    std::vector<CatalogItem> items;
};

struct GeoScaleItem {
    int id;
    const char* zh;
    const char* en;
};

constexpr int MIN_TEXTURE_SHADES = 1;
constexpr int MAX_TEXTURE_SHADES = 4;

constexpr int MIN_RIBBON_SHADES = 1;
constexpr int MAX_RIBBON_SHADES = 3;

const std::vector<CatalogGroup>& pattern_groups();
const std::vector<CatalogGroup>& texture_groups();
const std::vector<CatalogGroup>& ribbon_groups();
const std::vector<GeoScaleItem>& geo_scales();

int texture_period(const std::string& tex);
bool texture_uses_geo_scale(const std::string& tex);
int natural_geo_scale(const std::string& tex);
std::vector<GeoScaleItem> geo_scales_for(const std::string& tex);

std::set<int> used_texture_shades(
    const std::string& tex,
    double amount,
    int shades = 4,
    int cell_scale = 3,
    int ripple_scale = 4,
    int geo_scale = 1,
    int32_t seed = 0
);

bool ribbon_uses_period(const std::string& id);
double ribbon_min_width(const std::string& id);

std::set<int> used_ribbon_shades(
    const std::string& id,
    double width_px,
    double amount,
    int shades,
    int period,
    bool invert
);

} // namespace atm
