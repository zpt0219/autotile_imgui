#include <doctest/doctest.h>
#include "model/recipe_library.h"
#include "handler/library_handler.h"
#include "command/library_command.h"
#include "command/library_command_handler.h"
#include <random>

using namespace atm;

namespace {

/**
 * Every present override array matches the count that governs it.
 *
 * Checked after each command rather than only at the end: a mismatch is not an
 * error until the recipe next round-trips through JSON, at which point
 * sanitizeRecipe drops the array. Waiting until the end would let a later
 * command paper over the moment it went wrong.
 */
void require_overrides_consistent(const RecipeLibrary& lib, int step) {
    for (const auto& e : lib.entries()) {
        const Recipe& r = e->recipe;
        INFO("step " << step << ", recipe " << e->hash);
        if (r.customShadesHex) {
            REQUIRE(r.customShadesHex->size() == static_cast<size_t>(r.bandSteps) + 2);
        }
        if (r.customRibbonHex) {
            REQUIRE(r.customRibbonHex->size() == static_cast<size_t>(r.ribbonShades) + 1);
        }
        if (r.customTexHexA) {
            REQUIRE(r.customTexHexA->size() == static_cast<size_t>(r.textureShadesA) + 1);
        }
        if (r.customTexHexB) {
            REQUIRE(r.customTexHexB->size() == static_cast<size_t>(r.textureShadesB) + 1);
        }
        // The invariant that actually matters: nothing is lost on save/load.
        Recipe back = sanitize_recipe(recipe_to_json(r));
        REQUIRE(back.customShadesHex.has_value() == r.customShadesHex.has_value());
        REQUIRE(back.customRibbonHex.has_value() == r.customRibbonHex.has_value());
        REQUIRE(back.customTexHexA.has_value() == r.customTexHexA.has_value());
        REQUIRE(back.customTexHexB.has_value() == r.customTexHexB.has_value());
    }
}

} // namespace

TEST_CASE("Command Stack Monkey Fuzz Testing with Overrides and Resizing") {
    LibraryHandler handler;
    LibraryCommandHandler cmd_handler(300);

    std::mt19937 rng(1337);
    auto rand_int = [&](int lo, int hi) {
        return std::uniform_int_distribution<int>(lo, hi)(rng);
    };

    // Record initial state
    nlohmann::json initial_json = handler.library()->to_json();

    // Perform 200 random commands covering all edit operations
    for (int i = 0; i < 200; ++i) {
        int action = rand_int(0, 8);
        if (action == 0 || handler.library()->entries().empty()) {
            Recipe r = get_default_recipe();
            r.edgeSeed = rand_int(0, 50000);
            cmd_handler.add_and_execute_command(
                std::make_unique<AddRecipeCommand>(r, "Fuzz Recipe " + std::to_string(i)),
                handler
            );
        } else if (action == 1 && handler.library()->entries().size() > 1) {
            int idx = rand_int(0, static_cast<int>(handler.library()->entries().size()) - 1);
            std::string hash = handler.library()->entries()[idx]->hash;
            cmd_handler.add_and_execute_command(
                std::make_unique<RemoveRecipeCommand>(hash),
                handler
            );
        } else if (action == 2) {
            int idx = rand_int(0, static_cast<int>(handler.library()->entries().size()) - 1);
            std::string hash = handler.library()->entries()[idx]->hash;
            cmd_handler.add_and_execute_command(
                std::make_unique<DuplicateRecipeCommand>(hash),
                handler
            );
        } else if (action == 3) {
            int idx = rand_int(0, static_cast<int>(handler.library()->entries().size()) - 1);
            std::string hash = handler.library()->entries()[idx]->hash;
            cmd_handler.add_and_execute_command(
                std::make_unique<RenameRecipeCommand>(hash, "Renamed " + std::to_string(i), EditPhase::Begin),
                handler
            );
        } else if (action == 4) {
            int idx = rand_int(0, static_cast<int>(handler.library()->entries().size()) - 1);
            std::string hash = handler.library()->entries()[idx]->hash;
            RoleHex roles;
            roles.terrainA = "#" + std::to_string(rand_int(100000, 999999));
            roles.terrainB = "#" + std::to_string(rand_int(100000, 999999));
            roles.edge = "#" + std::to_string(rand_int(100000, 999999));
            std::optional<std::vector<std::string>> shades = std::nullopt;
            if (rand_int(0, 1) == 1) {
                int steps = handler.library()->entries()[idx]->recipe.bandSteps;
                shades = std::vector<std::string>(steps + 2, "#112233");
            }
            cmd_handler.add_and_execute_command(
                std::make_unique<UpdateRecipeColoursCommand>(hash, roles, shades, EditPhase::Begin),
                handler
            );
        } else if (action == 5) {
            int idx = rand_int(0, static_cast<int>(handler.library()->entries().size()) - 1);
            std::string hash = handler.library()->entries()[idx]->hash;
            int new_steps = rand_int(3, 5);
            cmd_handler.add_and_execute_command(
                std::make_unique<UpdateRecipeBandCommand>(hash, new_steps, rand_int(0, 1) == 1, rand_int(0, 1) == 1, 0.0, EditPhase::Begin),
                handler
            );
        } else if (action == 6) {
            int idx = rand_int(0, static_cast<int>(handler.library()->entries().size()) - 1);
            std::string hash = handler.library()->entries()[idx]->hash;
            int new_shades = rand_int(1, 3);
            std::optional<std::vector<std::optional<std::string>>> rib_hex = std::nullopt;
            switch (rand_int(0, 2)) {
                case 0: break;  // no override
                case 1:  // correctly sized for the new count
                    rib_hex = std::vector<std::optional<std::string>>(new_shades + 1, "#aabbcc");
                    break;
                default:
                    // Stale: sized for the count *before* this edit, which is
                    // exactly what the panel hands over when only the shade
                    // slider moves. The command has to fix it up.
                    rib_hex = handler.library()->entries()[idx]->recipe.customRibbonHex;
                    if (!rib_hex) {
                        rib_hex = std::vector<std::optional<std::string>>(
                            handler.library()->entries()[idx]->recipe.ribbonShades + 1, "#aabbcc");
                    }
                    break;
            }
            cmd_handler.add_and_execute_command(
                std::make_unique<UpdateRecipeRibbonCommand>(hash, "bevel", 0.5, 4, new_shades, false, rib_hex, EditPhase::Begin),
                handler
            );
        } else if (action == 7) {
            int idx = rand_int(0, static_cast<int>(handler.library()->entries().size()) - 1);
            std::string hash = handler.library()->entries()[idx]->hash;
            // Carries the previous arrays forward untouched, as the panel does
            // when only the shade slider moves — so half these edits arrive
            // with arrays sized for the old count.
            Recipe nr = handler.library()->entries()[idx]->recipe;
            nr.textureShadesA = rand_int(1, 4);
            nr.textureShadesB = rand_int(1, 4);
            if (rand_int(0, 1) == 1) {
                nr.customTexHexA = std::vector<std::optional<std::string>>(nr.textureShadesA + 1, "#445566");
            }
            if (rand_int(0, 1) == 1) {
                nr.customTexHexB = std::vector<std::optional<std::string>>(nr.textureShadesB + 1, "#778899");
            }
            cmd_handler.add_and_execute_command(
                std::make_unique<UpdateRecipeTextureCommand>(hash, nr, EditPhase::Begin),
                handler
            );
        } else {
            int idx = rand_int(0, static_cast<int>(handler.library()->entries().size()) - 1);
            std::string hash = handler.library()->entries()[idx]->hash;
            cmd_handler.add_and_execute_command(
                std::make_unique<UpdateRecipePatternCommand>(hash, "wave", rand_int(0, 1000), rand_int(1, 4), EditPhase::Begin),
                handler
            );
        }

        require_overrides_consistent(*handler.library(), i);
    }

    // Save executed state
    nlohmann::json all_executed_json = handler.library()->to_json();

    // Undo all
    int undo_step = 0;
    while (cmd_handler.can_undo()) {
        auto res = cmd_handler.undo(handler);
        REQUIRE(res.success);
        require_overrides_consistent(*handler.library(), --undo_step);
    }

    nlohmann::json fully_undone_json = handler.library()->to_json();
    CHECK(fully_undone_json.dump() == initial_json.dump());

    // Redo all
    int redo_step = 1000;
    while (cmd_handler.can_redo()) {
        auto res = cmd_handler.redo(handler);
        REQUIRE(res.success);
        require_overrides_consistent(*handler.library(), ++redo_step);
    }

    nlohmann::json fully_redone_json = handler.library()->to_json();
    CHECK(fully_redone_json.dump() == all_executed_json.dump());
}
