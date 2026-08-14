# TASKS — work breakdown with acceptance gates

One task at a time. **A task is done when its gate command passes**, not when the
code looks finished. Tick the box and move on.

Architecture and rationale live in `PLAN.md`; the rules you must not break live
in `../CLAUDE.md`. Read both before T1.

Legend: **G** = gate (the command that decides done). `[x]` = gate passed.
`[~]` = code is written but the gate has **not** passed; the task carries a note
saying what is missing. A `[~]` is not a lesser tick, it is an open item.

---

## P1 — Skeleton and float conformance

Nothing here renders anything. It exists so that every later failure has exactly
one possible cause.

- [x] **T1.1 Repo skeleton.** Root `CMakeLists.txt` (C++17, warnings, `autotile`
  static lib, `BUILD_DESKTOP` option, `ADNA_BUILD_TESTS` option), `cmake/`,
  `src/CMakeLists.txt`, `tests/CMakeLists.txt` with doctest via FetchContent.
  Lift the shape from `D:\tile_map_editor_imgui\CMakeLists.txt` — **and add the
  MSVC branch it is missing** (see CLAUDE.md rule 7).
  **G:** `cmake -B build-desktop -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5` configures
  clean, and `cmake --build build-desktop --target autotile_tests` builds an empty
  suite that `ctest` runs.

- [x] **T1.2 Vendor third-party.** Copy `nlohmann/`, `stb/`, `miniz/` from
  `D:\tile_map_editor_imgui\third_party\` into `third_party/`. Wrap the
  single-header includes in `#pragma GCC diagnostic push/pop` (`src/util/image.cpp`
  over there is the reference example) and silence miniz per-source, not per-target.
  **G:** a throwaway test that writes a 2x2 PNG with stb and reads it back builds
  warning-free and passes.

- [x] **T1.3 `src/pattern/js_math.h`.** JS semantics for `imul`, `hypot`, `sin`,
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

- [x] **T2.1 `pattern_data.h/.cpp`** — transcribe `reference/generated.ts`
  verbatim: the per-mask field strings, `FIELD_STEP`, `FIELD_CHARS`,
  `PATTERN_BANDS`, `PATTERN_OFFSET_RANGE`, `PATTERN_TILE_SIZE`. Data only.
  **G:** a test asserts, for all 11 patterns and all 47 masks, that the stored
  string lengths and the decoded field's min/max match values read out of the TS.

- [x] **T2.2 `blob47.h/.cpp`** — 48-slot layout, `blobSlotForMask`, the bit
  constants.
  **G:** `maskToSlot` for all 256 masks equals `manifest.json`'s `sheet.layout`
  round-trip; slot count 48; layout matches the manifest array exactly.

- [x] **T2.3 `blob47_pattern.h/.cpp`** — `patternLevelsFor`, `bandsFor`,
  `outlineWidthPx`, `bandNoiseSpan`, `patternLevelsForMask`, `patternBandCoords`.
  This is where `js_math` first earns its keep.
  **G:** wire up `--render-corpus`'s `.lvl` output (see `corpus/README.md`), then
  `python corpus/verify.py --exe ... --quick` reports **no LEVEL GRID differences**
  on all 210 L0+L1 cases. Colour differences are expected and fine at this point.

---

## P3 — Colour (gate: L1 fully green)

- [x] **T3.1 `pattern_paint`: colour model** — `RGB`, hex parse/format,
  `rgbToHsv` / `hsvToRgb` exactly as the reference writes them (including the
  achromatic `s0 < 1e-6` branch taking the recipe hue outright), `shadeColour`,
  `patternRamp`.
  **G:** `tests/test_palette.cpp` reproduces a table of
  `(colour, role, t) -> RGB` dumped from the TS; all rows exact.

- [x] **T3.2 `pattern_noise`** — the three grain algorithms and `noiseStep`.
  **G:** a dumped `(x, y, seed, strength, noises) -> step` table matches exactly.

- [x] **T3.3 `pattern_paint`: the main loop** — `paintPatternTileRGBA` with the
  band level gate. Copy the asymmetric target rule verbatim:
  `keep = targets.includes(sourceRole) && (destRole != 'edge' || targets.includes('edge'))`.
  Both halves are load-bearing; either alone leaks visibly.

- [x] **T3.4 `recipe` + `sanitizeRecipe` + `recipe_to_paint_args`** — port
  `reference/recipe.ts` and `reference/renderSheet.ts`. The clamping in
  `sanitizeRecipe` is part of the render: same recipe JSON must clamp the same way.

- [x] **T3.5 `sheet` + the `--render-corpus` CLI** — assemble 48 tiles into
  256x192, write `<id>.rgba` and `<id>.lvl` per `corpus/README.md`.
  **G:** `python corpus/verify.py --exe ... --quick` → **210/210 pass**, and
  `--tier L5` passes too (it needs no textures or motifs).

---

## P4 — Textures (the big one; gate: L2 green, group by group)

`reference/patternTexture.ts` is 1506 lines. Do it in the order below and run the
matching filter after each; do not start the next group until the current one is
green. `--id` accepts repeats, and every L2 case id is `L2_<texture>_<A|B>_...`.

- [x] **T4.1 Hash + value noise core** (`hash01`, `smooth`) and the three simple
  ones: `white`, `blue`, `ordered`.
  **G:** `verify.py --exe ... --tier L2` shows those ids passing.
- [x] **T4.2 `ripple`, `ripple_diag`, `cells`** (Voronoi — first real `hypot` user).
- [x] **T4.3 Baked tables:** `weave`, `cobbles2`, `brick_floor`, `paving`,
  `paving3`, `paving5`, `stone_floor`, `breeze_block`, `brick_wall`, `water`,
  `field`, `rubble`. Transcribe the tables; `bakedShade` / `rankToShade` are shared.
- [x] **T4.4 Generated geometry:** `square`, `isometric`, `isometric_grid`,
  `octagonal`, `hexagon`, `brick_bond`, `nonslip` — plus `geoScalesFor` /
  `naturalGeoScale`.
- [x] **T4.5 `textureColour` / `textureRamp` / `usedTextureShades`.**
  **G for P4:** `python corpus/verify.py --exe ... --tier L2` → **286/286 pass**.

---

## P5 — Ribbon motifs

- [x] **T5.1** All 14 motifs, `ribbonShadeAt`, `usedRibbonShades`,
  `RIBBON_MIN_WIDTH`, `ribbonUsesPeriod` / `ribbonUsesInvert`.
  **G:** `--tier L3` → 97/97 pass.

---

## P6 — Full parity

- [x] **T6.1** `--tier L4` → 43/43 (this is the grain-targets gate).
- [x] **T6.2** Full run, no filter → **1161/1161 pass at `maxDelta` 0**.
  **This is the project's headline acceptance criterion.**
- [x] **T6.3** Wire the parity run into `ctest` as its own test so `ctest`
  catches a regression without anyone remembering to run Python.

---

## P7 — Document model, commands, undo/redo *(no UI; parallelisable with P2–P6)*

- [x] **T7.1** `model/recipe_library.h/.cpp` (`RecipeEntry` with a stable hash,
  `RecipeLibrary`), `handler/library_handler.h/.cpp` (document + selection).
- [x] **T7.2** `command/library_command.h` + `library_command_handler` — copy the
  shape of `D:\tile_map_editor_imgui\src\command\tile_map_command{,_handler}.h`:
  `init/execute/mergeWith/undo/redo`, a `Kind` enum, `flag` as the drag phase,
  `timeStamp`. Drop the `_replaceTileMap` / `clearHistory` machinery — `Recipe`
  is a value type, so undo snapshots are plain copies.
- [x] **T7.3** `command/library_command_callbacks.h` and the command set in
  `PLAN.md` §3.2. **`mergeWith` is required, not an optimisation** — an ImGui
  slider reports changed every frame.
- [x] **T7.4** `tests/test_command_undo.cpp` and `test_command_monkey.cpp`
  (model the latter on the tilemap repo's).
  **G:** a random command sequence, fully undone, serialises byte-identically to
  the starting library; fully redone, byte-identical to the all-executed state.

---

## P8–P10 — UI and bridges

- [x] **T8.1** `desktop/`: GLFW + ImGui bootstrap, `app`, `view_model` with
  `register_panel` / `fan_out`, `sheet_renderer` (CPU RGBA + GL texture, driven
  by `DirtyMask` per `PLAN.md` §3.4).
- [x] **T8.2** Panels: `library_panel`, `recipe_inspector`, `sheet_view`, `log_view`.
- [~] **T9.1** `variant_matrix` + `batch_export_panel`: axis cross-product,
  worker-thread rendering, progress fanned out through a thread-safe queue
  drained on the main thread (rendering is pure CPU and safe to thread; **GL
  uploads are main-thread only**).
  **G:** batch-export the whole corpus through the UI path and verify the output
  with `verify.py --actual`.
  > Panels, cross-product and threading are done: the worker only calls
  > `ViewModel::queue_batch_progress` (mutex-guarded), and
  > `drain_progress_queue()` runs the fan-out on the main thread from
  > `app.cpp`'s frame loop before any panel draws.
  > **The gate itself has not been run.** The UI exports `<template>.png`, while
  > `verify.py --actual` wants `<id>.rgba` + `<id>.lvl`, so the two do not meet
  > without a shim nobody has written. Note that the export worker, the preview
  > renderer and `--render-corpus` all call the same `atm::render_sheet_rgba`,
  > so the 1161/1161 CLI run already covers the pixels; what stays unverified is
  > the UI-specific plumbing (naming template, sidecar, cancel).

- [~] **T10.1** `recipe_codec` port (share-code import). **G:** round-trip test
  against codes encoded by the web app.
  > The port exists and `tests/test_commands.cpp` round-trips it, but **only
  > C++ -> C++**, which an encoder and decoder that are wrong in the same way
  > would also pass. No share code produced by the web app was available to test
  > against, so the gate as written is unmet. To close it: paste real codes from
  > the web app into `tests/data/` and assert they decode to the expected
  > recipes. Do not substitute another self-round-trip.

- [~] **T10.2** Import the web app's exported `.zip` (PNG + JSON sidecar).
  > `src/codec/zip_import.{h,cpp}` + the Import ZIP modal in `library_panel` are
  > implemented and defensive (any `.json` entry is accepted, `recipe` sub-object
  > or whole document, name falls back to the filename, everything goes through
  > `sanitize_recipe`), and imports land via `AddRecipeCommand` so undo works.
  > **The gate is unmet and the format is unverified.** `tests/test_zip_import.cpp`
  > builds its archive in-process with miniz — the same library it exercises — so
  > it shows that miniz reads what miniz wrote, not that this reader understands
  > the web app's archive. The sidecar schema it assumes is the one *this app's*
  > exporter writes; nothing in `reference/` or `docs/PLAN.md` specifies the web
  > format. To close it: commit a real web-app-exported `.zip` under
  > `tests/data/` and assert the recovered recipes render byte-identically to the
  > PNGs inside it.

---

## Acceptance Verification Summary

Legend: `[x]` gate passed · `[~]` implemented, gate **not** passed — see the note
under the task for what is missing and how to close it.

**Passing**

- **Corpus parity:** 1,161 / 1,161, bit-for-bit at `maxDelta = 0`
  (`python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe`).
- **Unit tests:** 13 cases / 9,091 assertions
  (`ctest` → `autotile_unit_tests`, `autotile_corpus_parity_quick`;
  `ctest -L slow` adds `autotile_corpus_parity_full` over all 1,161).
- **Build:** full rebuild is warning-free under `-Wall -Wextra -Wpedantic`.
- **Desktop app:** DockSpace, panels, ViewModel, undo/redo and viewport renderer
  all present; batch export is race-free (queue drained on the main thread).

**Not passing — three gates remain open**

| Task | What is missing |
| --- | --- |
| T9.1 | The UI export path has never been checked against `verify.py --actual`; output filenames do not match what the verifier expects. |
| T10.1 | Share-code round-trip is C++ → C++ only; no code produced by the web app has been tested against. |
| T10.2 | The ZIP test builds its own archive with miniz; no real web-app export has been read, so the sidecar schema is an assumption. |

All three need an artefact produced by the web app. None can be closed by
writing more tests against data this repository generated — that is precisely
the failure mode these notes exist to prevent.
