#pragma once

#include "ui_constants.h"
#include "pattern/pattern_paint.h"
#include "command/library_callbacks.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <optional>
#include <set>
#include <functional>

namespace atm_desktop {
namespace ui {

inline atm::EditPhase current_drag_phase() {
    if (ImGui::IsItemDeactivatedAfterEdit()) return atm::EditPhase::End;
    if (ImGui::IsItemActivated()) return atm::EditPhase::Begin;
    return atm::EditPhase::Continue;
}

// Swatch strip for full band replacement (all-or-nothing mode)
inline bool draw_band_shade_strip(
    const char* id,
    int steps,
    const atm::RoleColours& roles,
    std::optional<std::vector<std::string>>& custom_shades,
    std::function<void(const std::optional<std::vector<std::string>>& new_shades, atm::EditPhase phase)> on_change
) {
    bool modified = false;
    size_t count = static_cast<size_t>(steps + 2);
    auto default_ramp = atm::pattern_ramp(roles, steps);

    bool is_custom = custom_shades.has_value() && custom_shades->size() == count;
    bool checkbox_val = is_custom;

    std::string cb_id = std::string("Manual Override##CB_") + id;
    if (ImGui::Checkbox(cb_id.c_str(), &checkbox_val)) {
        if (checkbox_val) {
            std::vector<std::string> hexes(count);
            for (size_t i = 0; i < count; ++i) {
                hexes[i] = (i < default_ramp.size()) ? atm::to_hex_colour(default_ramp[i]) : "#ffffff";
            }
            custom_shades = hexes;
            on_change(custom_shades, atm::EditPhase::End);
        } else {
            custom_shades = std::nullopt;
            on_change(std::nullopt, atm::EditPhase::End);
        }
        modified = true;
    }

    ImGui::Spacing();

    for (size_t i = 0; i < count; ++i) {
        if (i > 0) ImGui::SameLine();

        atm::RGB current_rgb;
        if (is_custom && i < custom_shades->size()) {
            current_rgb = atm::parse_hex_colour((*custom_shades)[i]);
        } else if (i < default_ramp.size()) {
            current_rgb = default_ramp[i];
        }

        ImVec4 col(current_rgb.r / 255.0f, current_rgb.g / 255.0f, current_rgb.b / 255.0f, 1.0f);
        std::string btn_id = std::string("##Swatch_") + id + "_" + std::to_string(i);

        if (!is_custom) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
        }

        if (ImGui::ColorButton(btn_id.c_str(), col, ImGuiColorEditFlags_NoAlpha, ImVec2(24, 24))) {
            if (is_custom) {
                ImGui::OpenPopup((std::string("PickerPopup_") + id + "_" + std::to_string(i)).c_str());
            }
        }

        if (!is_custom) {
            ImGui::PopStyleVar();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("Step %d (Computed)", (int)i);
            }
        } else {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("Step %d: %s (Click to edit)", (int)i, (*custom_shades)[i].c_str());
            }

            std::string popup_name = std::string("PickerPopup_") + id + "_" + std::to_string(i);
            if (ImGui::BeginPopup(popup_name.c_str())) {
                float fcol[3] = { col.x, col.y, col.z };
                if (ImGui::ColorPicker3("##picker", fcol, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview)) {
                    (*custom_shades)[i] = atm::to_hex_colour({
                        static_cast<uint8_t>(fcol[0] * 255),
                        static_cast<uint8_t>(fcol[1] * 255),
                        static_cast<uint8_t>(fcol[2] * 255)
                    });
                    on_change(custom_shades, current_drag_phase());
                    modified = true;
                }
                ImGui::EndPopup();
            }
        }
    }

    return modified;
}

// Swatch strip for sparse element-by-element overrides (Ribbon & Texture mode)
inline bool draw_sparse_shade_strip(
    const char* id,
    size_t shade_count,
    const std::vector<atm::RGB>& default_colors,
    const std::set<int>& used_shades,
    std::optional<std::vector<std::optional<std::string>>>& custom_shades,
    std::function<void(const std::optional<std::vector<std::optional<std::string>>>& new_shades, atm::EditPhase phase)> on_change
) {
    bool modified = false;

    for (size_t i = 0; i < shade_count; ++i) {
        if (i > 0) ImGui::SameLine();

        bool is_used = (used_shades.find(static_cast<int>(i)) != used_shades.end());
        bool is_overridden = custom_shades.has_value() && i < custom_shades->size() && (*custom_shades)[i].has_value();

        atm::RGB current_rgb;
        if (is_overridden) {
            current_rgb = atm::parse_hex_colour(*(*custom_shades)[i]);
        } else if (i < default_colors.size()) {
            current_rgb = default_colors[i];
        }

        ImVec4 col(current_rgb.r / 255.0f, current_rgb.g / 255.0f, current_rgb.b / 255.0f, 1.0f);
        std::string btn_id = std::string("##SparseSwatch_") + id + "_" + std::to_string(i);

        if (!is_used) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
        }

        if (ImGui::ColorButton(btn_id.c_str(), col, ImGuiColorEditFlags_NoAlpha, ImVec2(24, 24))) {
            ImGui::OpenPopup((std::string("SparsePicker_") + id + "_" + std::to_string(i)).c_str());
        }

        if (!is_used) {
            ImGui::PopStyleVar();
        }

        // Tooltip
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            if (!is_used) {
                ImGui::SetTooltip("Shade %d: Unused by current algorithm settings", (int)i);
            } else if (is_overridden) {
                ImGui::SetTooltip("Shade %d: %s (Overridden - Right click to reset)", (int)i, (*custom_shades)[i]->c_str());
            } else {
                ImGui::SetTooltip("Shade %d: Computed (Click to override)", (int)i);
            }
        }

        // Context menu to reset override
        if (ImGui::BeginPopupContextItem()) {
            if (is_overridden && ImGui::MenuItem("Reset to Computed Color / 恢复默认")) {
                (*custom_shades)[i] = std::nullopt;
                // If all are nullopt, reset container to nullopt
                bool any_left = false;
                for (const auto& opt : *custom_shades) {
                    if (opt.has_value()) { any_left = true; break; }
                }
                if (!any_left) {
                    custom_shades = std::nullopt;
                }
                on_change(custom_shades, atm::EditPhase::End);
                modified = true;
            }
            ImGui::EndPopup();
        }

        // Popup color picker
        std::string popup_name = std::string("SparsePicker_") + id + "_" + std::to_string(i);
        if (ImGui::BeginPopup(popup_name.c_str())) {
            float fcol[3] = { col.x, col.y, col.z };
            if (ImGui::ColorPicker3("##picker", fcol, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview)) {
                if (!custom_shades.has_value()) {
                    custom_shades = std::vector<std::optional<std::string>>(shade_count, std::nullopt);
                } else if (custom_shades->size() < shade_count) {
                    custom_shades->resize(shade_count, std::nullopt);
                }

                (*custom_shades)[i] = atm::to_hex_colour({
                    static_cast<uint8_t>(fcol[0] * 255),
                    static_cast<uint8_t>(fcol[1] * 255),
                    static_cast<uint8_t>(fcol[2] * 255)
                });
                on_change(custom_shades, current_drag_phase());
                modified = true;
            }
            ImGui::EndPopup();
        }
    }

    return modified;
}

} // namespace ui
} // namespace atm_desktop
