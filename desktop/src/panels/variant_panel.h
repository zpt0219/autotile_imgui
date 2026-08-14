#pragma once

#include "panel.h"

namespace atm_desktop {

class VariantPanel : public IPanel {
public:
    VariantPanel() = default;
    ~VariantPanel() override = default;

    const char* get_name() const override { return "Variant Matrix"; }
    void draw(ViewModel& vm) override;

private:
    bool use_all_patterns_ = false;
    bool use_all_textures_ = false;
    bool use_all_ribbons_ = false;
};

} // namespace atm_desktop
