#include "library_panel.h"
#include "view_model/view_model.h"
#include "command/library_command.h"
#include "codec/recipe_codec.h"
#include "codec/zip_import.h"
#include "file_dialog.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace atm_desktop {

void LibraryPanel::draw(ViewModel& vm) {
    if (!open_) return;

    if (ImGui::Begin("Recipe Library", &open_)) {
        bool use_zh = vm.use_zh;

        // Toolbar
        if (ImGui::Button(use_zh ? "+ 新配方" : "+ New")) {
            vm.show_new_recipe_modal = true;
            vm.new_recipe_name_buffer = "New Recipe";
        }
        ImGui::SameLine();
        if (ImGui::Button(use_zh ? "导入代码" : "Import Code")) {
            vm.show_import_share_modal = true;
            vm.import_share_code_buffer.clear();
            vm.import_share_error.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button(use_zh ? "导入 ZIP" : "Import ZIP")) {
            auto path = fd::open_file("Import Recipe ZIP Archive", "", { "*.zip" }, "ZIP Archives (*.zip)");
            if (path.has_value()) {
                auto zip_res = atm::import_recipes_from_zip(*path);
                if (zip_res.success) {
                    for (const auto& entry : zip_res.entries) {
                        vm.execute_command(std::make_unique<atm::AddRecipeCommand>(entry.recipe, entry.name));
                    }
                    vm.log("Imported " + std::to_string(zip_res.entries.size()) + " recipes from " + *path, "INFO");
                } else {
                    std::string err = zip_res.errors.empty() ? "Failed to import ZIP archive" : zip_res.errors[0];
                    vm.log(err, "ERROR");
                }
            }
        }

        ImGui::SameLine(0, 15.0f);
        if (ImGui::Button(is_grid_view_ ? (use_zh ? "列表视图 [≡]" : "List [≡]") : (use_zh ? "网格视图 [⊞]" : "Grid [⊞]"))) {
            is_grid_view_ = !is_grid_view_;
        }

        ImGui::Separator();

        // Search Filter
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##Search", use_zh ? "搜索配方..." : "Search recipes...", search_filter_, sizeof(search_filter_));

        ImGui::Separator();

        // Recipe List / Grid
        auto* lib = vm.handler().library();
        auto* selected = vm.handler().selected_recipe();
        std::string filter_str(search_filter_);
        std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), ::tolower);

        ImGui::BeginChild("RecipeListChild", ImVec2(0, 0), true);
        if (lib) {
            std::vector<atm::RecipeEntry*> filtered_entries;
            for (const auto& entry : lib->entries()) {
                if (!filter_str.empty()) {
                    std::string lower_name = entry->name;
                    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                    if (lower_name.find(filter_str) == std::string::npos) {
                        continue;
                    }
                }
                filtered_entries.push_back(entry.get());
            }

            if (!is_grid_view_) {
                // List View
                for (auto* entry : filtered_entries) {
                    bool is_selected = (selected && selected->hash == entry->hash);

                    ImGui::PushID(entry->hash.c_str());

                    std::string label = entry->name + " (" + entry->recipe.patternId + ")";
                    if (ImGui::Selectable(label.c_str(), is_selected)) {
                        vm.execute_command(std::make_unique<atm::SelectRecipeCommand>(entry->hash));
                    }

                    // Context Menu
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem(use_zh ? "复制副本" : "Duplicate")) {
                            vm.execute_command(std::make_unique<atm::DuplicateRecipeCommand>(entry->hash));
                        }
                        if (ImGui::MenuItem(use_zh ? "重命名" : "Rename")) {
                            renaming_hash_ = entry->hash;
                            std::strncpy(rename_buffer_, entry->name.c_str(), sizeof(rename_buffer_) - 1);
                            rename_buffer_[sizeof(rename_buffer_) - 1] = 0;
                        }
                        if (ImGui::MenuItem(use_zh ? "复制分享代码" : "Copy Share Code")) {
                            std::string code = atm::encode_recipe(entry->recipe);
                            ImGui::SetClipboardText(code.c_str());
                            vm.log("Share code copied to clipboard", "INFO");
                        }
                        ImGui::Separator();
                        if (lib->entries().size() > 1 && ImGui::MenuItem(use_zh ? "删除" : "Delete", nullptr, false, true)) {
                            vm.execute_command(std::make_unique<atm::RemoveRecipeCommand>(entry->hash));
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                }
            } else {
                // Grid / Thumbnail View
                float avail_w = ImGui::GetContentRegionAvail().x;
                int cols = std::max(1, static_cast<int>(avail_w / 115.0f));

                // Card (96x72) + button padding + caption line. Only needs to
                // be close enough for the clipper to pick the right rows.
                const float ROW_HEIGHT = 72.0f + 8.0f + ImGui::GetTextLineHeightWithSpacing() + 8.0f;

                if (ImGui::BeginTable("ThumbGridTable", cols, ImGuiTableFlags_SizingFixedFit)) {
                    // Only the visible rows are walked, so only the visible
                    // cards ask the cache for a thumbnail. Without this the
                    // grid queues a render for every recipe in the library the
                    // moment it opens.
                    const int row_count = (static_cast<int>(filtered_entries.size()) + cols - 1) / cols;
                    ImGuiListClipper clipper;
                    clipper.Begin(row_count, ROW_HEIGHT);

                    while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    ImGui::TableNextRow();
                    for (int col = 0; col < cols; ++col) {
                        const size_t i = static_cast<size_t>(row) * static_cast<size_t>(cols)
                                       + static_cast<size_t>(col);
                        if (i >= filtered_entries.size()) break;

                        auto* entry = filtered_entries[i];
                        bool is_selected = (selected && selected->hash == entry->hash);

                        ImGui::TableNextColumn();
                        ImGui::PushID(entry->hash.c_str());

                        // 0 means "not rendered yet"; the request is queued and
                        // a later frame will have it.
                        uint32_t thumb_tex = vm.thumbnail_cache().get(entry->hash, entry->recipe);

                        ImVec2 card_sz(96, 72);
                        ImVec4 bg_col = is_selected ? ImVec4(0.2f, 0.4f, 0.6f, 1.0f) : ImVec4(0.15f, 0.16f, 0.18f, 1.0f);
                        ImVec4 tint_col = ImVec4(1, 1, 1, 1);

                        std::string btn_id = "##Card_" + entry->hash;
                        bool clicked = false;
                        if (thumb_tex != 0) {
                            clicked = ImGui::ImageButton(
                                btn_id.c_str(),
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(thumb_tex)),
                                card_sz,
                                ImVec2(0, 0), ImVec2(1, 1),
                                bg_col, tint_col
                            );
                        } else {
                            // Placeholder card, same footprint so nothing jumps
                            // when the real thumbnail lands.
                            ImGui::PushStyleColor(ImGuiCol_Button, bg_col);
                            clicked = ImGui::Button(btn_id.c_str(),
                                                    ImVec2(card_sz.x + 8, card_sz.y + 8));
                            ImGui::PopStyleColor();
                            ImVec2 rmin = ImGui::GetItemRectMin();
                            ImVec2 rmax = ImGui::GetItemRectMax();
                            const char* dots = "...";
                            ImVec2 ts = ImGui::CalcTextSize(dots);
                            ImGui::GetWindowDrawList()->AddText(
                                ImVec2((rmin.x + rmax.x - ts.x) * 0.5f,
                                       (rmin.y + rmax.y - ts.y) * 0.5f),
                                ImGui::GetColorU32(ImGuiCol_TextDisabled), dots);
                        }
                        if (clicked) {
                            vm.execute_command(std::make_unique<atm::SelectRecipeCommand>(entry->hash));
                        }

                        // Context Menu
                        if (ImGui::BeginPopupContextItem()) {
                            if (ImGui::MenuItem(use_zh ? "复制副本" : "Duplicate")) {
                                vm.execute_command(std::make_unique<atm::DuplicateRecipeCommand>(entry->hash));
                            }
                            if (ImGui::MenuItem(use_zh ? "重命名" : "Rename")) {
                                renaming_hash_ = entry->hash;
                                std::strncpy(rename_buffer_, entry->name.c_str(), sizeof(rename_buffer_) - 1);
                                rename_buffer_[sizeof(rename_buffer_) - 1] = 0;
                            }
                            if (ImGui::MenuItem(use_zh ? "复制分享代码" : "Copy Share Code")) {
                                std::string code = atm::encode_recipe(entry->recipe);
                                ImGui::SetClipboardText(code.c_str());
                                vm.log("Share code copied to clipboard", "INFO");
                            }
                            ImGui::Separator();
                            if (lib->entries().size() > 1 && ImGui::MenuItem(use_zh ? "删除" : "Delete", nullptr, false, true)) {
                                vm.execute_command(std::make_unique<atm::RemoveRecipeCommand>(entry->hash));
                            }
                            ImGui::EndPopup();
                        }

                        // Caption & Tooltip
                        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 96.0f);
                        ImGui::TextUnformatted(entry->name.c_str());
                        ImGui::PopTextWrapPos();

                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                            ImGui::BeginTooltip();
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", entry->name.c_str());
                            ImGui::Separator();
                            ImGui::Text("Pattern: %s", entry->recipe.patternId.c_str());
                            ImGui::Text("Texture A: %s", entry->recipe.textureAlgoA.c_str());
                            ImGui::Text("Texture B: %s", entry->recipe.textureAlgoB.c_str());
                            ImGui::Text("Ribbon: %s", entry->recipe.ribbonAlgo.c_str());
                            ImGui::EndTooltip();
                        }

                        ImGui::PopID();
                    }
                    }
                    }
                    clipper.End();
                    ImGui::EndTable();
                }
            }
        }
        ImGui::EndChild();

        // Modals: New Recipe
        if (vm.show_new_recipe_modal) {
            ImGui::OpenPopup("Create New Recipe");
        }
        if (ImGui::BeginPopupModal("Create New Recipe", &vm.show_new_recipe_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char name_buf[128] = "New Recipe";
            ImGui::Text("%s", use_zh ? "输入配方名称:" : "Enter recipe name:");
            ImGui::InputText("##NewName", name_buf, sizeof(name_buf));
            ImGui::Spacing();

            if (ImGui::Button(use_zh ? "创建" : "Create", ImVec2(120, 0))) {
                atm::Recipe r = atm::get_default_recipe();
                vm.execute_command(std::make_unique<atm::AddRecipeCommand>(r, name_buf));
                vm.show_new_recipe_modal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(use_zh ? "取消" : "Cancel", ImVec2(120, 0))) {
                vm.show_new_recipe_modal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Modals: Import Share Code
        if (vm.show_import_share_modal) {
            ImGui::OpenPopup("Import Share Code");
        }
        if (ImGui::BeginPopupModal("Import Share Code", &vm.show_import_share_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char code_buf[1024] = { 0 };
            ImGui::Text("%s", use_zh ? "粘贴来自 Web 工具的 Base64URL 分享代码:" : "Paste Base64URL share code from web tool:");
            ImGui::InputTextMultiline("##ShareCode", code_buf, sizeof(code_buf), ImVec2(350, 80));

            if (!vm.import_share_error.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", vm.import_share_error.c_str());
            }

            ImGui::Spacing();
            if (ImGui::Button(use_zh ? "导入" : "Import", ImVec2(120, 0))) {
                auto decoded = atm::decode_recipe(code_buf);
                if (decoded.has_value()) {
                    vm.execute_command(std::make_unique<atm::AddRecipeCommand>(*decoded, "Imported Recipe"));
                    vm.show_import_share_modal = false;
                    code_buf[0] = 0;
                    vm.import_share_error.clear();
                    ImGui::CloseCurrentPopup();
                } else {
                    vm.import_share_error = "Invalid or corrupted share code!";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(use_zh ? "取消" : "Cancel", ImVec2(120, 0))) {
                vm.show_import_share_modal = false;
                code_buf[0] = 0;
                vm.import_share_error.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Modals: Rename
        if (!renaming_hash_.empty()) {
            ImGui::OpenPopup("Rename Recipe");
        }
        if (ImGui::BeginPopupModal("Rename Recipe", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", use_zh ? "输入新名称:" : "Enter new name:");
            ImGui::InputText("##RenameInput", rename_buffer_, sizeof(rename_buffer_));
            ImGui::Spacing();

            if (ImGui::Button(use_zh ? "保存" : "Save", ImVec2(120, 0))) {
                vm.execute_command(std::make_unique<atm::RenameRecipeCommand>(renaming_hash_, rename_buffer_, 2));
                renaming_hash_.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(use_zh ? "取消" : "Cancel", ImVec2(120, 0))) {
                renaming_hash_.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

} // namespace atm_desktop
