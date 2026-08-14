#include "library_panel.h"
#include "view_model/view_model.h"
#include "command/library_command.h"
#include "codec/recipe_codec.h"
#include "codec/zip_import.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace atm_desktop {

void LibraryPanel::draw(ViewModel& vm) {
    if (!open_) return;

    if (ImGui::Begin("Recipe Library", &open_)) {
        // Toolbar
        if (ImGui::Button("+ New")) {
            vm.show_new_recipe_modal = true;
            vm.new_recipe_name_buffer = "New Recipe";
        }
        ImGui::SameLine();
        if (ImGui::Button("Import Code")) {
            vm.show_import_share_modal = true;
            vm.import_share_code_buffer.clear();
            vm.import_share_error.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Import ZIP")) {
            show_import_zip_modal_ = true;
            import_zip_path_buffer_[0] = 0;
            import_zip_error_.clear();
        }

        ImGui::Separator();

        // Search Filter
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##Search", "Search recipes...", search_filter_, sizeof(search_filter_));

        ImGui::Separator();

        // Recipe List
        auto* lib = vm.handler().library();
        auto* selected = vm.handler().selected_recipe();
        std::string filter_str(search_filter_);
        std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), ::tolower);

        ImGui::BeginChild("RecipeListChild", ImVec2(0, 0), true);
        if (lib) {
            for (size_t i = 0; i < lib->entries().size(); ++i) {
                const auto& entry = lib->entries()[i];

                if (!filter_str.empty()) {
                    std::string lower_name = entry->name;
                    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                    if (lower_name.find(filter_str) == std::string::npos) {
                        continue;
                    }
                }

                bool is_selected = (selected && selected->hash == entry->hash);

                ImGui::PushID(entry->hash.c_str());

                // Selectable item
                std::string label = entry->name + " (" + entry->recipe.patternId + ")";
                if (ImGui::Selectable(label.c_str(), is_selected)) {
                    vm.execute_command(std::make_unique<atm::SelectRecipeCommand>(entry->hash));
                }

                // Context Menu
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Duplicate")) {
                        vm.execute_command(std::make_unique<atm::DuplicateRecipeCommand>(entry->hash));
                    }
                    if (ImGui::MenuItem("Rename")) {
                        renaming_hash_ = entry->hash;
                        std::strncpy(rename_buffer_, entry->name.c_str(), sizeof(rename_buffer_) - 1);
                        rename_buffer_[sizeof(rename_buffer_) - 1] = 0;
                    }
                    if (ImGui::MenuItem("Copy Share Code")) {
                        std::string code = atm::encode_recipe(entry->recipe);
                        ImGui::SetClipboardText(code.c_str());
                        vm.log("Share code copied to clipboard", "INFO");
                    }
                    ImGui::Separator();
                    if (lib->entries().size() > 1 && ImGui::MenuItem("Delete", nullptr, false, true)) {
                        vm.execute_command(std::make_unique<atm::RemoveRecipeCommand>(entry->hash));
                    }
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        // Modals: New Recipe
        if (vm.show_new_recipe_modal) {
            ImGui::OpenPopup("Create New Recipe");
        }
        if (ImGui::BeginPopupModal("Create New Recipe", &vm.show_new_recipe_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char name_buf[128] = "New Recipe";
            ImGui::Text("Enter recipe name:");
            ImGui::InputText("##NewName", name_buf, sizeof(name_buf));
            ImGui::Spacing();

            if (ImGui::Button("Create", ImVec2(120, 0))) {
                atm::Recipe r = atm::get_default_recipe();
                vm.execute_command(std::make_unique<atm::AddRecipeCommand>(r, name_buf));
                vm.show_new_recipe_modal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
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
            ImGui::Text("Paste Base64URL share code from web tool:");
            ImGui::InputTextMultiline("##ShareCode", code_buf, sizeof(code_buf), ImVec2(350, 80));

            if (!vm.import_share_error.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", vm.import_share_error.c_str());
            }

            ImGui::Spacing();
            if (ImGui::Button("Import", ImVec2(120, 0))) {
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
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                vm.show_import_share_modal = false;
                code_buf[0] = 0;
                vm.import_share_error.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Modals: Import ZIP Archive
        if (show_import_zip_modal_) {
            ImGui::OpenPopup("Import ZIP Archive");
        }
        if (ImGui::BeginPopupModal("Import ZIP Archive", &show_import_zip_modal_, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter path to exported .zip file:");
            ImGui::InputText("##ZipPath", import_zip_path_buffer_, sizeof(import_zip_path_buffer_));

            if (!import_zip_error_.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", import_zip_error_.c_str());
            }

            ImGui::Spacing();
            if (ImGui::Button("Import##ZipBtn", ImVec2(120, 0))) {
                auto zip_res = atm::import_recipes_from_zip(import_zip_path_buffer_);
                if (zip_res.success) {
                    for (const auto& entry : zip_res.entries) {
                        vm.execute_command(std::make_unique<atm::AddRecipeCommand>(entry.recipe, entry.name));
                    }
                    vm.log("Imported " + std::to_string(zip_res.entries.size()) + " recipes from ZIP", "INFO");
                    show_import_zip_modal_ = false;
                    import_zip_path_buffer_[0] = 0;
                    import_zip_error_.clear();
                    ImGui::CloseCurrentPopup();
                } else {
                    import_zip_error_ = zip_res.errors.empty() ? "Failed to import ZIP archive" : zip_res.errors[0];
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel##ZipBtn", ImVec2(120, 0))) {
                show_import_zip_modal_ = false;
                import_zip_path_buffer_[0] = 0;
                import_zip_error_.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Modals: Rename
        if (!renaming_hash_.empty()) {
            ImGui::OpenPopup("Rename Recipe");
        }
        if (ImGui::BeginPopupModal("Rename Recipe", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter new name:");
            ImGui::InputText("##RenameInput", rename_buffer_, sizeof(rename_buffer_));
            ImGui::Spacing();

            if (ImGui::Button("Save", ImVec2(120, 0))) {
                vm.execute_command(std::make_unique<atm::RenameRecipeCommand>(renaming_hash_, rename_buffer_, 2));
                renaming_hash_.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                renaming_hash_.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

} // namespace atm_desktop
