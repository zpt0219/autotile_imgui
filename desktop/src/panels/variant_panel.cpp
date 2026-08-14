#include "variant_panel.h"
#include "view_model/view_model.h"
#include "command/library_command.h"
#include <imgui.h>

namespace atm_desktop {

static const std::vector<std::string> ALL_PATTERNS = {
    "square", "sharp", "rounded", "wave", "jagged", "gravel",
    "boulder", "thorn", "coast", "moss", "billow"
};

static const std::vector<std::string> POPULAR_TEXTURES = {
    "none", "ordered", "ripple", "cells", "brick_wall", "cobbles2",
    "brick_floor", "hexagon", "isometric", "weave", "water"
};

static const std::vector<std::string> POPULAR_RIBBONS = {
    "none", "bevel", "dashes", "ticks", "beads", "rope", "wave", "grain"
};

void VariantPanel::draw(ViewModel& vm) {
    if (!open_) return;

    if (ImGui::Begin("Variant Matrix", &open_)) {
        auto* selected = vm.handler().selected_recipe();
        if (!selected) {
            ImGui::TextDisabled("Select a base recipe from the library to generate variants.");
            ImGui::End();
            return;
        }

        ImGui::Text("Base Recipe: %s (%s)", selected->name.c_str(), selected->recipe.patternId.c_str());
        ImGui::Separator();

        ImGui::Text("Select Axes to Cross-Multiply:");
        ImGui::Checkbox("Patterns (11 built-in boundary styles)", &use_all_patterns_);
        ImGui::Checkbox("Popular Textures (11 surface textures)", &use_all_textures_);
        ImGui::Checkbox("Ribbon Motifs (8 boundary motifs)", &use_all_ribbons_);

        int total_variants = 1;
        if (use_all_patterns_) total_variants *= static_cast<int>(ALL_PATTERNS.size());
        if (use_all_textures_) total_variants *= static_cast<int>(POPULAR_TEXTURES.size());
        if (use_all_ribbons_) total_variants *= static_cast<int>(POPULAR_RIBBONS.size());

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Total Combinations: %d sheets", total_variants);
        ImGui::Spacing();

        if (total_variants > 1 && ImGui::Button("Generate & Add All to Library", ImVec2(240, 32))) {
            std::vector<std::string> pats = use_all_patterns_ ? ALL_PATTERNS : std::vector<std::string>{ selected->recipe.patternId };
            std::vector<std::string> texs = use_all_textures_ ? POPULAR_TEXTURES : std::vector<std::string>{ selected->recipe.textureAlgoA };
            std::vector<std::string> ribs = use_all_ribbons_ ? POPULAR_RIBBONS : std::vector<std::string>{ selected->recipe.ribbonAlgo };

            for (const auto& p : pats) {
                for (const auto& t : texs) {
                    for (const auto& r_algo : ribs) {
                        atm::Recipe vr = selected->recipe;
                        vr.patternId = p;
                        vr.textureAlgoA = t;
                        vr.ribbonAlgo = r_algo;
                        std::string var_name = selected->name + " [" + p + "/" + t + "/" + r_algo + "]";
                        vm.execute_command(std::make_unique<atm::AddRecipeCommand>(vr, var_name));
                    }
                }
            }
            vm.log("Generated " + std::to_string(total_variants) + " variants into library", "INFO");
        }
    }
    ImGui::End();
}

} // namespace atm_desktop
