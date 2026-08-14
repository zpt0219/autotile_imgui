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

void LibraryHandler::set_library(std::unique_ptr<RecipeLibrary> lib, LibraryCallbacks* cb, int flag) {
    library_ = std::move(lib);
    selected_ = (library_ && !library_->entries().empty()) ? library_->entries().front().get() : nullptr;
    multi_selected_hashes_.clear();
    if (selected_) {
        multi_selected_hashes_.push_back(selected_->hash);
    }
    if (cb) {
        cb->onLibraryLoaded(flag);
        cb->onRecipeSelected(selected_, flag);
    }
}

void LibraryHandler::select_recipe(const std::string& hash, LibraryCallbacks* cb, int flag) {
    if (!library_) {
        selected_ = nullptr;
        multi_selected_hashes_.clear();
        if (cb) cb->onRecipeSelected(nullptr, flag);
        return;
    }
    auto entry = library_->find_by_hash(hash);
    select_recipe(entry.get(), cb, flag);
}

void LibraryHandler::select_recipe(RecipeEntry* entry, LibraryCallbacks* cb, int flag) {
    selected_ = entry;
    multi_selected_hashes_.clear();
    if (selected_) {
        multi_selected_hashes_.push_back(selected_->hash);
    }
    if (cb) {
        cb->onRecipeSelected(selected_, flag);
    }
}

void LibraryHandler::notify_recipe_updated(RecipeEntry* entry, DirtyMask dirty, LibraryCallbacks* cb, int flag) {
    if (cb) {
        cb->onRecipeUpdated(entry, dirty, flag);
    }
}

void LibraryHandler::notify_list_updated(LibraryCallbacks* cb, int flag) {
    if (cb) {
        cb->onLibraryListUpdated(flag);
    }
}

void LibraryHandler::notify_axes_updated(LibraryCallbacks* cb, int flag) {
    if (cb) {
        cb->onVariantAxesUpdated(flag);
    }
}

void LibraryHandler::notify_export_settings_updated(LibraryCallbacks* cb, int flag) {
    if (cb) {
        cb->onExportSettingsUpdated(flag);
    }
}

} // namespace atm
