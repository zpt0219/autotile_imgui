const fs = require('fs');
const path = require('path');

// =========================================================================
// 1. Math vectors
// =========================================================================
function doubleToHex(d) {
    const buf = Buffer.alloc(8);
    buf.writeDoubleBE(d, 0);
    return buf.toString('hex');
}

const mathVectors = {
    sin: [],
    cos: [],
    round: [],
    hypot: [],
    imul: [],
    atan2: []
};

// 500 rows for sin & cos
for (let i = 0; i < 500; i++) {
    const x = -50.0 + (i * 100.0) / 499.0;
    mathVectors.sin.push({ x, x_hex: doubleToHex(x), res: doubleToHex(Math.sin(x)) });
    mathVectors.cos.push({ x, x_hex: doubleToHex(x), res: doubleToHex(Math.cos(x)) });
}

// 100 rows for round
for (let i = 0; i < 100; i++) {
    const x = -10.0 + (i * 20.0) / 99.0;
    mathVectors.round.push({ x, x_hex: doubleToHex(x), res: doubleToHex(Math.round(x)) });
}

// 100 rows for imul
for (let i = 0; i < 10; i++) {
    for (let j = 0; j < 10; j++) {
        const a = (i * 1234567 - 5000000) | 0;
        const b = (j * 7654321 - 3000000) | 0;
        mathVectors.imul.push({ a, b, res: Math.imul(a, b) });
    }
}

// 630 rows for atan2 & hypot
for (let i = 0; i < 30; i++) {
    for (let j = 0; j < 21; j++) {
        const y = -15.0 + i * 1.0;
        const x = -10.0 + j * 1.0;
        mathVectors.atan2.push({ y, x, y_hex: doubleToHex(y), x_hex: doubleToHex(x), res: doubleToHex(Math.atan2(y, x)) });
        mathVectors.hypot.push({ x, y, x_hex: doubleToHex(x), y_hex: doubleToHex(y), res: doubleToHex(Math.hypot(x, y)) });
    }
}

fs.writeFileSync(path.join(__dirname, 'js_math_vectors.json'), JSON.stringify(mathVectors, null, 2));

// =========================================================================
// 2. Palette vectors (Verbatim from reference/blob47Pattern.ts & reference/patternPaint.ts)
// =========================================================================
const SHADE_RECIPES = {
  terrainA: { hue: 0, greyHue: 0.541667, sat: 0.129032, val: 1.000000 },
  terrainB: { hue: 0.012037, greyHue: 0.012037, sat: 0.166667, val: 0.888889 },
  edge:     { hue: 0, greyHue: 0, sat: 0, val: 1 },
};

function clamp255(v) { return Math.max(0, Math.min(255, Math.round(v))); }

function rgbToHsv({ r, g, b }) {
  const R = r / 255, G = g / 255, B = b / 255;
  const mx = Math.max(R, G, B);
  const mn = Math.min(R, G, B);
  const range = mx - mn;
  if (range === 0) return [0, 0, mx];
  const rc = (mx - R) / range;
  const gc = (mx - G) / range;
  const bc = (mx - B) / range;
  let h;
  if (R === mx) h = bc - gc;
  else if (G === mx) h = 2 + rc - bc;
  else h = 4 + gc - rc;
  h = (h / 6) % 1;
  return [h < 0 ? h + 1 : h, range / mx, mx];
}

function hsvToRgb(h, s, v) {
  if (s === 0) return { r: clamp255(v * 255), g: clamp255(v * 255), b: clamp255(v * 255) };
  const i = Math.floor(h * 6);
  const f = h * 6 - i;
  const p = v * (1 - s);
  const q = v * (1 - s * f);
  const t = v * (1 - s * (1 - f));
  const table = [
    [v, t, p], [q, v, p], [p, v, t], [p, q, v], [t, p, v], [v, p, q],
  ];
  const [R, G, B] = table[((i % 6) + 6) % 6];
  return { r: clamp255(R * 255), g: clamp255(G * 255), b: clamp255(B * 255) };
}

function shadeColour(c, role, t = 1) {
  if (t <= 0) return c;
  const recipe = SHADE_RECIPES[role];
  const [h0, s0, v0] = rgbToHsv(c);
  const h = s0 < 1e-6 ? recipe.greyHue : (h0 + recipe.hue * t + 1) % 1;
  return hsvToRgb(
    h,
    Math.max(0, Math.min(1, s0 + recipe.sat * t)),
    Math.max(0, Math.min(1, v0 * (1 + (recipe.val - 1) * t)))
  );
}

const paletteVectors = [];
const testColours = [
    { r: 248, g: 248, b: 248 },
    { r: 58, g: 127, b: 201 },
    { r: 93, g: 168, b: 50 },
    { r: 232, g: 213, b: 160 },
    { r: 0, g: 0, b: 0 },
    { r: 255, g: 0, b: 0 },
    { r: 0, g: 255, b: 0 },
    { r: 0, g: 0, b: 255 },
    { r: 128, g: 128, b: 128 },
    { r: 200, g: 150, b: 100 }
];
const roles = ['terrainA', 'terrainB', 'edge'];
const ts = [0.0, 0.25, 0.5, 0.75, 1.0];

for (const c of testColours) {
    for (const role of roles) {
        for (const t of ts) {
            paletteVectors.push({
                c, role, t,
                res: shadeColour(c, role, t)
            });
        }
    }
}

fs.writeFileSync(path.join(__dirname, 'palette_vectors.json'), JSON.stringify(paletteVectors, null, 2));

// =========================================================================
// 3. Noise vectors (Verbatim from reference/patternNoise.ts)
// =========================================================================
const BLUE = [
  228, 182, 246, 42, 98, 29, 127, 44, 211, 8, 83, 164, 217, 2, 93, 123,
  85, 26, 135, 74, 163, 238, 194, 89, 175, 117, 225, 31, 106, 151, 252, 40,
  174, 109, 196, 218, 3, 141, 60, 229, 25, 156, 49, 199, 66, 189, 55, 210,
  9, 236, 53, 91, 179, 111, 19, 149, 76, 250, 100, 140, 239, 16, 130, 158,
  65, 147, 125, 36, 247, 70, 221, 185, 124, 193, 4, 171, 84, 114, 227, 97,
  249, 192, 17, 201, 134, 167, 46, 102, 33, 64, 208, 45, 219, 27, 178, 38,
  79, 169, 103, 231, 82, 10, 204, 233, 159, 242, 92, 126, 157, 63, 139, 213,
  119, 52, 32, 155, 59, 118, 144, 86, 14, 136, 181, 18, 253, 195, 96, 1,
  202, 244, 132, 214, 180, 254, 41, 190, 113, 54, 222, 75, 108, 48, 226, 146,
  23, 183, 87, 5, 105, 24, 209, 68, 232, 165, 34, 148, 188, 12, 168, 69,
  110, 62, 153, 230, 72, 173, 131, 152, 0, 99, 207, 121, 235, 90, 129, 237,
  160, 43, 206, 116, 35, 241, 94, 50, 248, 184, 73, 21, 56, 212, 39, 197,
  95, 251, 20, 138, 162, 198, 22, 220, 122, 37, 137, 240, 172, 150, 81, 6,
  133, 216, 77, 186, 88, 57, 112, 177, 80, 161, 215, 101, 13, 115, 245, 187,
  166, 107, 51, 224, 7, 234, 143, 15, 200, 61, 28, 191, 78, 223, 58, 30,
  67, 11, 154, 120, 203, 170, 71, 255, 104, 145, 243, 128, 47, 176, 142, 205,
];

const BAYER8 = [
  0, 32, 8, 40, 2, 34, 10, 42,
  48, 16, 56, 24, 50, 18, 58, 26,
  12, 44, 4, 36, 14, 46, 6, 38,
  60, 28, 52, 20, 62, 30, 54, 22,
  3, 35, 11, 43, 1, 33, 9, 41,
  51, 19, 59, 27, 49, 17, 57, 25,
  15, 47, 7, 39, 13, 45, 5, 37,
  63, 31, 55, 23, 61, 29, 53, 21,
];

const AMOUNT = {
  white: 0.22,
  blue: 0.24,
  ordered: 0.19,
};
const MAX_SHARE = 0.5;

function hash01(x, y, seed) {
  let n = Math.imul(x, 374761393) + Math.imul(y, 668265263) + Math.imul(seed, 1442695041);
  n = Math.imul(n ^ (n >>> 13), 1274126177);
  return ((n ^ (n >>> 16)) >>> 0) / 4294967296;
}

function seedBits(seed, salt) {
  if (seed === 0) return 0;
  let n = Math.imul(seed, 0x9e3779b1) ^ Math.imul(salt, 0x85ebca6b);
  n = Math.imul(n ^ (n >>> 15), 0xc2b2ae35);
  return (n ^ (n >>> 13)) >>> 0;
}

const wrap16 = (v) => ((v % 16) + 16) % 16;

function sample(noise, x, y, seed) {
  const px = wrap16(x);
  const py = wrap16(y);
  switch (noise) {
    case 'blue': {
      const s = seedBits(seed, 1);
      let ax = (px + (s & 15)) & 15;
      let ay = (py + ((s >>> 4) & 15)) & 15;
      const t = (s >>> 8) & 7;
      if (t & 1) { const k = ax; ax = ay; ay = k; }
      if (t & 2) ax = 15 - ax;
      if (t & 4) ay = 15 - ay;
      return BLUE[ay * 16 + ax] / 256;
    }
    case 'ordered': {
      const s = seedBits(seed, 2);
      const ax = (px + (s & 7)) & 7;
      const ay = (py + ((s >>> 3) & 7)) & 7;
      return BAYER8[ay * 8 + ax] / 64;
    }
    case 'white': return hash01(px, py, seed);
    default: return 0.5;
  }
}

function stepOf(noise, x, y, seed, strength) {
  const p = Math.min(MAX_SHARE, (AMOUNT[noise] ?? 0) * strength);
  if (p <= 0) return 0;
  const n = sample(noise, x, y, seed);
  if (n < p) return -1;
  if (n >= 1 - p) return 1;
  return 0;
}

function noiseStep(noises, x, y, seed, strength) {
  if (!noises || noises.length === 0 || strength <= 0) return 0;
  let total = 0;
  for (const id of noises) {
    total += stepOf(id, x, y, seed, strength);
  }
  return Math.max(-1, Math.min(1, total));
}

const noiseVectors = [];
const noiseConfigs = [
    ['white'],
    ['blue'],
    ['ordered'],
    ['white', 'blue'],
    ['blue', 'ordered'],
    ['white', 'blue', 'ordered']
];

for (let x = 0; x < 16; x += 3) {
    for (let y = 0; y < 16; y += 3) {
        for (let seed of [0, 42, 1234]) {
            for (let strength of [0.5, 1.0, 1.5, 2.0]) {
                for (let noises of noiseConfigs) {
                    noiseVectors.push({
                        x, y, seed, strength, noises,
                        res: noiseStep(noises, x, y, seed, strength)
                    });
                }
            }
        }
    }
}

fs.writeFileSync(path.join(__dirname, 'noise_vectors.json'), JSON.stringify(noiseVectors, null, 2));

// =========================================================================
// 4. Stored-field bounds, read straight out of reference/generated.ts
//
// Unlike the sections above, nothing here is retyped: the field strings are
// parsed out of the reference file itself and decoded with FIELD_CHARS as
// blob47Pattern.ts defines it (printable ASCII 35..126 less ' and \). What
// lands in field_bounds.json is therefore a property of the specification,
// not of anything this repo wrote — which is the whole point of the T2.1
// gate. Regenerate with:  node tests/data/dump_reference_vectors.js
// =========================================================================
const FIELD_CHARS = (() => {
  let s = '';
  for (let c = 35; c <= 126; c++) {
    if (c === 39 || c === 92) continue;
    s += String.fromCharCode(c);
  }
  return s;
})();

const CHAR_VALUE = (() => {
  const t = new Array(128).fill(0);
  for (let i = 0; i < FIELD_CHARS.length; i++) t[FIELD_CHARS.charCodeAt(i)] = i;
  return t;
})();

const FIELD_STEP = 0.25;

const generatedPath = path.join(__dirname, '..', '..', 'reference', 'generated.ts');
const generatedSrc = fs.readFileSync(generatedPath, 'utf8');

// `  <pattern>: {` opens a block; `    <mask>: '<1024 chars>',` is one entry.
const fieldBounds = [];
let currentPattern = null;
for (const line of generatedSrc.split(/\r?\n/)) {
  const patMatch = line.match(/^\s{2}([A-Za-z_][A-Za-z0-9_]*):\s*\{\s*$/);
  if (patMatch) { currentPattern = patMatch[1]; continue; }
  if (/^\s{2}\},?\s*$/.test(line)) { currentPattern = null; continue; }
  if (!currentPattern) continue;

  const maskMatch = line.match(/^\s{4}(\d+):\s*'([^']*)'/);
  if (!maskMatch) continue;

  const mask = parseInt(maskMatch[1], 10);
  const field = maskMatch[2];
  if (field.length !== 1024) {
    throw new Error(`${currentPattern} mask ${mask}: expected 1024 chars, got ${field.length}`);
  }

  let minIdx = Infinity, maxIdx = -Infinity;
  for (let i = 0; i < field.length; i++) {
    const v = CHAR_VALUE[field.charCodeAt(i)];
    if (v < minIdx) minIdx = v;
    if (v > maxIdx) maxIdx = v;
  }

  fieldBounds.push({
    pattern: currentPattern,
    mask,
    length: field.length,
    minIndex: minIdx,
    maxIndex: maxIdx,
    minDistance: minIdx * FIELD_STEP,
    maxDistance: maxIdx * FIELD_STEP,
  });
}

if (fieldBounds.length === 0) {
  throw new Error('parsed no fields out of reference/generated.ts - the format changed');
}

fs.writeFileSync(path.join(__dirname, 'field_bounds.json'), JSON.stringify(fieldBounds, null, 2));
console.log(`field_bounds.json: ${fieldBounds.length} (pattern, mask) entries from reference/generated.ts`);

console.log('Successfully regenerated all reference vector json files with exact recipe values!');
