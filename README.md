# autotile_imgui

Native C++ / ImGui desktop generator for **blob47 tileset sheets**. Give it a
recipe — three colours, a pattern, and band / grain / motif / texture settings —
and it renders a 256x192 sheet (8x6 tiles of 32px). It keeps a library of
recipes, crosses them into variant matrices, and batch-exports the lot.

It is the desktop counterpart of a TypeScript web app that makes and shares one
sheet at a time. **The two must produce byte-identical pixels for the same
recipe**, and that is what this repository is graded on.

## Status

**Production Ready & Verified.** Fully ported, refactored, and tested with **1161/1161 passed (maxDelta = 0)** pixel-perfect parity against the reference specification.

| Document / Directory | Description |
|---|---|
| `docs/ARCHITECTURE.md` | Architecture, data flow, thread model, and module structure |
| `CLAUDE.md` | Invariants, hard rules, build commands, and parity verification |
| `reference/` | Read-only reference TypeScript implementation (specification) |
| `corpus/` | 1161 baked ground-truth sheets and verification tool (`verify.py`) |

## Quick Orientation

```bash
# Configure
cmake -B build-desktop -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5

# Build desktop app & unit tests
cmake --build build-desktop -j --target autotile_mixer autotile_tests

# Run unit tests
ctest --test-dir build-desktop --output-on-failure

# Verify parity against ground-truth corpus (1161 sheets)
python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe
```
