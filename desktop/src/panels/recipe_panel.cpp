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

static void draw_texture_tab(
    ViewModel& vm,
    const std::string& hash,
    const atm::Recipe& r,
    bool is_tab_a,
    bool use_zh
) {
    const char* tab_name = is_tab_a
        ? (use_zh ? "地形 A (内侧)" : "Terrain A (Interior)")
        : (use_zh ? "地形 B (外侧)" : "Terrain B (Exterior)");

    if (!ImGui::BeginTabItem(tab_name)) return;

    const char* combo_label = is_tab_a
        ? (use_zh ? "纹理类型 (Texture A)" : "Texture A##Combo")
        : (use_zh ? "纹理类型 (Texture B)" : "Texture B##Combo");

    const std::string& current_algo = is_tab_a ? r.textureAlgoA : r.textureAlgoB;
    double current_amount = is_tab_a ? r.textureAmountA : r.textureAmountB;
    int current_shades = is_tab_a ? r.textureShadesA : r.textureShadesB;
    int current_seed = is_tab_a ? r.textureSeedA : r.textureSeedB;
    int current_cell_scale = is_tab_a ? r.cellScaleA : r.cellScaleB;
    int current_ripple_scale = is_tab_a ? r.rippleScaleA : r.rippleScaleB;
    int current_geo_scale = is_tab_a ? r.geoScaleA : r.geoScaleB;
    const auto& current_custom_hex = is_tab_a ? r.customTexHexA : r.customTexHexB;
    const std::string& base_role_hex = is_tab_a ? r.roleHex.terrainA : r.roleHex.terrainB;

    auto update_recipe = [&](auto&& mutate, atm::EditPhase phase) {
        atm::Recipe nr = r;
        mutate(nr);
        vm.execute_command(std::make_unique<atm::UpdateRecipeTextureCommand>(hash, nr, phase));
    };

    // Algo Combo
    auto new_tex = ui::grouped_combo(combo_label, atm::texture_groups(), current_algo, use_zh);
    if (new_tex.has_value() && *new_tex != current_algo) {
        update_recipe([&](atm::Recipe& nr) {
            if (is_tab_a) {
                nr.textureAlgoA = *new_tex;
                nr.geoScaleA = atm::natural_geo_scale(*new_tex);
            } else {
                nr.textureAlgoB = *new_tex;
                nr.geoScaleB = atm::natural_geo_scale(*new_tex);
            }
        }, atm::EditPhase::End);
    }

    // Amount
    bool amount_active = atm::texture_uses_amount(current_algo);
    if (!amount_active) ImGui::BeginDisabled();
    float amount_f = static_cast<float>(current_amount);
    const char* amount_label = is_tab_a
        ? (use_zh ? "强度 (Amount A)" : "Amount A")
        : (use_zh ? "强度 (Amount B)" : "Amount B");
    if (ImGui::SliderFloat(amount_label, &amount_f, 0.0f, 1.0f, "%.2f")) {
        update_recipe([&](atm::Recipe& nr) {
            if (is_tab_a) nr.textureAmountA = static_cast<double>(amount_f);
            else nr.textureAmountB = static_cast<double>(amount_f);
        }, ui::current_drag_phase());
    }
    if (!amount_active) {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", use_zh ? "本算法不使用此参数" : "This algorithm does not use Amount");
        }
    }

    // Shades
    int shades = current_shades;
    const char* shades_label = is_tab_a
        ? (use_zh ? "层次数 (Shades A)" : "Shades A")
        : (use_zh ? "层次数 (Shades B)" : "Shades B");
    if (ImGui::SliderInt(shades_label, &shades, 1, 4)) {
        update_recipe([&](atm::Recipe& nr) {
            if (is_tab_a) nr.textureShadesA = shades;
            else nr.textureShadesB = shades;
        }, ui::current_drag_phase());
    }

    // Seed
    int seed = current_seed;
    const char* seed_label = is_tab_a
        ? (use_zh ? "纹理种子 (Seed A)" : "Seed A")
        : (use_zh ? "纹理种子 (Seed B)" : "Seed B");
    const char* seed_id = is_tab_a ? "SeedA" : "SeedB";
    if (ui::drag_int_with_dice(seed_label, &seed, 0, 99999, seed_id)) {
        update_recipe([&](atm::Recipe& nr) {
            if (is_tab_a) nr.textureSeedA = seed;
            else nr.textureSeedB = seed;
        }, atm::EditPhase::End);
    }

    // Cell Scale (only cells)
    bool cells_active = (current_algo == "cells");
    if (!cells_active) ImGui::BeginDisabled();
    int cell_scale = current_cell_scale;
    const char* cell_scale_label = is_tab_a
        ? (use_zh ? "细胞尺度 (Cell Scale A)" : "Cell Scale A")
        : (use_zh ? "细胞尺度 (Cell Scale B)" : "Cell Scale B");
    if (ImGui::SliderInt(cell_scale_label, &cell_scale, 2, 6)) {
        update_recipe([&](atm::Recipe& nr) {
            if (is_tab_a) nr.cellScaleA = cell_scale;
            else nr.cellScaleB = cell_scale;
        }, ui::current_drag_phase());
    }
    if (!cells_active) {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", use_zh ? "仅多边形细胞 (cells) 算法使用此参数" : "Only Polygonal Cells uses Cell Scale");
        }
    }

    // Ripple Scale (only ripple / ripple_diag)
    bool ripple_active = (current_algo == "ripple" || current_algo == "ripple_diag");
    if (!ripple_active) ImGui::BeginDisabled();
    int ripple_scale = current_ripple_scale;
    const char* ripple_scale_label = is_tab_a
        ? (use_zh ? "波纹尺度 (Ripple Scale A)" : "Ripple Scale A")
        : (use_zh ? "波纹尺度 (Ripple Scale B)" : "Ripple Scale B");
    if (ImGui::SliderInt(ripple_scale_label, &ripple_scale, 2, 8)) {
        update_recipe([&](atm::Recipe& nr) {
            if (is_tab_a) nr.rippleScaleA = ripple_scale;
            else nr.rippleScaleB = ripple_scale;
        }, ui::current_drag_phase());
    }
    if (!ripple_active) {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", use_zh ? "仅水面波纹 (ripple) 算法使用此参数" : "Only Ripple algorithms use Ripple Scale");
        }
    }

    // Geo Scale (Size)
    bool geo_active = atm::texture_uses_geo_scale(current_algo);
    if (!geo_active) ImGui::BeginDisabled();
    auto scales = atm::geo_scales_for(current_algo);
    std::string geo_label = "32px";
    for (const auto& s : scales) {
        if (s.id == current_geo_scale) {
            geo_label = use_zh ? s.zh : s.en;
            break;
        }
    }
    const char* geo_combo_label = is_tab_a
        ? (use_zh ? "尺寸规格 (Size A)" : "Size A")
        : (use_zh ? "尺寸规格 (Size B)" : "Size B");
    if (ImGui::BeginCombo(geo_combo_label, geo_label.c_str())) {
        for (const auto& s : scales) {
            bool is_sel = (s.id == current_geo_scale);
            if (ImGui::Selectable(use_zh ? s.zh : s.en, is_sel)) {
                update_recipe([&](atm::Recipe& nr) {
                    if (is_tab_a) nr.geoScaleA = s.id;
                    else nr.geoScaleB = s.id;
                }, atm::EditPhase::End);
            }
        }
        ImGui::EndCombo();
    }
    if (!geo_active) {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", use_zh ? "仅几何与铺砖纹理支持尺寸缩放" : "Only Geometric/Paving textures use Size");
        }
    }

    // Texture Overrides Strip
    if (current_algo != "none") {
        ImGui::Separator();
        const char* strip_title = is_tab_a
            ? (use_zh ? "纹理 A 配色覆盖 (Texture A Palette):" : "Texture A Palette Overrides:")
            : (use_zh ? "纹理 B 配色覆盖 (Texture B Palette):" : "Texture B Palette Overrides:");
        ImGui::TextDisabled("%s", strip_title);
        auto used_shades = atm::used_texture_shades(current_algo, current_amount, current_shades, current_cell_scale, current_ripple_scale, current_geo_scale, current_seed);
        size_t num_shades = static_cast<size_t>(current_shades + 1);
        atm::RGB base = atm::parse_hex_colour(base_role_hex);
        auto default_ramp = atm::texture_ramp(base, std::nullopt, current_shades);

        auto custom_tex_copy = current_custom_hex;
        const char* strip_id = is_tab_a ? "TexAStrip" : "TexBStrip";
        ui::draw_sparse_shade_strip(strip_id, num_shades, default_ramp, used_shades, custom_tex_copy, [&](const auto& new_hexes, atm::EditPhase ph) {
            update_recipe([&](atm::Recipe& nr) {
                if (is_tab_a) nr.customTexHexA = new_hexes;
                else nr.customTexHexB = new_hexes;
            }, ph);
        });
    }

    ImGui::EndTabItem();
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
            atm::EditPhase phase = atm::EditPhase::Continue;

            if (ImGui::ColorEdit3(use_zh ? "地形 A (Terrain A)" : "Terrain A", fA, ImGuiColorEditFlags_NoInputs)) {
                changed = true;
                phase = ui::current_drag_phase();
            }
            if (ImGui::ColorEdit3(use_zh ? "地形 B (Terrain B)" : "Terrain B", fB, ImGuiColorEditFlags_NoInputs)) {
                changed = true;
                phase = ui::current_drag_phase();
            }
            if (ImGui::ColorEdit3(use_zh ? "描边 (Edge)" : "Edge", fE, ImGuiColorEditFlags_NoInputs)) {
                changed = true;
                phase = ui::current_drag_phase();
            }

            if (changed) {
                atm::RoleHex new_roles;
                new_roles.terrainA = atm::to_hex_colour({ static_cast<uint8_t>(fA[0]*255), static_cast<uint8_t>(fA[1]*255), static_cast<uint8_t>(fA[2]*255) });
                new_roles.terrainB = atm::to_hex_colour({ static_cast<uint8_t>(fB[0]*255), static_cast<uint8_t>(fB[1]*255), static_cast<uint8_t>(fB[2]*255) });
                new_roles.edge = atm::to_hex_colour({ static_cast<uint8_t>(fE[0]*255), static_cast<uint8_t>(fE[1]*255), static_cast<uint8_t>(fE[2]*255) });
                vm.execute_command(std::make_unique<atm::UpdateRecipeColoursCommand>(hash, new_roles, r.customShadesHex, phase));
            }

            ImGui::Separator();
            ImGui::TextDisabled("%s", use_zh ? "色带覆盖 (Band Overrides):" : "Band Shade Overrides:");
            atm::RoleColours roles{ colA, colB, colE };
            auto custom_shades_copy = r.customShadesHex;
            ui::draw_band_shade_strip("Band", r.bandSteps, roles, custom_shades_copy, [&](const auto& new_shades, atm::EditPhase ph) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeColoursCommand>(hash, r.roleHex, new_shades, ph));
            });
        }

        // ---------------- 2. Silhouette & Boundary ----------------
        if (ImGui::CollapsingHeader(use_zh ? "轮廓与边界 (Silhouette & Boundary)" : "Silhouette & Boundary", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto new_pat = ui::grouped_combo(use_zh ? "轮廓形状 (Pattern)" : "Pattern", atm::pattern_groups(), r.patternId, use_zh);
            if (new_pat.has_value() && *new_pat != r.patternId) {
                vm.execute_command(std::make_unique<atm::UpdateRecipePatternCommand>(hash, *new_pat, r.edgeSeed, r.outlineWidth, atm::EditPhase::End));
            }

            int seed = r.edgeSeed;
            if (ui::drag_int_with_dice(use_zh ? "边缘种子 (Edge Seed)" : "Edge Seed", &seed, 0, 99999, "EdgeSeed")) {
                vm.execute_command(std::make_unique<atm::UpdateRecipePatternCommand>(hash, r.patternId, seed, r.outlineWidth, atm::EditPhase::End));
            }

            int outline = r.outlineWidth;
            if (ImGui::SliderInt(use_zh ? "描边宽度 (Outline Px)" : "Outline Width (px)", &outline, 1, 4)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipePatternCommand>(hash, r.patternId, r.edgeSeed, outline, ui::current_drag_phase()));
            }

            int steps = r.bandSteps;
            if (ImGui::SliderInt(use_zh ? "过渡阶数 (Band Steps)" : "Band Steps", &steps, 3, 5)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(hash, steps, r.hardEdgeB, r.transparentB, r.bandBias, ui::current_drag_phase()));
            }

            float bias = static_cast<float>(r.bandBias);
            if (ImGui::SliderFloat(use_zh ? "过渡偏置 (Band Bias)" : "Band Bias", &bias, -1.0f, 1.0f, "%.2f")) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(hash, r.bandSteps, r.hardEdgeB, r.transparentB, static_cast<double>(bias), ui::current_drag_phase()));
            }

            bool hard_edge = r.hardEdgeB;
            if (ImGui::Checkbox(use_zh ? "B 侧硬边缘 (Hard Edge B)" : "Hard Edge B", &hard_edge)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(hash, r.bandSteps, hard_edge, r.transparentB, r.bandBias, atm::EditPhase::End));
            }

            bool trans_b = r.transparentB;
            if (ImGui::Checkbox(use_zh ? "B 侧透明 (Transparent B)" : "Transparent Terrain B", &trans_b)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeBandCommand>(hash, r.bandSteps, r.hardEdgeB, trans_b, r.bandBias, atm::EditPhase::End));
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
                vm.execute_command(std::make_unique<atm::UpdateRecipeNoiseCommand>(hash, new_noises, r.patternNoiseSeed, r.patternNoiseStrength, atm::EditPhase::End));
            }

            float strength = static_cast<float>(r.patternNoiseStrength);
            if (ImGui::SliderFloat(use_zh ? "噪声强度 (Strength)" : "Noise Strength", &strength, 0.0f, 2.0f, "%.2f")) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeNoiseCommand>(hash, r.patternNoise, r.patternNoiseSeed, static_cast<double>(strength), ui::current_drag_phase()));
            }

            int nseed = r.patternNoiseSeed;
            if (ui::drag_int_with_dice(use_zh ? "噪声种子 (Noise Seed)" : "Noise Seed", &nseed, 0, 99999, "NoiseSeed")) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeNoiseCommand>(hash, r.patternNoise, nseed, r.patternNoiseStrength, atm::EditPhase::End));
            }
        }

        // ---------------- 4. Ribbon Motif ----------------
        if (ImGui::CollapsingHeader(use_zh ? "花纹 (Ribbon Motif)" : "Ribbon Motif", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto new_rib = ui::grouped_combo(use_zh ? "花纹类型 (Motif)" : "Motif Algo", atm::ribbon_groups(), r.ribbonAlgo, use_zh);
            if (new_rib.has_value() && *new_rib != r.ribbonAlgo) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, *new_rib, r.ribbonAmount, r.ribbonPeriod, r.ribbonShades, r.ribbonInvert, r.customRibbonHex, atm::EditPhase::End
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
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, static_cast<double>(amount), r.ribbonPeriod, r.ribbonShades, r.ribbonInvert, r.customRibbonHex, ui::current_drag_phase()
                ));
            }

            // Period (greyed-out if aperiodic)
            bool period_active = atm::ribbon_uses_period(r.ribbonAlgo);
            if (!period_active) ImGui::BeginDisabled();
            int period = r.ribbonPeriod;
            if (ImGui::SliderInt(use_zh ? "周期 (Period Px)" : "Period (px)", &period, 1, 16)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, r.ribbonAmount, period, r.ribbonShades, r.ribbonInvert, r.customRibbonHex, ui::current_drag_phase()
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
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, r.ribbonAmount, r.ribbonPeriod, shades, r.ribbonInvert, r.customRibbonHex, ui::current_drag_phase()
                ));
            }

            // Invert (greyed-out if unflippable)
            bool invert_active = atm::ribbon_uses_invert(r.ribbonAlgo);
            if (!invert_active) ImGui::BeginDisabled();
            bool invert = r.ribbonInvert;
            if (ImGui::Checkbox(use_zh ? "反转高光 (Invert)" : "Invert Ribbon", &invert)) {
                vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                    hash, r.ribbonAlgo, r.ribbonAmount, r.ribbonPeriod, r.ribbonShades, invert, r.customRibbonHex, atm::EditPhase::End
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
                ui::draw_sparse_shade_strip("RibbonStrip", num_shades, default_ribbon_colors, used_shades, custom_ribbon_copy, [&](const auto& new_hexes, atm::EditPhase ph) {
                    vm.execute_command(std::make_unique<atm::UpdateRecipeRibbonCommand>(
                        hash, r.ribbonAlgo, r.ribbonAmount, r.ribbonPeriod, r.ribbonShades, r.ribbonInvert, new_hexes, ph
                    ));
                });
            }
        }

        // ---------------- 5. Surface Textures ----------------
        if (ImGui::CollapsingHeader(use_zh ? "表面纹理 (Surface Textures)" : "Surface Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTabBar("TerrainTexturesTabBar")) {
                draw_texture_tab(vm, hash, r, /*is_tab_a=*/true, use_zh);
                draw_texture_tab(vm, hash, r, /*is_tab_a=*/false, use_zh);
                ImGui::EndTabBar();
            }
        }
    }
    ImGui::End();
}

} // namespace atm_desktop
