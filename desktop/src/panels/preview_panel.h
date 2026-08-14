#pragma once

#include "panel.h"
#include <imgui.h>

namespace atm_desktop {

class PreviewPanel : public IPanel {
public:
    PreviewPanel() = default;
    ~PreviewPanel() override = default;

    const char* get_name() const override { return "Sheet Preview"; }
    void draw(ViewModel& vm) override;

private:
    ImVec2 pan_offset_ = ImVec2(0.0f, 0.0f);
    int focused_slot_ = -1;
    bool is_panning_ = false;
    ImVec2 pan_start_mouse_ = ImVec2(0.0f, 0.0f);
    ImVec2 pan_start_offset_ = ImVec2(0.0f, 0.0f);
};

} // namespace atm_desktop
