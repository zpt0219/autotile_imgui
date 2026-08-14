#include <doctest/doctest.h>
#include "pattern/pattern_data.h"
#include "pattern/blob47.h"
#include "pattern/blob47_pattern.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstring>
#include <algorithm>

TEST_CASE("T2.1 Pattern Data integrity and decoded distance field boundaries") {
    const std::vector<std::string> patterns = {
        "square", "sharp", "rounded", "wave", "jagged", "gravel",
        "boulder", "thorn", "coast", "moss", "billow"
    };

    std::vector<float> decoded(1024);

    for (const auto& pat : patterns) {
        for (uint8_t mask : atm::BLOB47_MASKS) {
            const char* field = atm::pattern_data::get_field_string(pat, mask);
            INFO("Checking pattern " << pat << " mask " << (int)mask);
            REQUIRE(field != nullptr);
            size_t len = std::strlen(field);
            CHECK(len == 1024);

            // Decode field string into distance values
            for (size_t i = 0; i < 1024; ++i) {
                decoded[i] = atm::pattern_data::char_to_value(field[i]) * atm::pattern_data::FIELD_STEP;
            }

            float min_val = *std::min_element(decoded.begin(), decoded.end());
            float max_val = *std::max_element(decoded.begin(), decoded.end());

            // 90 field digits at 0.25px step cover 0..22.25px distance field
            CHECK(min_val >= 0.0f);
            CHECK(max_val <= 22.5f);
            CHECK(max_val >= min_val);
        }
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
