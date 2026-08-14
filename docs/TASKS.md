# TASKS — work breakdown with acceptance gates

One task at a time. **A task is done when its gate command passes**, not when the
code looks finished. Tick the box and move on.

Architecture and rationale live in `PLAN.md`; the rules you must not break live
in `../CLAUDE.md`. Read both before T1.

Legend: **G** = gate (the command that decides done).

---

## P1 — Skeleton and float conformance

Nothing here renders anything. It exists so that every later failure has exactly
one possible cause.

- [ ] **T1.1 Repo skeleton.** Root `CMakeLists.txt` (C++17, warnings, `autotile`
  static lib, `BUILD_DESKTOP` option, `ADNA_BUILD_TESTS` option), `cmake/`,
  `src/CMakeLists.txt`, `tests/CMakeLists.txt` with doctest via FetchContent.
  Lift the shape from `D:\tile_map_editor_imgui\CMakeLists.txt` — **and add the
  MSVC branch it is missing** (see CLAUDE.md rule 7).
  **G:** `cmake -B build-desktop -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5` configures
  clean, and `cmake --build build-desktop --target autotile_tests` builds an empty
  suite that `ctest` runs.

- [ ] **T1.2 Vendor third-party.** Copy `nlohmann/`, `stb/`, `miniz/` from
  `D:\tile_map_editor_imgui\third_party\` into `third_party/`. Wrap the
  single-header includes in `#pragma GCC diagnostic push/pop` (`src/util/image.cpp`
  over there is the reference example) and silence miniz per-source, not per-target.
  **G:** a throwaway test that writes a 2x2 PNG with stb and reads it back builds
  warning-free and passes.

- [ ] **T1.3 `src/pattern/js_math.h`.** JS semantics for `imul`, `hypot`, `sin`,
  `atan2`, `round`, plus the `>>>` / int32 coercion helpers. `hypot` must be V8's
  scaled-compensated algorithm and `sin`/`atan2` fdlibm-derived — **not**
  `std::hypot` / `std::sin`, which differ in the last ulp.
  **G:** `tests/test_js_math.cpp` compares against a bit-pattern table dumped
  from Node (`(x, y, doubleToHex(result))`, several hundred rows spanning the
  argument ranges the textures actually use) and every row matches **bit for bit**.
  Generate the table with a small script and commit it under `tests/data/`.

> If T1.3 cannot be made bit-exact for one specific function, stop and say so
> rather than proceeding — that finding changes the plan, and discovering it in
> P4 with 1500 lines of texture code on top is much more expensive.

---

## P2 — Silhouette (gate: corpus Level A + L1)

- [ ] **T2.1 `pattern_data.h/.cpp`** — transcribe `reference/generated.ts`
  verbatim: the per-mask field strings, `FIELD_STEP`, `FIELD_CHARS`,
  `PATTERN_BANDS`, `PATTERN_OFFSET_RANGE`, `PATTERN_TILE_SIZE`. Data only.
  **G:** a test asserts, for all 11 patterns and all 47 masks, that the stored
  string lengths and the decoded field's min/max match values read out of the TS.

- [ ] **T2.2 `blob47.h/.cpp`** — 48-slot layout, `blobSlotForMask`, the bit
  constants.
  **G:** `maskToSlot` for all 256 masks equals `manifest.json`'s `sheet.layout`
  round-trip; slot count 48; layout matches the manifest array exactly.

- [ ] **T2.3 `blob47_pattern.h/.cpp`** — `patternLevelsFor`, `bandsFor`,
  `outlineWidthPx`, `bandNoiseSpan`, `patternLevelsForMask`, `patternBandCoords`.
  This is where `js_math` first earns its keep.
  **G:** wire up `--render-corpus`'s `.lvl` output (see `corpus/README.md`), then
  `python corpus/verify.py --exe ... --quick` reports **no LEVEL GRID differences**
  on all 210 L0+L1 cases. Colour differences are expected and fine at this point.

---

## P3 — Colour (gate: L1 fully green)

- [ ] **T3.1 `pattern_paint`: colour model** — `RGB`, hex parse/format,
  `rgbToHsv` / `hsvToRgb` exactly as the reference writes them (including the
  achromatic `s0 < 1e-6` branch taking the recipe hue outright), `shadeColour`,
  `patternRamp`.
  **G:** `tests/test_palette.cpp` reproduces a table of
  `(colour, role, t) -> RGB` dumped from the TS; all rows exact.

- [ ] **T3.2 `pattern_noise`** — the three grain algorithms and `noiseStep`.
  **G:** a dumped `(x, y, seed, strength, noises) -> step` table matches exactly.

- [ ] **T3.3 `pattern_paint`: the main loop** — `paintPatternTileRGBA` with the
  band level gate. Copy the asymmetric target rule verbatim:
  `keep = targets.includes(sourceRole) && (destRole != 'edge' || targets.includes('edge'))`.
  Both halves are load-bearing; either alone leaks visibly.

- [ ] **T3.4 `recipe` + `sanitizeRecipe` + `recipe_to_paint_args`** — port
  `reference/recipe.ts` and `reference/renderSheet.ts`. The clamping in
  `sanitizeRecipe` is part of the render: same recipe JSON must clamp the same way.

- [ ] **T3.5 `sheet` + the `--render-corpus` CLI** — assemble 48 tiles into
  256x192, write `<id>.rgba` and `<id>.lvl` per `corpus/README.md`.
  **G:** `python corpus/verify.py --exe ... --quick` → **210/210 pass**, and
  `--tier L5` passes too (it needs no textures or motifs).

---

## P4 — Textures (the big one; gate: L2 green, group by group)

`reference/patternTexture.ts` is 1506 lines. Do it in the order below and run the
matching filter after each; do not start the next group until the current one is
green. `--id` accepts repeats, and every L2 case id is `L2_<texture>_<A|B>_...`.

- [ ] **T4.1 Hash + value noise core** (`hash01`, `smooth`) and the three simple
  ones: `white`, `blue`, `ordered`.
  **G:** `verify.py --exe ... --tier L2` shows those ids passing.
- [ ] **T4.2 `ripple`, `ripple_diag`, `cells`** (Voronoi — first real `hypot` user).
- [ ] **T4.3 Baked tables:** `weave`, `cobbles2`, `brick_floor`, `paving`,
  `paving3`, `paving5`, `stone_floor`, `breeze_block`, `brick_wall`, `water`,
  `field`, `rubble`. Transcribe the tables; `bakedShade` / `rankToShade` are shared.
- [ ] **T4.4 Generated geometry:** `square`, `isometric`, `isometric_grid`,
  `octagonal`, `hexagon`, `brick_bond`, `nonslip` — plus `geoScalesFor` /
  `naturalGeoScale`.
- [ ] **T4.5 `textureColour` / `textureRamp` / `usedTextureShades`.**
  **G for P4:** `python corpus/verify.py --exe ... --tier L2` → **286/286 pass**.

---

## P5 — Ribbon motifs

- [ ] **T5.1** All 14 motifs, `ribbonShadeAt`, `usedRibbonShades`,
  `RIBBON_MIN_WIDTH`, `ribbonUsesPeriod` / `ribbonUsesInvert`.
  **G:** `--tier L3` → 97/97 pass.

---

## P6 — Full parity

- [ ] **T6.1** `--tier L4` → 43/43 (this is the grain-targets gate).
- [ ] **T6.2** Full run, no filter → **1161/1161 pass at `maxDelta` 0**.
  **This is the project's headline acceptance criterion.**
- [ ] **T6.3** Wire the parity run into `ctest` as its own test so `ctest`
  catches a regression without anyone remembering to run Python.

---

## P7 — Document model, commands, undo/redo *(no UI; parallelisable with P2–P6)*

- [ ] **T7.1** `model/recipe_library.h/.cpp` (`RecipeEntry` with a stable hash,
  `RecipeLibrary`), `handler/library_handler.h/.cpp` (document + selection).
- [ ] **T7.2** `command/library_command.h` + `library_command_handler` — copy the
  shape of `D:\tile_map_editor_imgui\src\command\tile_map_command{,_handler}.h`:
  `init/execute/mergeWith/undo/redo`, a `Kind` enum, `flag` as the drag phase,
  `timeStamp`. Drop the `_replaceTileMap` / `clearHistory` machinery — `Recipe`
  is a value type, so undo snapshots are plain copies.
- [ ] **T7.3** `command/library_command_callbacks.h` and the command set in
  `PLAN.md` §3.2. **`mergeWith` is required, not an optimisation** — an ImGui
  slider reports changed every frame.
- [ ] **T7.4** `tests/test_command_undo.cpp` and `test_command_monkey.cpp`
  (model the latter on the tilemap repo's).
  **G:** a random command sequence, fully undone, serialises byte-identically to
  the starting library; fully redone, byte-identical to the all-executed state.

---

## P8–P10 — UI and bridges

- [ ] **T8.1** `desktop/`: GLFW + ImGui bootstrap, `app`, `view_model` with
  `register_panel` / `fan_out`, `sheet_renderer` (CPU RGBA + GL texture, driven
  by `DirtyMask` per `PLAN.md` §3.4).
- [ ] **T8.2** Panels: `library_panel`, `recipe_inspector`, `sheet_view`, `log_view`.
- [ ] **T9.1** `variant_matrix` + `batch_export_panel`: axis cross-product,
  worker-thread rendering, progress fanned out through a thread-safe queue
  drained on the main thread (rendering is pure CPU and safe to thread; **GL
  uploads are main-thread only**).
  **G:** batch-export the whole corpus through the UI path and verify the output
  with `verify.py --actual`.
- [ ] **T10.1** `recipe_codec` port (share-code import). **G:** round-trip test
  against codes encoded by the web app.
- [ ] **T10.2** Import the web app's exported `.zip` (PNG + JSON sidecar).

---

## Open questions — ask, do not guess

1. **`noiseTargets` / `noiseColours` are not in `Recipe`.** They change pixels but
   do not survive a save or a share link; the corpus reaches them through an
   `overrides` field. If the desktop app needs to render non-default grain
   targets, this has to be decided (add to the schema, which changes the
   share-code format — or accept it as web-only).
2. **Corpus sync.** `corpus/` is a copy. If the web renderer changes, someone has
   to re-bake it there and bring it over. Decide whether that becomes a submodule
   or stays a manual copy.
