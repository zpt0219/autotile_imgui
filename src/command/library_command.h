#pragma once

#include "model/recipe_library.h"
#include "handler/library_handler.h"
#include "library_callbacks.h"
#include <string>
#include <memory>
#include <chrono>

namespace atm {

struct EditorResult {
    bool success = true;
    std::string error_message;

    static EditorResult Ok() { return { true, "" }; }
    static EditorResult Error(std::string msg) { return { false, std::move(msg) }; }
};

enum class CommandKind {
    UpdateRecipeColours,
    UpdateRecipePattern,
    UpdateRecipeBand,
    UpdateRecipeNoise,
    UpdateRecipeRibbon,
    UpdateRecipeTexture,
    AddRecipe,
    RemoveRecipe,
    DuplicateRecipe,
    RenameRecipe,
    ReorderRecipe,
    SelectRecipe,
    AddVariantAxis,
    UpdateVariantAxis,
    RemoveVariantAxis,
    UpdateExportSettings
};

class LibraryCommand {
public:
    virtual ~LibraryCommand() = default;

    virtual CommandKind get_kind() const = 0;
    virtual EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) = 0;
    virtual EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) = 0;
    virtual EditorResult redo(LibraryHandler& handler, LibraryCallbacks* cb) {
        return execute(handler, cb);
    }

    virtual bool merge_with(const LibraryCommand* other);

    int get_flag() const { return flag_; }
    void set_flag(int flag) { flag_ = flag; }

    int64_t get_timestamp() const { return timestamp_; }
    void set_timestamp(int64_t ts) { timestamp_ = ts; }

    const std::string& get_target_hash() const { return target_hash_; }
    void set_target_hash(std::string hash) { target_hash_ = std::move(hash); }

protected:
    int flag_ = 0; // 0 = begin, 1 = continue, 2 = end
    int64_t timestamp_ = 0;
    std::string target_hash_;
};

// --- Recipe field commands --------------------------------------------------
//
// Every "update one group of recipe fields" command has the same shape:
// snapshot the fields it owns on first execute, write the new values, re-sync
// any colour-override array whose length those fields govern, and undo by
// restoring the snapshot verbatim. A subclass says what State it owns and how
// to read and write it; this base supplies execute / undo / merge_with once, so
// the field list appears twice (read, write) instead of five times.
//
// `sync()` is called on the forward path only. Undo must NOT re-sync: growing
// then shrinking an override array would rebuild the trimmed levels from
// computed colours and quietly lose the user's. That was a real bug once — see
// commit e790a50 and the invariant block in model/recipe.h.
template <class State>
class RecipeFieldCommand : public LibraryCommand {
public:
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override {
        auto entry = handler.library()->find_by_hash(target_hash_);
        if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);
        if (!initialized_) {
            old_ = read(entry->recipe);
            initialized_ = true;
        }
        carry_over(new_, entry->recipe);
        write(entry->recipe, new_);
        sync(entry->recipe);
        handler.notify_recipe_updated(entry.get(), dirty(), cb, flag_);
        return EditorResult::Ok();
    }

    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override {
        auto entry = handler.library()->find_by_hash(target_hash_);
        if (!entry) return EditorResult::Error("Recipe not found: " + target_hash_);
        write(entry->recipe, old_);   // no sync() here, deliberately
        handler.notify_recipe_updated(entry.get(), dirty(), cb, 2);
        return EditorResult::Ok();
    }

    bool merge_with(const LibraryCommand* other) override {
        if (!other || other->get_kind() != get_kind()) return false;
        if (other->get_target_hash() != target_hash_) return false;
        if (flag_ == 2) return false;   // previous already ended
        // Kinds match, so the dynamic type matches too.
        auto* o = static_cast<const RecipeFieldCommand<State>*>(other);
        new_ = o->new_;
        flag_ = o->flag_;
        timestamp_ = o->timestamp_;
        return true;
    }

protected:
    virtual State read(const Recipe& r) const = 0;
    virtual void write(Recipe& r, const State& s) const = 0;
    virtual void sync(Recipe&) const {}

    // Fields the forward path carries over from whatever is on the recipe
    // rather than replacing. They are still part of State so that the snapshot
    // captures them and undo puts them back. Runs on every forward execute, not
    // just the first, so a merged-in command cannot drop them.
    virtual void carry_over(State& next, const Recipe& current) const {
        (void)next;
        (void)current;
    }
    virtual DirtyMask dirty() const = 0;

    State old_{};
    State new_{};
    bool initialized_ = false;
};

// --- Concrete Commands ---

struct ColoursState {
    RoleHex roles;
    std::optional<std::vector<std::string>> shades;
};

class UpdateRecipeColoursCommand : public RecipeFieldCommand<ColoursState> {
public:
    UpdateRecipeColoursCommand(std::string hash, RoleHex new_roles, std::optional<std::vector<std::string>> new_shades, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::UpdateRecipeColours; }

protected:
    ColoursState read(const Recipe& r) const override { return { r.roleHex, r.customShadesHex }; }
    void write(Recipe& r, const ColoursState& s) const override {
        r.roleHex = s.roles;
        r.customShadesHex = s.shades;
    }
    // Defensive: a caller that built the array against a stale bandSteps would
    // otherwise hand over one the sanitiser drops.
    void sync(Recipe& r) const override { sync_band_overrides(r); }
    DirtyMask dirty() const override { return DIRTY_COLOUR; }
};

struct PatternState {
    std::string pattern_id;
    int edge_seed = 0;
    int outline_width = 2;
};

class UpdateRecipePatternCommand : public RecipeFieldCommand<PatternState> {
public:
    UpdateRecipePatternCommand(std::string hash, std::string pattern_id, int edge_seed, int outline_width, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::UpdateRecipePattern; }

protected:
    PatternState read(const Recipe& r) const override { return { r.patternId, r.edgeSeed, r.outlineWidth }; }
    void write(Recipe& r, const PatternState& s) const override {
        r.patternId = s.pattern_id;
        r.edgeSeed = s.edge_seed;
        r.outlineWidth = s.outline_width;
    }
    DirtyMask dirty() const override { return DIRTY_SILHOUETTE; }
};

struct BandState {
    int band_steps = 4;
    bool hard_edge_b = false;
    bool transparent_b = false;
    double band_bias = 0.0;
    // bandSteps owns this array's length, so the resize sync() performs is part
    // of the edit and travels in the snapshot with it.
    std::optional<std::vector<std::string>> custom_shades;
};

class UpdateRecipeBandCommand : public RecipeFieldCommand<BandState> {
public:
    UpdateRecipeBandCommand(std::string hash, int band_steps, bool hard_edge_b, bool transparent_b, double band_bias, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::UpdateRecipeBand; }

protected:
    BandState read(const Recipe& r) const override {
        return { r.bandSteps, r.hardEdgeB, r.transparentB, r.bandBias, r.customShadesHex };
    }
    void write(Recipe& r, const BandState& s) const override {
        r.bandSteps = s.band_steps;
        r.hardEdgeB = s.hard_edge_b;
        r.transparentB = s.transparent_b;
        r.bandBias = s.band_bias;
        r.customShadesHex = s.custom_shades;
    }
    // Changing the step count must not discard the user's shades — it resizes
    // them. So the forward path keeps whatever array is on the recipe and lets
    // sync() adjust its length; only undo puts a different array back.
    void carry_over(BandState& next, const Recipe& current) const override {
        next.custom_shades = current.customShadesHex;
    }
    void sync(Recipe& r) const override { sync_band_overrides(r); }
    DirtyMask dirty() const override { return DIRTY_SILHOUETTE; }
};

struct NoiseState {
    std::vector<NoiseId> noises;
    int seed = 0;
    double strength = 0.0;
};

class UpdateRecipeNoiseCommand : public RecipeFieldCommand<NoiseState> {
public:
    UpdateRecipeNoiseCommand(std::string hash, std::vector<NoiseId> noises, int seed, double strength, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::UpdateRecipeNoise; }

protected:
    NoiseState read(const Recipe& r) const override {
        return { r.patternNoise, r.patternNoiseSeed, r.patternNoiseStrength };
    }
    void write(Recipe& r, const NoiseState& s) const override {
        r.patternNoise = s.noises;
        r.patternNoiseSeed = s.seed;
        r.patternNoiseStrength = s.strength;
    }
    DirtyMask dirty() const override { return DIRTY_NOISE; }
};

struct RibbonState {
    std::string algo = "none";
    double amount = 0.0;
    int period = 4;
    int shades = 2;
    bool invert = false;
    std::optional<std::vector<std::optional<std::string>>> custom_hex;
};

class UpdateRecipeRibbonCommand : public RecipeFieldCommand<RibbonState> {
public:
    UpdateRecipeRibbonCommand(
        std::string hash,
        std::string algo,
        double amount,
        int period,
        int shades,
        bool invert,
        std::optional<std::vector<std::optional<std::string>>> custom_hex,
        int flag = 0
    );
    CommandKind get_kind() const override { return CommandKind::UpdateRecipeRibbon; }

protected:
    RibbonState read(const Recipe& r) const override {
        return { r.ribbonAlgo, r.ribbonAmount, r.ribbonPeriod, r.ribbonShades, r.ribbonInvert, r.customRibbonHex };
    }
    void write(Recipe& r, const RibbonState& s) const override {
        r.ribbonAlgo = s.algo;
        r.ribbonAmount = s.amount;
        r.ribbonPeriod = s.period;
        r.ribbonShades = s.shades;
        r.ribbonInvert = s.invert;
        r.customRibbonHex = s.custom_hex;
    }
    // ribbonShades owns this array's length. Enforced here rather than at the
    // call sites so a caller passing the previous array cannot invalidate it.
    void sync(Recipe& r) const override { sync_ribbon_overrides(r); }
    DirtyMask dirty() const override { return DIRTY_RIBBON; }
};

struct TextureState {
    std::string algoA = "none", algoB = "none";
    double amountA = 0.35, amountB = 0.35;
    int shadesA = 2, shadesB = 2;
    int seedA = 1, seedB = 1;
    int cellScaleA = 4, cellScaleB = 4;
    int rippleScaleA = 4, rippleScaleB = 4;
    int geoScaleA = 1, geoScaleB = 1;
    std::optional<std::vector<std::optional<std::string>>> customA, customB;
};

class UpdateRecipeTextureCommand : public RecipeFieldCommand<TextureState> {
public:
    UpdateRecipeTextureCommand(std::string hash, const Recipe& new_state, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::UpdateRecipeTexture; }

protected:
    TextureState read(const Recipe& r) const override {
        return { r.textureAlgoA, r.textureAlgoB, r.textureAmountA, r.textureAmountB,
                 r.textureShadesA, r.textureShadesB, r.textureSeedA, r.textureSeedB,
                 r.cellScaleA, r.cellScaleB, r.rippleScaleA, r.rippleScaleB,
                 r.geoScaleA, r.geoScaleB, r.customTexHexA, r.customTexHexB };
    }
    void write(Recipe& r, const TextureState& s) const override {
        r.textureAlgoA = s.algoA;       r.textureAlgoB = s.algoB;
        r.textureAmountA = s.amountA;   r.textureAmountB = s.amountB;
        r.textureShadesA = s.shadesA;   r.textureShadesB = s.shadesB;
        r.textureSeedA = s.seedA;       r.textureSeedB = s.seedB;
        r.cellScaleA = s.cellScaleA;    r.cellScaleB = s.cellScaleB;
        r.rippleScaleA = s.rippleScaleA; r.rippleScaleB = s.rippleScaleB;
        r.geoScaleA = s.geoScaleA;      r.geoScaleB = s.geoScaleB;
        r.customTexHexA = s.customA;    r.customTexHexB = s.customB;
    }
    // textureShadesA/B own these arrays' lengths.
    void sync(Recipe& r) const override { sync_texture_overrides(r); }
    DirtyMask dirty() const override { return static_cast<DirtyMask>(DIRTY_TEXTURE_A | DIRTY_TEXTURE_B); }
};

class AddRecipeCommand : public LibraryCommand {
public:
    AddRecipeCommand(Recipe recipe, std::string name);
    CommandKind get_kind() const override { return CommandKind::AddRecipe; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;

private:
    Recipe recipe_;
    std::string name_;
    std::string created_hash_;
    std::string previous_selected_hash_;
};

class RemoveRecipeCommand : public LibraryCommand {
public:
    explicit RemoveRecipeCommand(std::string hash);
    CommandKind get_kind() const override { return CommandKind::RemoveRecipe; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;

private:
    std::shared_ptr<RecipeEntry> removed_entry_;
    int removed_index_ = -1;
    std::string previous_selected_hash_;
};

class DuplicateRecipeCommand : public LibraryCommand {
public:
    explicit DuplicateRecipeCommand(std::string hash);
    CommandKind get_kind() const override { return CommandKind::DuplicateRecipe; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;

private:
    std::string source_hash_;
    std::string created_hash_;
    std::string previous_selected_hash_;
};

class RenameRecipeCommand : public LibraryCommand {
public:
    RenameRecipeCommand(std::string hash, std::string new_name, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::RenameRecipe; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;
    bool merge_with(const LibraryCommand* other) override;

private:
    std::string old_name_;
    std::string new_name_;
    bool initialized_ = false;
};

class ReorderRecipeCommand : public LibraryCommand {
public:
    ReorderRecipeCommand(int from_index, int to_index);
    CommandKind get_kind() const override { return CommandKind::ReorderRecipe; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;

private:
    int from_index_;
    int to_index_;
};

class SelectRecipeCommand : public LibraryCommand {
public:
    explicit SelectRecipeCommand(std::string target_hash);
    CommandKind get_kind() const override { return CommandKind::SelectRecipe; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;

private:
    std::string old_hash_;
    std::string new_hash_;
    bool initialized_ = false;
};

class AddVariantAxisCommand : public LibraryCommand {
public:
    explicit AddVariantAxisCommand(VariantAxis axis);
    CommandKind get_kind() const override { return CommandKind::AddVariantAxis; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;

private:
    VariantAxis axis_;
};

class RemoveVariantAxisCommand : public LibraryCommand {
public:
    explicit RemoveVariantAxisCommand(int index);
    CommandKind get_kind() const override { return CommandKind::RemoveVariantAxis; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;

private:
    int index_;
    VariantAxis removed_axis_;
};

class UpdateExportSettingsCommand : public LibraryCommand {
public:
    explicit UpdateExportSettingsCommand(ExportSettings settings, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::UpdateExportSettings; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;
    bool merge_with(const LibraryCommand* other) override;

private:
    ExportSettings old_settings_;
    ExportSettings new_settings_;
    bool initialized_ = false;
};

} // namespace atm
