#pragma once

#include "library_callbacks.h"
#include "model/recipe.h"
#include "model/recipe_library.h"
#include <string>
#include <memory>
#include <vector>
#include <optional>

namespace atm {

class LibraryHandler;

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
    RenameRecipeCommand(std::string hash, std::string new_name, EditPhase phase = EditPhase::Begin);
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
    explicit UpdateExportSettingsCommand(ExportSettings settings, EditPhase phase = EditPhase::Begin);
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
