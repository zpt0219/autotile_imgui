#include "log_panel.h"
#include "view_model/view_model.h"
#include <imgui.h>

namespace atm_desktop {

void LogPanel::draw(ViewModel& vm) {
    if (!open_) return;

    if (ImGui::Begin("Activity Log", &open_)) {
        if (ImGui::Button("Clear")) {
            vm.clear_logs();
        }
        ImGui::Separator();

        ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& entry : vm.get_logs()) {
            ImVec4 col(0.8f, 0.8f, 0.8f, 1.0f);
            if (entry.level == "WARN") col = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            else if (entry.level == "ERROR") col = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", entry.timestamp.c_str());
            ImGui::SameLine();
            ImGui::TextColored(col, "[%s] %s", entry.level.c_str(), entry.message.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

} // namespace atm_desktop
