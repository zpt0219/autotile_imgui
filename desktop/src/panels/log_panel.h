#pragma once

#include "panel.h"

namespace atm_desktop {

class LogPanel : public IPanel {
public:
    LogPanel() = default;
    ~LogPanel() override = default;

    const char* get_name() const override { return "Activity Log"; }
    void draw(ViewModel& vm) override;
};

} // namespace atm_desktop
