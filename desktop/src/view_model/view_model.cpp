#include "view_model.h"
#include "pattern/sheet.h"
#include "util/image.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;

namespace atm_desktop {

static std::string current_time_str() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

ViewModel::ViewModel() {
    log("AutoTile Mixer initialized", "INFO");
    if (handler_.selected_recipe()) {
        sheet_renderer_.update(handler_.selected_recipe()->recipe, atm::PaintOverrides{}, atm::DIRTY_ALL);
    }
}

ViewModel::~ViewModel() {
    if (export_thread_.joinable()) {
        cancel_export_requested = true;
        export_thread_.join();
    }
}

void ViewModel::register_panel(IPanel* panel) {
    if (!panel) return;
    for (auto* p : panels_) {
        if (p == panel) return;
    }
    panels_.push_back(panel);
}

void ViewModel::unregister_panel(IPanel* panel) {
    for (auto it = panels_.begin(); it != panels_.end(); ++it) {
        if (*it == panel) {
            panels_.erase(it);
            return;
        }
    }
}

atm::EditorResult ViewModel::execute_command(std::unique_ptr<atm::LibraryCommand> command) {
    return cmd_handler_.add_and_execute_command(std::move(command), handler_, this);
}

atm::EditorResult ViewModel::undo() {
    auto res = cmd_handler_.undo(handler_, this);
    if (res.success) {
        log("Undo successful", "INFO");
    } else {
        log("Undo failed: " + res.error_message, "WARN");
    }
    return res;
}

atm::EditorResult ViewModel::redo() {
    auto res = cmd_handler_.redo(handler_, this);
    if (res.success) {
        log("Redo successful", "INFO");
    } else {
        log("Redo failed: " + res.error_message, "WARN");
    }
    return res;
}

void ViewModel::onLibraryLoaded(int flag) {
    thumbnail_cache_.clear();
    auto panels_snapshot = panels_;
    for (auto* p : panels_snapshot) {
        p->onLibraryLoaded(flag);
    }
    if (handler_.selected_recipe()) {
        sheet_renderer_.update(handler_.selected_recipe()->recipe, atm::PaintOverrides{}, atm::DIRTY_ALL);
    }
    log("Library loaded", "INFO");
}

void ViewModel::onLibraryListUpdated(int flag) {
    auto panels_snapshot = panels_;
    for (auto* p : panels_snapshot) {
        p->onLibraryListUpdated(flag);
    }
}

void ViewModel::onRecipeSelected(atm::RecipeEntry* entry, int flag) {
    auto panels_snapshot = panels_;
    for (auto* p : panels_snapshot) {
        p->onRecipeSelected(entry, flag);
    }
    if (entry) {
        sheet_renderer_.update(entry->recipe, atm::PaintOverrides{}, atm::DIRTY_ALL);
    }
}

void ViewModel::onRecipeUpdated(atm::RecipeEntry* entry, atm::DirtyMask dirty, int flag) {
    if (entry) {
        thumbnail_cache_.invalidate(entry->hash);
    }
    auto panels_snapshot = panels_;
    for (auto* p : panels_snapshot) {
        p->onRecipeUpdated(entry, dirty, flag);
    }
    if (entry && handler_.selected_recipe() && entry->hash == handler_.selected_recipe()->hash) {
        sheet_renderer_.update(entry->recipe, atm::PaintOverrides{}, dirty);
    }
}

void ViewModel::onVariantAxesUpdated(int flag) {
    auto panels_snapshot = panels_;
    for (auto* p : panels_snapshot) {
        p->onVariantAxesUpdated(flag);
    }
}

// The LibraryCallbacks entry point, which any thread may reach. It only queues;
// drain_progress_queue() does the real work on the main thread. Doing the work
// here as well would reintroduce the race the queue exists to remove, so this
// deliberately stays a one-liner rather than a second copy of the fan-out.
void ViewModel::onBatchProgress(const atm::BatchProgress& progress, int flag) {
    (void)flag;
    queue_batch_progress(progress);
}

void ViewModel::onExportSettingsUpdated(int flag) {
    auto panels_snapshot = panels_;
    for (auto* p : panels_snapshot) {
        p->onExportSettingsUpdated(flag);
    }
}

void ViewModel::queue_batch_progress(const atm::BatchProgress& progress) {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    pending_progress_.push_back(progress);
}

void ViewModel::drain_progress_queue() {
    std::vector<atm::BatchProgress> drained;
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        if (pending_progress_.empty()) return;
        drained.swap(pending_progress_);
    }

    for (const auto& progress : drained) {
        current_export_progress = progress;
        if (progress.finished) {
            is_exporting = false;
            log("Batch export finished: " + std::to_string(progress.total) + " sheets", "INFO");
        } else if (progress.cancelled) {
            is_exporting = false;
            log("Batch export cancelled", "WARN");
        }
        auto panels_snapshot = panels_;
        for (auto* p : panels_snapshot) {
            p->onBatchProgress(progress, 0);
        }
    }
}

void ViewModel::log(const std::string& msg, const std::string& level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    logs_.push_back({ msg, level, current_time_str() });
    if (logs_.size() > 500) {
        logs_.erase(logs_.begin());
    }
}

std::vector<LogEntry> ViewModel::get_logs() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return logs_;
}

void ViewModel::clear_logs() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    logs_.clear();
}

static std::string sanitize_filename(std::string name) {
    for (char& c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return name;
}

static std::string format_template(const std::string& tmpl, const atm::RecipeEntry& entry) {
    std::string out = tmpl;
    auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    };
    replace_all(out, "{name}", entry.name);
    replace_all(out, "{pattern}", entry.recipe.patternId);
    replace_all(out, "{texA}", entry.recipe.textureAlgoA);
    replace_all(out, "{texB}", entry.recipe.textureAlgoB);
    replace_all(out, "{ribbon}", entry.recipe.ribbonAlgo);
    return sanitize_filename(out);
}

void ViewModel::start_batch_export(const std::vector<atm::RecipeEntry>& recipes, const atm::ExportSettings& settings) {
    if (is_exporting) return;
    if (recipes.empty()) {
        log("No recipes to export", "WARN");
        return;
    }

    if (export_thread_.joinable()) {
        export_thread_.join();
    }

    is_exporting = true;
    cancel_export_requested = false;
    current_export_progress = atm::BatchProgress{ 0, static_cast<int>(recipes.size()), "", false, false, "" };

    log("Starting batch export to " + settings.out_dir, "INFO");

    export_thread_ = std::thread([this, recipes, settings]() {
        try {
            fs::create_directories(settings.out_dir);
        } catch (const std::exception& e) {
            atm::BatchProgress prog;
            prog.finished = true;
            prog.error_message = e.what();
            queue_batch_progress(prog);
            return;
        }

        int count = static_cast<int>(recipes.size());
        for (int i = 0; i < count; ++i) {
            if (cancel_export_requested) {
                atm::BatchProgress prog{ i, count, "", false, true, "Cancelled by user" };
                queue_batch_progress(prog);
                return;
            }

            const auto& item = recipes[i];
            std::string filename_base = format_template(settings.name_template, item);
            if (filename_base.empty()) filename_base = "sheet_" + std::to_string(i);

            // Render sheet
            auto rgba = atm::render_sheet_rgba(item.recipe);

            // Export PNG
            if (settings.export_png) {
                std::string png_path = (fs::path(settings.out_dir) / (filename_base + ".png")).string();
                atm::write_png(png_path, 256, 192, rgba.data());
            }

            // Export JSON Sidecar
            if (settings.export_json_sidecar) {
                std::string json_path = (fs::path(settings.out_dir) / (filename_base + ".recipe.json")).string();
                std::ofstream f(json_path);
                if (f.is_open()) {
                    nlohmann::json sidecar;
                    sidecar["name"] = item.name;
                    sidecar["hash"] = item.hash;
                    sidecar["recipe"] = recipe_to_json(item.recipe);
                    sidecar["tags"] = item.tags;
                    f << sidecar.dump(2);
                }
            }

            atm::BatchProgress prog{ i + 1, count, item.name, false, false, "" };
            queue_batch_progress(prog);
        }

        atm::BatchProgress finish_prog{ count, count, "Completed", true, false, "" };
        queue_batch_progress(finish_prog);
    });
}

} // namespace atm_desktop
