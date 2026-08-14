# reference/ — the web implementation, vendored read-only

These are **snapshots of the TypeScript the C++ port must reproduce**. They are
here so the port never has to reach into the other repository mid-task, and so a
diff can show whether upstream has moved.

**Do not edit anything in this directory.** Nothing here is compiled or shipped.
If a file looks wrong, it is not — it is the specification, and the C++ has to
match it, including the parts that look like bugs (see CLAUDE.md, "Faithful, not
better").

## Provenance

Copied 2026-08-13 from `D:\adna_tilemap_editor\autotile_mixer\src\utils\`.

Base commit `83e13626bd33c85aa08eab13a214fca3fed737b9`, **plus the uncommitted
working-tree changes of that session** — the `tileSize` removal from `Recipe` and
the `renderSheet.ts` extraction. So this snapshot is ahead of that commit. The
corpus in `../corpus` was baked from exactly this code, which is the thing that
actually matters: reference and corpus agree.

Refresh (from the other repo, if upstream moves):

```bash
cp D:/adna_tilemap_editor/autotile_mixer/src/utils/*.ts             D:/autotile_imgui/reference/
cp D:/adna_tilemap_editor/autotile_mixer/src/utils/patterns/generated.ts D:/autotile_imgui/reference/
```

...and then **re-bake the corpus too** (`npm run gen-corpus` over there), because
a reference that is newer than the corpus is worse than one that is older: the
tests would be checking the port against pixels the reference no longer produces.

## What each file is

| file | lines | role in the port |
|---|---:|---|
| `renderSheet.ts` | 230 | **Start here.** `Recipe -> paint args -> RGBA`. The one entry point; everything else is called from it. |
| `patternPaint.ts` | 412 | The per-tile paint loop. `paintPatternTileRGBA` is the function the whole port exists to reproduce. |
| `blob47Pattern.ts` | 663 | Level grids: thresholds the stored distance field into shade levels. |
| `blob47.ts` | 178 | 48-slot sheet layout, mask -> slot. |
| `generated.ts` | 582 | **Baked data. Transcribe, never regenerate.** Quantised distance field, base-62, one char per pixel. |
| `patternTexture.ts` | 1506 | 26 textures. The biggest single job. |
| `patternRibbon.ts` | 284 | 15 outline motifs. |
| `patternNoise.ts` | 173 | 3 grain algorithms + the target-zone gate. |
| `recipe.ts` | 240 | The schema and `sanitizeRecipe`'s clamping — the port must clamp identically or it renders a different recipe. |
| `recipeCodec.ts` | 380 | Share-code bit packing. Only needed for P10. |
| `AUTOTILE_PATTERN_BAKE.md` | — | How `generated.ts` was produced. Read before touching pattern data. |
| `AUTOTILE_SCHEMES.md` | — | Why blob47 is 47 tiles and not 256. |
