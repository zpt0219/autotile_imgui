#include <doctest/doctest.h>
#include "pattern/pattern_paint.h"
#include <nlohmann/json.hpp>
#include <fstream>

TEST_CASE("Palette colour model matches reference vectors exactly") {
    std::ifstream file("tests/data/palette_vectors.json");
    if (!file.is_open()) file.open("../tests/data/palette_vectors.json");
    if (!file.is_open()) file.open("../../tests/data/palette_vectors.json");
    REQUIRE(file.is_open());

    nlohmann::json j;
    file >> j;

    for (const auto& item : j) {
        atm::RGB c{
            static_cast<uint8_t>(item["c"]["r"].get<int>()),
            static_cast<uint8_t>(item["c"]["g"].get<int>()),
            static_cast<uint8_t>(item["c"]["b"].get<int>())
        };
        std::string role_str = item["role"].get<std::string>();
        atm::PatternRole role = atm::PatternRole::TerrainA;
        if (role_str == "terrainB") role = atm::PatternRole::TerrainB;
        else if (role_str == "edge") role = atm::PatternRole::Edge;

        double t = item["t"].get<double>();
        atm::RGB expected{
            static_cast<uint8_t>(item["res"]["r"].get<int>()),
            static_cast<uint8_t>(item["res"]["g"].get<int>()),
            static_cast<uint8_t>(item["res"]["b"].get<int>())
        };

        atm::RGB actual = atm::shade_colour(c, role, static_cast<float>(t));
        CHECK(actual.r == expected.r);
        CHECK(actual.g == expected.g);
        CHECK(actual.b == expected.b);
    }
}
