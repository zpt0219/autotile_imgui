#include <doctest/doctest.h>
#include "model/recipe_library.h"
#include "handler/library_handler.h"
#include "command/library_command.h"
#include "command/library_command_handler.h"

using namespace atm;

TEST_CASE("Recipe Library & Command Undo/Redo") {
    LibraryHandler handler;
    LibraryCommandHandler cmd_handler;

    REQUIRE(handler.library()->entries().size() == 1);
    std::string orig_hash = handler.selected_recipe()->hash;

    // 1. Rename command
    auto rename_cmd = std::make_unique<RenameRecipeCommand>(orig_hash, "My First Recipe", EditPhase::Begin);
    auto res = cmd_handler.add_and_execute_command(std::move(rename_cmd), handler);
    REQUIRE(res.success);
    CHECK(handler.selected_recipe()->name == "My First Recipe");

    // 2. Add recipe command
    Recipe r2 = get_default_recipe();
    r2.patternId = "sharp";
    auto add_cmd = std::make_unique<AddRecipeCommand>(r2, "Second Recipe");
    res = cmd_handler.add_and_execute_command(std::move(add_cmd), handler);
    REQUIRE(res.success);
    REQUIRE(handler.library()->entries().size() == 2);
    std::string second_hash = handler.selected_recipe()->hash;
    CHECK(handler.selected_recipe()->name == "Second Recipe");

    // 3. Update colours command
    RoleHex roles;
    roles.terrainA = "#112233";
    roles.terrainB = "#445566";
    roles.edge = "#778899";
    auto col_cmd = std::make_unique<UpdateRecipeColoursCommand>(second_hash, roles, std::nullopt, EditPhase::Begin);
    res = cmd_handler.add_and_execute_command(std::move(col_cmd), handler);
    REQUIRE(res.success);
    CHECK(handler.selected_recipe()->recipe.roleHex.terrainA == "#112233");

    // 4. Test Undo sequence
    REQUIRE(cmd_handler.can_undo());
    res = cmd_handler.undo(handler);
    REQUIRE(res.success);
    // Rolled back colour
    CHECK(handler.selected_recipe()->recipe.roleHex.terrainA != "#112233");

    res = cmd_handler.undo(handler);
    REQUIRE(res.success);
    // Rolled back add recipe
    REQUIRE(handler.library()->entries().size() == 1);
    CHECK(handler.selected_recipe()->hash == orig_hash);

    res = cmd_handler.undo(handler);
    REQUIRE(res.success);
    // Rolled back rename
    CHECK(handler.selected_recipe()->name == "Default Meadow");

    // 5. Test Redo sequence
    REQUIRE(cmd_handler.can_redo());
    res = cmd_handler.redo(handler);
    REQUIRE(res.success);
    CHECK(handler.selected_recipe()->name == "My First Recipe");

    res = cmd_handler.redo(handler);
    REQUIRE(res.success);
    REQUIRE(handler.library()->entries().size() == 2);

    res = cmd_handler.redo(handler);
    REQUIRE(res.success);
    CHECK(handler.selected_recipe()->recipe.roleHex.terrainA == "#112233");
}
