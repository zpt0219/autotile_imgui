#pragma once

#include "library_callbacks.h"
#include "model/recipe.h"
#include "model/recipe_library.h"
#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <chrono>

namespace atm {

class LibraryHandler;

enum class CommandKind {
    Unknown,
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
    RemoveVariantAxis,
    UpdateExportSettings
};

class LibraryCommand {
public:
    virtual ~LibraryCommand() = default;

    virtual CommandKind get_kind() const = 0;
    virtual EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) = 0;
    virtual EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) = 0;
    virtual bool merge_with(const LibraryCommand* other);

    const std::string& get_target_hash() const { return target_hash_; }
    EditPhase get_phase() const { return phase_; }

protected:
    std::string target_hash_;
    EditPhase phase_ = EditPhase::Begin;
    std::chrono::steady_clock::time_point timestamp_ = std::chrono::steady_clock::now();
};

// Generic CRTP base for leaf-property commands.
template <typename State>
class RecipeFieldCommand : public LibraryCommand {
public:
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;

    bool merge_with(const LibraryCommand* other) override {
        if (!other || other->get_kind() != this->get_kind()) return false;
        const auto* o = static_cast<const RecipeFieldCommand<State>*>(other);
        if (o->get_target_hash() != this->get_target_hash()) return false;
        if (this->phase_ == EditPhase::End) return false;

        new_ = o->new_;
        this->phase_ = o->phase_;
        this->timestamp_ = o->timestamp_;
        return true;
    }

protected:
    virtual State read(const Recipe& r) const = 0;
    virtual void write(Recipe& r, const State& s) const = 0;
    virtual void carry_over(State& next, const Recipe& current) const { (void)next; (void)current; }
    virtual void sync(Recipe& r) const { (void)r; }
    virtual DirtyMask dirty() const = 0;

    State old_{};
    State new_{};
    bool initialized_ = false;
};

} // namespace atm

#include "recipe_field_commands.h"
#include "library_structure_commands.h"
