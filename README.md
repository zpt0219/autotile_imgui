# autotile_imgui

Native C++ / ImGui desktop generator for **blob47 tileset sheets**. Give it a
recipe — three colours, a pattern, and band / grain / motif / texture settings —
and it renders a 256x192 sheet (8x6 tiles of 32px). It keeps a library of
recipes, crosses them into variant matrices, and batch-exports the lot.

It is the desktop counterpart of a TypeScript web app that makes and shares one
sheet at a time. **The two must produce byte-identical pixels for the same
recipe**, and that is what this repository is graded on.

## Status

Not implemented yet. What exists is everything needed to implement it:

| | |
|---|---|
| `docs/PLAN.md` | architecture, the ViewModel / command-queue / fan-out design, phases, known traps |
| `docs/TASKS.md` | the work breakdown, one task at a time, each with the command that decides it is done |
| `CLAUDE.md` | the rules that are not negotiable, and the build/test commands |
| `reference/` | read-only snapshot of the TypeScript being ported — the specification |
| `corpus/` | 1161 baked ground-truth sheets — the grader |

## Quick orientation

```bash
cmake -B build-desktop -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-desktop -j --target autotile_mixer
ctest --test-dir build-desktop --output-on-failure

python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe --quick
```

The last command is the one that matters. When it reports **1161/1161** with no
filter, the port is done.
