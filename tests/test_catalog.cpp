#include <doctest/doctest.h>
#include "pattern/catalog.h"
#include "model/recipe.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <string>

namespace {

std::set<std::string> flatten(const std::vector<atm::CatalogGroup>& groups) {
    std::set<std::string> ids;
    for (const auto& g : groups) {
        for (const auto& item : g.items) ids.insert(item.id);
    }
    return ids;
}

std::ifstream open_repo_file(const std::string& relative) {
    std::ifstream f(relative);
    if (!f.is_open()) f.open("../" + relative);
    if (!f.is_open()) f.open("../../" + relative);
    return f;
}

} // namespace

// The counts below used to be hand-copied constants, which only catches a
// catalogue that lost an entry - not one that was never given the entry, and
// not a label that drifted. catalogue_ids.json is parsed straight out of
// reference/*.ts by tests/data/dump_reference_vectors.js, so this compares the
// port against the specification in both directions.
TEST_CASE("U1 Catalogues match reference/*.ts exactly, both directions") {
    std::ifstream file = open_repo_file("tests/data/catalogue_ids.json");
    REQUIRE(file.is_open());

    nlohmann::json expected;
    file >> expected;

    struct Case {
        const char* key;
        std::set<std::string> actual;
        const std::vector<atm::CatalogGroup>* groups;
    };
    std::vector<Case> cases = {
        { "pattern", flatten(atm::pattern_groups()), &atm::pattern_groups() },
        { "texture", flatten(atm::texture_groups()), &atm::texture_groups() },
        { "ribbon",  flatten(atm::ribbon_groups()),  &atm::ribbon_groups()  },
    };

    for (auto& c : cases) {
        INFO("catalogue: " << c.key);
        REQUIRE(expected.contains(c.key));

        std::set<std::string> want;
        std::map<std::string, std::pair<std::string, std::string>> labels;
        for (const auto& item : expected[c.key]) {
            const std::string id = item["id"].get<std::string>();
            want.insert(id);
            labels[id] = { item["zh"].get<std::string>(), item["en"].get<std::string>() };
        }

        CHECK(c.actual.size() == want.size());

        // Direction 1: nothing in the port that the reference does not have.
        for (const auto& id : c.actual) {
            INFO("id present in C++ but not in reference: " << id);
            CHECK(want.count(id) == 1);
        }
        // Direction 2: nothing in the reference the port failed to carry over.
        for (const auto& id : want) {
            INFO("id in reference but missing from C++ catalogue: " << id);
            CHECK(c.actual.count(id) == 1);
        }

        // The labels are the whole reason these catalogues exist; a picker with
        // the right ids and drifted text is still wrong.
        for (const auto& g : *c.groups) {
            for (const auto& item : g.items) {
                auto it = labels.find(item.id);
                if (it == labels.end()) continue;
                INFO("label drift on " << item.id);
                CHECK(std::string(item.zh) == it->second.first);
                CHECK(std::string(item.en) == it->second.second);
            }
        }
    }
}

TEST_CASE("U1 Catalog metadata counts and items") {
    const auto& g_scales = atm::geo_scales();

    std::set<std::string> patterns = flatten(atm::pattern_groups());
    std::set<std::string> textures = flatten(atm::texture_groups());
    std::set<std::string> ribbons = flatten(atm::ribbon_groups());

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
