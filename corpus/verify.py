#!/usr/bin/env python3
"""verify.py - check a blob47 renderer against the baked parity corpus.

The corpus in this directory is ground truth produced by the web app
(autotile_mixer/tools/gen-corpus.ts). This script renders the same recipes with
another implementation - the C++ desktop port - and reports, per case, whether
the pixels agree.

    python verify.py --exe path/to/autotile_mixer.exe
    python verify.py --actual out/           # compare already-rendered output
    python verify.py --exe ... --quick       # L0+L1 only, for a fast loop
    python verify.py --exe ... --tier L2     # one tier
    python verify.py --actual out/ --max-delta 1 --diff-dir diff/

Comparison rules
----------------
* A pixel whose expected alpha is 0 is compared on ALPHA ALONE. Nothing is
  drawn there, so its RGB is unspecified - and a canvas or an encoder is
  entitled to have zeroed it.
* Otherwise every channel must match within `maxDelta` (per case, from the
  manifest; 0 unless a specific libm divergence has been recorded).
* maxDelta is NOT a similarity threshold. A quantiser boundary flip - the
  failure mode a float difference actually produces - lands the pixel in a
  different shade entirely and blows past any sane allowance. The only thing
  a delta of 1 forgives is the last-bit rounding of a palette computation.

What the renderer has to provide
--------------------------------
With --exe, the binary is invoked once as

    <exe> --render-corpus <manifest.json> --out <dir>

and is expected to write <dir>/<id>.rgba (raw RGBA bytes, row-major) or
<dir>/<id>.png for every case in the manifest. Raw is preferred and is what the
comparison uses when both exist: it keeps two PNG encoders out of a test that is
about pixels, not containers. Optionally it may also write <dir>/<id>.lvl, the
level grid as one ASCII digit per pixel; if present it is checked first, because
a silhouette mismatch explains a colour mismatch and not the other way round.

Only the standard library is required. numpy and Pillow are used if importable,
purely for speed.
"""

import argparse
import gzip
import json
import struct
import subprocess
import sys
import tempfile
import zlib
from collections import Counter
from pathlib import Path

try:
    import numpy as np
except ImportError:
    np = None


# --------------------------------------------------------------------------
# PNG decoding
# --------------------------------------------------------------------------

def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def decode_png(data):
    """Decode an 8-bit RGBA non-interlaced PNG. Returns (width, height, bytearray)."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos = 8
    width = height = None
    idat = bytearray()
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        ctype = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, depth, color, comp, filt, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or color != 6:
                raise ValueError(f"expected 8-bit RGBA, got depth={depth} colour_type={color}")
            if interlace != 0:
                raise ValueError("interlaced PNG is not supported")
        elif ctype == b"IDAT":
            idat += body
        elif ctype == b"IEND":
            break
    if width is None:
        raise ValueError("no IHDR")

    raw = zlib.decompress(bytes(idat))
    stride = width * 4
    out = bytearray(height * stride)

    # Filter 0 on every row (what gen-corpus.ts emits) unfilters to a straight
    # copy; the other four are what a general encoder such as stb_image_write
    # produces, so they are all implemented.
    prev = bytearray(stride)
    src = 0
    for y in range(height):
        ftype = raw[src]
        src += 1
        line = bytearray(raw[src:src + stride])
        src += stride
        if ftype == 0:
            pass
        elif ftype == 1:
            for i in range(4, stride):
                line[i] = (line[i] + line[i - 4]) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                a = line[i - 4] if i >= 4 else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                a = line[i - 4] if i >= 4 else 0
                c = prev[i - 4] if i >= 4 else 0
                line[i] = (line[i] + _paeth(a, prev[i], c)) & 0xFF
        else:
            raise ValueError(f"bad filter type {ftype} on row {y}")
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, out


def encode_png(rgba, width, height):
    """Minimal 8-bit RGBA writer, filter 0 - used only for the diff images."""
    def chunk(tag, body):
        return (struct.pack(">I", len(body)) + tag + body
                + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF))

    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        raw += rgba[y * stride:(y + 1) * stride]
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def load_image(path):
    """Read a case's pixels from either a raw .rgba dump or a .png."""
    if path.suffix == ".rgba":
        return bytearray(path.read_bytes())
    _, _, px = decode_png(path.read_bytes())
    return px


# --------------------------------------------------------------------------
# Comparison
# --------------------------------------------------------------------------

class Diff:
    def __init__(self):
        self.differing = 0        # pixels with any delta at all
        self.failing = 0          # pixels beyond the allowance
        self.max_delta = 0
        self.max_channel = None
        self.first_fail = None    # (index, expected tuple, actual tuple, delta)
        self.alpha_mismatch = 0
        self.fail_indices = []    # capped; enough for the attribution histograms

FAIL_SAMPLE_CAP = 200000
CHANNELS = ("R", "G", "B", "A")


def compare(exp, act, max_delta):
    """Per-pixel comparison under the alpha-0 rule. Returns a Diff."""
    d = Diff()
    if len(exp) != len(act):
        raise ValueError(f"size mismatch: expected {len(exp)} bytes, got {len(act)}")

    if np is not None:
        e = np.frombuffer(bytes(exp), dtype=np.uint8).reshape(-1, 4).astype(np.int16)
        a = np.frombuffer(bytes(act), dtype=np.uint8).reshape(-1, 4).astype(np.int16)
        delta = np.abs(e - a)
        opaque = e[:, 3] != 0
        # Where expected alpha is 0, only alpha counts.
        delta[~opaque, 0:3] = 0
        per_pixel = delta.max(axis=1)
        d.alpha_mismatch = int((delta[:, 3] != 0).sum())
        nz = np.flatnonzero(per_pixel)
        d.differing = int(nz.size)
        fail = np.flatnonzero(per_pixel > max_delta)
        d.failing = int(fail.size)
        if nz.size:
            d.max_delta = int(per_pixel.max())
            worst = int(np.argmax(per_pixel))
            d.max_channel = CHANNELS[int(np.argmax(delta[worst]))]
        if fail.size:
            i = int(fail[0])
            d.first_fail = (i, tuple(int(v) for v in e[i]), tuple(int(v) for v in a[i]),
                            int(per_pixel[i]))
            d.fail_indices = fail[:FAIL_SAMPLE_CAP].tolist()
        return d

    n = len(exp) // 4
    for i in range(n):
        o = i * 4
        ea, aa = exp[o + 3], act[o + 3]
        dl = abs(ea - aa)
        ch = 3
        if ea != 0:
            for c in range(3):
                dc = abs(exp[o + c] - act[o + c])
                if dc > dl:
                    dl, ch = dc, c
        if dl == 0:
            continue
        if abs(ea - aa) != 0:
            d.alpha_mismatch += 1
        d.differing += 1
        if dl > d.max_delta:
            d.max_delta, d.max_channel = dl, CHANNELS[ch]
        if dl > max_delta:
            d.failing += 1
            if d.first_fail is None:
                d.first_fail = (i, tuple(exp[o:o + 4]), tuple(act[o:o + 4]), dl)
            if len(d.fail_indices) < FAIL_SAMPLE_CAP:
                d.fail_indices.append(i)
    return d


# --------------------------------------------------------------------------
# Attribution
# --------------------------------------------------------------------------

def on_level_boundary(levels, idx, width, height):
    """True when a pixel's 4-neighbourhood in the level grid is not uniform."""
    x, y = idx % width, idx // width
    c = levels[idx]
    if x > 0 and levels[idx - 1] != c:
        return True
    if x + 1 < width and levels[idx + 1] != c:
        return True
    if y > 0 and levels[idx - width] != c:
        return True
    if y + 1 < height and levels[idx + width] != c:
        return True
    return False


def attribute(d, levels, sheet):
    """Turn a set of failing pixels into a guess about which layer is wrong.

    The discriminator is delta MAGNITUDE first, spatial shape second - and in
    that order, because several levels of the band are one-pixel rings, so
    "nearly every failing pixel sits next to a level edge" is true of a thin
    level whatever went wrong with it. Reading boundary proximity first would
    call every palette bug a float bug.

      * delta <= 2 filling a level    -> the palette arithmetic for that level
      * delta <= 2 sprinkled          -> rounding in a ramp/mix, not a level flip
      * larger, hugging boundaries    -> a quantiser flip, i.e. a float difference
      * larger, inside one level      -> that level's texture/motif algorithm
    """
    width, height = sheet["width"], sheet["height"]
    tile, cols = sheet["tileSize"], sheet["columns"]
    layout = sheet["layout"]

    by_level = Counter()
    by_slot = Counter()
    boundary = 0
    for idx in d.fail_indices:
        x, y = idx % width, idx // width
        slot = (y // tile) * cols + (x // tile)
        by_slot[slot] += 1
        if levels:
            by_level[levels[idx]] += 1
            if on_level_boundary(levels, idx, width, height):
                boundary += 1

    n = max(1, len(d.fail_indices))
    frac_boundary = boundary / n
    lines = []

    # How much of each affected level went wrong. A level that is ~entirely
    # wrong points at the colour that level is painted with; a level that is
    # partly wrong points at whatever is painted *into* it.
    coverage = {}
    if levels:
        level_totals = Counter(levels)
        for lv, c in by_level.items():
            coverage[lv] = c / max(1, level_totals[lv])
        dist = " ".join(f"L{lv}:{c}({coverage[lv] * 100:.0f}% of level)"
                        for lv, c in sorted(by_level.items()))
        lines.append(f"  by level      {dist}")
        lines.append(f"  boundary      {boundary} of {n} failing pixels sit next to a "
                     f"level edge ({frac_boundary * 100:.0f}%)")
    top = ", ".join(f"slot {s} (mask {layout[s] if s < len(layout) else '?'}): {c}"
                    for s, c in by_slot.most_common(5))
    lines.append(f"  by slot       {top}")

    if d.alpha_mismatch > n * 0.5:
        guess = ("alpha, not colour - the transparentB role test or the level->role "
                 "mapping disagrees.")
    elif not levels:
        guess = "no level grid available; re-bake the corpus to get attribution."
    elif d.max_delta <= 2 and coverage and min(coverage.values()) > 0.9:
        lv = ", ".join(f"L{k}" for k in sorted(coverage))
        guess = (f"palette arithmetic for {lv} - every pixel of those levels is off by "
                 f"<={d.max_delta}. Check shadeColour's HSV round trip and Math.round "
                 f"(JS rounds half away from zero; use floor(x+0.5), not std::round or "
                 f"banker's rounding).")
    elif d.max_delta <= 2:
        guess = (f"off-by-<={d.max_delta} on part of a level - a ramp/mix rounding "
                 f"difference rather than a level flip. Check textureRamp / ribbon ramp.")
    elif frac_boundary > 0.8:
        guess = ("quantiser boundary flip - a float difference, not a logic error. "
                 "Check js_math (hypot / sin / atan2) before touching the algorithm.")
    elif len(by_level) == 1:
        lv = next(iter(by_level))
        guess = (f"confined to level {lv} and away from its edges - the texture or motif "
                 f"painted into that level is wrong.")
    else:
        guess = "spread across levels - likely the level table or the band geometry itself."
    lines.append(f"  attribution   {guess}")
    return lines


def write_diff_png(path, exp, act, d, width, height):
    """Expected, dimmed, with failing pixels in red scaled by how wrong they are."""
    out = bytearray(len(exp))
    for i in range(0, len(exp), 4):
        out[i] = exp[i] // 3
        out[i + 1] = exp[i + 1] // 3
        out[i + 2] = exp[i + 2] // 3
        out[i + 3] = 255
    for idx in d.fail_indices:
        o = idx * 4
        dl = max(abs(exp[o + c] - act[o + c]) for c in range(4))
        out[o] = min(255, 100 + dl * 3)
        out[o + 1] = 0
        out[o + 2] = 0
        out[o + 3] = 255
    path.write_bytes(encode_png(out, width, height))


# --------------------------------------------------------------------------
# Driver
# --------------------------------------------------------------------------

def render_with_exe(exe, manifest_path, out_dir):
    cmd = [str(exe), "--render-corpus", str(manifest_path), "--out", str(out_dir)]
    print("running:", " ".join(cmd))
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stdout, file=sys.stderr)
        print(proc.stderr, file=sys.stderr)
        sys.exit(f"renderer exited {proc.returncode}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--corpus", type=Path, default=Path(__file__).parent,
                    help="corpus directory (default: this file's directory)")
    ap.add_argument("--exe", type=Path, help="renderer to invoke")
    ap.add_argument("--actual", type=Path, help="directory of already-rendered output")
    ap.add_argument("--tier", action="append", help="only these tiers (repeatable)")
    ap.add_argument("--quick", action="store_true", help="shorthand for --tier L0 --tier L1")
    ap.add_argument("--id", action="append", help="only these case ids (repeatable)")
    ap.add_argument("--max-delta", type=int, default=None,
                    help="override every case's allowance (diagnostic only)")
    ap.add_argument("--diff-dir", type=Path, help="write a diff PNG per failing case")
    ap.add_argument("--max-report", type=int, default=10, help="detailed reports to print")
    args = ap.parse_args()

    if not args.exe and not args.actual:
        ap.error("one of --exe or --actual is required")

    manifest_path = args.corpus / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    sheet = manifest["sheet"]
    width, height = sheet["width"], sheet["height"]

    cases = manifest["cases"]
    tiers = set(args.tier or [])
    if args.quick:
        tiers |= {"L0", "L1"}
    if tiers:
        cases = [c for c in cases if c["tier"] in tiers]
    if args.id:
        wanted = set(args.id)
        cases = [c for c in cases if c["id"] in wanted]
    if not cases:
        sys.exit("no cases selected")

    if np is None:
        print("note: numpy not importable - falling back to the pure-python comparator, "
              "which is correct but slow. `pip install numpy` if this matters.")

    tmp = None
    if args.actual:
        actual_dir = args.actual
    else:
        tmp = tempfile.TemporaryDirectory()
        actual_dir = Path(tmp.name)
        render_with_exe(args.exe, manifest_path, actual_dir)

    passed, failed, missing = [], [], []
    reported = 0
    if args.diff_dir:
        args.diff_dir.mkdir(parents=True, exist_ok=True)

    for case in cases:
        cid = case["id"]
        raw = actual_dir / f"{cid}.rgba"
        png = actual_dir / f"{cid}.png"
        src = raw if raw.exists() else png
        if not src.exists():
            missing.append(cid)
            continue

        exp = load_image(args.corpus / case["image"])
        act = load_image(src)

        allowance = args.max_delta if args.max_delta is not None else case.get("maxDelta", 0)

        levels = None
        lvl_path = args.corpus / case["levels"]
        if lvl_path.exists():
            levels = gzip.decompress(lvl_path.read_bytes()).decode("latin1")

        # Silhouette first: if the renderer dumped its own level grid and it
        # disagrees, every colour difference downstream is a consequence.
        act_lvl = actual_dir / f"{cid}.lvl"
        level_note = None
        if act_lvl.exists() and levels is not None:
            got = act_lvl.read_text(encoding="latin1").strip()
            if got != levels:
                nbad = sum(1 for a, b in zip(got, levels) if a != b) + abs(len(got) - len(levels))
                first = next((i for i, (a, b) in enumerate(zip(got, levels)) if a != b), -1)
                level_note = (f"LEVEL GRID differs in {nbad} positions, first at "
                              f"({first % width},{first // width}) "
                              f"expected '{levels[first]}' got '{got[first]}'")

        try:
            d = compare(exp, act, allowance)
        except ValueError as exc:
            failed.append(cid)
            print(f"FAIL  {cid}\n  {exc}")
            continue

        if d.failing == 0 and level_note is None:
            passed.append((cid, d.differing))
            continue

        failed.append(cid)
        if reported < args.max_report:
            reported += 1
            total_px = width * height
            print(f"\nFAIL  {cid}   [{case['tier']}] {case.get('note','')}")
            if level_note:
                print(f"  {level_note}")
                print("  -> the silhouette is wrong; fix that before reading the colour "
                      "numbers below.")
            print(f"  differing     {d.differing} / {total_px} "
                  f"({d.differing / total_px * 100:.2f}%), beyond allowance: {d.failing}"
                  + (f", allowance {allowance}" if allowance else ""))
            print(f"  max delta     {d.max_delta} on channel {d.max_channel}"
                  + (f"   (alpha mismatches: {d.alpha_mismatch})" if d.alpha_mismatch else ""))
            if d.first_fail:
                idx, e, a, dl = d.first_fail
                x, y = idx % width, idx // width
                tile, cols = sheet["tileSize"], sheet["columns"]
                slot = (y // tile) * cols + (x // tile)
                mask = sheet["layout"][slot] if slot < len(sheet["layout"]) else "?"
                print(f"  first failure slot {slot} (mask {mask}) at tile ({x % tile},{y % tile}), "
                      f"sheet ({x},{y})")
                print(f"                expected rgba{e}   actual rgba{a}   delta {dl}")
            for line in attribute(d, levels, sheet):
                print(line)
            if args.diff_dir:
                p = args.diff_dir / f"{cid}.png"
                write_diff_png(p, exp, act, d, width, height)
                print(f"  diff image    {p}")

    print("\n" + "=" * 72)
    tolerated = sum(1 for _, n in passed if n > 0)
    print(f"passed {len(passed)}  failed {len(failed)}  missing {len(missing)}  "
          f"(of {len(cases)} selected)")
    if tolerated:
        print(f"note: {tolerated} passing case(s) were not byte-exact but stayed within "
              f"their allowance - that is a divergence, just a small one.")
    if missing:
        print(f"missing output for {len(missing)} case(s), e.g. {', '.join(missing[:5])}")
    if failed and reported < len(failed):
        print(f"({len(failed) - reported} further failures not detailed; raise --max-report)")

    if tmp:
        tmp.cleanup()
    sys.exit(1 if (failed or missing) else 0)


if __name__ == "__main__":
    main()
