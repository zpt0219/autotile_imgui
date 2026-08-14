#include <doctest/doctest.h>
#include "model/recipe_library.h"
#include "handler/library_handler.h"
#include "command/library_command.h"
#include "command/library_command_handler.h"
#include <random>

using namespace atm;

TEST_CASE("Command Stack Monkey Fuzz Testing") {
    LibraryHandler handler;
    LibraryCommandHandler cmd_handler(200);

    std::mt19937 rng(1337);
    auto rand_int = [&](int lo, int hi) {
        return std::uniform_int_distribution<int>(lo, hi)(rng);
    };

    // Record initial state
    nlohmann::json initial_json = handler.library()->to_json();

    // Perform 100 random commands
    for (int i = 0; i < 100; ++i) {
        int action = rand_int(0, 5);
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
                std::make_unique<RenameRecipeCommand>(hash, "Renamed " + std::to_string(i), 0),
                handler
            );
        } else if (action == 4) {
            int idx = rand_int(0, static_cast<int>(handler.library()->entries().size()) - 1);
            std::string hash = handler.library()->entries()[idx]->hash;
            RoleHex roles;
            roles.terrainA = "#" + std::to_string(rand_int(100000, 999999));
            roles.terrainB = "#" + std::to_string(rand_int(100000, 999999));
            roles.edge = "#" + std::to_string(rand_int(100000, 999999));
            cmd_handler.add_and_execute_command(
                std::make_unique<UpdateRecipeColoursCommand>(hash, roles, std::nullopt, 0),
                handler
            );
        } else {
            int idx = rand_int(0, static_cast<int>(handler.library()->entries().size()) - 1);
            std::string hash = handler.library()->entries()[idx]->hash;
            cmd_handler.add_and_execute_command(
                std::make_unique<UpdateRecipePatternCommand>(hash, "wave", rand_int(0, 1000), rand_int(1, 4), 0),
                handler
            );
        }
    }

    // Save executed state
    nlohmann::json all_executed_json = handler.library()->to_json();

    // Undo all
    while (cmd_handler.can_undo()) {
        auto res = cmd_handler.undo(handler);
        REQUIRE(res.success);
    }

    nlohmann::json fully_undone_json = handler.library()->to_json();
    CHECK(fully_undone_json.dump() == initial_json.dump());

    // Redo all
    while (cmd_handler.can_redo()) {
        auto res = cmd_handler.redo(handler);
        REQUIRE(res.success);
    }

    nlohmann::json fully_redone_json = handler.library()->to_json();
    CHECK(fully_redone_json.dump() == all_executed_json.dump());
}
