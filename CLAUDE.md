# CLAUDE.md

Guidance for Claude Code working in this repository.

## What this project is

`autotile_imgui` is a native C++ / ImGui desktop app that generates **blob47
tileset sheets** from a *recipe* (three colours + a pattern + band/grain/motif/
texture settings). It is the desktop counterpart of a web app that already does
this in TypeScript: the web tool makes and shares one sheet, this one keeps a
library of recipes and batch-exports many.

**The entire project is graded on one thing: pixel-for-pixel agreement with the
web implementation.** A sheet is 256x192 RGBA (8 columns x 6 rows of 32px
tiles). Given the same recipe, this app's bytes must equal the web app's bytes.

See `docs/ARCHITECTURE.md` for full architecture, data flow, and module design.

## The two directories you must not fight with

- **`reference/`** — a read-only snapshot of the TypeScript being ported. This is
  the specification. Never edit it. See `reference/README.md`.
- **`corpus/`** — 1161 baked ground-truth sheets. This is the grader. Never edit
  a `.png`, a `.lvl.gz`, or `manifest.json` by hand; if the corpus and your code
  disagree, your code is wrong. See `corpus/README.md`.

## Build & test

```bash
# Configure. CMAKE_POLICY_VERSION_MINIMUM is needed because doctest declares a
# min CMake version older than this project's 3.20.
cmake -B build-desktop -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5

cmake --build build-desktop -j --target autotile_mixer   # the app
cmake --build build-desktop -j --target autotile_tests   # unit tests
ctest --test-dir build-desktop --output-on-failure
```

Parity check (the one that matters):

```bash
python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe --quick   # L0+L1, seconds
python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe           # all 1161
python corpus/verify.py --exe ... --tier L2 --diff-dir out/diff --max-report 3   # one tier, with images
```

`pip install numpy` — optional, but it takes a full run from minutes to ~5s.

## Hard rules

These are not style preferences. Each one is a mistake that has already been made
or has already cost measurable time.

1. **Port from `reference/renderSheet.ts` and what it calls — never from the web
   app's `App.tsx`.** The recipe-to-paint mapping used to exist twice and the
   second copy had silently drifted (band position scaled by the wrong constant,
   no special-casing for water or the pavings). `renderSheet.ts` is the one that
   baked the corpus.

2. **`generated.ts` is machine output and the generator does not exist any more.**
   Transcribe the data verbatim into a C++ header. Do not "recompute", "clean up",
   or "regenerate" it — several of its generators were never solved, and the
   attempt will produce data that looks plausible and is wrong.

3. **Do `js_math` before any algorithm.** JS's `Math.imul`, `Math.hypot`,
   `Math.sin` and `Math.atan2` are not `std::` equivalents, and `Math.round` is
   half-away-from-zero (use `std::floor(x + 0.5)`, never `std::round` or
   banker's rounding). Get the conformance test green first, or every later bug
   costs an extra hour of "is it my logic or is it the float?".

4. **Faithful, not better.** The reference has known oddities: a fully saturated
   terrain A collapses the inner band steps; there are three grain-colour pickers
   against up to five band steps; some motifs paint below their own minimum
   width. Reproduce them exactly. "Fixing" one turns a green test red and is not
   this project's job.

5. **Never mutate model state from a panel.** Every mutation goes through a
   `LibraryCommand` dispatched via `LibraryCommandHandler::addAndExecuteCommand`,
   so undo/redo and the callback fan-out stay consistent. The ViewModel is the
   only `LibraryCallbacks` implementation the handler knows about; panels
   register with the ViewModel and receive events from its `fan_out`.

6. **Anything that changes a sheet's pixels must emit `onRecipeUpdated`**, or the
   preview goes stale — the renderer only recomputes what the `DirtyMask` says.

7. **Windows/MSVC:** when lifting CMake from `D:\tile_map_editor_imgui`, note it
   has no MSVC branch — it passes `-w` and `-Wno-error=int-conversion` to
   vendored C unconditionally. Guard those with `if(MSVC) ... /w ... else()`.

## Working rhythm

Run the parity check (`corpus/verify.py`) and unit tests after any change. Changes must maintain 1161/1161 passed with maxDelta 0.

When a parity check fails, **read the whole failure report before changing code**.
It tells you the first differing pixel by slot/mask/tile coordinate, the spread
across levels, how many failures sit on a level boundary, and a guess at which
layer is at fault. Its attribution is usually right and it is the difference
between a ten-minute fix and an afternoon.

Do not widen `maxDelta` to make a test pass. It is a per-case recorded concession
about one specific libm divergence, not a similarity knob — and the failure mode
a float difference actually produces (a quantiser boundary flip) blows past any
allowance anyway, so widening it hides nothing useful and hides plenty that is.

## Conventions

- C++17. `-Wall -Wextra -Wpedantic` (`/W4` on MSVC).
- Core library `autotile` under `src/`: no GL, no ImGui, no I/O in the pattern
  code. It must stay callable from a test binary and from a worker thread.
- Desktop front-end under `desktop/`, ImGui + GLFW + OpenGL 3.3, mirroring
  `D:\tile_map_editor_imgui\desktop\` (`app` / `view_model` / `panels/`).
- Tests use doctest under `tests/`.
- Vendored single-header C (stb, miniz) gets its warnings silenced per-source,
  not by relaxing flags on the whole target.
