#include "pattern_texture.h"
#include "texture_tables.h"
#include "pattern_noise.h"
#include "pattern_hash.h"
#include "catalog.h"
#include "js_math.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace atm {

const RGB WATER_DOT_COLOUR = { 215, 215, 215 };
const RGB DEFAULT_TEXTURE_TERRAIN_A = texture_colour({ 58, 127, 201 }, 1.0f);
const RGB DEFAULT_TEXTURE_TERRAIN_B = texture_colour({ 93, 168, 50 }, 1.0f);

static const int BAKED_RANKS = 4;
static const int JOINT_RANK = 4;
static const int FACE_RANKS = 4;
// Salt for texture seed derivation. Reference: renderSheet.ts textureShadeAt()
static const int32_t TEXTURE_SALT = 0x5bd1;

static int rank_to_shade(int rank, float amount, int shades) {
    float a = std::min(1.0f, amount);
    auto cl = [shades](int k) { return std::max(0, std::min(shades, k)); };
    if (rank >= BAKED_RANKS) {
        return cl(static_cast<int>(js_math::round(static_cast<float>(shades) * a)));
    }
    float num = static_cast<float>(rank * (shades - 1)) * a;
    return std::min(shades - 1, cl(static_cast<int>(js_math::round(num / static_cast<float>(BAKED_RANKS - 1)))));
}

static int baked_shade(
    const char* table,
    int size,
    int x,
    int y,
    uint32_t seed,
    float amount,
    int shades,
    bool joint_at_zero = false
) {
    int m = size - 1;
    int px = js_math::wrap(x + static_cast<int>(seed & m), size);
    int py = js_math::wrap(y + static_cast<int>((seed >> 4) & m), size);
    int raw = table[py * size + px] - '0';
    return rank_to_shade(joint_at_zero ? js_math::wrap(raw - 1, BAKED_RANKS + 1) : raw, amount, shades);
}

static inline float smooth(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static float ripple_field(int x, int y, int32_t seed, float per_x) {
    const int per_y = 32;
    float fx = (static_cast<float>(x) / 32.0f) * per_x;
    int iy = js_math::wrap(y, per_y);
    int x0 = static_cast<int>(std::floor(fx));
    float u = smooth(fx - static_cast<float>(x0));
    int iper_x = static_cast<int>(per_x);
    auto h = [iy, seed, iper_x](int ix) {
        return hash01(js_math::wrap(ix, iper_x), iy, seed);
    };
    return h(x0) * (1.0f - u) + h(x0 + 1) * u;
}

static float ripple_diag_field(int x, int y, int32_t seed, float per_diag) {
    int diag_line = js_math::wrap(x - y, 32);
    float along = (static_cast<float>(x + y) / 32.0f) * per_diag;
    int x0 = static_cast<int>(std::floor(along));
    float u = smooth(along - static_cast<float>(x0));
    int iper_diag = static_cast<int>(per_diag);
    auto h = [diag_line, seed, iper_diag](int ix) {
        return hash01(js_math::wrap(ix, iper_diag), diag_line, seed);
    };
    return h(x0) * (1.0f - u) + h(x0 + 1) * u;
}

struct VoronoiRes {
    float f1, f2;
    int nearest_x, nearest_y;
};

static VoronoiRes cells_at(int x, int y, int32_t seed, int per) {
    float fx = (static_cast<float>(x) / 32.0f) * static_cast<float>(per);
    float fy = (static_cast<float>(y) / 32.0f) * static_cast<float>(per);
    int cx = static_cast<int>(std::floor(fx));
    int cy = static_cast<int>(std::floor(fy));
    float f1 = 9.0f, f2 = 9.0f;
    int nearest_x = 0, nearest_y = 0;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int ix = cx + dx;
            int iy = cy + dy;
            int wx = js_math::wrap(ix, per);
            int wy = js_math::wrap(iy, per);

            // Jitter salts for cell Voronoi sites. Reference: renderSheet.ts cells()
            float px = (per >= 3)
                ? static_cast<float>(ix) + hash01(wx, wy, seed ^ 0x3c6ef3)
                : static_cast<float>(ix) + static_cast<float>(wy % 2) * 0.5f + 0.16f + hash01(wx, wy, seed ^ 0x3c6ef3) * 0.55f;
            float py = (per >= 3)
                ? static_cast<float>(iy) + hash01(wx, wy, seed ^ 0xa54ff5)
                : static_cast<float>(iy) + 0.16f + hash01(wx, wy, seed ^ 0xa54ff5) * 0.55f;

            float d = js_math::hypot(px - fx, py - fy);
            if (d < f1) {
                f2 = f1;
                f1 = d;
                nearest_x = wx;
                nearest_y = wy;
            } else if (d < f2) {
                f2 = d;
            }
        }
    }
    return { f1, f2, nearest_x, nearest_y };
}

static int cells_shade(int x, int y, int32_t seed, int per, float amount, int shades) {
    auto res = cells_at(x, y, seed, per);
    float cell_px = static_cast<float>(per) / 32.0f;
    bool on_boundary = (res.f2 - res.f1) < cell_px * 1.0f;

    int n_cells = per * per;
    // Cell rank dealing hash salt (0x3c6ef3). Reference: renderSheet.ts cells()
    int32_t hash_salt = seed ^ 0x3c6ef3;
    float my_score = hash01(res.nearest_x, res.nearest_y, hash_salt);
    int dealt = 0;
    for (int cy = 0; cy < per; ++cy) {
        for (int cx = 0; cx < per; ++cx) {
            if (hash01(cx, cy, hash_salt) < my_score) dealt++;
        }
    }
    int rank = on_boundary ? shades : (dealt * shades) / n_cells;
    return std::max(0, std::min(shades, static_cast<int>(js_math::round(static_cast<float>(rank) * std::min(1.0f, amount)))));
}

static int deal_face_rank(int cell_x, int cell_y, int32_t seed) {
    if (seed == 0) return js_math::wrap(cell_x, 2) + 2 * js_math::wrap(cell_y, 2);
    // Face rank randomisation salt (0x9e3779b9). Reference: renderSheet.ts square()
    return static_cast<int>(std::floor(hash01(cell_x, cell_y, seed ^ 0x9e3779b9) * static_cast<float>(FACE_RANKS)));
}

static int square_rank(int x_in, int y_in, int n_in, int32_t seed = 0) {
    double x = static_cast<double>(x_in);
    double y = static_cast<double>(y_in);
    double n = static_cast<double>(n_in);
    double S = 32.0 / n;
    auto to_grout = [S](double v) {
        double u = js_math::wrap(v, S);
        return std::min(u + 1.0, S - 1.0 - u);
    };
    double e = std::min(to_grout(x), to_grout(y));
    if (e == 0.0) return JOINT_RANK;
    return deal_face_rank(static_cast<int>(std::floor(x / S)), static_cast<int>(std::floor(y / S)), seed);
}

static int iso_face_rank(int cell_x, int cell_y, int n, int32_t seed) {
    int a = js_math::wrap(cell_x - cell_y, 4 * n);
    if (seed == 0) return js_math::wrap(a, FACE_RANKS);
    // Isometric face rank salt (0x9e3779b9). Reference: renderSheet.ts isometric()
    return static_cast<int>(std::floor(hash01(a, js_math::wrap(cell_x + cell_y, 2 * n), seed ^ 0x9e3779b9) * static_cast<float>(FACE_RANKS)));
}

struct IsoCell {
    int cell_x;
    int cell_y;
    double rel_x;
    double rel_y;
    double W;
    double H;
    bool is_joint;
};

static IsoCell iso_cell_at(double x, double y, double n) {
    double W = 16.0 / n;
    double H = 8.0 / n;

    double u = x / W;
    double v = y / H;

    int cell_x = static_cast<int>(std::floor((u + v) / 2.0));
    int cell_y = static_cast<int>(std::floor((u - v) / 2.0));

    double center_x = static_cast<double>(cell_x + cell_y + 1) * W;
    double center_y = static_cast<double>(cell_x - cell_y) * H;

    double rel_x = x - center_x;
    double rel_y = y - center_y;
    double dx = std::abs(rel_x);
    double dy = std::abs(rel_y);

    double dist_in_pixels = dx * H + dy * W;
    double max_dist = W * H;
    bool is_joint = (dist_in_pixels >= max_dist - std::max(1.0, H));

    return { cell_x, cell_y, rel_x, rel_y, W, H, is_joint };
}

static int isometric_rank(int x_in, int y_in, int n_in, int32_t seed = 0) {
    auto c = iso_cell_at(static_cast<double>(x_in), static_cast<double>(y_in), static_cast<double>(n_in));
    if (c.is_joint) return JOINT_RANK;
    return iso_face_rank(c.cell_x, c.cell_y, n_in, seed);
}

static int isometric_grid_rank(int x_in, int y_in, int n_in) {
    auto c = iso_cell_at(static_cast<double>(x_in), static_cast<double>(y_in), static_cast<double>(n_in));
    if (c.is_joint) return JOINT_RANK;
    // 3D Cube faces:
    //   Rank 2: Top face (illuminated)
    //   Rank 0: Left face (dark / shaded)
    //   Rank 1: Right face (mid-tone)
    if (c.rel_y < 0.0 && std::abs(c.rel_x) < c.W * (1.0 - std::abs(c.rel_y) / c.H)) return 2;
    return c.rel_x < 0.0 ? 0 : 1;
}

static int brick_bond_rank(int x_in, int y_in, int n_in) {
    double x = static_cast<double>(x_in);
    double y = static_cast<double>(y_in);
    double n = static_cast<double>(n_in);
    double bw = 32.0 / n;
    double bh = 16.0 / n;
    int course = static_cast<int>(std::floor(y / bh));
    double ry = js_math::wrap(y, bh);
    double vx = js_math::wrap(x - static_cast<double>(js_math::wrap(course, 2)) * (bw / 2.0), bw);

    // Brick mortar & bevel ranks:
    //   Rank 3: Horizontal bed mortar groove
    //   Rank 4: Vertical head mortar joint
    //   Rank 1: Top bevel highlight
    //   Rank 2: Bottom shadow lip
    //   Rank 0: Flat brick body
    if (ry == 0.0) return 3;
    if (vx == 0.0) return 4;
    if (ry == 1.0 && bh >= 5.0) return 1;
    if (ry == bh - 1.0) return 2;
    return 0;
}

static int hexagon_rank(int x_in, int y_in, int n_in) {
    double x = static_cast<double>(x_in);
    double y = static_cast<double>(y_in);
    double n = static_cast<double>(n_in);
    double step = 16.0 / n;
    double stagger = 8.0 / n;
    double ox = 10.0 / n;
    double oy = 8.0 / n;

    int c0 = static_cast<int>(js_math::round((x - ox) / step));
    double best_d = 1e18;
    int bc = 0, br = 0;
    double bx = 0.0, by = 0.0;

    for (int c = c0 - 1; c <= c0 + 1; ++c) {
        int r0 = static_cast<int>(js_math::round((y - oy - stagger * static_cast<double>(c)) / step));
        for (int r = r0 - 1; r <= r0 + 1; ++r) {
            double cx = ox + step * static_cast<double>(c);
            double cy = oy + stagger * static_cast<double>(c) + step * static_cast<double>(r);
            double d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (d < best_d) {
                best_d = d;
                bc = c; br = r; bx = cx; by = cy;
            }
        }
    }

    const double nb[6][2] = {
        { step, stagger }, { -step, -stagger }, { 0.0, step }, { 0.0, -step },
        { step, stagger - step }, { -step, step - stagger }
    };
    double edge = 1e18;
    for (int i = 0; i < 6; ++i) {
        double nx = nb[i][0];
        double ny = nb[i][1];
        double d = ((nx * nx + ny * ny) / 2.0 - ((x - bx) * nx + (y - by) * ny)) / js_math::hypot(nx, ny);
        if (d < edge) edge = d;
    }
    if (edge < 0.5) return JOINT_RANK;

    static const int HEX_FACES[2][2] = { { 0, 1 }, { 2, 3 } };
    int col_idx = js_math::wrap(bc, 2);
    int row_idx = js_math::wrap(br + static_cast<int>(std::floor(static_cast<double>(bc) / 2.0)), 2);
    return HEX_FACES[col_idx][row_idx];
}

static int octagonal_rank(int x_in, int y_in, int n_in, int32_t seed = 0) {
    double x = static_cast<double>(x_in);
    double y = static_cast<double>(y_in);
    double n = static_cast<double>(n_in);
    double S = 32.0 / n;
    double H = S / 2.0;
    double ux = js_math::wrap(x + 1.0, S);
    double uy = js_math::wrap(y + 1.0, S);
    double dx = std::abs(ux - H);
    double dy = std::abs(uy - H);
    double C = js_math::round(S * 0.6875);
    double m = dx + dy;
    auto face = [x, y, S, n_in, seed]() {
        int cx = js_math::wrap(static_cast<int>(std::floor((x + 1.0) / S)), n_in);
        int cy = js_math::wrap(static_cast<int>(std::floor((y + 1.0) / S)), n_in);
        static const int OCT_FACES[4] = { 0, 1, 2, 0 };
        if (seed == 0) return OCT_FACES[js_math::wrap(cx, 2) + 2 * js_math::wrap(cy, 2)];
        // Octagonal face rank salt (0x9e3779b9). Reference: renderSheet.ts octagonal()
        return static_cast<int>(std::floor(hash01(cx, cy, seed ^ 0x9e3779b9) * 3.0f));
    };
    // Chamfer square corner fill rank in 4-rank palette (0..3)
    const int CHAMFER_INFILL_RANK = FACE_RANKS - 1; // 3
    if (dx == H || dy == H) return (m <= C) ? JOINT_RANK : CHAMFER_INFILL_RANK;
    if (m > C) return CHAMFER_INFILL_RANK;
    if (m == C) return JOINT_RANK;
    return face();
}

static int nonslip_rank(int x_in, int y_in, int n_in, float amount = 1.0f) {
    double n = static_cast<double>(n_in);
    int u = std::max(1, static_cast<int>(js_math::round(4.0 / n)));
    int S = 8 * u;
    auto inR = [S](int v, int lo, int hi) {
        int w = js_math::wrap(v, S);
        return w >= lo && w <= hi;
    };
    int s = x_in + y_in;
    int d = x_in - y_in;
    int ax = js_math::wrap(x_in, S);
    int core = std::max(1, std::min(4 * u - 1, static_cast<int>(js_math::round(3.0 * static_cast<double>(u) * std::min(1.0, static_cast<double>(amount))))));

    // Diagonal grip ribs:
    //   Positive slope: Rank 3 (core) / Rank 1 (edge border)
    if (inR(s, 2 * u, 2 * u + 1) && ax <= core - 1) return 3;
    if (inR(s, 2 * u + 2, 2 * u + 2) && ax <= core) return 1;
    if (inR(s, 2 * u + 1, 2 * u + 2) && ax == core) return 1;

    //   Negative slope: Rank 4 (core) / Rank 2 (edge border)
    bool coreB = inR(d, S - 1, S - 1) || inR(d, 0, 0);
    if (coreB && ax >= 4 * u && ax <= 4 * u + core - 1) return 4;
    if (inR(d, S - 2, S - 2) && ax >= 4 * u && ax <= 4 * u + core - 1) return 2;
    if (coreB && ax == 4 * u + core) return 2;

    // Background base
    return 0;
}

// The three speckle textures are the noise generators under another name; this
// keeps the mapping off the per-pixel string path.
static NoiseId noise_id_for_kind(TextureKind kind) {
    switch (kind) {
        case TextureKind::Blue:    return NoiseId::Blue;
        case TextureKind::Ordered: return NoiseId::Ordered;
        default:                   return NoiseId::White;
    }
}

template <typename F>
static int geo_shade(F rank_fn, int x, int y, uint32_t seed, float amount, int shades, int n) {
    int gx = js_math::wrap(x + static_cast<int>(seed & 31), 32);
    int gy = js_math::wrap(y + static_cast<int>((seed >> 4) & 31), 32);
    return rank_to_shade(rank_fn(gx, gy, n), amount, shades);
}

int texture_shade_at(
    const std::string& texture,
    int x,
    int y,
    int32_t seed,
    float amount,
    int shades,
    int cell_scale,
    int ripple_scale,
    int geo_scale
) {
    if (texture == "none" || amount <= 0.0f || shades < 1) return 0;
    TextureKind kind = texture_kind(texture);
    // An id the registry does not know paints nothing. Unreachable in practice:
    // sanitize_recipe() gates every algo through is_known_texture(), and the
    // along_* ribbons name registry ids.
    if (kind == TextureKind::None || kind == TextureKind::Unknown) return 0;

    uint32_t s = js_math::urshift(static_cast<uint32_t>(seed ^ TEXTURE_SALT), 0);
    int geo = std::max(1, geo_scale);
    // Whether the baked table parks the joint at rank 0 instead of BAKED_RANKS
    // stays a registry fact — read it, do not hardcode it per case below.
    const bool joint_at_zero = texture_joint_at_rank_0(texture);

    switch (kind) {
        case TextureKind::Weave:
            return baked_shade(WEAVE, 16, x, y, s, amount, shades);
        case TextureKind::Paving:
            return baked_shade(PAVING, 32, x, y, s, amount, shades);
        case TextureKind::Paving3:
            return baked_shade(PAVING3, 32, x, y, s, amount, shades);
        case TextureKind::Paving5:
            return baked_shade(PAVING5, 32, x, y, s, amount, shades);

        case TextureKind::StoneFloor:
            return baked_shade(STONE_FLOOR, 32, x, y, s, amount, shades, joint_at_zero);
        case TextureKind::BreezeBlock:
            return baked_shade(BREEZE_BLOCK, 32, x, y, s, amount, shades, joint_at_zero);
        case TextureKind::BrickWall:
            return baked_shade(BRICK_WALL, 32, x, y, s, amount, shades, joint_at_zero);
        case TextureKind::Cobbles2:
            return baked_shade(COBBLES2, 16, x, y, s, amount, shades, joint_at_zero);
        case TextureKind::BrickFloor:
            return baked_shade(BRICK_FLOOR, 16, x, y, s, amount, shades, joint_at_zero);

        // The seed handed to geo_shade() below is the *pixel-shift* seed, and it
        // is deliberately not the same one everywhere: hexagon / brick_bond /
        // nonslip shift by the salted `s`, the four that follow shift by the raw
        // `seed`. That asymmetry is what renderSheet.ts does. Do not unify it —
        // it moves pixels and turns the corpus red.
        case TextureKind::Hexagon:
            return geo_shade([](int gx, int gy, int gn) { return hexagon_rank(gx, gy, gn); }, x, y, s, amount, shades, geo);
        case TextureKind::BrickBond:
            return geo_shade([](int gx, int gy, int gn) { return brick_bond_rank(gx, gy, gn); }, x, y, s, amount, shades, geo);

        case TextureKind::Isometric:
            return geo_shade([seed](int gx, int gy, int gn) { return isometric_rank(gx, gy, gn, seed); }, x, y, static_cast<uint32_t>(seed), amount, shades, geo);
        case TextureKind::IsometricGrid:
            return geo_shade([](int gx, int gy, int gn) { return isometric_grid_rank(gx, gy, gn); }, x, y, static_cast<uint32_t>(seed), amount, shades, geo);
        case TextureKind::Octagonal:
            return geo_shade([seed](int gx, int gy, int gn) { return octagonal_rank(gx, gy, gn, seed); }, x, y, static_cast<uint32_t>(seed), amount, shades, geo);
        case TextureKind::Square:
            return geo_shade([seed](int gx, int gy, int gn) { return square_rank(gx, gy, gn, seed); }, x, y, static_cast<uint32_t>(seed), amount, shades, geo);

        case TextureKind::Water: {
            int rank = baked_shade(WATER, 32, x, y, s, 1.0f, shades);
            if (rank == 0 || amount >= 1.0f) return rank;
            // Water subpixel noise salt (0x2f6e2b1). Reference: renderSheet.ts water()
            return (hash01(js_math::wrap(x, 32), js_math::wrap(y, 32), s ^ 0x2f6e2b1) < amount) ? rank : 0;
        }
        case TextureKind::Field:
            return baked_shade(FIELD, 32, x, y, s, amount, shades);
        case TextureKind::Rubble:
            return baked_shade(RUBBLE, 32, x, y, s, amount, shades);

        case TextureKind::Nonslip:
            return geo_shade([amount](int gx, int gy, int gn) { return nonslip_rank(gx, gy, gn, amount); }, x, y, s, 1.0f, shades, geo);

        case TextureKind::Cells: {
            int c_scale = std::max(MIN_CELL_SCALE, std::min(MAX_CELL_SCALE, cell_scale));
            return cells_shade(x, y, s, c_scale, amount, shades);
        }

        case TextureKind::Ripple:
        case TextureKind::RippleDiag:
        case TextureKind::White:
        case TextureKind::Blue:
        case TextureKind::Ordered: {
            float r_scale = static_cast<float>(std::max(MIN_RIPPLE_SCALE, std::min(MAX_RIPPLE_SCALE, ripple_scale)));
            float n = (kind == TextureKind::Ripple)     ? ripple_field(x, y, s, r_scale)
                    : (kind == TextureKind::RippleDiag) ? ripple_diag_field(x, y, s, r_scale)
                    : sample_noise(noise_id_for_kind(kind), x, y, s);

            float cut = 1.0f - std::min(1.0f, amount);
            if (n < cut) return 0;
            float u = (cut >= 1.0f) ? 1.0f : (n - cut) / (1.0f - cut);
            return std::min(shades, 1 + static_cast<int>(std::floor(static_cast<float>(shades) * u * u)));
        }

        default:
            return 0;
    }
}

static double luminance(RGB c) {
    return (0.2126 * static_cast<double>(c.r) + 0.7152 * static_cast<double>(c.g) + 0.0722 * static_cast<double>(c.b)) / 255.0;
}

RGB texture_colour(RGB c, float t_f) {
    if (t_f <= 0.0f) return c;
    double t = static_cast<double>(t_f);
    auto [h, s, v] = rgb_to_hsv(c);
    bool lighten = luminance(c) < 0.5;
    double nv = lighten ? v * (1.0 + 0.3 * t) : v * (1.0 - 0.18 * t);
    double ns = lighten ? s * (1.0 - 0.15 * t) : s * (1.0 + 0.1 * t);
    auto clamp01 = [](double val) { return std::max(0.0, std::min(1.0, val)); };
    return hsv_to_rgb(h, clamp01(ns), clamp01(nv));
}

static inline uint8_t mix_byte(uint8_t a, uint8_t b, double t) {
    return static_cast<uint8_t>(std::max(0.0, std::min(255.0, std::floor(static_cast<double>(a) + (static_cast<double>(b) - static_cast<double>(a)) * t + 0.5))));
}

std::vector<RGB> texture_ramp(
    RGB base,
    std::optional<RGB> target,
    int shades,
    const std::optional<std::vector<std::optional<RGB>>>& overrides
) {
    int n = std::max(1, shades);
    std::vector<RGB> out;
    out.reserve(n + 1);
    for (int k = 0; k <= n; ++k) {
        if (overrides.has_value() && k < static_cast<int>(overrides->size()) && (*overrides)[k].has_value()) {
            out.push_back(*((*overrides)[k]));
            continue;
        }
        double t = static_cast<double>(k) / static_cast<double>(n);
        if (!target.has_value()) {
            out.push_back(texture_colour(base, static_cast<float>(t)));
        } else {
            out.push_back({
                mix_byte(base.r, target->r, t),
                mix_byte(base.g, target->g, t),
                mix_byte(base.b, target->b, t)
            });
        }
    }
    return out;
}

} // namespace atm
