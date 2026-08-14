#pragma once

#include "panel.h"

namespace atm_desktop {

class PreviewPanel : public IPanel {
public:
    PreviewPanel() = default;
    ~PreviewPanel() override = default;

    const char* get_name() const override { return "Sheet Preview"; }
    void draw(ViewModel& vm) override;
};

} // namespace atm_desktop
