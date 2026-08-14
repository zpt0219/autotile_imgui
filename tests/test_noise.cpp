#include <doctest/doctest.h>
#include "pattern/pattern_noise.h"
#include <nlohmann/json.hpp>
#include <fstream>

TEST_CASE("Pattern grain noiseStep matches reference vectors exactly") {
    std::ifstream file("tests/data/noise_vectors.json");
    if (!file.is_open()) file.open("../tests/data/noise_vectors.json");
    if (!file.is_open()) file.open("../../tests/data/noise_vectors.json");
    REQUIRE(file.is_open());

    nlohmann::json j;
    file >> j;

    for (const auto& item : j) {
        int x = item["x"].get<int>();
        int y = item["y"].get<int>();
        int seed = item["seed"].get<int>();
        double strength = item["strength"].get<double>();
        int expected = item["res"].get<int>();

        std::vector<atm::NoiseId> noises;
        for (const auto& n_str : item["noises"]) {
            std::string s = n_str.get<std::string>();
            if (s == "white") noises.push_back(atm::NoiseId::White);
            else if (s == "blue") noises.push_back(atm::NoiseId::Blue);
            else if (s == "ordered") noises.push_back(atm::NoiseId::Ordered);
        }

        int actual = atm::noise_step(noises, x, y, seed, static_cast<float>(strength));
        CHECK(actual == expected);
    }
}
