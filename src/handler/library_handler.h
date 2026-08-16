#pragma once

#include "model/recipe_library.h"
#include "command/library_callbacks.h"
#include <memory>
#include <vector>
#include <string>

namespace atm {

class LibraryHandler {
public:
    LibraryHandler();
    explicit LibraryHandler(std::unique_ptr<RecipeLibrary> library);
    ~LibraryHandler() = default;

    RecipeLibrary* library() const { return library_.get(); }
    void set_library(std::unique_ptr<RecipeLibrary> lib, LibraryCallbacks* cb = nullptr, EditPhase phase = EditPhase::Begin);

    RecipeEntry* selected_recipe() const { return selected_; }
    void select_recipe(const std::string& hash, LibraryCallbacks* cb = nullptr, EditPhase phase = EditPhase::Begin);
    void select_recipe(RecipeEntry* entry, LibraryCallbacks* cb = nullptr, EditPhase phase = EditPhase::Begin);

    const std::vector<std::string>& selected_hashes() const { return multi_selected_hashes_; }
    void set_selected_hashes(const std::vector<std::string>& hashes) { multi_selected_hashes_ = hashes; }

    void notify_recipe_updated(RecipeEntry* entry, DirtyMask dirty, LibraryCallbacks* cb, EditPhase phase = EditPhase::Begin);
    void notify_list_updated(LibraryCallbacks* cb, EditPhase phase = EditPhase::Begin);
    void notify_axes_updated(LibraryCallbacks* cb, EditPhase phase = EditPhase::Begin);
    void notify_export_settings_updated(LibraryCallbacks* cb, EditPhase phase = EditPhase::Begin);

private:
    std::unique_ptr<RecipeLibrary> library_;
    RecipeEntry* selected_ = nullptr;
    std::vector<std::string> multi_selected_hashes_;
};

} // namespace atm
