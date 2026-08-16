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
template <typename State>
class RecipeFieldCommand : public LibraryCommand {
public:
    EditorResult execute(LibraryHandler& handler, LibraryCallbacks* cb) override;
    EditorResult undo(LibraryHandler& handler, LibraryCallbacks* cb) override;

    bool merge_with(const LibraryCommand* other) override {
        if (!other || other->get_kind() != this->get_kind()) return false;
        // Kinds match, so the dynamic type matches too.
        const auto* o = static_cast<const RecipeFieldCommand<State>*>(other);
        if (o->get_target_hash() != this->get_target_hash()) return false;
        if (this->phase_ == EditPhase::End) return false;   // previous already ended

        new_ = o->new_;
        this->phase_ = o->phase_;
        return true;
    }

protected:
    virtual State read(const Recipe& r) const = 0;
    virtual void write(Recipe& r, const State& s) const = 0;

    // Fields the forward path carries over from whatever is on the recipe
    // rather than replacing. They are still part of State so that the snapshot
    // captures them and undo puts them back. Runs on every forward execute, not
    // just the first, so a merged-in command cannot drop them.
    virtual void carry_over(State& next, const Recipe& current) const { (void)next; (void)current; }
    virtual void sync(Recipe& r) const { (void)r; }
    virtual DirtyMask dirty() const = 0;

    State old_{};
    State new_{};
    bool initialized_ = false;
};

} // namespace atm

// The concrete commands, split out by what they touch. Both need
// RecipeFieldCommand / LibraryCommand above, so they are included at the end
// rather than at the top; include this header, never those two directly.
#include "recipe_field_commands.h"
#include "library_structure_commands.h"
