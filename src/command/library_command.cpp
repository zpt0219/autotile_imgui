#include "library_command.h"

namespace atm {

bool LibraryCommand::merge_with(const LibraryCommand* other) {
    (void)other;
    return false;
}

// ---------------- UpdateRecipeColoursCommand ----------------

UpdateRecipeColoursCommand::UpdateRecipeColoursCommand(
    std::string hash, RoleHex new_roles, std::optional<std::vector<std::string>> new_shades, int flag)
    : new_roles_(std::move(new_roles)), new_shades_(std::move(new_shades)) {
    target_hash_ = std::move(hash);
    flag_ = flag;
}

EditorResult UpdateRecipeColoursCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    if (!initialized_) {
        old_roles_ = entry->recipe.roleHex;
        old_shades_ = entry->recipe.customShadesHex;
        initialized_ = true;
    }

    entry->recipe.roleHex = new_roles_;
    entry->recipe.customShadesHex = new_shades_;
    handler.notify_recipe_updated(entry.get(), DIRTY_COLOUR, cb, flag_);
    return EditorResult::Ok();
}

EditorResult UpdateRecipeColoursCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    entry->recipe.roleHex = old_roles_;
    entry->recipe.customShadesHex = old_shades_;
    handler.notify_recipe_updated(entry.get(), DIRTY_COLOUR, cb, 2);
    return EditorResult::Ok();
}

bool UpdateRecipeColoursCommand::merge_with(const LibraryCommand* other) {
    if (!other || other->get_kind() != CommandKind::UpdateRecipeColours) return false;
    auto* o = static_cast<const UpdateRecipeColoursCommand*>(other);
    if (o->target_hash_ != target_hash_) return false;
    if (flag_ == 2) return false; // previous already ended

    new_roles_ = o->new_roles_;
    new_shades_ = o->new_shades_;
    flag_ = o->flag_;
    timestamp_ = o->timestamp_;
    return true;
}

// ---------------- UpdateRecipePatternCommand ----------------

UpdateRecipePatternCommand::UpdateRecipePatternCommand(
    std::string hash, std::string pattern_id, int edge_seed, int outline_width, int flag)
    : new_pattern_id_(std::move(pattern_id)), new_edge_seed_(edge_seed), new_outline_width_(outline_width) {
    target_hash_ = std::move(hash);
    flag_ = flag;
}

EditorResult UpdateRecipePatternCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    if (!initialized_) {
        old_pattern_id_ = entry->recipe.patternId;
        old_edge_seed_ = entry->recipe.edgeSeed;
        old_outline_width_ = entry->recipe.outlineWidth;
        initialized_ = true;
    }

    entry->recipe.patternId = new_pattern_id_;
    entry->recipe.edgeSeed = new_edge_seed_;
    entry->recipe.outlineWidth = new_outline_width_;
    handler.notify_recipe_updated(entry.get(), DIRTY_SILHOUETTE, cb, flag_);
    return EditorResult::Ok();
}

EditorResult UpdateRecipePatternCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    entry->recipe.patternId = old_pattern_id_;
    entry->recipe.edgeSeed = old_edge_seed_;
    entry->recipe.outlineWidth = old_outline_width_;
    handler.notify_recipe_updated(entry.get(), DIRTY_SILHOUETTE, cb, 2);
    return EditorResult::Ok();
}

bool UpdateRecipePatternCommand::merge_with(const LibraryCommand* other) {
    if (!other || other->get_kind() != CommandKind::UpdateRecipePattern) return false;
    auto* o = static_cast<const UpdateRecipePatternCommand*>(other);
    if (o->target_hash_ != target_hash_) return false;
    if (flag_ == 2) return false;

    new_pattern_id_ = o->new_pattern_id_;
    new_edge_seed_ = o->new_edge_seed_;
    new_outline_width_ = o->new_outline_width_;
    flag_ = o->flag_;
    timestamp_ = o->timestamp_;
    return true;
}

// ---------------- UpdateRecipeBandCommand ----------------

UpdateRecipeBandCommand::UpdateRecipeBandCommand(
    std::string hash, int band_steps, bool hard_edge_b, bool transparent_b, double band_bias, int flag)
    : new_band_steps_(band_steps), new_hard_edge_b_(hard_edge_b), new_transparent_b_(transparent_b), new_band_bias_(band_bias) {
    target_hash_ = std::move(hash);
    flag_ = flag;
}

EditorResult UpdateRecipeBandCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    if (!initialized_) {
        old_band_steps_ = entry->recipe.bandSteps;
        old_hard_edge_b_ = entry->recipe.hardEdgeB;
        old_transparent_b_ = entry->recipe.transparentB;
        old_band_bias_ = entry->recipe.bandBias;
        initialized_ = true;
    }

    entry->recipe.bandSteps = new_band_steps_;
    entry->recipe.hardEdgeB = new_hard_edge_b_;
    entry->recipe.transparentB = new_transparent_b_;
    entry->recipe.bandBias = new_band_bias_;
    handler.notify_recipe_updated(entry.get(), DIRTY_SILHOUETTE, cb, flag_);
    return EditorResult::Ok();
}

EditorResult UpdateRecipeBandCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    entry->recipe.bandSteps = old_band_steps_;
    entry->recipe.hardEdgeB = old_hard_edge_b_;
    entry->recipe.transparentB = old_transparent_b_;
    entry->recipe.bandBias = old_band_bias_;
    handler.notify_recipe_updated(entry.get(), DIRTY_SILHOUETTE, cb, 2);
    return EditorResult::Ok();
}

bool UpdateRecipeBandCommand::merge_with(const LibraryCommand* other) {
    if (!other || other->get_kind() != CommandKind::UpdateRecipeBand) return false;
    auto* o = static_cast<const UpdateRecipeBandCommand*>(other);
    if (o->target_hash_ != target_hash_) return false;
    if (flag_ == 2) return false;

    new_band_steps_ = o->new_band_steps_;
    new_hard_edge_b_ = o->new_hard_edge_b_;
    new_transparent_b_ = o->new_transparent_b_;
    new_band_bias_ = o->new_band_bias_;
    flag_ = o->flag_;
    timestamp_ = o->timestamp_;
    return true;
}

// ---------------- UpdateRecipeNoiseCommand ----------------

UpdateRecipeNoiseCommand::UpdateRecipeNoiseCommand(
    std::string hash, std::vector<NoiseId> noises, int seed, double strength, int flag)
    : new_noises_(std::move(noises)), new_seed_(seed), new_strength_(strength) {
    target_hash_ = std::move(hash);
    flag_ = flag;
}

EditorResult UpdateRecipeNoiseCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    if (!initialized_) {
        old_noises_ = entry->recipe.patternNoise;
        old_seed_ = entry->recipe.patternNoiseSeed;
        old_strength_ = entry->recipe.patternNoiseStrength;
        initialized_ = true;
    }

    entry->recipe.patternNoise = new_noises_;
    entry->recipe.patternNoiseSeed = new_seed_;
    entry->recipe.patternNoiseStrength = new_strength_;
    handler.notify_recipe_updated(entry.get(), DIRTY_NOISE, cb, flag_);
    return EditorResult::Ok();
}

EditorResult UpdateRecipeNoiseCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    entry->recipe.patternNoise = old_noises_;
    entry->recipe.patternNoiseSeed = old_seed_;
    entry->recipe.patternNoiseStrength = old_strength_;
    handler.notify_recipe_updated(entry.get(), DIRTY_NOISE, cb, 2);
    return EditorResult::Ok();
}

bool UpdateRecipeNoiseCommand::merge_with(const LibraryCommand* other) {
    if (!other || other->get_kind() != CommandKind::UpdateRecipeNoise) return false;
    auto* o = static_cast<const UpdateRecipeNoiseCommand*>(other);
    if (o->target_hash_ != target_hash_) return false;
    if (flag_ == 2) return false;

    new_noises_ = o->new_noises_;
    new_seed_ = o->new_seed_;
    new_strength_ = o->new_strength_;
    flag_ = o->flag_;
    timestamp_ = o->timestamp_;
    return true;
}

// ---------------- UpdateRecipeRibbonCommand ----------------

UpdateRecipeRibbonCommand::UpdateRecipeRibbonCommand(
    std::string hash,
    std::string algo,
    double amount,
    int period,
    int shades,
    bool invert,
    std::optional<std::vector<std::optional<std::string>>> custom_hex,
    int flag
) : new_algo_(std::move(algo)), new_amount_(amount), new_period_(period), new_shades_(shades),
    new_invert_(invert), new_custom_hex_(std::move(custom_hex)) {
    target_hash_ = std::move(hash);
    flag_ = flag;
}

EditorResult UpdateRecipeRibbonCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    if (!initialized_) {
        old_algo_ = entry->recipe.ribbonAlgo;
        old_amount_ = entry->recipe.ribbonAmount;
        old_period_ = entry->recipe.ribbonPeriod;
        old_shades_ = entry->recipe.ribbonShades;
        old_invert_ = entry->recipe.ribbonInvert;
        old_custom_hex_ = entry->recipe.customRibbonHex;
        initialized_ = true;
    }

    entry->recipe.ribbonAlgo = new_algo_;
    entry->recipe.ribbonAmount = new_amount_;
    entry->recipe.ribbonPeriod = new_period_;
    entry->recipe.ribbonShades = new_shades_;
    entry->recipe.ribbonInvert = new_invert_;
    entry->recipe.customRibbonHex = new_custom_hex_;
    handler.notify_recipe_updated(entry.get(), DIRTY_RIBBON, cb, flag_);
    return EditorResult::Ok();
}

EditorResult UpdateRecipeRibbonCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    entry->recipe.ribbonAlgo = old_algo_;
    entry->recipe.ribbonAmount = old_amount_;
    entry->recipe.ribbonPeriod = old_period_;
    entry->recipe.ribbonShades = old_shades_;
    entry->recipe.ribbonInvert = old_invert_;
    entry->recipe.customRibbonHex = old_custom_hex_;
    handler.notify_recipe_updated(entry.get(), DIRTY_RIBBON, cb, 2);
    return EditorResult::Ok();
}

bool UpdateRecipeRibbonCommand::merge_with(const LibraryCommand* other) {
    if (!other || other->get_kind() != CommandKind::UpdateRecipeRibbon) return false;
    auto* o = static_cast<const UpdateRecipeRibbonCommand*>(other);
    if (o->target_hash_ != target_hash_) return false;
    if (flag_ == 2) return false;

    new_algo_ = o->new_algo_;
    new_amount_ = o->new_amount_;
    new_period_ = o->new_period_;
    new_shades_ = o->new_shades_;
    new_invert_ = o->new_invert_;
    new_custom_hex_ = o->new_custom_hex_;
    flag_ = o->flag_;
    timestamp_ = o->timestamp_;
    return true;
}

// ---------------- UpdateRecipeTextureCommand ----------------

UpdateRecipeTextureCommand::UpdateRecipeTextureCommand(std::string hash, const Recipe& new_state, int flag)
    : new_recipe_(new_state) {
    target_hash_ = std::move(hash);
    flag_ = flag;
}

EditorResult UpdateRecipeTextureCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    if (!initialized_) {
        old_recipe_ = entry->recipe;
        initialized_ = true;
    }

    entry->recipe.textureAlgoA = new_recipe_.textureAlgoA;
    entry->recipe.textureAlgoB = new_recipe_.textureAlgoB;
    entry->recipe.textureAmountA = new_recipe_.textureAmountA;
    entry->recipe.textureAmountB = new_recipe_.textureAmountB;
    entry->recipe.textureShadesA = new_recipe_.textureShadesA;
    entry->recipe.textureShadesB = new_recipe_.textureShadesB;
    entry->recipe.textureSeedA = new_recipe_.textureSeedA;
    entry->recipe.textureSeedB = new_recipe_.textureSeedB;
    entry->recipe.cellScaleA = new_recipe_.cellScaleA;
    entry->recipe.cellScaleB = new_recipe_.cellScaleB;
    entry->recipe.rippleScaleA = new_recipe_.rippleScaleA;
    entry->recipe.rippleScaleB = new_recipe_.rippleScaleB;
    entry->recipe.geoScaleA = new_recipe_.geoScaleA;
    entry->recipe.geoScaleB = new_recipe_.geoScaleB;
    entry->recipe.customTexHexA = new_recipe_.customTexHexA;
    entry->recipe.customTexHexB = new_recipe_.customTexHexB;

    handler.notify_recipe_updated(entry.get(), static_cast<DirtyMask>(DIRTY_TEXTURE_A | DIRTY_TEXTURE_B), cb, flag_);
    return EditorResult::Ok();
}

EditorResult UpdateRecipeTextureCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    auto entry = handler.library()->find_by_hash(target_hash_);
    if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);

    entry->recipe = old_recipe_;
    handler.notify_recipe_updated(entry.get(), static_cast<DirtyMask>(DIRTY_TEXTURE_A | DIRTY_TEXTURE_B), cb, 2);
    return EditorResult::Ok();
}

bool UpdateRecipeTextureCommand::merge_with(const LibraryCommand* other) {
    if (!other || other->get_kind() != CommandKind::UpdateRecipeTexture) return false;
    auto* o = static_cast<const UpdateRecipeTextureCommand*>(other);
    if (o->target_hash_ != target_hash_) return false;
    if (flag_ == 2) return false;

    new_recipe_ = o->new_recipe_;
    flag_ = o->flag_;
    timestamp_ = o->timestamp_;
    return true;
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
    if (!other || other->get_kind() != CommandKind::RenameRecipe) return false;
    auto* o = static_cast<const RenameRecipeCommand*>(other);
    if (o->target_hash_ != target_hash_) return false;
    if (flag_ == 2) return false;

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
