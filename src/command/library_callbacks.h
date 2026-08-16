#pragma once

#include "model/recipe_library.h"
#include <string>
#include <cstdint>
#include <utility>

namespace atm {

enum DirtyMask : uint32_t {
    DIRTY_NONE       = 0,
    DIRTY_SILHOUETTE = 1 << 0,  // pattern / bandSteps / hardEdgeB / bandBias / edgeSeed / outlineWidth
    DIRTY_COLOUR     = 1 << 1,  // role colours / custom shades ramp
    DIRTY_PALETTE    = 1 << 1,  // alias for DIRTY_COLOUR
    DIRTY_NOISE      = 1 << 2,  // patternNoise / seed / strength / targets
    DIRTY_RIBBON     = 1 << 3,  // ribbon algo / amount / period / shades / invert / ramp
    DIRTY_TEXTURE_A  = 1 << 4,  // textureA algo / amount / shades / seed / scale / ramp
    DIRTY_TEXTURE_B  = 1 << 5,  // textureB algo / amount / shades / seed / scale / ramp
    DIRTY_ALL        = 0xFFFFFFFF
};

struct EditorResult {
    bool success = true;
    std::string error_message;
    static EditorResult Ok() { return { true, "" }; }
    static EditorResult Error(std::string msg) { return { false, std::move(msg) }; }
};

struct BatchProgress {
    int current = 0;
    int total = 0;
    std::string current_name;
    bool finished = false;
    bool cancelled = false;
    std::string error_message;
};

// Phase of an edit interaction (e.g. continuous slider dragging).
enum class EditPhase : int {
    Begin = 0,
    Continue = 1,
    End = 2
};

class LibraryCallbacks {
public:
    virtual ~LibraryCallbacks() = default;
    virtual void onLibraryLoaded(EditPhase phase = EditPhase::Begin) { (void)phase; }
    virtual void onLibraryListUpdated(EditPhase phase = EditPhase::Begin) { (void)phase; }
    virtual void onRecipeSelected(RecipeEntry* entry, EditPhase phase = EditPhase::Begin) { (void)entry; (void)phase; }
    virtual void onRecipeUpdated(RecipeEntry* entry, DirtyMask dirty, EditPhase phase = EditPhase::Begin) { (void)entry; (void)dirty; (void)phase; }
    virtual void onVariantAxesUpdated(EditPhase phase = EditPhase::Begin) { (void)phase; }
    virtual void onBatchProgress(const BatchProgress& progress, EditPhase phase = EditPhase::Begin) { (void)progress; (void)phase; }
    virtual void onExportSettingsUpdated(EditPhase phase = EditPhase::Begin) { (void)phase; }
};

} // namespace atm
