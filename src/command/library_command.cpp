#include "library_command.h"

namespace atm {

bool LibraryCommand::merge_with(const LibraryCommand* other) {
    (void)other;
    return false;
}

// Shared by the two commands that are not RecipeFieldCommand<State> but still
// coalesce successive edits (rename, export settings).
template <typename Cmd>
static const Cmd* cast_merge_target(const LibraryCommand* self, const LibraryCommand* other, CommandKind kind) {
    if (!other || other->get_kind() != kind) return nullptr;
    auto* o = static_cast<const Cmd*>(other);
    if (o->get_target_hash() != self->get_target_hash()) return nullptr;
    if (self->get_flag() == 2) return nullptr;
    return o;
}

// The six field commands are all RecipeFieldCommand<State> now: the base does
// execute / undo / merge_with, the header says which fields each owns, and all
// that is left here is stashing the incoming values into `new_`.

UpdateRecipeColoursCommand::UpdateRecipeColoursCommand(
    std::string hash, RoleHex new_roles, std::optional<std::vector<std::string>> new_shades, int flag) {
    new_ = { std::move(new_roles), std::move(new_shades) };
    target_hash_ = std::move(hash);
    flag_ = flag;
}

UpdateRecipePatternCommand::UpdateRecipePatternCommand(
    std::string hash, std::string pattern_id, int edge_seed, int outline_width, int flag) {
    new_ = { std::move(pattern_id), edge_seed, outline_width };
    target_hash_ = std::move(hash);
    flag_ = flag;
}

UpdateRecipeBandCommand::UpdateRecipeBandCommand(
    std::string hash, int band_steps, bool hard_edge_b, bool transparent_b, double band_bias, int flag) {
    // custom_shades is left empty: the forward write copies the array that is
    // already on the recipe, then sync() resizes it to the new step count.
    new_ = { band_steps, hard_edge_b, transparent_b, band_bias, std::nullopt };
    target_hash_ = std::move(hash);
    flag_ = flag;
}

UpdateRecipeNoiseCommand::UpdateRecipeNoiseCommand(
    std::string hash, std::vector<NoiseId> noises, int seed, double strength, int flag) {
    new_ = { std::move(noises), seed, strength };
    target_hash_ = std::move(hash);
    flag_ = flag;
}

UpdateRecipeRibbonCommand::UpdateRecipeRibbonCommand(
    std::string hash,
    std::string algo,
    double amount,
    int period,
    int shades,
    bool invert,
    std::optional<std::vector<std::optional<std::string>>> custom_hex,
    int flag
) {
    new_ = { std::move(algo), amount, period, shades, invert, std::move(custom_hex) };
    target_hash_ = std::move(hash);
    flag_ = flag;
}

UpdateRecipeTextureCommand::UpdateRecipeTextureCommand(std::string hash, const Recipe& new_state, int flag) {
    new_ = read(new_state);
    target_hash_ = std::move(hash);
    flag_ = flag;
}

// ---------------- AddRecipeCommand ----------------

AddRecipeCommand::AddRecipeCommand(Recipe recipe, std::string name)
    : recipe_(std::move(recipe)), name_(std::move(name)) {}

EditorResult AddRecipeCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (handler.selected_recipe()) {
        previous_selected_hash_ = handler.selected_recipe()->hash;
    }
    auto entry = handler.library()->add_recipe(recipe_, name_, created_hash_);
    created_hash_ = entry->hash;
    handler.select_recipe(entry.get(), cb, 0);
    handler.notify_list_updated(cb, 0);
    return EditorResult::Ok();
}

EditorResult AddRecipeCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    handler.library()->remove_recipe(created_hash_);
    if (!previous_selected_hash_.empty()) {
        handler.select_recipe(previous_selected_hash_, cb, 0);
    } else if (!handler.library()->entries().empty()) {
        handler.select_recipe(handler.library()->entries().front().get(), cb, 0);
    }
    handler.notify_list_updated(cb, 0);
    return EditorResult::Ok();
}

// ---------------- RemoveRecipeCommand ----------------

RemoveRecipeCommand::RemoveRecipeCommand(std::string hash) {
    target_hash_ = std::move(hash);
}

EditorResult RemoveRecipeCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (handler.selected_recipe()) {
        previous_selected_hash_ = handler.selected_recipe()->hash;
    }
    removed_index_ = handler.library()->index_of_hash(target_hash_);
    if (removed_index_ < 0) return EditorResult::Error("Recipe not found: " + target_hash_);

    removed_entry_ = handler.library()->entries()[removed_index_];
    handler.library()->remove_recipe(target_hash_);

    if (handler.selected_recipe() && handler.selected_recipe()->hash == target_hash_) {
        if (!handler.library()->entries().empty()) {
            int new_idx = std::min(removed_index_, static_cast<int>(handler.library()->entries().size()) - 1);
            handler.select_recipe(handler.library()->entries()[new_idx].get(), cb, 0);
        } else {
            handler.select_recipe(static_cast<RecipeEntry*>(nullptr), cb, 0);
        }
    }
    handler.notify_list_updated(cb, 0);
    return EditorResult::Ok();
}

EditorResult RemoveRecipeCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (!removed_entry_) return EditorResult::Error("No removed entry to restore");

    int insert_idx = std::min(removed_index_, static_cast<int>(handler.library()->entries().size()));
    handler.library()->entries().insert(handler.library()->entries().begin() + insert_idx, removed_entry_);

    if (!previous_selected_hash_.empty()) {
        handler.select_recipe(previous_selected_hash_, cb, 0);
    }
    handler.notify_list_updated(cb, 0);
    return EditorResult::Ok();
}

// ---------------- DuplicateRecipeCommand ----------------

DuplicateRecipeCommand::DuplicateRecipeCommand(std::string hash)
    : source_hash_(std::move(hash)) {}

EditorResult DuplicateRecipeCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (handler.selected_recipe()) {
        previous_selected_hash_ = handler.selected_recipe()->hash;
    }
    auto dup = handler.library()->duplicate_recipe(source_hash_, created_hash_);
    if (!dup) return EditorResult::Error("Could not duplicate: " + source_hash_);

    created_hash_ = dup->hash;
    handler.select_recipe(dup.get(), cb, 0);
    handler.notify_list_updated(cb, 0);
    return EditorResult::Ok();
}

EditorResult DuplicateRecipeCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    handler.library()->remove_recipe(created_hash_);
    if (!previous_selected_hash_.empty()) {
        handler.select_recipe(previous_selected_hash_, cb, 0);
    }
    handler.notify_list_updated(cb, 0);
    return EditorResult::Ok();
}

// ---------------- RenameRecipeCommand ----------------

RenameRecipeCommand::RenameRecipeCommand(std::string hash, std::string new_name, int flag)
    : new_name_(std::move(new_name)) {
    target_hash_ = std::move(hash);
    flag_ = flag;
}

EditorResult RenameRecipeCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    if (!initialized_) {
        old_name_ = entry->name;
        initialized_ = true;
    }

    entry->name = new_name_;
    handler.notify_list_updated(cb, flag_);
    return EditorResult::Ok();
}

EditorResult RenameRecipeCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    entry->name = old_name_;
    handler.notify_list_updated(cb, 2);
    return EditorResult::Ok();
}

bool RenameRecipeCommand::merge_with(const LibraryCommand* other) {
    auto* o = cast_merge_target<RenameRecipeCommand>(this, other, CommandKind::RenameRecipe);
    if (!o) return false;
    new_name_ = o->new_name_;
    flag_ = o->flag_;
    timestamp_ = o->timestamp_;
    return true;
}

// ---------------- ReorderRecipeCommand ----------------

ReorderRecipeCommand::ReorderRecipeCommand(int from_index, int to_index)
    : from_index_(from_index), to_index_(to_index) {}

EditorResult ReorderRecipeCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (!handler.library()->reorder_recipe(from_index_, to_index_)) {
        return EditorResult::Error("Reorder indices invalid");
    }
    handler.notify_list_updated(cb, 0);
    return EditorResult::Ok();
}

EditorResult ReorderRecipeCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (!handler.library()->reorder_recipe(to_index_, from_index_)) {
        return EditorResult::Error("Undo reorder indices invalid");
    }
    handler.notify_list_updated(cb, 0);
    return EditorResult::Ok();
}

// ---------------- SelectRecipeCommand ----------------

SelectRecipeCommand::SelectRecipeCommand(std::string target_hash)
    : new_hash_(std::move(target_hash)) {}

EditorResult SelectRecipeCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (!initialized_) {
        old_hash_ = handler.selected_recipe() ? handler.selected_recipe()->hash : "";
        initialized_ = true;
    }
    handler.select_recipe(new_hash_, cb, 0);
    return EditorResult::Ok();
}

EditorResult SelectRecipeCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    handler.select_recipe(old_hash_, cb, 0);
    return EditorResult::Ok();
}

// ---------------- AddVariantAxisCommand ----------------

AddVariantAxisCommand::AddVariantAxisCommand(VariantAxis axis)
    : axis_(std::move(axis)) {}

EditorResult AddVariantAxisCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    handler.library()->axes().push_back(axis_);
    handler.notify_axes_updated(cb, 0);
    return EditorResult::Ok();
}

EditorResult AddVariantAxisCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (!handler.library()->axes().empty()) {
        handler.library()->axes().pop_back();
    }
    handler.notify_axes_updated(cb, 0);
    return EditorResult::Ok();
}

// ---------------- RemoveVariantAxisCommand ----------------

RemoveVariantAxisCommand::RemoveVariantAxisCommand(int index)
    : index_(index) {}

EditorResult RemoveVariantAxisCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (index_ < 0 || index_ >= static_cast<int>(handler.library()->axes().size())) {
        return EditorResult::Error("Axis index out of bounds");
    }
    removed_axis_ = handler.library()->axes()[index_];
    handler.library()->axes().erase(handler.library()->axes().begin() + index_);
    handler.notify_axes_updated(cb, 0);
    return EditorResult::Ok();
}

EditorResult RemoveVariantAxisCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    int insert_idx = std::min(index_, static_cast<int>(handler.library()->axes().size()));
    handler.library()->axes().insert(handler.library()->axes().begin() + insert_idx, removed_axis_);
    handler.notify_axes_updated(cb, 0);
    return EditorResult::Ok();
}

// ---------------- UpdateExportSettingsCommand ----------------

UpdateExportSettingsCommand::UpdateExportSettingsCommand(ExportSettings settings, int flag)
    : new_settings_(std::move(settings)) {
    flag_ = flag;
}

EditorResult UpdateExportSettingsCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    if (!initialized_) {
        old_settings_ = handler.library()->export_settings();
        initialized_ = true;
    }
    handler.library()->export_settings() = new_settings_;
    handler.notify_export_settings_updated(cb, flag_);
    return EditorResult::Ok();
}

EditorResult UpdateExportSettingsCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    handler.library()->export_settings() = old_settings_;
    handler.notify_export_settings_updated(cb, 2);
    return EditorResult::Ok();
}

bool UpdateExportSettingsCommand::merge_with(const LibraryCommand* other) {
    if (!other || other->get_kind() != CommandKind::UpdateExportSettings) return false;
    auto* o = static_cast<const UpdateExportSettingsCommand*>(other);
    if (flag_ == 2) return false;

    new_settings_ = o->new_settings_;
    flag_ = o->flag_;
    timestamp_ = o->timestamp_;
    return true;
}

} // namespace atm
