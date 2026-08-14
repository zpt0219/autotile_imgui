#pragma once

#include "model/recipe_library.h"
#include <string>
#include <cstdint>

namespace atm {

enum DirtyMask : uint32_t {
    DIRTY_NONE       = 0,
    DIRTY_SILHOUETTE = 1 << 0,  // pattern / bandSteps / hardEdgeB / bandBias / edgeSeed / outlineWidth
    DIRTY_COLOUR     = 1 << 1,  // role colours / custom shades ramp
    DIRTY_NOISE      = 1 << 2,  // patternNoise / seed / strength / targets
    DIRTY_RIBBON     = 1 << 3,  // ribbon algo / amount / period / shades / invert / ramp
    DIRTY_TEXTURE_A  = 1 << 4,  // textureA algo / amount / shades / seed / scale / ramp
    DIRTY_TEXTURE_B  = 1 << 5,  // textureB algo / amount / shades / seed / scale / ramp
    DIRTY_ALL        = 0xFFFFFFFF
};

struct BatchProgress {
    int current = 0;
    int total = 0;
    std::string current_name;
    bool finished = false;
    bool cancelled = false;
    std::string error_message;
};

class LibraryCallbacks {
public:
    virtual ~LibraryCallbacks() = default;
    virtual void onLibraryLoaded(int flag) { (void)flag; }
    virtual void onLibraryListUpdated(int flag) { (void)flag; }
    virtual void onRecipeSelected(RecipeEntry* entry, int flag) { (void)entry; (void)flag; }
    virtual void onRecipeUpdated(RecipeEntry* entry, DirtyMask dirty, int flag) { (void)entry; (void)dirty; (void)flag; }
    virtual void onVariantAxesUpdated(int flag) { (void)flag; }
    virtual void onBatchProgress(const BatchProgress& progress, int flag) { (void)progress; (void)flag; }
    virtual void onExportSettingsUpdated(int flag) { (void)flag; }
};

} // namespace atm
