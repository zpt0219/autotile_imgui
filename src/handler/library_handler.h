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
    void set_library(std::unique_ptr<RecipeLibrary> lib, LibraryCallbacks* cb = nullptr, int flag = 0);

    RecipeEntry* selected_recipe() const { return selected_; }
    void select_recipe(const std::string& hash, LibraryCallbacks* cb = nullptr, int flag = 0);
    void select_recipe(RecipeEntry* entry, LibraryCallbacks* cb = nullptr, int flag = 0);

    const std::vector<std::string>& selected_hashes() const { return multi_selected_hashes_; }
    void set_selected_hashes(const std::vector<std::string>& hashes) { multi_selected_hashes_ = hashes; }

    void notify_recipe_updated(RecipeEntry* entry, DirtyMask dirty, LibraryCallbacks* cb, int flag = 0);
    void notify_list_updated(LibraryCallbacks* cb, int flag = 0);
    void notify_axes_updated(LibraryCallbacks* cb, int flag = 0);
    void notify_export_settings_updated(LibraryCallbacks* cb, int flag = 0);

private:
    std::unique_ptr<RecipeLibrary> library_;
    RecipeEntry* selected_ = nullptr;
    std::vector<std::string> multi_selected_hashes_;
};

} // namespace atm
