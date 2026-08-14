#include <doctest/doctest.h>
#include "pattern/catalog.h"
#include "model/recipe.h"
#include <set>
#include <vector>
#include <string>

TEST_CASE("U1 Catalog metadata counts and items") {
    const auto& p_groups = atm::pattern_groups();
    const auto& t_groups = atm::texture_groups();
    const auto& r_groups = atm::ribbon_groups();
    const auto& g_scales = atm::geo_scales();

    std::set<std::string> patterns;
    for (const auto& g : p_groups) {
        for (const auto& item : g.items) {
            patterns.insert(item.id);
        }
    }
    CHECK(patterns.size() == 11);

    std::set<std::string> textures;
    for (const auto& g : t_groups) {
        for (const auto& item : g.items) {
            textures.insert(item.id);
        }
    }
    CHECK(textures.size() == 26);

    std::set<std::string> ribbons;
    for (const auto& g : r_groups) {
        for (const auto& item : g.items) {
            ribbons.insert(item.id);
        }
    }
    CHECK(ribbons.size() == 15);

    CHECK(g_scales.size() == 4);

    SUBCASE("Bidirectional check: patterns against sanitize_recipe") {
        for (const auto& pid : patterns) {
            nlohmann::json j = { { "patternId", pid } };
            atm::Recipe r = atm::sanitize_recipe(j);
            CHECK(r.patternId == pid);
        }
    }

    SUBCASE("Bidirectional check: textures against sanitize_recipe") {
        for (const auto& tid : textures) {
            nlohmann::json j = { { "textureAlgoA", tid }, { "textureAlgoB", tid } };
            atm::Recipe r = atm::sanitize_recipe(j);
            CHECK(r.textureAlgoA == tid);
            CHECK(r.textureAlgoB == tid);
        }
    }

    SUBCASE("Bidirectional check: ribbons against sanitize_recipe") {
        for (const auto& rid : ribbons) {
            nlohmann::json j = { { "ribbonAlgo", rid } };
            atm::Recipe r = atm::sanitize_recipe(j);
            CHECK(r.ribbonAlgo == rid);
        }
    }

    SUBCASE("Geo scale helpers") {
        CHECK(atm::natural_geo_scale("nonslip") == 4);
        CHECK(atm::natural_geo_scale("brick_bond") == 2);
        CHECK(atm::natural_geo_scale("square") == 2);
        CHECK(atm::natural_geo_scale("octagonal") == 2);
        CHECK(atm::natural_geo_scale("cells") == 1);
        CHECK(atm::natural_geo_scale("none") == 1);

        CHECK(atm::texture_uses_geo_scale("square") == true);
        CHECK(atm::texture_uses_geo_scale("cells") == false);
        CHECK(atm::texture_uses_geo_scale("none") == false);

        auto scales_nonslip = atm::geo_scales_for("nonslip");
        CHECK(scales_nonslip.size() == 3); // 1, 2, 4
        for (const auto& s : scales_nonslip) {
            CHECK(s.id <= 4);
        }

        auto scales_square = atm::geo_scales_for("square");
        CHECK(scales_square.size() == 4);
    }

    SUBCASE("Ribbon period and invert helpers") {
        const std::vector<std::string> aperiodic = {
            "none", "bevel", "grain", "speckle",
            "along_brick_wall", "along_cobbles2", "along_weave",
            "along_stone_floor", "along_breeze_block", "along_octagonal"
        };
        for (const auto& id : aperiodic) {
            CHECK_MESSAGE(atm::ribbon_uses_period(id) == false, "Expected aperiodic: " << id);
        }

        const std::vector<std::string> periodic = {
            "dashes", "ticks", "beads", "rope", "wave"
        };
        for (const auto& id : periodic) {
            CHECK_MESSAGE(atm::ribbon_uses_period(id) == true, "Expected periodic: " << id);
        }

        CHECK(atm::ribbon_uses_invert("bevel") == true);
        CHECK(atm::ribbon_uses_invert("wave") == true);
        CHECK(atm::ribbon_uses_invert("rope") == true);
        CHECK(atm::ribbon_uses_invert("none") == false);
        CHECK(atm::ribbon_uses_invert("dashes") == false);

        CHECK(atm::ribbon_min_width("none") == 1.0);
        CHECK(atm::ribbon_min_width("bevel") == 2.0);
        CHECK(atm::ribbon_min_width("beads") == 3.0);
    }

    SUBCASE("Used shades scanning") {
        auto used_none = atm::used_texture_shades("none", 0.5);
        CHECK(used_none.empty());

        auto used_cells = atm::used_texture_shades("cells", 0.5, 4, 3, 4, 1, 0);
        CHECK(!used_cells.empty());

        auto used_ribbon_none = atm::used_ribbon_shades("none", 2.0, 0.5, 2, 8, false);
        CHECK(used_ribbon_none.empty());

        auto used_ribbon_bevel = atm::used_ribbon_shades("bevel", 2.0, 0.5, 2, 8, false);
        CHECK(!used_ribbon_bevel.empty());
    }
}
