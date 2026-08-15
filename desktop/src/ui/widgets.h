#pragma once

#include "ui_constants.h"
#include "pattern/catalog.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <optional>
#include <random>

namespace atm_desktop {
namespace ui {

inline void help_marker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

inline std::optional<std::string> grouped_combo(
    const char* label,
    const std::vector<atm::CatalogGroup>& groups,
    const std::string& current_id,
    bool use_zh = true
) {
    std::string preview_text = current_id;
    for (const auto& group : groups) {
        for (const auto& item : group.items) {
            if (current_id == item.id) {
                preview_text = use_zh ? item.zh : item.en;
                break;
            }
        }
    }

    std::optional<std::string> chosen = std::nullopt;

    if (ImGui::BeginCombo(label, preview_text.c_str())) {
        for (size_t g_idx = 0; g_idx < groups.size(); ++g_idx) {
            const auto& group = groups[g_idx];
            if (g_idx > 0) {
                ImGui::Separator();
            }

            // Group header title
            const char* group_title = use_zh ? group.zh : group.en;
            ImGui::TextDisabled("--- %s ---", group_title);

            for (const auto& item : group.items) {
                bool is_selected = (current_id == item.id);
                const char* item_text = use_zh ? item.zh : item.en;

                ImGui::PushID(item.id);
                if (ImGui::Selectable(item_text, is_selected)) {
                    chosen = item.id;
                }

                // Hover tooltip with bilingual descriptions
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", item.id);
                    ImGui::Separator();
                    ImGui::Text("%s", item.zh);
                    ImGui::TextDisabled("%s", item.en);
                    ImGui::EndTooltip();
                }

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndCombo();
    }

    return chosen;
}

inline bool drag_int_with_dice(
    const char* label,
    int* value,
    int min_val,
    int max_val,
    const char* id_suffix = ""
) {
    bool changed = false;

    ImGui::SetNextItemWidth(COMPACT_INPUT_WIDTH);
    std::string input_id = std::string(label) + "##Input_" + id_suffix;
    if (ImGui::DragInt(input_id.c_str(), value, 1.0f, min_val, max_val)) {
        changed = true;
    }

    ImGui::SameLine();
    std::string dice_id = "Dice##Dice_" + std::string(id_suffix);
    if (ImGui::Button(dice_id.c_str())) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(min_val, max_val);
        *value = dis(gen);
        changed = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("Randomize seed / 随机种子");
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(label);

    return changed;
}

} // namespace ui
} // namespace atm_desktop
