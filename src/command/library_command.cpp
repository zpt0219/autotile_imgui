#include "library_command.h"

namespace atm {

bool LibraryCommand::merge_with(const LibraryCommand* other) {
    (void)other;
    return false;
}

// ---------------- Helpers for Recipe mutation commands ----------------

template <typename Cmd>
static const Cmd* cast_merge_target(const LibraryCommand* self, const LibraryCommand* other, CommandKind kind) {
    if (!other || other->get_kind() != kind) return nullptr;
    auto* o = static_cast<const Cmd*>(other);
    if (o->get_target_hash() != self->get_target_hash()) return nullptr;
    if (self->get_flag() == 2) return nullptr;
    return o;
}

// Run a field edit forward: snapshot the old values once, write the new ones,
// then (optionally) re-sync any colour-override array whose length this edit
// governs. See the invariant block in model/recipe.h.
//
// `sync` belongs to the forward path only. `undo_recipe_mutation` deliberately
// has no equivalent: undo restores the snapshot verbatim, because re-syncing on
// the way back would rebuild trimmed levels from computed colours and quietly
// lose the user's. That was a real bug once — see commit e790a50.
template <typename InitFn, typename ApplyFn, typename SyncFn = std::nullptr_t>
static EditorResult execute_recipe_mutation(
    LibraryHandler& handler,
    const std::string& hash,
    bool& initialized,
    InitFn on_init,
    ApplyFn apply,
    DirtyMask dirty,
    LibraryCallbacks* cb,
    int flag,
    SyncFn sync = nullptr
) {
    auto entry = handler.library()->find_by_hash(hash);
    if (!entry) return EditorResult::Error("Recipe not found: " + hash);
    if (!initialized) {
        on_init(entry->recipe);
        initialized = true;
    }
    apply(entry->recipe);
    if constexpr (!std::is_null_pointer_v<SyncFn>) {
        if (sync) sync(entry->recipe);
    }
    handler.notify_recipe_updated(entry.get(), dirty, cb, flag);
    return EditorResult::Ok();
}

template <typename UndoFn>
static EditorResult undo_recipe_mutation(
    LibraryHandler& handler,
    const std::string& hash,
    UndoFn undo_apply,
    DirtyMask dirty,
    LibraryCallbacks* cb
) {
    auto entry = handler.library()->find_by_hash(hash);
    if (!entry) return EditorResult::Error("Recipe not found: " + hash);
    undo_apply(entry->recipe);
    handler.notify_recipe_updated(entry.get(), dirty, cb, 2);
    return EditorResult::Ok();
}

// ---------------- UpdateRecipeColoursCommand ----------------

UpdateRecipeColoursCommand::UpdateRecipeColoursCommand(
    std::string hash, RoleHex new_roles, std::optional<std::vector<std::string>> new_shades, int flag)
    : new_roles_(std::move(new_roles)), new_shades_(std::move(new_shades)) {
    target_hash_ = std::move(hash);
    flag_ = flag;
}

EditorResult UpdateRecipeColoursCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    return execute_recipe_mutation(
        handler, target_hash_, initialized_,
        [&](const Recipe& r) { old_roles_ = r.roleHex; old_shades_ = r.customShadesHex; },
        [&](Recipe& r) { r.roleHex = new_roles_; r.customShadesHex = new_shades_; },
        // Defensive sync: a caller that built the array against a stale
        // bandSteps would otherwise hand over one the sanitiser drops.
        DIRTY_COLOUR, cb, flag_, sync_band_overrides
    );
}

EditorResult UpdateRecipeColoursCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    return undo_recipe_mutation(handler, target_hash_, [&](Recipe& r) {
        r.roleHex = old_roles_;
        r.customShadesHex = old_shades_;
    }, DIRTY_COLOUR, cb);
}

bool UpdateRecipeColoursCommand::merge_with(const LibraryCommand* other) {
    auto* o = cast_merge_target<UpdateRecipeColoursCommand>(this, other, CommandKind::UpdateRecipeColours);
    if (!o) return false;
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
    return execute_recipe_mutation(
        handler, target_hash_, initialized_,
        [&](const Recipe& r) { old_pattern_id_ = r.patternId; old_edge_seed_ = r.edgeSeed; old_outline_width_ = r.outlineWidth; },
        [&](Recipe& r) { r.patternId = new_pattern_id_; r.edgeSeed = new_edge_seed_; r.outlineWidth = new_outline_width_; },
        DIRTY_SILHOUETTE, cb, flag_
    );
}

EditorResult UpdateRecipePatternCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    return undo_recipe_mutation(handler, target_hash_, [&](Recipe& r) {
        r.patternId = old_pattern_id_;
        r.edgeSeed = old_edge_seed_;
        r.outlineWidth = old_outline_width_;
    }, DIRTY_SILHOUETTE, cb);
}

bool UpdateRecipePatternCommand::merge_with(const LibraryCommand* other) {
    auto* o = cast_merge_target<UpdateRecipePatternCommand>(this, other, CommandKind::UpdateRecipePattern);
    if (!o) return false;
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
    return execute_recipe_mutation(
        handler, target_hash_, initialized_,
        [&](const Recipe& r) {
            old_band_steps_ = r.bandSteps;
            old_hard_edge_b_ = r.hardEdgeB;
            old_transparent_b_ = r.transparentB;
            old_band_bias_ = r.bandBias;
            // bandSteps owns the length of customShadesHex, so the resize the
            // sync below performs is part of this edit and must be undone with it.
            old_custom_shades_ = r.customShadesHex;
        },
        [&](Recipe& r) {
            r.bandSteps = new_band_steps_;
            r.hardEdgeB = new_hard_edge_b_;
            r.transparentB = new_transparent_b_;
            r.bandBias = new_band_bias_;
        },
        DIRTY_SILHOUETTE, cb, flag_, sync_band_overrides
    );
}

EditorResult UpdateRecipeBandCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    return undo_recipe_mutation(handler, target_hash_, [&](Recipe& r) {
        r.bandSteps = old_band_steps_;
        r.hardEdgeB = old_hard_edge_b_;
        r.transparentB = old_transparent_b_;
        r.bandBias = old_band_bias_;
        // Restore rather than re-sync: growing then shrinking would otherwise
        // leave the trimmed levels holding computed colours instead of the user's.
        r.customShadesHex = old_custom_shades_;
    }, DIRTY_SILHOUETTE, cb);
}

bool UpdateRecipeBandCommand::merge_with(const LibraryCommand* other) {
    auto* o = cast_merge_target<UpdateRecipeBandCommand>(this, other, CommandKind::UpdateRecipeBand);
    if (!o) return false;
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
    return execute_recipe_mutation(
        handler, target_hash_, initialized_,
        [&](const Recipe& r) { old_noises_ = r.patternNoise; old_seed_ = r.patternNoiseSeed; old_strength_ = r.patternNoiseStrength; },
        [&](Recipe& r) { r.patternNoise = new_noises_; r.patternNoiseSeed = new_seed_; r.patternNoiseStrength = new_strength_; },
        DIRTY_NOISE, cb, flag_
    );
}

EditorResult UpdateRecipeNoiseCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    return undo_recipe_mutation(handler, target_hash_, [&](Recipe& r) {
        r.patternNoise = old_noises_;
        r.patternNoiseSeed = old_seed_;
        r.patternNoiseStrength = old_strength_;
    }, DIRTY_NOISE, cb);
}

bool UpdateRecipeNoiseCommand::merge_with(const LibraryCommand* other) {
    auto* o = cast_merge_target<UpdateRecipeNoiseCommand>(this, other, CommandKind::UpdateRecipeNoise);
    if (!o) return false;
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
    return execute_recipe_mutation(
        handler, target_hash_, initialized_,
        [&](const Recipe& r) {
            old_algo_ = r.ribbonAlgo;
            old_amount_ = r.ribbonAmount;
            old_period_ = r.ribbonPeriod;
            old_shades_ = r.ribbonShades;
            old_invert_ = r.ribbonInvert;
            old_custom_hex_ = r.customRibbonHex;
        },
        [&](Recipe& r) {
            r.ribbonAlgo = new_algo_;
            r.ribbonAmount = new_amount_;
            r.ribbonPeriod = new_period_;
            r.ribbonShades = new_shades_;
            r.ribbonInvert = new_invert_;
            r.customRibbonHex = new_custom_hex_;
        },
        // ribbonShades owns this array's length. Enforced here rather than at
        // the call sites so a caller passing the previous array cannot invalidate it.
        DIRTY_RIBBON, cb, flag_, sync_ribbon_overrides
    );
}

EditorResult UpdateRecipeRibbonCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    return undo_recipe_mutation(handler, target_hash_, [&](Recipe& r) {
        r.ribbonAlgo = old_algo_;
        r.ribbonAmount = old_amount_;
        r.ribbonPeriod = old_period_;
        r.ribbonShades = old_shades_;
        r.ribbonInvert = old_invert_;
        r.customRibbonHex = old_custom_hex_;
    }, DIRTY_RIBBON, cb);
}

bool UpdateRecipeRibbonCommand::merge_with(const LibraryCommand* other) {
    auto* o = cast_merge_target<UpdateRecipeRibbonCommand>(this, other, CommandKind::UpdateRecipeRibbon);
    if (!o) return false;
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

static void copy_texture_fields(Recipe& dst, const Recipe& src) {
    dst.textureAlgoA = src.textureAlgoA;
    dst.textureAlgoB = src.textureAlgoB;
    dst.textureAmountA = src.textureAmountA;
    dst.textureAmountB = src.textureAmountB;
    dst.textureShadesA = src.textureShadesA;
    dst.textureShadesB = src.textureShadesB;
    dst.textureSeedA = src.textureSeedA;
    dst.textureSeedB = src.textureSeedB;
    dst.cellScaleA = src.cellScaleA;
    dst.cellScaleB = src.cellScaleB;
    dst.rippleScaleA = src.rippleScaleA;
    dst.rippleScaleB = src.rippleScaleB;
    dst.geoScaleA = src.geoScaleA;
    dst.geoScaleB = src.geoScaleB;
    dst.customTexHexA = src.customTexHexA;
    dst.customTexHexB = src.customTexHexB;
}

UpdateRecipeTextureCommand::UpdateRecipeTextureCommand(std::string hash, const Recipe& new_state, int flag)
    : new_recipe_(new_state) {
    target_hash_ = std::move(hash);
    flag_ = flag;
}

EditorResult UpdateRecipeTextureCommand::execute(LibraryHandler& handler, LibraryCallbacks* cb) {
    return execute_recipe_mutation(
        handler, target_hash_, initialized_,
        [&](const Recipe& r) { old_recipe_ = r; },
        [&](Recipe& r) { copy_texture_fields(r, new_recipe_); },
        // textureShadesA/B own these arrays' lengths.
        static_cast<DirtyMask>(DIRTY_TEXTURE_A | DIRTY_TEXTURE_B), cb, flag_, sync_texture_overrides
    );
}

EditorResult UpdateRecipeTextureCommand::undo(LibraryHandler& handler, LibraryCallbacks* cb) {
    return undo_recipe_mutation(handler, target_hash_, [&](Recipe& r) {
        copy_texture_fields(r, old_recipe_);
    }, static_cast<DirtyMask>(DIRTY_TEXTURE_A | DIRTY_TEXTURE_B), cb);
}

bool UpdateRecipeTextureCommand::merge_with(const LibraryCommand* other) {
    auto* o = cast_merge_target<UpdateRecipeTextureCommand>(this, other, CommandKind::UpdateRecipeTexture);
    if (!o) return false;
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
