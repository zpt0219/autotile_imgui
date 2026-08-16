// Scope note: every archive in this file is built here with miniz, the same
// library the importer reads with. That is enough to pin down how this reader
// behaves — which entries it picks up, what it does with broken input, that it
// runs everything through sanitizeRecipe — but it is NOT evidence that the
// format matches the web app's export. Do not read a pass
// here as format compatibility.
#include <doctest/doctest.h>
#include "codec/zip_import.h"

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
#include <cstring>
#include "model/recipe.h"

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

    // A non-JSON entry that the reader must skip rather than choke on. It is a
    // bare PNG signature, not a real sheet: the importer takes recipes from the
    // sidecars and never decodes the images, so there is nothing here to render
    // against and this deliberately does not pretend otherwise.
    uint8_t png_signature[] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    REQUIRE(mz_zip_writer_add_mem(&zip, "sheet_0.png", png_signature, sizeof(png_signature), MZ_BEST_COMPRESSION));

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

// The parse-failure path used to free the extracted buffer twice (once at the
// top of the try, once in the catch), so a single malformed sidecar corrupted
// the heap. Nothing exercised it, because every other case feeds valid JSON.
TEST_CASE("ZIP import survives malformed sidecars without corrupting the heap") {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    REQUIRE(mz_zip_writer_init_heap(&zip, 0, 1024));

    // Several broken sidecars, so a double free has many chances to be caught
    // by the allocator, plus one good one to prove the loop keeps going.
    const char* truncated = R"({"name": "Truncated", "recipe": {"patternId": "square")";
    const char* not_json = "this is not json at all";
    const char* empty = "";
    const char* good = R"({"name": "Good One", "recipe": { "patternId": "moss", "edgeSeed": 7 }})";

    REQUIRE(mz_zip_writer_add_mem(&zip, "a_truncated.recipe.json", truncated, std::strlen(truncated), MZ_BEST_COMPRESSION));
    REQUIRE(mz_zip_writer_add_mem(&zip, "b_garbage.recipe.json", not_json, std::strlen(not_json), MZ_BEST_COMPRESSION));
    REQUIRE(mz_zip_writer_add_mem(&zip, "c_empty.recipe.json", empty, 0, MZ_BEST_COMPRESSION));
    REQUIRE(mz_zip_writer_add_mem(&zip, "d_good.recipe.json", good, std::strlen(good), MZ_BEST_COMPRESSION));

    void* zip_buf = nullptr;
    size_t zip_sz = 0;
    REQUIRE(mz_zip_writer_finalize_heap_archive(&zip, &zip_buf, &zip_sz));
    mz_zip_writer_end(&zip);

    auto result = atm::import_recipes_from_zip_memory(reinterpret_cast<const uint8_t*>(zip_buf), zip_sz);

    // The three broken ones are reported and skipped; the good one still lands.
    CHECK(result.errors.size() == 3);
    REQUIRE(result.entries.size() == 1);
    CHECK(result.entries[0].name == "Good One");
    CHECK(result.entries[0].recipe.patternId == "moss");
    CHECK(result.entries[0].recipe.edgeSeed == 7);
    CHECK(result.success);

    mz_free(zip_buf);
}

// A sidecar whose recipe is nonsense must still yield a usable recipe: the
// import runs everything through sanitizeRecipe, which is part of the render.
TEST_CASE("ZIP import clamps out-of-range sidecar values through sanitizeRecipe") {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    REQUIRE(mz_zip_writer_init_heap(&zip, 0, 1024));

    const char* wild = R"({
        "name": "Out Of Range",
        "recipe": {
            "patternId": "not_a_pattern",
            "bandSteps": 99,
            "outlineWidth": -4,
            "edgeSeed": 999999999,
            "bandBias": 12.5
        }
    })";
    REQUIRE(mz_zip_writer_add_mem(&zip, "wild.recipe.json", wild, std::strlen(wild), MZ_BEST_COMPRESSION));

    void* zip_buf = nullptr;
    size_t zip_sz = 0;
    REQUIRE(mz_zip_writer_finalize_heap_archive(&zip, &zip_buf, &zip_sz));
    mz_zip_writer_end(&zip);

    auto result = atm::import_recipes_from_zip_memory(reinterpret_cast<const uint8_t*>(zip_buf), zip_sz);
    REQUIRE(result.entries.size() == 1);

    const auto& r = result.entries[0].recipe;
    atm::Recipe def = atm::get_default_recipe();
    CHECK(r.patternId == def.patternId);   // unknown id falls back, per recipe.ts
    CHECK(r.bandSteps >= 3);
    CHECK(r.bandSteps <= 5);
    CHECK(r.outlineWidth >= 1);
    CHECK(r.outlineWidth <= 4);
    CHECK(r.bandBias >= -1.0f);
    CHECK(r.bandBias <= 1.0f);

    mz_free(zip_buf);
}
