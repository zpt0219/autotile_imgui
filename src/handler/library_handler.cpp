#include "library_handler.h"
#include <algorithm>

namespace atm {

LibraryHandler::LibraryHandler()
    : library_(std::make_unique<RecipeLibrary>()) {
    if (!library_->entries().empty()) {
        selected_ = library_->entries().front().get();
        multi_selected_hashes_.push_back(selected_->hash);
    }
}

LibraryHandler::LibraryHandler(std::unique_ptr<RecipeLibrary> library)
    : library_(std::move(library)) {
    if (library_ && !library_->entries().empty()) {
        selected_ = library_->entries().front().get();
        multi_selected_hashes_.push_back(selected_->hash);
    }
}

void LibraryHandler::set_library(std::unique_ptr<RecipeLibrary> lib, LibraryCallbacks* cb, EditPhase phase) {
    library_ = std::move(lib);
    selected_ = (library_ && !library_->entries().empty()) ? library_->entries().front().get() : nullptr;
    multi_selected_hashes_.clear();
    if (selected_) {
        multi_selected_hashes_.push_back(selected_->hash);
    }
    if (cb) {
        cb->onLibraryLoaded(phase);
        cb->onRecipeSelected(selected_, phase);
    }
}

void LibraryHandler::select_recipe(const std::string& hash, LibraryCallbacks* cb, EditPhase phase) {
    if (!library_) {
        selected_ = nullptr;
        multi_selected_hashes_.clear();
        if (cb) cb->onRecipeSelected(nullptr, phase);
        return;
    }
    auto entry = library_->find_by_hash(hash);
    select_recipe(entry.get(), cb, phase);
}

void LibraryHandler::select_recipe(RecipeEntry* entry, LibraryCallbacks* cb, EditPhase phase) {
    selected_ = entry;
    multi_selected_hashes_.clear();
    if (selected_) {
        multi_selected_hashes_.push_back(selected_->hash);
    }
    if (cb) {
        cb->onRecipeSelected(selected_, phase);
    }
}

void LibraryHandler::notify_recipe_updated(RecipeEntry* entry, DirtyMask dirty, LibraryCallbacks* cb, EditPhase phase) {
    if (cb) {
        cb->onRecipeUpdated(entry, dirty, phase);
    }
}

void LibraryHandler::notify_list_updated(LibraryCallbacks* cb, EditPhase phase) {
    if (cb) {
        cb->onLibraryListUpdated(phase);
    }
}

void LibraryHandler::notify_axes_updated(LibraryCallbacks* cb, EditPhase phase) {
    if (cb) {
        cb->onVariantAxesUpdated(phase);
    }
}

void LibraryHandler::notify_export_settings_updated(LibraryCallbacks* cb, EditPhase phase) {
    if (cb) {
        cb->onExportSettingsUpdated(phase);
    }
}

} // namespace atm
