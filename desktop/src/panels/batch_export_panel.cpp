#include "batch_export_panel.h"
#include "view_model/view_model.h"
#include <imgui.h>

namespace atm_desktop {

void BatchExportPanel::draw(ViewModel& vm) {
    if (!open_) return;

    if (ImGui::Begin("Batch Export", &open_)) {
        ImGui::Text("Destination Directory:");
        ImGui::InputText("##OutDir", out_dir_buffer_, sizeof(out_dir_buffer_));

        ImGui::Spacing();
        ImGui::Text("Naming Template:");
        ImGui::InputText("##NameTemplate", name_template_buffer_, sizeof(name_template_buffer_));
        ImGui::TextDisabled("Available tags: {name}, {pattern}, {texA}, {texB}, {ribbon}");

        ImGui::Separator();
        ImGui::Checkbox("Export 256x192 PNG Sheet", &export_png_);
        ImGui::Checkbox("Export JSON Recipe Sidecar (.recipe.json)", &export_json_);

        ImGui::Separator();

        auto* lib = vm.handler().library();
        int recipe_count = lib ? static_cast<int>(lib->entries().size()) : 0;
        ImGui::Text("Library contains: %d recipes to export", recipe_count);

        ImGui::Spacing();

        if (!vm.is_exporting) {
            if (recipe_count > 0 && ImGui::Button("Start Export", ImVec2(160, 32))) {
                atm::ExportSettings settings;
                settings.out_dir = out_dir_buffer_;
                settings.name_template = name_template_buffer_;
                settings.export_png = export_png_;
                settings.export_json_sidecar = export_json_;

                std::vector<atm::RecipeEntry> to_export;
                for (const auto& entry_ptr : lib->entries()) {
                    to_export.push_back(*entry_ptr);
                }
                vm.start_batch_export(to_export, settings);
            }
        } else {
            // Export Progress Bar
            float frac = (vm.current_export_progress.total > 0)
                ? static_cast<float>(vm.current_export_progress.current) / static_cast<float>(vm.current_export_progress.total)
                : 0.0f;

            char buf[64];
            snprintf(buf, sizeof(buf), "%d / %d", vm.current_export_progress.current, vm.current_export_progress.total);
            ImGui::ProgressBar(frac, ImVec2(-1, 24), buf);

            if (ImGui::Button("Cancel Export", ImVec2(120, 28))) {
                vm.cancel_export_requested = true;
            }
        }
    }
    ImGui::End();
}

} // namespace atm_desktop
