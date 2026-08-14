#pragma once

#include "panel.h"
#include <string>

namespace atm_desktop {

class LibraryPanel : public IPanel {
public:
    LibraryPanel() = default;
    ~LibraryPanel() override = default;

    const char* get_name() const override { return "Recipe Library"; }
    void draw(ViewModel& vm) override;

private:
    char search_filter_[128] = { 0 };
    std::string renaming_hash_;
    char rename_buffer_[128] = { 0 };
    bool is_grid_view_ = false;
};

} // namespace atm_desktop
