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

// --- per-pattern geometry, served from the pattern registry in catalog.cpp ---
//
// Distances are in field units (the baked distance fields are 32x32 with a
// FIELD_STEP of 0.25), not screen pixels.

struct PatternBands {
    float b0, b1, b2, b3;
};

struct PatternOffsetRange {
    float min_off, max_off;
};

PatternBands pattern_bands(const std::string& pattern);
PatternOffsetRange pattern_offset_range(const std::string& pattern);

// Only some silhouettes take an edge re-roll: jittering `sharp` or `rounded`
// would not read as a variation of them, it would read as a broken outline.
bool pattern_is_reseedable(const std::string& pattern);

// Which pattern's baked distance field to sample. Almost always the pattern
// itself — `wave` is the exception: it rides a sine on top of `rounded`'s field
// rather than carrying one of its own.
std::string pattern_field_source(const std::string& pattern);

// --- per-texture switches, all served from the one registry in catalog.cpp ---
int texture_period(const std::string& tex);
bool texture_uses_amount(const std::string& tex);
bool texture_joint_at_rank_0(const std::string& tex);
bool texture_uses_geo_scale(const std::string& tex);
int natural_geo_scale(const std::string& tex);
std::vector<GeoScaleItem> geo_scales_for(const std::string& tex);

// Whitelists for the recipe sanitiser — derived from the catalogues above so
// that a new entry cannot be picker-visible but rejected on load.
bool is_known_pattern(const std::string& pattern);
bool is_known_texture(const std::string& tex);
bool is_known_ribbon(const std::string& ribbon);

std::set<int> used_texture_shades(
    const std::string& tex,
    double amount,
    int shades = 4,
    int cell_scale = 3,
    int ripple_scale = 4,
    int geo_scale = 1,
    int32_t seed = 0
);

// --- per-ribbon switches, served from the ribbon registry in catalog.cpp -----
bool ribbon_uses_period(const std::string& id);
bool ribbon_uses_invert(const std::string& id);
double ribbon_min_width(const std::string& id);

// Non-empty for the along_* motifs: the texture id to paint along the band
// axis. Empty for every motif that draws itself.
std::string ribbon_along_source(const std::string& id);

std::set<int> used_ribbon_shades(
    const std::string& id,
    double width_px,
    double amount,
    int shades,
    int period,
    bool invert
);

} // namespace atm
