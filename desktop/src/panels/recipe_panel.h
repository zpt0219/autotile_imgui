#pragma once

#include "panel.h"

namespace atm_desktop {

class RecipePanel : public IPanel {
public:
    RecipePanel() = default;
    ~RecipePanel() override = default;

    const char* get_name() const override { return "Recipe Inspector"; }
    void draw(ViewModel& vm) override;
};

} // namespace atm_desktop
