#include <doctest/doctest.h>
#include "pattern/pattern_data.h"
#include "pattern/blob47.h"
#include "pattern/blob47_pattern.h"
#include "pattern/catalog.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstring>
#include <algorithm>

static std::ifstream open_repo_file(const std::string& relative) {
    // The test binary is run from the build tree or the repo root depending on
    // whether it came from ctest or by hand.
    std::ifstream f(relative);
    if (!f.is_open()) f.open("../" + relative);
    if (!f.is_open()) f.open("../../" + relative);
    return f;
}

// T2.1's gate wants the decoded field's min/max checked against values read out
// of the TS, not merely against a plausible range. tests/data/field_bounds.json
// is parsed straight from reference/generated.ts by
// tests/data/dump_reference_vectors.js, so these expectations belong to the
// specification rather than to this port.
TEST_CASE("T2.1 Decoded field bounds match reference/generated.ts exactly") {
    std::ifstream file = open_repo_file("tests/data/field_bounds.json");
    REQUIRE(file.is_open());

    nlohmann::json bounds;
    file >> bounds;
    REQUIRE(bounds.is_array());
    // 11 patterns in GENERATED_FIELDS x 47 masks.
    REQUIRE(bounds.size() == 517);

    for (const auto& item : bounds) {
        const std::string pattern = item["pattern"].get<std::string>();
        const int mask = item["mask"].get<int>();
        INFO("pattern " << pattern << " mask " << mask);

        const char* field = atm::pattern_data::get_field_string(pattern, mask);
        REQUIRE(field != nullptr);

        const size_t len = std::strlen(field);
        CHECK(len == item["length"].get<size_t>());

        int min_idx = 255, max_idx = -1;
        for (size_t i = 0; i < len; ++i) {
            const int v = atm::pattern_data::char_to_value(field[i]);
            if (v < min_idx) min_idx = v;
            if (v > max_idx) max_idx = v;
        }

        CHECK(min_idx == item["minIndex"].get<int>());
        CHECK(max_idx == item["maxIndex"].get<int>());

        const float step = atm::pattern_data::FIELD_STEP;
        CHECK(min_idx * step == doctest::Approx(item["minDistance"].get<float>()));
        CHECK(max_idx * step == doctest::Approx(item["maxDistance"].get<float>()));
    }
}

// blob47Pattern.ts:207 is `wave: GENERATED_FIELDS.rounded` — 'wave' is a valid
// PatternId with no fields of its own, while 'bold' has fields and is not a
// valid PatternId. Both halves of that oddity are load-bearing; assert the
// aliasing rather than trusting it.
// The alias used to live inside get_field_string(); it now lives on the pattern
// registry as PatternDef::field_source, so pattern_data only holds baked data.
// Assert it at both ends: the registry says so, and the lookup callers actually
// use honours it.
TEST_CASE("T2.1 'wave' aliases the rounded field set, per blob47Pattern.ts") {
    CHECK(atm::pattern_field_source("wave") == "rounded");
    CHECK(atm::pattern_field_source("rounded") == "rounded");

    // An id with no registry row falls through to itself, which is what keeps
    // 'bold' — fields but not a valid PatternId — reachable.
    CHECK(atm::pattern_field_source("bold") == "bold");
    CHECK(atm::pattern_data::get_field_string("bold", 0) != nullptr);
    CHECK(atm::pattern_data::get_field_string("wave", 0) == nullptr);

    for (uint8_t mask : atm::BLOB47_MASKS) {
        const char* wave = atm::pattern_field_for_mask("wave", mask);
        const char* rounded = atm::pattern_field_for_mask("rounded", mask);
        INFO("mask " << (int)mask);
        REQUIRE(wave != nullptr);
        REQUIRE(rounded != nullptr);
        CHECK(std::strcmp(wave, rounded) == 0);
    }
}

TEST_CASE("T2.2 Blob47 layout matches corpus/manifest.json exactly and 256 masks round-trip") {
    std::ifstream manifest_file("corpus/manifest.json");
    if (!manifest_file.is_open()) manifest_file.open("../corpus/manifest.json");
    if (!manifest_file.is_open()) manifest_file.open("../../corpus/manifest.json");
    REQUIRE(manifest_file.is_open());

    nlohmann::json manifest_json;
    manifest_file >> manifest_json;

    REQUIRE(manifest_json.contains("sheet"));
    REQUIRE(manifest_json["sheet"].contains("layout"));
    const auto& manifest_layout = manifest_json["sheet"]["layout"];

    REQUIRE(manifest_layout.size() == 48);
    REQUIRE(atm::BLOB47_LAYOUT.size() == 48);

    // Assert element-by-element match against external corpus manifest.json
    for (size_t i = 0; i < 48; ++i) {
        uint8_t manifest_mask = static_cast<uint8_t>(manifest_layout[i].get<int>());
        CHECK(atm::BLOB47_LAYOUT[i] == manifest_mask);
    }

    // Check all 256 masks canonicalize and map to valid slots
    for (int m = 0; m < 256; ++m) {
        uint8_t raw_mask = static_cast<uint8_t>(m);
        uint8_t canon = atm::canonicalize_blob_mask(raw_mask);
        int slot = atm::blob_slot_for_mask(raw_mask);
        CHECK(slot >= 0);
        CHECK(slot < 48);
        CHECK(atm::BLOB47_LAYOUT[slot] == canon);

        int idx = atm::blob_index_for_mask(raw_mask);
        CHECK(idx >= 0);
        CHECK(idx < 47);
        CHECK(atm::BLOB47_MASKS[idx] == canon);
    }
}
