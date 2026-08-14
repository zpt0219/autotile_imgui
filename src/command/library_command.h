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
    ImportShareCode,
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

// --- Concrete Commands ---

class UpdateRecipeColoursCommand : public LibraryCommand {
public:
    UpdateRecipeColoursCommand(std::string hash, RoleHex new_roles, std::optional<std::vector<std::string>> new_shades, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::UpdateRecipeColours; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;
    bool merge_with(const LibraryCommand* other) override;

private:
    RoleHex old_roles_;
    RoleHex new_roles_;
    std::optional<std::vector<std::string>> old_shades_;
    std::optional<std::vector<std::string>> new_shades_;
    bool initialized_ = false;
};

class UpdateRecipePatternCommand : public LibraryCommand {
public:
    UpdateRecipePatternCommand(std::string hash, std::string pattern_id, int edge_seed, int outline_width, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::UpdateRecipePattern; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;
    bool merge_with(const LibraryCommand* other) override;

private:
    std::string old_pattern_id_;
    std::string new_pattern_id_;
    int old_edge_seed_ = 0;
    int new_edge_seed_ = 0;
    int old_outline_width_ = 2;
    int new_outline_width_ = 2;
    bool initialized_ = false;
};

class UpdateRecipeBandCommand : public LibraryCommand {
public:
    UpdateRecipeBandCommand(std::string hash, int band_steps, bool hard_edge_b, bool transparent_b, double band_bias, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::UpdateRecipeBand; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;
    bool merge_with(const LibraryCommand* other) override;

private:
    int old_band_steps_ = 4;
    int new_band_steps_ = 4;
    bool old_hard_edge_b_ = false;
    bool new_hard_edge_b_ = false;
    bool old_transparent_b_ = false;
    bool new_transparent_b_ = false;
    double old_band_bias_ = 0.0;
    double new_band_bias_ = 0.0;
    bool initialized_ = false;
};

class UpdateRecipeNoiseCommand : public LibraryCommand {
public:
    UpdateRecipeNoiseCommand(std::string hash, std::vector<NoiseId> noises, int seed, double strength, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::UpdateRecipeNoise; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;
    bool merge_with(const LibraryCommand* other) override;

private:
    std::vector<NoiseId> old_noises_;
    std::vector<NoiseId> new_noises_;
    int old_seed_ = 0;
    int new_seed_ = 0;
    double old_strength_ = 0.0;
    double new_strength_ = 0.0;
    bool initialized_ = false;
};

class UpdateRecipeRibbonCommand : public LibraryCommand {
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
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;
    bool merge_with(const LibraryCommand* other) override;

private:
    std::string old_algo_, new_algo_;
    double old_amount_ = 0.0, new_amount_ = 0.0;
    int old_period_ = 4, new_period_ = 4;
    int old_shades_ = 2, new_shades_ = 2;
    bool old_invert_ = false, new_invert_ = false;
    std::optional<std::vector<std::optional<std::string>>> old_custom_hex_, new_custom_hex_;
    bool initialized_ = false;
};

class UpdateRecipeTextureCommand : public LibraryCommand {
public:
    UpdateRecipeTextureCommand(std::string hash, const Recipe& new_state, int flag = 0);
    CommandKind get_kind() const override { return CommandKind::UpdateRecipeTexture; }
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;
    bool merge_with(const LibraryCommand* other) override;

private:
    Recipe old_recipe_;
    Recipe new_recipe_;
    bool initialized_ = false;
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
