#pragma once

#include "handler/library_handler.h"
#include "command/library_command_handler.h"
#include "renderer/sheet_renderer.h"
#include "renderer/thumbnail_cache.h"
#include "panels/panel.h"
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

namespace atm_desktop {

struct LogEntry {
    std::string message;
    std::string level; // "INFO", "WARN", "ERROR"
    std::string timestamp;
};

class ViewModel : public atm::LibraryCallbacks {
public:
    ViewModel();
    ~ViewModel() override;

    // Handler and commands
    atm::LibraryHandler& handler() { return handler_; }
    atm::LibraryCommandHandler& command_handler() { return cmd_handler_; }
    SheetRenderer& sheet_renderer() { return sheet_renderer_; }
    ThumbnailCache& thumbnail_cache() { return thumbnail_cache_; }

    atm::EditorResult execute_command(std::unique_ptr<atm::LibraryCommand> command);
    atm::EditorResult undo();
    atm::EditorResult redo();
    bool can_undo() const { return cmd_handler_.can_undo(); }
    bool can_redo() const { return cmd_handler_.can_redo(); }

    // Panel Management
    void register_panel(IPanel* panel);
    void unregister_panel(IPanel* panel);

    // Callbacks from LibraryHandler (fan_out to all panels)
    void onLibraryLoaded(int flag) override;
    void onLibraryListUpdated(int flag) override;
    void onRecipeSelected(atm::RecipeEntry* entry, int flag) override;
    void onRecipeUpdated(atm::RecipeEntry* entry, atm::DirtyMask dirty, int flag) override;
    void onVariantAxesUpdated(int flag) override;
    void onBatchProgress(const atm::BatchProgress& progress, int flag) override;
    void onExportSettingsUpdated(int flag) override;

    // Logging (thread-safe)
    void log(const std::string& msg, const std::string& level = "INFO");
    std::vector<LogEntry> get_logs() const;
    void clear_logs();

    // Dialog state

    bool show_new_recipe_modal = false;
    std::string new_recipe_name_buffer = "New Recipe";

    // Viewport and UI state
    float zoom_level = 2.0f;
    bool show_grid = true;
    bool show_checkerboard = true;
    bool use_zh = true;
    int hovered_slot = -1;

    // Batch Exporting State (thread-safe queue drained on main thread)
    std::atomic<bool> is_exporting{ false };
    atm::BatchProgress current_export_progress;
    std::atomic<bool> cancel_export_requested{ false };

    void start_batch_export(const std::vector<atm::RecipeEntry>& recipes, const atm::ExportSettings& settings);
    void queue_batch_progress(const atm::BatchProgress& progress);
    void drain_progress_queue();

private:
    atm::LibraryHandler handler_;
    atm::LibraryCommandHandler cmd_handler_;
    SheetRenderer sheet_renderer_;
    ThumbnailCache thumbnail_cache_;
    std::vector<IPanel*> panels_;

    mutable std::mutex log_mutex_;
    std::vector<LogEntry> logs_;

    std::mutex progress_mutex_;
    std::vector<atm::BatchProgress> pending_progress_;

    std::thread export_thread_;
};

} // namespace atm_desktop
