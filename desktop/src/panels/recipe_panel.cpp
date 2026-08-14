#include "recipe_panel.h"
#include "view_model/view_model.h"
#include "command/library_command.h"
#include "pattern/pattern_paint.h"
#include <imgui.h>
#include <random>

namespace atm_desktop {

static const char* PATTERN_NAMES[] = {
    "square", "sharp", "rounded", "wave", "jagged", "gravel",
    "boulder", "thorn", "coast", "moss", "billow"
};

static const char* RIBBON_NAMES[] = {
    "none", "bevel", "dashes", "ticks", "beads", "rope", "wave", "grain", "speckle",
    "along_brick_wall", "along_cobbles2", "along_weave", "along_stone_floor",
    "along_breeze_block", "along_octagonal"
};

static const char* TEXTURE_NAMES[] = {
    "none", "white", "blue", "ordered", "ripple", "ripple_diag", "cells",
    "breeze_block", "brick_wall", "cobbles2", "brick_floor", "hexagon",
    "isometric", "isometric_grid", "octagonal", "square", "weave",
    "paving", "paving3", "paving5", "stone_floor", "water", "brick_bond",
    "field", "rubble", "nonslip"
};

static int get_combo_index(const char* const items[], int count, const std::string& current) {
    for (int i = 0; i < count; ++i) {
        if (current == items[i]) return i;
    }
    return 0;
}

static int roll_seed() {
    static std::mt19937 rng(12345);
    return std::uniform_int_distribution<int>(1, 99999)(rng);
}

void RecipePanel::draw(ViewModel& vm) {
    if (!open_) return;

    if (ImGui::Begin("Recipe Inspector", &open_)) {
        auto* selected = vm.handler().selected_recipe();
        if (!selected) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No recipe selected.");
            ImGui::End();
            return;
        }

        const std::string& hash = selected->hash;
        const auto& r = selected->recipe;

        // ---------------- 1. Role Colours ----------------
        if (ImGui::CollapsingHeader("Palette & Roles", ImGuiTreeNodeFlags_DefaultOpen)) {
            atm::RGB colA = atm::parse_hex_colour(r.roleHex.terrainA);
            atm::RGB colB = atm::parse_hex_colour(r.roleHex.terrainB);
            atm::RGB colE = atm::parse_hex_colour(r.roleHex.edge);

            float fA[3] = { colA.r / 255.0f, colA.g / 255.0f, colA.b / 255.0f };
            float fB[3] = { colB.r / 255.0f, colB.g / 255.0f, colB.b / 255.0f };
            float fE[3] = { colE.r / 255.0f, colE.g / 255.0f, colE.b / 255.0f };

            bool changed = false;
            int flag = 1;

            if (ImGui::ColorEdit3("Terrain A", fA, ImGuiColorEditFlags_NoInputs)) {
                changed = true;
            }
            if (ImGui::IsItemActivated()) flag = 0;
            if (ImGui::IsItemDeactivatedAfterEdit()) flag = 2;

            if (ImGui::ColorEdit3("Terrain B", fB, ImGuiColorEditFlags_NoInputs)) {
                changed = true;
            }
            if (ImGui::IsItemActivated()) flag = 0;
            if (ImGui::IsItemDeactivatedAfterEdit()) flag = 2;

            if (ImGui::ColorEdit3("Edge", fE, ImGuiColorEditFlags_NoInputs)) {
                changed = true;
            }
            if (ImGui::IsItemActivated()) flag = 0;
            if (ImGui::IsItemDeactivatedAfterEdit()) flag = 2;

            if (changed) {
                atm::RoleHex new_roles;
                new_roles.terrainA = atm::to_hex_colour({ static_cast<uint8_t>(fA[0]*255), static_cast<uint8_t>(fA[1]*255), static_cast<uint8_t>(fA[2]*255) });
                new_roles.terrainB = atm::to_hex_colour({ static_cast<uint8_t>(fB[0]*255), static_cast<uint8_t>(fB[1]*255), static_cast<uint8_t>(fB[2]*255) });
                new_roles.edge = atm::to_hex_colour({ static_cast<uint8_t>(fE[0]*255), static_cast<uint8_t>(fE[1]*255), static_cast<uint8_t>(fE[2]*255) });
                vm.execute_command(std::make_unique<atm::UpdateRecipeColoursCommand>(hash, new_roles, r.customShadesHex, flag));
            }
        }

        // ---------------- 2. Silhouette & Pattern ----------------
        if (ImGui::CollapsingHeader("Silhouette & Boundary", ImGuiTreeNodeFlags_DefaultOpen)) {
            int pat_idx = get_combo_index(PATTERN_NAMES, IM_ARRAYSIZE(PATTERN_NAMES), r.patternId);
            if (ImGui::Combo("Pattern", &pat_idx, PATTERN_NAMES, IM_ARRAYSIZE(PATTERN_NAMES))) {
                vm.execute_command(std::make_unique<atm::UpdateRecipePatternCommand>(
                    hash, PATTERN_NAMES[pat_idx], r.edgeSeed, r.outlineWidth, 2
                ));
            }

            int seed = r.edgeSeed;
            if (ImGui::InputInt("Edge Seed", &seed)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipePatternCommand>(
                    hash, r.patternId, seed, r.outlineWidth, 2
                ));
            }
            ImGui::SameLine();
            if (ImGui::Button("Dice##Seed")) {
                vm.execute_command(std::make_unique<atm::UpdateRecipePatternCommand>(
                    hash, r.patternId, roll_seed(), r.outlineWidth, 2
                ));
            }

            int outline = r.outlineWidth;
            if (ImGui::SliderInt("Outline Width (px)", &outline, 1, 4)) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipePatternCommand>(
                    hash, r.patternId, r.edgeSeed, outline, flag
                ));
            }

            int steps = r.bandSteps;
            if (ImGui::SliderInt("Band Steps", &steps, 3, 5)) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(
                    hash, steps, r.hardEdgeB, r.transparentB, r.bandBias, flag
                ));
            }

            float bias = static_cast<float>(r.bandBias);
            if (ImGui::SliderFloat("Band Bias", &bias, -1.0f, 1.0f, "%.2f")) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(
                    hash, r.bandSteps, r.hardEdgeB, r.transparentB, static_cast<double>(bias), flag
                ));
            }

            bool hard_edge = r.hardEdgeB;
            if (ImGui::Checkbox("Hard Edge B", &hard_edge)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(
                    hash, r.bandSteps, hard_edge, r.transparentB, r.bandBias, 2
                ));
            }

            bool trans_b = r.transparentB;
            if (ImGui::Checkbox("Transparent Terrain B", &trans_b)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(
                    hash, r.bandSteps, r.hardEdgeB, trans_b, r.bandBias, 2
                ));
            }
        }

        // ---------------- 3. Grain Noise ----------------
        if (ImGui::CollapsingHeader("Grain Noise", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool has_white = false, has_blue = false, has_ordered = false;
            for (auto n : r.patternNoise) {
                if (n == atm::NoiseId::White) has_white = true;
                if (n == atm::NoiseId::Blue) has_blue = true;
                if (n == atm::NoiseId::Ordered) has_ordered = true;
            }

            bool noise_changed = false;
            if (ImGui::Checkbox("White Noise", &has_white)) noise_changed = true;
            ImGui::SameLine();
            if (ImGui::Checkbox("Blue Noise", &has_blue)) noise_changed = true;
            ImGui::SameLine();
            if (ImGui::Checkbox("Ordered Bayer", &has_ordered)) noise_changed = true;

            if (noise_changed) {
                std::vector<atm::NoiseId> new_noises;
                if (has_white) new_noises.push_back(atm::NoiseId::White);
                if (has_blue) new_noises.push_back(atm::NoiseId::Blue);
                if (has_ordered) new_noises.push_back(atm::NoiseId::Ordered);
                vm.execute_command(std::make_unique<atm::UpdateRecipeNoiseCommand>(
                    hash, new_noises, r.patternNoiseSeed, r.patternNoiseStrength, 2
                ));
            }

            float strength = static_cast<float>(r.patternNoiseStrength);
            if (ImGui::SliderFloat("Noise Strength", &strength, 0.0f, 2.0f, "%.2f")) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeNoiseCommand>(
                    hash, r.patternNoise, r.patternNoiseSeed, static_cast<double>(strength), flag
                ));
            }

            int nseed = r.patternNoiseSeed;
            if (ImGui::InputInt("Noise Seed", &nseed)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeNoiseCommand>(
                    hash, r.patternNoise, nseed, r.patternNoiseStrength, 2
                ));
            }
            ImGui::SameLine();
            if (ImGui::Button("Dice##NoiseSeed")) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeNoiseCommand>(
                    hash, r.patternNoise, roll_seed(), r.patternNoiseStrength, 2
                ));
            }
        }

        // ---------------- 4. Ribbon Motif ----------------
        if (ImGui::CollapsingHeader("Ribbon Motif", ImGuiTreeNodeFlags_DefaultOpen)) {
            int rib_idx = get_combo_index(RIBBON_NAMES, IM_ARRAYSIZE(RIBBON_NAMES), r.ribbonAlgo);
            if (ImGui::Combo("Motif Algo", &rib_idx, RIBBON_NAMES, IM_ARRAYSIZE(RIBBON_NAMES))) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, RIBBON_NAMES[rib_idx], r.ribbonAmount, r.ribbonPeriod, r.ribbonShades, r.ribbonInvert, r.customRibbonHex, 2
                ));
            }

            float amount = static_cast<float>(r.ribbonAmount);
            if (ImGui::SliderFloat("Duty / Amount", &amount, 0.0f, 1.0f, "%.2f")) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, static_cast<double>(amount), r.ribbonPeriod, r.ribbonShades, r.ribbonInvert, r.customRibbonHex, flag
                ));
            }

            int period = r.ribbonPeriod;
            if (ImGui::SliderInt("Period (px)", &period, 1, 8)) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, r.ribbonAmount, period, r.ribbonShades, r.ribbonInvert, r.customRibbonHex, flag
                ));
            }

            int shades = r.ribbonShades;
            if (ImGui::SliderInt("Shades Count", &shades, 1, 4)) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, r.ribbonAmount, r.ribbonPeriod, shades, r.ribbonInvert, r.customRibbonHex, flag
                ));
            }

            bool invert = r.ribbonInvert;
            if (ImGui::Checkbox("Invert Ribbon", &invert)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, r.ribbonAmount, r.ribbonPeriod, r.ribbonShades, invert, r.customRibbonHex, 2
                ));
            }
        }

        // ---------------- 5. Surface Textures ----------------
        if (ImGui::CollapsingHeader("Surface Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("Terrain A (Interior)");
            int texA_idx = get_combo_index(TEXTURE_NAMES, IM_ARRAYSIZE(TEXTURE_NAMES), r.textureAlgoA);
            if (ImGui::Combo("Texture A##Combo", &texA_idx, TEXTURE_NAMES, IM_ARRAYSIZE(TEXTURE_NAMES))) {
                atm::Recipe nr = r;
                nr.textureAlgoA = TEXTURE_NAMES[texA_idx];
                vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
            }

            float amountA = static_cast<float>(r.textureAmountA);
            if (ImGui::SliderFloat("Amount A", &amountA, 0.0f, 1.0f, "%.2f")) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                atm::Recipe nr = r;
                nr.textureAmountA = static_cast<double>(amountA);
                vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flag));
            }

            int seedA = r.textureSeedA;
            if (ImGui::InputInt("Seed A", &seedA)) {
                atm::Recipe nr = r;
                nr.textureSeedA = seedA;
                vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
            }
            ImGui::SameLine();
            if (ImGui::Button("Dice##SeedA")) {
                atm::Recipe nr = r;
                nr.textureSeedA = roll_seed();
                vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
            }

            ImGui::Separator();
            ImGui::TextDisabled("Terrain B (Exterior)");
            int texB_idx = get_combo_index(TEXTURE_NAMES, IM_ARRAYSIZE(TEXTURE_NAMES), r.textureAlgoB);
            if (ImGui::Combo("Texture B##Combo", &texB_idx, TEXTURE_NAMES, IM_ARRAYSIZE(TEXTURE_NAMES))) {
                atm::Recipe nr = r;
                nr.textureAlgoB = TEXTURE_NAMES[texB_idx];
                vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
            }

            float amountB = static_cast<float>(r.textureAmountB);
            if (ImGui::SliderFloat("Amount B", &amountB, 0.0f, 1.0f, "%.2f")) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                atm::Recipe nr = r;
                nr.textureAmountB = static_cast<double>(amountB);
                vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flag));
            }

            int seedB = r.textureSeedB;
            if (ImGui::InputInt("Seed B", &seedB)) {
                atm::Recipe nr = r;
                nr.textureSeedB = seedB;
                vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
            }
            ImGui::SameLine();
            if (ImGui::Button("Dice##SeedB")) {
                atm::Recipe nr = r;
                nr.textureSeedB = roll_seed();
                vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
            }
        }
    }
    ImGui::End();
}

} // namespace atm_desktop
