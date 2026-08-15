// Colour-override length invariants.
//
// Each override array's length is pinned to a count another field owns, and
// sanitizeRecipe drops any array whose length does not match. Changing one of
// those counts without resizing therefore loses the user's hand-tuned colours
// the next time the recipe round-trips through JSON — silently, on save/load.
// That is what these cover: not that the arrays exist, but that moving a count
// keeps them valid, and that undo puts the originals back.
#include <doctest/doctest.h>
#include "model/recipe.h"
#include "handler/library_handler.h"
#include "command/library_command.h"
#include "command/library_command_handler.h"

using namespace atm;

namespace {

/** True when every present override array matches the count that governs it. */
bool overrides_consistent(const Recipe& r) {
    if (r.customShadesHex && r.customShadesHex->size() != static_cast<size_t>(r.bandSteps) + 2) return false;
    if (r.customRibbonHex && r.customRibbonHex->size() != static_cast<size_t>(r.ribbonShades) + 1) return false;
    if (r.customTexHexA && r.customTexHexA->size() != static_cast<size_t>(r.textureShadesA) + 1) return false;
    if (r.customTexHexB && r.customTexHexB->size() != static_cast<size_t>(r.textureShadesB) + 1) return false;
    return true;
}

/** Which override arrays survive a save/load cycle. */
Recipe round_trip(const Recipe& r) {
    return sanitize_recipe(recipe_to_json(r));
}

} // namespace

TEST_CASE("Changing bandSteps keeps customShadesHex valid through save/load") {
    LibraryHandler handler;
    LibraryCommandHandler cmd;
    const std::string hash = handler.selected_recipe()->hash;

    const int steps0 = handler.selected_recipe()->recipe.bandSteps;
    RoleHex roles = handler.selected_recipe()->recipe.roleHex;
    std::vector<std::string> shades(static_cast<size_t>(steps0) + 2, "#112233");
    REQUIRE(cmd.add_and_execute_command(
        std::make_unique<UpdateRecipeColoursCommand>(hash, roles, shades, 2), handler).success);
    REQUIRE(handler.selected_recipe()->recipe.customShadesHex.has_value());

    for (int target : { 5, 3, 4 }) {
        REQUIRE(cmd.add_and_execute_command(
            std::make_unique<UpdateRecipeBandCommand>(hash, target, false, false, 0.0, 2), handler).success);

        const Recipe& r = handler.selected_recipe()->recipe;
        INFO("bandSteps -> " << target);
        CHECK(r.bandSteps == target);
        REQUIRE(r.customShadesHex.has_value());
        CHECK(r.customShadesHex->size() == static_cast<size_t>(target) + 2);
        CHECK(overrides_consistent(r));

        // The array must still be there after a save/load, which is where a
        // length mismatch actually costs the user their colours.
        CHECK(round_trip(r).customShadesHex.has_value());

        // All-or-nothing: a single blank entry would void the whole array.
        for (const auto& hex : *r.customShadesHex) {
            CHECK_FALSE(hex.empty());
        }
    }
}

TEST_CASE("Changing ribbonShades and textureShades keeps their overrides valid") {
    LibraryHandler handler;
    LibraryCommandHandler cmd;
    const std::string hash = handler.selected_recipe()->hash;

    SUBCASE("ribbon") {
        const Recipe& r0 = handler.selected_recipe()->recipe;
        std::vector<std::optional<std::string>> rib(static_cast<size_t>(r0.ribbonShades) + 1, std::string("#aabbcc"));
        REQUIRE(cmd.add_and_execute_command(std::make_unique<UpdateRecipeRibbonCommand>(
            hash, "bevel", 0.5, 4, r0.ribbonShades, false, rib, 2), handler).success);

        for (int target : { 3, 1, 2 }) {
            // Deliberately hand over the *previous* array, as the panel does.
            auto stale = handler.selected_recipe()->recipe.customRibbonHex;
            REQUIRE(cmd.add_and_execute_command(std::make_unique<UpdateRecipeRibbonCommand>(
                hash, "bevel", 0.5, 4, target, false, stale, 2), handler).success);

            const Recipe& r = handler.selected_recipe()->recipe;
            INFO("ribbonShades -> " << target);
            REQUIRE(r.customRibbonHex.has_value());
            CHECK(r.customRibbonHex->size() == static_cast<size_t>(target) + 1);
            CHECK(overrides_consistent(r));
            CHECK(round_trip(r).customRibbonHex.has_value());
        }
    }

    SUBCASE("texture") {
        Recipe nr = handler.selected_recipe()->recipe;
        nr.customTexHexA = std::vector<std::optional<std::string>>(
            static_cast<size_t>(nr.textureShadesA) + 1, std::string("#445566"));
        nr.customTexHexB = std::vector<std::optional<std::string>>(
            static_cast<size_t>(nr.textureShadesB) + 1, std::string("#778899"));
        REQUIRE(cmd.add_and_execute_command(
            std::make_unique<UpdateRecipeTextureCommand>(hash, nr, 2), handler).success);

        for (int target : { 4, 1, 3 }) {
            Recipe next = handler.selected_recipe()->recipe;  // carries the stale arrays
            next.textureShadesA = target;
            next.textureShadesB = target;
            REQUIRE(cmd.add_and_execute_command(
                std::make_unique<UpdateRecipeTextureCommand>(hash, next, 2), handler).success);

            const Recipe& r = handler.selected_recipe()->recipe;
            INFO("textureShades -> " << target);
            REQUIRE(r.customTexHexA.has_value());
            REQUIRE(r.customTexHexB.has_value());
            CHECK(r.customTexHexA->size() == static_cast<size_t>(target) + 1);
            CHECK(r.customTexHexB->size() == static_cast<size_t>(target) + 1);
            CHECK(overrides_consistent(r));

            Recipe back = round_trip(r);
            CHECK(back.customTexHexA.has_value());
            CHECK(back.customTexHexB.has_value());
        }
    }
}

TEST_CASE("Undo restores the user's colours rather than the grown defaults") {
    LibraryHandler handler;
    LibraryCommandHandler cmd;
    const std::string hash = handler.selected_recipe()->hash;

    const int steps0 = handler.selected_recipe()->recipe.bandSteps;
    RoleHex roles = handler.selected_recipe()->recipe.roleHex;
    std::vector<std::string> shades(static_cast<size_t>(steps0) + 2, "#0f0f0f");
    REQUIRE(cmd.add_and_execute_command(
        std::make_unique<UpdateRecipeColoursCommand>(hash, roles, shades, 2), handler).success);

    const auto before = handler.selected_recipe()->recipe.customShadesHex;
    REQUIRE(before.has_value());

    // Grow, then shrink back. Re-syncing on undo instead of restoring would
    // leave the trimmed levels holding computed colours, not "#0f0f0f".
    REQUIRE(cmd.add_and_execute_command(
        std::make_unique<UpdateRecipeBandCommand>(hash, 5, false, false, 0.0, 2), handler).success);
    REQUIRE(cmd.undo(handler).success);

    const Recipe& r = handler.selected_recipe()->recipe;
    CHECK(r.bandSteps == steps0);
    REQUIRE(r.customShadesHex.has_value());
    CHECK(*r.customShadesHex == *before);
    for (const auto& hex : *r.customShadesHex) {
        CHECK(hex == "#0f0f0f");
    }
}

TEST_CASE("sync helpers are idempotent and ignore absent arrays") {
    Recipe r = get_default_recipe();
    REQUIRE_FALSE(r.customShadesHex.has_value());

    sync_all_overrides(r);   // must not conjure arrays out of nothing
    CHECK_FALSE(r.customShadesHex.has_value());
    CHECK_FALSE(r.customRibbonHex.has_value());
    CHECK_FALSE(r.customTexHexA.has_value());
    CHECK_FALSE(r.customTexHexB.has_value());

    r.customShadesHex = std::vector<std::string>(static_cast<size_t>(r.bandSteps) + 2, "#010203");
    const auto snapshot = r.customShadesHex;
    sync_all_overrides(r);
    sync_all_overrides(r);
    CHECK(r.customShadesHex == snapshot);
}
