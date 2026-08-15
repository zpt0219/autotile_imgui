#include "variant_panel.h"
#include "view_model/view_model.h"
#include "command/library_command.h"
#include "pattern/catalog.h"
#include <imgui.h>
#include <vector>
#include <string>

namespace atm_desktop {

void VariantPanel::draw(ViewModel& vm) {
    if (!open_) return;

    if (ImGui::Begin("Variant Matrix", &open_)) {
        bool use_zh = vm.use_zh;

        auto* selected = vm.handler().selected_recipe();
        if (!selected) {
            ImGui::TextDisabled("%s", use_zh ? "从配方库中选择一个基础配方以生成变体矩阵。" : "Select a base recipe from the library to generate variants.");
            ImGui::End();
            return;
        }

        ImGui::Text("%s: %s (%s)", use_zh ? "基础配方" : "Base Recipe", selected->name.c_str(), selected->recipe.patternId.c_str());
        ImGui::Separator();

        ImGui::Text("%s", use_zh ? "选择参与笛卡尔积交叉生成的轴向:" : "Select Axes to Cross-Multiply:");
        ImGui::Checkbox(use_zh ? "所有轮廓边缘 (11 种图案)" : "All Patterns (11 boundary styles)", &use_all_patterns_);
        ImGui::Checkbox(use_zh ? "所有表面纹理 (26 种纹理)" : "All Textures (26 surface textures)", &use_all_textures_);
        ImGui::Checkbox(use_zh ? "所有边缘花纹 (15 种花纹)" : "All Ribbon Motifs (15 motifs)", &use_all_ribbons_);

        std::vector<std::string> all_pats;
        for (const auto& g : atm::pattern_groups()) {
            for (const auto& item : g.items) all_pats.push_back(item.id);
        }

        std::vector<std::string> all_texs;
        for (const auto& g : atm::texture_groups()) {
            for (const auto& item : g.items) all_texs.push_back(item.id);
        }

        std::vector<std::string> all_ribs;
        for (const auto& g : atm::ribbon_groups()) {
            for (const auto& item : g.items) all_ribs.push_back(item.id);
        }

        int total_variants = 1;
        if (use_all_patterns_) total_variants *= static_cast<int>(all_pats.size());
        if (use_all_textures_) total_variants *= static_cast<int>(all_texs.size());
        if (use_all_ribbons_) total_variants *= static_cast<int>(all_ribs.size());

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%s: %d %s",
            use_zh ? "总组合数" : "Total Combinations",
            total_variants,
            use_zh ? "套图纸" : "sheets"
        );
        ImGui::Spacing();

        if (total_variants > 1 && ImGui::Button(use_zh ? "生成并全部加入配方库" : "Generate & Add All to Library", ImVec2(240, 32))) {
            std::vector<std::string> pats = use_all_patterns_ ? all_pats : std::vector<std::string>{ selected->recipe.patternId };
            std::vector<std::string> texs = use_all_textures_ ? all_texs : std::vector<std::string>{ selected->recipe.textureAlgoA };
            std::vector<std::string> ribs = use_all_ribbons_ ? all_ribs : std::vector<std::string>{ selected->recipe.ribbonAlgo };

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
