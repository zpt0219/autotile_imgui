#include "recipe_panel.h"
#include "view_model/view_model.h"
#include "command/library_command.h"
#include "pattern/pattern_paint.h"
#include "pattern/pattern_texture.h"
#include "pattern/pattern_ribbon.h"
#include "pattern/catalog.h"
#include "ui/widgets.h"
#include "ui/shade_strip.h"
#include <imgui.h>
#include <algorithm>

namespace atm_desktop {

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
        bool use_zh = vm.use_zh;

        // ---------------- 1. Palette & Roles ----------------
        if (ImGui::CollapsingHeader(use_zh ? "调色板与角色 (Palette & Roles)" : "Palette & Roles", ImGuiTreeNodeFlags_DefaultOpen)) {
            atm::RGB colA = atm::parse_hex_colour(r.roleHex.terrainA);
            atm::RGB colB = atm::parse_hex_colour(r.roleHex.terrainB);
            atm::RGB colE = atm::parse_hex_colour(r.roleHex.edge);

            float fA[3] = { colA.r / 255.0f, colA.g / 255.0f, colA.b / 255.0f };
            float fB[3] = { colB.r / 255.0f, colB.g / 255.0f, colB.b / 255.0f };
            float fE[3] = { colE.r / 255.0f, colE.g / 255.0f, colE.b / 255.0f };

            bool changed = false;
            int flag = 1;

            if (ImGui::ColorEdit3(use_zh ? "地形 A (Terrain A)" : "Terrain A", fA, ImGuiColorEditFlags_NoInputs)) {
                changed = true;
            }
            if (ImGui::IsItemActivated()) flag = 0;
            if (ImGui::IsItemDeactivatedAfterEdit()) flag = 2;

            if (ImGui::ColorEdit3(use_zh ? "地形 B (Terrain B)" : "Terrain B", fB, ImGuiColorEditFlags_NoInputs)) {
                changed = true;
            }
            if (ImGui::IsItemActivated()) flag = 0;
            if (ImGui::IsItemDeactivatedAfterEdit()) flag = 2;

            if (ImGui::ColorEdit3(use_zh ? "描边 (Edge)" : "Edge", fE, ImGuiColorEditFlags_NoInputs)) {
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

            ImGui::Separator();
            ImGui::TextDisabled("%s", use_zh ? "色带覆盖 (Band Overrides):" : "Band Shade Overrides:");
            atm::RoleColours roles{ colA, colB, colE };
            auto custom_shades_copy = r.customShadesHex;
            ui::draw_band_shade_strip("Band", r.bandSteps, roles, custom_shades_copy, [&](const auto& new_shades, int flg) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeColoursCommand>(hash, r.roleHex, new_shades, flg));
            });
        }

        // ---------------- 2. Silhouette & Boundary ----------------
        if (ImGui::CollapsingHeader(use_zh ? "轮廓与边界 (Silhouette & Boundary)" : "Silhouette & Boundary", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto new_pat = ui::grouped_combo(use_zh ? "轮廓形状 (Pattern)" : "Pattern", atm::pattern_groups(), r.patternId, use_zh);
            if (new_pat.has_value() && *new_pat != r.patternId) {
                vm.execute_command(std::make_unique<atm::UpdateRecipePatternCommand>(hash, *new_pat, r.edgeSeed, r.outlineWidth, 2));
            }

            int seed = r.edgeSeed;
            if (ui::drag_int_with_dice(use_zh ? "边缘种子 (Edge Seed)" : "Edge Seed", &seed, 0, 99999, "EdgeSeed")) {
                vm.execute_command(std::make_unique<atm::UpdateRecipePatternCommand>(hash, r.patternId, seed, r.outlineWidth, 2));
            }

            int outline = r.outlineWidth;
            if (ImGui::SliderInt(use_zh ? "描边宽度 (Outline Px)" : "Outline Width (px)", &outline, 1, 4)) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipePatternCommand>(hash, r.patternId, r.edgeSeed, outline, flag));
            }

            int steps = r.bandSteps;
            if (ImGui::SliderInt(use_zh ? "过渡阶数 (Band Steps)" : "Band Steps", &steps, 3, 5)) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(hash, steps, r.hardEdgeB, r.transparentB, r.bandBias, flag));
            }

            float bias = static_cast<float>(r.bandBias);
            if (ImGui::SliderFloat(use_zh ? "过渡偏置 (Band Bias)" : "Band Bias", &bias, -1.0f, 1.0f, "%.2f")) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(hash, r.bandSteps, r.hardEdgeB, r.transparentB, static_cast<double>(bias), flag));
            }

            bool hard_edge = r.hardEdgeB;
            if (ImGui::Checkbox(use_zh ? "B 侧硬边缘 (Hard Edge B)" : "Hard Edge B", &hard_edge)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(hash, r.bandSteps, hard_edge, r.transparentB, r.bandBias, 2));
            }

            bool trans_b = r.transparentB;
            if (ImGui::Checkbox(use_zh ? "B 侧透明 (Transparent B)" : "Transparent Terrain B", &trans_b)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(hash, r.bandSteps, r.hardEdgeB, trans_b, r.bandBias, 2));
            }
        }

        // ---------------- 3. Grain Noise ----------------
        if (ImGui::CollapsingHeader(use_zh ? "颗粒噪声 (Grain Noise)" : "Grain Noise", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool has_white = false, has_blue = false, has_ordered = false;
            for (auto n : r.patternNoise) {
                if (n == atm::NoiseId::White) has_white = true;
                if (n == atm::NoiseId::Blue) has_blue = true;
                if (n == atm::NoiseId::Ordered) has_ordered = true;
            }

            bool noise_changed = false;
            if (ImGui::Checkbox(use_zh ? "白噪声 (White)" : "White Noise", &has_white)) noise_changed = true;
            ImGui::SameLine();
            if (ImGui::Checkbox(use_zh ? "蓝噪声 (Blue)" : "Blue Noise", &has_blue)) noise_changed = true;
            ImGui::SameLine();
            if (ImGui::Checkbox(use_zh ? "拜耳半调 (Ordered)" : "Ordered Bayer", &has_ordered)) noise_changed = true;

            if (noise_changed) {
                std::vector<atm::NoiseId> new_noises;
                if (has_white) new_noises.push_back(atm::NoiseId::White);
                if (has_blue) new_noises.push_back(atm::NoiseId::Blue);
                if (has_ordered) new_noises.push_back(atm::NoiseId::Ordered);
                vm.execute_command(std::make_unique<atm::UpdateRecipeNoiseCommand>(hash, new_noises, r.patternNoiseSeed, r.patternNoiseStrength, 2));
            }

            float strength = static_cast<float>(r.patternNoiseStrength);
            if (ImGui::SliderFloat(use_zh ? "噪声强度 (Strength)" : "Noise Strength", &strength, 0.0f, 2.0f, "%.2f")) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeNoiseCommand>(hash, r.patternNoise, r.patternNoiseSeed, static_cast<double>(strength), flag));
            }

            int nseed = r.patternNoiseSeed;
            if (ui::drag_int_with_dice(use_zh ? "噪声种子 (Noise Seed)" : "Noise Seed", &nseed, 0, 99999, "NoiseSeed")) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeNoiseCommand>(hash, r.patternNoise, nseed, r.patternNoiseStrength, 2));
            }
        }

        // ---------------- 4. Ribbon Motif ----------------
        if (ImGui::CollapsingHeader(use_zh ? "花纹 (Ribbon Motif)" : "Ribbon Motif", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto new_rib = ui::grouped_combo(use_zh ? "花纹类型 (Motif)" : "Motif Algo", atm::ribbon_groups(), r.ribbonAlgo, use_zh);
            if (new_rib.has_value() && *new_rib != r.ribbonAlgo) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, *new_rib, r.ribbonAmount, r.ribbonPeriod, r.ribbonShades, r.ribbonInvert, r.customRibbonHex, 2
                ));
            }

            // Minimum width warning
            double min_w = atm::ribbon_min_width(r.ribbonAlgo);
            if (r.ribbonAlgo != "none" && r.outlineWidth < min_w) {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                    use_zh ? "⚠ 当前描边宽度 %dpx 小于本花纹所需最小宽度 %.0fpx" : "⚠ Current outline width %dpx < minimum required %.0fpx",
                    r.outlineWidth, min_w);
            }

            float amount = static_cast<float>(r.ribbonAmount);
            if (ImGui::SliderFloat(use_zh ? "花纹强度 (Amount)" : "Duty / Amount", &amount, 0.0f, 1.0f, "%.2f")) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, static_cast<double>(amount), r.ribbonPeriod, r.ribbonShades, r.ribbonInvert, r.customRibbonHex, flag
                ));
            }

            // Period (greyed-out if aperiodic)
            bool period_active = atm::ribbon_uses_period(r.ribbonAlgo);
            if (!period_active) ImGui::BeginDisabled();
            int period = r.ribbonPeriod;
            if (ImGui::SliderInt(use_zh ? "周期 (Period Px)" : "Period (px)", &period, 1, 16)) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, r.ribbonAmount, period, r.ribbonShades, r.ribbonInvert, r.customRibbonHex, flag
                ));
            }
            if (!period_active) {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("%s", use_zh ? "本算法不使用此参数" : "This algorithm does not use Period");
                }
            }

            int shades = r.ribbonShades;
            if (ImGui::SliderInt(use_zh ? "层次数 (Shades)" : "Shades Count", &shades, 1, 3)) {
                int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, r.ribbonAmount, r.ribbonPeriod, shades, r.ribbonInvert, r.customRibbonHex, flag
                ));
            }

            // Invert (greyed-out if unflippable)
            bool invert_active = atm::ribbon_uses_invert(r.ribbonAlgo);
            if (!invert_active) ImGui::BeginDisabled();
            bool invert = r.ribbonInvert;
            if (ImGui::Checkbox(use_zh ? "反转高光 (Invert)" : "Invert Ribbon", &invert)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, r.ribbonAmount, r.ribbonPeriod, r.ribbonShades, invert, r.customRibbonHex, 2
                ));
            }
            if (!invert_active) {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("%s", use_zh ? "本算法不使用此参数" : "This algorithm cannot be inverted");
                }
            }

            // Ribbon Color Overrides Strip
            if (r.ribbonAlgo != "none") {
                ImGui::Separator();
                ImGui::TextDisabled("%s", use_zh ? "花纹配色覆盖 (Ribbon Palette):" : "Ribbon Palette Overrides:");
                auto used_shades = atm::used_ribbon_shades(r.ribbonAlgo, r.outlineWidth, r.ribbonAmount, r.ribbonShades, r.ribbonPeriod, r.ribbonInvert);

                size_t num_shades = static_cast<size_t>(r.ribbonShades + 1);
                std::vector<atm::RGB> default_ribbon_colors(num_shades);
                atm::RGB edge_col = atm::parse_hex_colour(r.roleHex.edge);
                for (size_t i = 0; i < num_shades; ++i) {
                    default_ribbon_colors[i] = atm::shade_colour(edge_col, atm::PatternRole::Edge, static_cast<float>(i) / std::max(1, r.ribbonShades));
                }

                auto custom_ribbon_copy = r.customRibbonHex;
                ui::draw_sparse_shade_strip("RibbonStrip", num_shades, default_ribbon_colors, used_shades, custom_ribbon_copy, [&](const auto& new_hexes, int flg) {
                    vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                        hash, r.ribbonAlgo, r.ribbonAmount, r.ribbonPeriod, r.ribbonShades, r.ribbonInvert, new_hexes, flg
                    ));
                });
            }
        }

        // ---------------- 5. Surface Textures ----------------
        if (ImGui::CollapsingHeader(use_zh ? "表面纹理 (Surface Textures)" : "Surface Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTabBar("TerrainTexturesTabBar")) {
                // Tab A
                if (ImGui::BeginTabItem(use_zh ? "地形 A (内侧)" : "Terrain A (Interior)")) {
                    auto new_texA = ui::grouped_combo(use_zh ? "纹理类型 (Texture A)" : "Texture A##Combo", atm::texture_groups(), r.textureAlgoA, use_zh);
                    if (new_texA.has_value() && *new_texA != r.textureAlgoA) {
                        atm::Recipe nr = r;
                        nr.textureAlgoA = *new_texA;
                        nr.geoScaleA = atm::natural_geo_scale(*new_texA);
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
                    }

                    // Amount A
                    bool amountA_active = atm::texture_uses_amount(r.textureAlgoA);
                    if (!amountA_active) ImGui::BeginDisabled();
                    float amountA = static_cast<float>(r.textureAmountA);
                    if (ImGui::SliderFloat(use_zh ? "强度 (Amount A)" : "Amount A", &amountA, 0.0f, 1.0f, "%.2f")) {
                        int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                        atm::Recipe nr = r;
                        nr.textureAmountA = static_cast<double>(amountA);
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flag));
                    }
                    if (!amountA_active) {
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            ImGui::SetTooltip("%s", use_zh ? "本算法不使用此参数" : "This algorithm does not use Amount");
                        }
                    }

                    // Shades A
                    int shadesA = r.textureShadesA;
                    if (ImGui::SliderInt(use_zh ? "层次数 (Shades A)" : "Shades A", &shadesA, 1, 4)) {
                        int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                        atm::Recipe nr = r;
                        nr.textureShadesA = shadesA;
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flag));
                    }

                    // Seed A
                    int seedA = r.textureSeedA;
                    if (ui::drag_int_with_dice(use_zh ? "纹理种子 (Seed A)" : "Seed A", &seedA, 0, 99999, "SeedA")) {
                        atm::Recipe nr = r;
                        nr.textureSeedA = seedA;
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
                    }

                    // Cell Scale A (only cells)
                    bool cellsA_active = (r.textureAlgoA == "cells");
                    if (!cellsA_active) ImGui::BeginDisabled();
                    int cellScaleA = r.cellScaleA;
                    if (ImGui::SliderInt(use_zh ? "细胞尺度 (Cell Scale A)" : "Cell Scale A", &cellScaleA, 2, 6)) {
                        int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                        atm::Recipe nr = r;
                        nr.cellScaleA = cellScaleA;
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flag));
                    }
                    if (!cellsA_active) {
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            ImGui::SetTooltip("%s", use_zh ? "仅多边形细胞 (cells) 算法使用此参数" : "Only Polygonal Cells uses Cell Scale");
                        }
                    }

                    // Ripple Scale A (only ripple / ripple_diag)
                    bool rippleA_active = (r.textureAlgoA == "ripple" || r.textureAlgoA == "ripple_diag");
                    if (!rippleA_active) ImGui::BeginDisabled();
                    int rippleScaleA = r.rippleScaleA;
                    if (ImGui::SliderInt(use_zh ? "波纹尺度 (Ripple Scale A)" : "Ripple Scale A", &rippleScaleA, 2, 8)) {
                        int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                        atm::Recipe nr = r;
                        nr.rippleScaleA = rippleScaleA;
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flag));
                    }
                    if (!rippleA_active) {
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            ImGui::SetTooltip("%s", use_zh ? "仅水面波纹 (ripple) 算法使用此参数" : "Only Ripple algorithms use Ripple Scale");
                        }
                    }

                    // Geo Scale A (Size)
                    bool geoA_active = atm::texture_uses_geo_scale(r.textureAlgoA);
                    if (!geoA_active) ImGui::BeginDisabled();
                    auto scalesA = atm::geo_scales_for(r.textureAlgoA);
                    std::string geoA_label = "32px";
                    for (const auto& s : scalesA) {
                        if (s.id == r.geoScaleA) {
                            geoA_label = use_zh ? s.zh : s.en;
                            break;
                        }
                    }
                    if (ImGui::BeginCombo(use_zh ? "尺寸规格 (Size A)" : "Size A", geoA_label.c_str())) {
                        for (const auto& s : scalesA) {
                            bool is_sel = (s.id == r.geoScaleA);
                            if (ImGui::Selectable(use_zh ? s.zh : s.en, is_sel)) {
                                atm::Recipe nr = r;
                                nr.geoScaleA = s.id;
                                vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (!geoA_active) {
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            ImGui::SetTooltip("%s", use_zh ? "仅几何与铺砖纹理支持尺寸缩放" : "Only Geometric/Paving textures use Size");
                        }
                    }

                    // Texture A Overrides Strip
                    if (r.textureAlgoA != "none") {
                        ImGui::Separator();
                        ImGui::TextDisabled("%s", use_zh ? "纹理 A 配色覆盖 (Texture A Palette):" : "Texture A Palette Overrides:");
                        auto used_shadesA = atm::used_texture_shades(r.textureAlgoA, r.textureAmountA, r.textureShadesA, r.cellScaleA, r.rippleScaleA, r.geoScaleA, r.textureSeedA);
                        size_t num_shadesA = static_cast<size_t>(r.textureShadesA + 1);
                        atm::RGB baseA = atm::parse_hex_colour(r.roleHex.terrainA);
                        auto default_rampA = atm::texture_ramp(baseA, std::nullopt, r.textureShadesA);

                        auto custom_texA_copy = r.customTexHexA;
                        ui::draw_sparse_shade_strip("TexAStrip", num_shadesA, default_rampA, used_shadesA, custom_texA_copy, [&](const auto& new_hexes, int flg) {
                            atm::Recipe nr = r;
                            nr.customTexHexA = new_hexes;
                            vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flg));
                        });
                    }

                    ImGui::EndTabItem();
                }

                // Tab B
                if (ImGui::BeginTabItem(use_zh ? "地形 B (外侧)" : "Terrain B (Exterior)")) {
                    auto new_texB = ui::grouped_combo(use_zh ? "纹理类型 (Texture B)" : "Texture B##Combo", atm::texture_groups(), r.textureAlgoB, use_zh);
                    if (new_texB.has_value() && *new_texB != r.textureAlgoB) {
                        atm::Recipe nr = r;
                        nr.textureAlgoB = *new_texB;
                        nr.geoScaleB = atm::natural_geo_scale(*new_texB);
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
                    }

                    // Amount B
                    bool amountB_active = atm::texture_uses_amount(r.textureAlgoB);
                    if (!amountB_active) ImGui::BeginDisabled();
                    float amountB = static_cast<float>(r.textureAmountB);
                    if (ImGui::SliderFloat(use_zh ? "强度 (Amount B)" : "Amount B", &amountB, 0.0f, 1.0f, "%.2f")) {
                        int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                        atm::Recipe nr = r;
                        nr.textureAmountB = static_cast<double>(amountB);
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flag));
                    }
                    if (!amountB_active) {
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            ImGui::SetTooltip("%s", use_zh ? "本算法不使用此参数" : "This algorithm does not use Amount");
                        }
                    }

                    // Shades B
                    int shadesB = r.textureShadesB;
                    if (ImGui::SliderInt(use_zh ? "层次数 (Shades B)" : "Shades B", &shadesB, 1, 4)) {
                        int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                        atm::Recipe nr = r;
                        nr.textureShadesB = shadesB;
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flag));
                    }

                    // Seed B
                    int seedB = r.textureSeedB;
                    if (ui::drag_int_with_dice(use_zh ? "纹理种子 (Seed B)" : "Seed B", &seedB, 0, 99999, "SeedB")) {
                        atm::Recipe nr = r;
                        nr.textureSeedB = seedB;
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
                    }

                    // Cell Scale B (only cells)
                    bool cellsB_active = (r.textureAlgoB == "cells");
                    if (!cellsB_active) ImGui::BeginDisabled();
                    int cellScaleB = r.cellScaleB;
                    if (ImGui::SliderInt(use_zh ? "细胞尺度 (Cell Scale B)" : "Cell Scale B", &cellScaleB, 2, 6)) {
                        int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                        atm::Recipe nr = r;
                        nr.cellScaleB = cellScaleB;
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flag));
                    }
                    if (!cellsB_active) {
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            ImGui::SetTooltip("%s", use_zh ? "仅多边形细胞 (cells) 算法使用此参数" : "Only Polygonal Cells uses Cell Scale");
                        }
                    }

                    // Ripple Scale B (only ripple / ripple_diag)
                    bool rippleB_active = (r.textureAlgoB == "ripple" || r.textureAlgoB == "ripple_diag");
                    if (!rippleB_active) ImGui::BeginDisabled();
                    int rippleScaleB = r.rippleScaleB;
                    if (ImGui::SliderInt(use_zh ? "波纹尺度 (Ripple Scale B)" : "Ripple Scale B", &rippleScaleB, 2, 8)) {
                        int flag = ImGui::IsItemDeactivatedAfterEdit() ? 2 : (ImGui::IsItemActivated() ? 0 : 1);
                        atm::Recipe nr = r;
                        nr.rippleScaleB = rippleScaleB;
                        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flag));
                    }
                    if (!rippleB_active) {
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            ImGui::SetTooltip("%s", use_zh ? "仅水面波纹 (ripple) 算法使用此参数" : "Only Ripple algorithms use Ripple Scale");
                        }
                    }

                    // Geo Scale B (Size)
                    bool geoB_active = atm::texture_uses_geo_scale(r.textureAlgoB);
                    if (!geoB_active) ImGui::BeginDisabled();
                    auto scalesB = atm::geo_scales_for(r.textureAlgoB);
                    std::string geoB_label = "32px";
                    for (const auto& s : scalesB) {
                        if (s.id == r.geoScaleB) {
                            geoB_label = use_zh ? s.zh : s.en;
                            break;
                        }
                    }
                    if (ImGui::BeginCombo(use_zh ? "尺寸规格 (Size B)" : "Size B", geoB_label.c_str())) {
                        for (const auto& s : scalesB) {
                            bool is_sel = (s.id == r.geoScaleB);
                            if (ImGui::Selectable(use_zh ? s.zh : s.en, is_sel)) {
                                atm::Recipe nr = r;
                                nr.geoScaleB = s.id;
                                vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, 2));
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (!geoB_active) {
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            ImGui::SetTooltip("%s", use_zh ? "仅几何与铺砖纹理支持尺寸缩放" : "Only Geometric/Paving textures use Size");
                        }
                    }

                    // Texture B Overrides Strip
                    if (r.textureAlgoB != "none") {
                        ImGui::Separator();
                        ImGui::TextDisabled("%s", use_zh ? "纹理 B 配色覆盖 (Texture B Palette):" : "Texture B Palette Overrides:");
                        auto used_shadesB = atm::used_texture_shades(r.textureAlgoB, r.textureAmountB, r.textureShadesB, r.cellScaleB, r.rippleScaleB, r.geoScaleB, r.textureSeedB);
                        size_t num_shadesB = static_cast<size_t>(r.textureShadesB + 1);
                        atm::RGB baseB = atm::parse_hex_colour(r.roleHex.terrainB);
                        auto default_rampB = atm::texture_ramp(baseB, std::nullopt, r.textureShadesB);

                        auto custom_texB_copy = r.customTexHexB;
                        ui::draw_sparse_shade_strip("TexBStrip", num_shadesB, default_rampB, used_shadesB, custom_texB_copy, [&](const auto& new_hexes, int flg) {
                            atm::Recipe nr = r;
                            nr.customTexHexB = new_hexes;
                            vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, flg));
                        });
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
    }
    ImGui::End();
}

} // namespace atm_desktop
