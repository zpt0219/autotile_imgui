#pragma once

#include "library_callbacks.h"
#include "model/recipe.h"
#include <string>
#include <vector>
#include <optional>

namespace atm {

struct ColoursState {
    RoleHex roles;
    std::optional<std::vector<std::string>> custom_shades;
};

class UpdateRecipeColoursCommand : public RecipeFieldCommand<ColoursState> {
public:
    // `new_shades` is deliberately not defaulted: passing nullopt clears the
    // user's ramp, so a caller has to say which it means.
    UpdateRecipeColoursCommand(
        std::string hash,
        RoleHex new_roles,
        std::optional<std::vector<std::string>> new_shades,
        EditPhase phase = EditPhase::Begin
    );
    CommandKind get_kind() const override { return CommandKind::UpdateRecipeColours; }

protected:
    ColoursState read(const Recipe& r) const override {
        return { r.roleHex, r.customShadesHex };
    }
    void write(Recipe& r, const ColoursState& s) const override {
        r.roleHex = s.roles;
        r.customShadesHex = s.custom_shades;
    }
    // Defensive: a caller that built the array against a stale bandSteps would
    // otherwise hand over one the sanitiser drops.
    void sync(Recipe& r) const override { sync_band_overrides(r); }
    DirtyMask dirty() const override { return DIRTY_COLOUR; }
};

struct PatternState {
    std::string pattern_id = "square";
    int edge_seed = 0;
    int outline_width = 2;
};

class UpdateRecipePatternCommand : public RecipeFieldCommand<PatternState> {
public:
    UpdateRecipePatternCommand(
        std::string hash,
        std::string pattern_id,
        int edge_seed,
        int outline_width,
        EditPhase phase = EditPhase::Begin
    );
    CommandKind get_kind() const override { return CommandKind::UpdateRecipePattern; }

protected:
    PatternState read(const Recipe& r) const override {
        return { r.patternId, r.edgeSeed, r.outlineWidth };
    }
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
    UpdateRecipeBandCommand(
        std::string hash,
        int band_steps,
        bool hard_edge_b,
        bool transparent_b,
        double band_bias,
        EditPhase phase = EditPhase::Begin
    );
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
    UpdateRecipeNoiseCommand(
        std::string hash,
        std::vector<NoiseId> noises,
        int seed,
        double strength,
        EditPhase phase = EditPhase::Begin
    );
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
        EditPhase phase = EditPhase::Begin
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

struct TextureSideState {
    std::string algo = "none";
    double amount = 0.35;
    int shades = 2;
    int seed = 1;
    int cell_scale = 4;
    int ripple_scale = 4;
    int geo_scale = 1;
    std::optional<std::vector<std::optional<std::string>>> custom_hex;
};

struct TextureState {
    TextureSideState a;
    TextureSideState b;
};

class UpdateRecipeTextureCommand : public RecipeFieldCommand<TextureState> {
public:
    UpdateRecipeTextureCommand(std::string hash, const Recipe& new_state, EditPhase phase = EditPhase::Begin);
    CommandKind get_kind() const override { return CommandKind::UpdateRecipeTexture; }

protected:
    TextureState read(const Recipe& r) const override {
        return {
            { r.textureAlgoA, r.textureAmountA, r.textureShadesA, r.textureSeedA, r.cellScaleA, r.rippleScaleA, r.geoScaleA, r.customTexHexA },
            { r.textureAlgoB, r.textureAmountB, r.textureShadesB, r.textureSeedB, r.cellScaleB, r.rippleScaleB, r.geoScaleB, r.customTexHexB }
        };
    }
    void write(Recipe& r, const TextureState& s) const override {
        r.textureAlgoA = s.a.algo;          r.textureAlgoB = s.b.algo;
        r.textureAmountA = s.a.amount;      r.textureAmountB = s.b.amount;
        r.textureShadesA = s.a.shades;      r.textureShadesB = s.b.shades;
        r.textureSeedA = s.a.seed;          r.textureSeedB = s.b.seed;
        r.cellScaleA = s.a.cell_scale;      r.cellScaleB = s.b.cell_scale;
        r.rippleScaleA = s.a.ripple_scale;  r.rippleScaleB = s.b.ripple_scale;
        r.geoScaleA = s.a.geo_scale;        r.geoScaleB = s.b.geo_scale;
        r.customTexHexA = s.a.custom_hex;   r.customTexHexB = s.b.custom_hex;
    }
    // textureShadesA/B own these arrays' lengths.
    void sync(Recipe& r) const override { sync_texture_overrides(r); }
    DirtyMask dirty() const override { return static_cast<DirtyMask>(DIRTY_TEXTURE_A | DIRTY_TEXTURE_B); }
};

} // namespace atm
