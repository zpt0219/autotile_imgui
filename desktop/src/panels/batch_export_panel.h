#pragma once

#include "panel.h"

namespace atm_desktop {

class BatchExportPanel : public IPanel {
public:
    BatchExportPanel() = default;
    ~BatchExportPanel() override = default;

    const char* get_name() const override { return "Batch Export"; }
    void draw(ViewModel& vm) override;

private:
    char out_dir_buffer_[256] = "./export";
    char name_template_buffer_[128] = "{name}_{pattern}_{texA}";
    int export_scale_ = 1;
    bool export_png_ = true;
    bool export_json_ = true;
};

} // namespace atm_desktop
