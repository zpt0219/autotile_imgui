#include <doctest/doctest.h>
#include "codec/zip_import.h"
#include "pattern/sheet.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "miniz/miniz.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
#include <fstream>
#include <vector>

TEST_CASE("ZIP import extracts recipes and parses JSON sidecars") {
    // Create an in-memory ZIP archive using miniz
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    REQUIRE(mz_zip_writer_init_heap(&zip, 0, 1024));

    // Add a sidecar JSON
    const char* sample_json = R"({
        "name": "Grass Variant",
        "hash": "test_hash_123",
        "recipe": {
            "roleHex": { "terrainA": "#448822", "terrainB": "#113300", "edge": "#225511" },
            "patternId": "square",
            "bandSteps": 4,
            "outlineWidth": 2,
            "bandBias": 0.0,
            "hardEdgeB": false,
            "transparentB": false,
            "edgeSeed": 42,
            "patternNoise": ["white"],
            "patternNoiseStrength": 0.5,
            "patternNoiseSeed": 1234,
            "ribbonAlgo": "none",
            "ribbonAmount": 0.5,
            "ribbonPeriod": 4,
            "ribbonShades": 2,
            "ribbonInvert": false,
            "textureAlgoA": "none",
            "textureAmountA": 0.0,
            "textureShadesA": 2,
            "textureSeedA": 0,
            "textureAlgoB": "none",
            "textureAmountB": 0.0,
            "textureShadesB": 2,
            "textureSeedB": 0
        }
    })";

    REQUIRE(mz_zip_writer_add_mem(&zip, "sheet_0.recipe.json", sample_json, std::strlen(sample_json), MZ_BEST_COMPRESSION));
    // Add a dummy PNG alongside
    uint8_t dummy_png[] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    REQUIRE(mz_zip_writer_add_mem(&zip, "sheet_0.png", dummy_png, sizeof(dummy_png), MZ_BEST_COMPRESSION));

    void* zip_buf = nullptr;
    size_t zip_sz = 0;
    REQUIRE(mz_zip_writer_finalize_heap_archive(&zip, &zip_buf, &zip_sz));
    mz_zip_writer_end(&zip);

    SUBCASE("Memory import") {
        auto result = atm::import_recipes_from_zip_memory(reinterpret_cast<const uint8_t*>(zip_buf), zip_sz);
        CHECK(result.success);
        CHECK(result.entries.size() == 1);
        if (!result.entries.empty()) {
            CHECK(result.entries[0].name == "Grass Variant");
            CHECK(result.entries[0].recipe.patternId == "square");
            CHECK(result.entries[0].recipe.edgeSeed == 42);
        }
    }

    SUBCASE("File import") {
        std::string temp_zip_path = "tests_temp_import.zip";
        {
            std::ofstream f(temp_zip_path, std::ios::binary);
            f.write(reinterpret_cast<const char*>(zip_buf), zip_sz);
        }

        auto result = atm::import_recipes_from_zip(temp_zip_path);
        CHECK(result.success);
        CHECK(result.entries.size() == 1);
        if (!result.entries.empty()) {
            CHECK(result.entries[0].name == "Grass Variant");
            CHECK(result.entries[0].recipe.patternId == "square");
        }

        std::remove(temp_zip_path.c_str());
    }

    mz_free(zip_buf);
}
