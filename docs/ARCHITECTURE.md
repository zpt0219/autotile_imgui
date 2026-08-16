# Architecture & System Design

`autotile_imgui` is a high-performance, deterministic C++17 / ImGui generator for **blob47 tileset sheets** from structured *recipes*. It provides batch generation, variant cross-product matrices, interactive recipe editing, and pixel-for-pixel parity against the reference web specification.

---

## 1. System Overview & Layering

The codebase is strictly layered into two independent targets:

```
┌─────────────────────────────────────────────────────────────┐
│  Desktop Application (desktop/)                             │
│  - ImGui + GLFW + OpenGL 3.3 Desktop Shell (app, main)      │
│  - Reactive ViewModel & Callback Fan-out (view_model)       │
│  - Panels (recipe, library, preview, variant, batch_export) │
│  - Async Multi-threaded Sheet Renderer & Thumbnail Cache    │
└──────────────────────────────┬──────────────────────────────┘
                               │ commands / events / render calls
┌──────────────────────────────▼──────────────────────────────┐
│  Core Static Library (src/ -> libautotile.a)                │
│  - Pure computational engine (No GL, No ImGui, Thread-safe) │
│  - Pattern, texture, noise, and ribbon synthesis (pattern/) │
│  - Recipe data models and sanitization (model/)             │
│  - Command queue & Undo/Redo history (command/, handler/)   │
│  - Zip presets import (codec/) and PNG codec (util/)        │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Core Generator (`src/`)

### 2.1 Pattern & Sheet Synthesis (`src/pattern/`)
- **`pattern_data.h/.cpp`**: Verbatim transcription of distance field strings (`GENERATED_FIELDS`) and ASCII character lookup tables for 11 base patterns across 47 canonical masks.
- **`blob47_pattern.h/.cpp`**: Sample distance fields, apply edge-jitter and sine wave displacement (`wave_offset_at`), and quantize fields into 1024-char level strings using `FieldParams`.
- **`pattern_hash.h/.cpp`**: Bit-exact 32-bit integer mixer (`hash_bits`) and normalized float noise sampler (`hash01`).
- **`pattern_noise.h/.cpp`**: Multi-target white, blue, and ordered halftone grain generators.
- **`pattern_texture.h/.cpp` & `texture_tables.h/.cpp`**: Procedural textures (cells, isometric, hex, non-slip) and 12 baked ASCII texture tables.
- **`pattern_ribbon.h/.cpp`**: Tangent-distance motif synthesis (bevel, rope, beads, wave, dashes, ticks, grain, along-axis).
- **`pattern_paint.h/.cpp`**: Single 32x32 RGBA tile composition, coordinating palette ramps, noise grains (`apply_grain`), and ribbon/texture overlays (`pick_overlay`).
- **`sheet.h/.cpp`**: Sheet layout assembly (256x192 RGBA composed of 48 tiles arranged in 8 cols × 6 rows).
- **`catalog.h/.cpp`**: Unified `TextureDef`, `PatternDef`, and `RibbonDef` registries and UI metadata.
- **`js_math.h/.cpp`**: IEEE 754 float/double math utilities faithfully replicating JS semantics (`imul`, `hypot`, `round`, `urshift`, `sin`, `atan2`).

### 2.2 Model & Command History (`src/model/`, `src/command/`, `src/handler/`)
- **`recipe.h/.cpp`**: `Recipe` struct defining all visual attributes (colours, pattern, bands, noise, ribbon, textures) and resilient JSON deserialization (`sanitize_recipe`).
- **`recipe_library.h/.cpp`**: Container managing collections of recipes and variant axes.
- **`library_command.h/.cpp`**: Command pattern hierarchy implementing discrete edit actions with undo/redo and dirty masks (`DIRTY_COLOUR`, `DIRTY_SILHOUETTE`, `DIRTY_NOISE`, `DIRTY_RIBBON`, `DIRTY_TEXTURE_A`, `DIRTY_TEXTURE_B`).
- **`library_command_handler.h/.cpp`**: Executes commands, manages undo/redo stacks, and dispatches callbacks to listeners.

---

## 3. Desktop Application (`desktop/`)

### 3.1 Architecture & Data Flow
```
               User Interaction (UI Panels)
                            │
               Dispatch Command via Handler
                            │
         ┌──────────────────┴──────────────────┐
         ▼                                     ▼
Execute Command & Mutate Model       Emit Callback to ViewModel
         │                                     │
         │                                     ▼
         │                            Fan-out to Registered Panels
         │                                     │
         └──────────────────┬──────────────────┘
                            ▼
           Trigger Async Sheet Re-render
                            │
                  Background Worker Thread
                            │
                   Upload OpenGL Texture
```

- **`view_model.h/.cpp`**: Central state coordinator. Implements `LibraryCallbacks` and fans out events to panels. Tracks selections, dirty states, and preview rendering.
- **`renderer/sheet_renderer.h/.cpp`**: Multi-threaded sheet generation off the main UI thread with dirty-mask caching.
- **`renderer/thumbnail_cache.h/.cpp`**: Background LRU thumbnail rendering for recipe list browsing.
- **`panels/`**:
  - `recipe_panel.cpp`: Full recipe editor (colours, patterns, bands, noise, ribbon, textures).
  - `library_panel.cpp`: Recipe list, duplicate, add, delete, rename, reorder.
  - `preview_panel.cpp`: Interactive zoom/pan viewport, background colour toggle, auto-tiling tester.
  - `variant_panel.cpp`: Multi-axis variant matrix generation.
  - `batch_export_panel.cpp`: Multi-threaded batch exporter with progress queue.
  - `log_panel.cpp`: Real-time application log stream.

---

## 4. Verification & Testing

1. **Parity Grader (`corpus/verify.py`)**:
   - Renders 1161 ground-truth baked recipes from `corpus/manifest.json` against reference outputs.
   - Strict requirement: **1161/1161 passed with `maxDelta = 0` (zero pixel drift)**.
2. **C++ Unit Tests (`autotile_tests`)**:
   - Doctest suite covering `js_math` bit-exact vectors, field bounds, catalog synchronization, undo/redo monkey testing, and zip import.
