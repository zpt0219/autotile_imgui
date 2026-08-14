# blob47 parity corpus

Ground truth for the desktop C++ port (`D:\autotile_imgui`). Every case here is a
recipe plus the exact 256x192 RGBA sheet the web app renders it to. The port is
correct when it reproduces all of them.

The generator lives in the **web** repo, because it has to run the TypeScript it
is capturing. Regenerate only when the web renderer intentionally changed:

```bash
cd D:/adna_tilemap_editor/autotile_mixer
CORPUS_GIT_SHA=$(git -C .. rev-parse HEAD) npm run gen-corpus   # writes ../../autotile_imgui/corpus
```

Then review the diff here: the cases whose hashes moved are exactly the recipes
whose pixels changed. That diff is the mechanism that stops the web app drifting
away from the desktop port unnoticed.

```bash
# verify a renderer
python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe
python corpus/verify.py --exe ... --quick            # L0+L1, fast loop
python corpus/verify.py --actual out/ --diff-dir d/  # compare pre-rendered output
```

`verify.py` needs only the standard library. It uses `numpy` if importable, which
takes the full run from minutes to ~5 seconds; install it if you run this often.

## Layout

| path | what |
|---|---|
| `manifest.json` | index: id, tier, note, hashes, per-case allowance, sheet geometry |
| `recipes/<id>.json` | the input the renderer reads |
| `expected/<id>.png` | ground truth, 8-bit RGBA, non-interlaced, filter 0 |
| `expected/<id>.lvl.gz` | the level grid, one ASCII digit per pixel — used to explain failures |

## What the renderer must implement

Invoked once as:

```
<exe> --render-corpus <manifest.json> --out <dir>
```

For every case in the manifest it writes to `<dir>`:

- **`<id>.rgba`** — raw RGBA bytes, row-major, `width * height * 4`. Preferred.
  Or `<id>.png` (8-bit RGBA, non-interlaced) if raw is inconvenient. Raw wins
  when both exist: a parity test about pixels should not also be a test of two
  PNG encoders agreeing.
- **`<id>.lvl`** *(optional but do it)* — the level grid as one ASCII digit per
  pixel, same layout as the sheet. When present it is checked **first**, because
  a silhouette mismatch explains a colour mismatch and not the other way round.
  This is Level A of the porting plan, free.

The recipe is in `recipes/<id>.json` under `recipe`, and any paint parameters
that are not part of a Recipe are under `overrides` (currently `noiseTargets` and
`noiseColours` — see below).

## Comparison rules

1. **A pixel whose expected alpha is 0 is compared on alpha alone.** Nothing is
   drawn there, so its RGB is unspecified, and an encoder or a canvas is entitled
   to have zeroed it.
2. Every other channel must match within the case's `maxDelta`, which is `0`
   everywhere unless a specific libm divergence has been recorded with a reason.
3. `maxDelta` is **not** a similarity threshold, and widening it is not how a
   failing port gets fixed. The failure mode a float difference actually produces
   is a *quantiser boundary flip*: one ulp moves a pixel across a level
   threshold and it lands in a different shade entirely, tens of units away. The
   only thing an allowance of 1 forgives is the last-bit rounding of a palette
   computation, which is why anything above 1 should be argued for in writing.

## Tiers

| tier | cases | what it pins |
|---|---:|---|
| L0 | 1 | smoke |
| L1 | 209 | silhouette geometry — 11 patterns x band steps x outline width x band position x hard edge |
| L2 | 286 | **all 25 textures, on terrain A and on terrain B**, across shades / strength / motif size / cell / ripple, plus paired and transparent-B combinations |
| L3 | 97 | all 14 ribbon motifs across shades / coverage / period / invert / a 1px outline |
| L4 | 43 | band grain: every non-empty algorithm combination, strengths, seeds, all 7 target subsets, picked colours |
| L5 | 25 | colour edges: achromatic bases, saturated terrain A, custom ramps, transparent B |
| L6 | 500 | seeded fuzz (`mulberry32`, seed in the manifest) |

Coverage is enumerated from the app's own option lists (`TEXTURE_GROUPS`,
`RIBBON_GROUPS`, `PATTERN_GROUPS`), so adding a texture to the app adds it to the
corpus instead of quietly leaving the new one untested.

## Failure reports

A failure prints the first differing pixel located by slot / mask / tile
coordinate, the distribution of failures by level and by slot, how many sit on a
level boundary, and a guess at which layer is wrong. The guess reads delta
magnitude before spatial shape, because several band levels are one-pixel rings
— "nearly every failing pixel is next to an edge" is true of a thin level no
matter what went wrong with it.

```
FAIL  L0_base   [L0] The corpus base recipe, untouched.
  differing     3752 / 49152 (7.63%), beyond allowance: 3752
  max delta     1 on channel G
  first failure slot 0 (mask 6) at tile (14,6), sheet (14,6)
                expected rgba(32, 114, 201, 255)   actual rgba(32, 115, 201, 255)   delta 1
  by level      L3:3752(100% of level)
  boundary      3568 of 3752 failing pixels sit next to a level edge (95%)
  by slot       slot 1 (mask 10): 128, slot 15 (mask 5): 128, ...
  attribution   palette arithmetic for L3 - every pixel of those levels is off by <=1.
                Check shadeColour's HSV round trip and Math.round (JS rounds half away
                from zero; use floor(x+0.5), not std::round or banker's rounding).
```

`--diff-dir` writes a PNG per failing case: the expected sheet dimmed, with
failing pixels in red scaled by how wrong they are.

## Known gap: `noiseTargets` / `noiseColours`

These change pixels but were never added to the Recipe schema, so they do not
survive a save, a share link, or a trip through the desktop app. The corpus
reaches them through the `overrides` field instead of widening the schema (which
would change the share-code format). **If the desktop app is ever to render a
sheet made with non-default grain targets, this has to be settled** — either add
them to the recipe or accept that they are a web-only preview control.

## Regenerating

The generator is deterministic: an unchanged corpus re-bakes byte-identically, so
`git status` stays quiet unless the renderer's output actually moved. When it
does move, the diff names exactly which cases changed — which is the mechanism
that stops the web app drifting away from the desktop port unnoticed.
