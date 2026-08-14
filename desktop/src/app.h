#pragma once

#include "view_model/view_model.h"
#include "panels/library_panel.h"
#include "panels/recipe_panel.h"
#include "panels/preview_panel.h"
#include "panels/variant_panel.h"
#include "panels/batch_export_panel.h"
#include "panels/log_panel.h"

struct GLFWwindow;

namespace atm_desktop {

class App {
public:
    App();
    ~App();

    bool initialize();
    void run();
    void cleanup();

private:
    void render_menu_bar();
    void setup_dockspace();
    void set_dark_theme();

    GLFWwindow* window_ = nullptr;
    ViewModel view_model_;

    LibraryPanel library_panel_;
    RecipePanel recipe_panel_;
    PreviewPanel preview_panel_;
    VariantPanel variant_panel_;
    BatchExportPanel batch_export_panel_;
    LogPanel log_panel_;

    bool is_running_ = false;
};

} // namespace atm_desktop
