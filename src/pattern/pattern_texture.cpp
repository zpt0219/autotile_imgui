#include "pattern_texture.h"
#include "pattern_noise.h"
#include "js_math.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>

namespace atm {

const RGB WATER_DOT_COLOUR = { 215, 215, 215 };
const RGB DEFAULT_TEXTURE_TERRAIN_A = texture_colour({ 58, 127, 201 }, 1.0f);
const RGB DEFAULT_TEXTURE_TERRAIN_B = texture_colour({ 93, 168, 50 }, 1.0f);

static inline int wrapN(int v, int n) {
    return ((v % n) + n) % n;
}

static inline double wrapN_f(double v, double n) {
    return std::fmod(std::fmod(v, n) + n, n);
}

static const std::unordered_set<std::string> NO_AMOUNT = {
    "cells", "square", "hexagon", "isometric", "isometric_grid", "octagonal",
    "brick_wall", "brick_bond", "cobbles2", "brick_floor", "weave", "breeze_block",
    "paving", "paving3", "paving5", "stone_floor", "field", "rubble"
};

bool texture_uses_amount(const std::string& texture) {
    return texture != "none" && !NO_AMOUNT.count(texture);
}

static const char WEAVE[] =
  "0032222222311300"
  "0003222222233000"
  "0004422222230000"
  "0043342222300000"
  "0433334223000000"
  "4333333430000000"
  "4333333340000003"
  "1433333334000031"
  "1143333333400311"
  "1114333333343111"
  "1113433333341111"
  "1132243333411111"
  "1322224334111111"
  "3222222441111111"
  "3222222231111113"
  "0322222223111130";

static const char COBBLES2[] =
  "3323322014332230"
  "1222222122222221"
  "1222221012222110"
  "0111000001110000"
  "3221143332202433"
  "2221122222212222"
  "2211122222101222"
  "0010011011001111"
  "1333232014213221"
  "2222222112212221"
  "1222221012211110"
  "0110100000111000"
  "3221033332201433"
  "2221122222202222"
  "2111022222101222"
  "1000011110001111";

static const char BRICK_FLOOR[] =
  "2222221043222222"
  "2222220422322222"
  "2222204332222222"
  "2222042322222222"
  "2220423223222222"
  "2204322222222222"
  "2042323222222222"
  "0423222222222222"
  "0322222222222220"
  "2032222222222204"
  "2203222222222043"
  "3220322222220432"
  "2222032222204222"
  "2222203222042322"
  "2222220320423222"
  "2222222004232222";

static const char PAVING[] =
  "22222222224222222222241111111114"
  "22222222224222222222241111111114"
  "22222222224222222222241111111114"
  "22222222224222222222241111111114"
  "22222222224222222222241111111114"
  "44444444444222222222241111111114"
  "33333400004222222222241111111114"
  "33333400004222222222241111111114"
  "33333400004222222222241111111114"
  "33333400004222222222241111111114"
  "33333444444444444444441111111114"
  "33333422222222224000041111111114"
  "33333422222222224000041111111114"
  "33333422222222224000041111111114"
  "33333422222222224000041111111114"
  "44444422222222224444444444444444"
  "22222422222222224333333333422222"
  "22222422222222224333333333422222"
  "22222422222222224333333333422222"
  "22222422222222224333333333422222"
  "22222422222222224333333333422222"
  "22222444444444444444444444422222"
  "22222400004111111111111111422222"
  "22222400004111111111111111422222"
  "22222400004111111111111111422222"
  "22222400004111111111111111422222"
  "44444444444111111111111111444444"
  "22222222224111111111111111400004"
  "22222222224111111111111111400004"
  "22222222224111111111111111400004"
  "22222222224111111111111111400004"
  "22222222224444444444444444444444";

static const char PAVING3[] =
  "22222240000000000000000042222222"
  "22222240000000000000000042222222"
  "22222240000000000000000042222222"
  "22222240000000000000000042222222"
  "22222240000000000000000042222222"
  "22222240000000000000000042222222"
  "44444443000000000000000344444444"
  "11111134300000000000003431111114"
  "11111113430000000000034311111114"
  "11111111343000000000343111111114"
  "11111111134300000003431111111114"
  "11111111113430000034311111111114"
  "11111111111343000343111111111114"
  "11111111111134303431111111111114"
  "11111111111113434311111111111114"
  "11111111111111343111111111111114"
  "11111111111113434311111111111114"
  "11111111111134303431111111111114"
  "11111111111343000343111111111114"
  "11111111113430000034311111111114"
  "11111111134300000003431111111114"
  "11111111343000000000343111111114"
  "11111113430000000000034311111114"
  "11111134300000000000003431111114"
  "44444443000000000000000344444444"
  "22222240000000000000000042222222"
  "22222240000000000000000042222222"
  "22222240000000000000000042222222"
  "22222240000000000000000042222222"
  "22222240000000000000000042222222"
  "22222240000000000000000042222222"
  "22222244444444444444444442222222";

static const char PAVING5[] =
  "44444444444222222222241111111111"
  "43333333334222222222241111111111"
  "33333333333422222222241111111114"
  "33333333333422222222241111111114"
  "33333333333342222222241111111143"
  "33333333333342222222444111111143"
  "33333333333342222444000444111143"
  "33333333333334444000000000444433"
  "43333333334444000000000000000444"
  "24443334441114000000000000000422"
  "22224441111111400000000000004222"
  "22222411111111400000000000004222"
  "22222411111111140000000000042222"
  "22222411111111140000000000042222"
  "22222411111111140000000000042222"
  "22222411111111114000000000422222"
  "22222411111111114444444444422222"
  "22222411111111114333333333422222"
  "22222411111111143333333333342222"
  "22222411111111143333333333342222"
  "22222411111111143333333333342222"
  "22222411111111433333333333334222"
  "22224441111111433333333333334222"
  "24440004441114333333333333333422"
  "40000000004444333333333333333444"
  "00000000000004444333333333444400"
  "00000000000042222444333444111140"
  "00000000000042222222444111111140"
  "00000000000042222222241111111140"
  "00000000000422222222241111111114"
  "00000000000422222222241111111114"
  "40000000004222222222241111111111";

static const char STONE_FLOOR[] =
  "22444444443334201344441013444310"
  "43222223322222303322232023212240"
  "42222222222222303222333122222240"
  "42122222222222413222234142222240"
  "41222222222222413222332133222241"
  "44323333222223302222234132322241"
  "23444444434443102332234143322341"
  "00011111111110003332233143222341"
  "14443333344222103322224043223241"
  "43222211222222214222234033223331"
  "42222212222222313222224033222231"
  "43212222222222312233324013333331"
  "42322222222222314223234123333340"
  "43232222222233303233333024333240"
  "43444444434443101423341023444320"
  "00000011011110000111100000000000"
  "12444410134433102243323443333420"
  "43322310432221303332222332222241"
  "43212210432221204233222222222241"
  "42122221432222204222112233223341"
  "31123231332222214222221133233341"
  "32221231322222214432323323333430"
  "33322241432212312344444443332210"
  "33322231422123410001111111100000"
  "42221240421232211444333333143310"
  "42112340321122114322223333332221"
  "43112240332212314223233333232231"
  "42223240322212314322223332233331"
  "42332341232232404232233322223231"
  "33333330143332404322333233333330"
  "14233420234443203344344443444320"
  "01111000000000000000111001111000";

static const char BREEZE_BLOCK[] =
  "00000000000000000000000000000000"
  "33333333333333333333333332044333"
  "33333333333333333333333322043333"
  "33333333333333333333333322033333"
  "33333333333333333333333322033333"
  "33333333333333333333333322033333"
  "33333333333333333333333322033333"
  "33233333333333333333333322033333"
  "23333333333333333332333322033333"
  "33333332333323332333333322033333"
  "33333333333333333333332322033323"
  "23323333323333333332333322033333"
  "33323323333322333333323322033333"
  "22332333333332232323333322033333"
  "22222222222222222222222211032222"
  "22222222222222222222222211022222"
  "00000000000000000000000000000000"
  "33333333320443333333333333333333"
  "33333333220433333333333333333333"
  "33333333220333333333333333333333"
  "33333333220333333333333333333333"
  "33333333220333333333333333333333"
  "33333333220333333333333333333333"
  "33333333220333333323333333333333"
  "33323333220333332333333333333333"
  "23333333220333333333333233332333"
  "33333323220333233333333333333333"
  "33323333220333332332333332333333"
  "33333233220333333332332333332233"
  "23233333220333332233233333333223"
  "22222222110322222222222222222222"
  "22222222110222222222222222222222";

static const char BRICK_WALL[] =
  "00000000000000000000000000000000"
  "33333333330433333333333333043333"
  "33333333320333333333333332033333"
  "33333333220333323333333322033332"
  "33332333320333233333233332033323"
  "33313312320331323331331232033132"
  "12311331120331131231133112033113"
  "11111111110111111111111111011111"
  "00000000000000000000000000000000"
  "33043333333333333304333333333333"
  "32033333333333333203333333333333"
  "22033332333333332203333233333333"
  "32033323333323333203332333332333"
  "32033132333133123203313233313312"
  "12033113123113311203311312311331"
  "11011111111111111101111111111111"
  "00000000000000000000000000000000"
  "33333333330433333333333333043333"
  "33333333320333333333333332033333"
  "33333333220333323333333322033332"
  "33332333320333233333233332033323"
  "33313312320331323331331232033132"
  "12311331120331131231133112033113"
  "11111111110111111111111111011111"
  "00000000000000000000000000000000"
  "33043333333333333304333333333333"
  "32033333333333333203333333333333"
  "22033332333333332203333233333333"
  "32033323333323333203332333332333"
  "32033132333133123203313233313312"
  "12033113123113311203311312311331"
  "11011111111111111101111111111111";

static const char WATER[] =
  "00000002200000002222000202222000"
  "00000020020000222000222000002200"
  "00002220002244000000002200000222"
  "22244000000002000000002000000002"
  "00002400000002000000220200000020"
  "00000200000222200042020240000020"
  "00000222242000222200000004422220"
  "20022022024000000200000022000022"
  "42200000002000000200000020000000"
  "24000000002000000200002220000000"
  "00200000002000022000000020000002"
  "00022022222222220420020020000002"
  "00000220000020000042200002240002"
  "00042200000020000002000000024422"
  "22220000000200000002000000022002"
  "00022220002400000002200022220000"
  "00002022222220000024022200020000"
  "00022000000022000020000000022000"
  "22200000000002222220000000200222"
  "00220000000020000020000000200000"
  "00022244222220000020000000200000"
  "02200000000024000240000002222200"
  "22000000000022222204222220000222"
  "00000000000020000000200000000004"
  "40000000004220000000220000000002"
  "22220222222200000000020000000002"
  "00002200000200000000002200000020"
  "00002000000020400000244022224220"
  "00002000000022022222222000002400"
  "00244000000020020000002000000200"
  "22202440002200020000000200000222"
  "00000020022000024000000220002200";

static const char FIELD[] =
  "02444434432222432222224300000310"
  "03432003442222332222223000002432"
  "43200002442223442222333100003444"
  "10000000443344443344444431024444"
  "10000000344102334444313444334444"
  "30000000143000001343111234444444"
  "41000000033000000442111112344444"
  "43000000241000001431111111343234"
  "44100134440000003411111113442223"
  "34313444431000003421111114422222"
  "23444431344321014443211134222222"
  "22443211134444334444431243222222"
  "23442111124444444322443444222222"
  "44443111113423442222344444422222"
  "44444311234222342222244334442224"
  "31444423442222233222244211134344"
  "00344444422222224223444111134331"
  "00044444432222234444444111134100"
  "00024222234222343344444111134300"
  "00024222234423440012344333344400"
  "02344222234444430000001344444420"
  "34444222234444430000000344213442"
  "44444333333134410000001444111134"
  "34444444321124410000002442111111"
  "34423443111113300000003431111111"
  "44222343111112310000003421111113"
  "42222234311111343321014411111114"
  "22222223411123444444334433111134"
  "32222224431344443322244444321144"
  "43222244444444422222234443443344"
  "24322444443323422222234420244441"
  "00434444432222422222224400002420";

static const char RUBBLE[] =
  "44422224422222242220000000222422"
  "22244442200000024220000000244200"
  "02222422000000024220000002422000"
  "00022422000000002420000002420000"
  "00022422000000002422000024220000"
  "00002422200000002422000224220000"
  "00002244222000002422202242200000"
  "00000222422220022242222422200000"
  "00000222244222222224224222200000"
  "00000022422422220002442442200000"
  "00000222422242200000222224200000"
  "00000224220224200000002222420000"
  "00002242200024220000000022420000"
  "00022422000002422200000002422000"
  "02224220000000244220000022422200"
  "22242222000000022422222224222222"
  "44444442000000002244444442444444"
  "22222224200000224422222422222222"
  "22222222422002442220022422220000"
  "00000022244224222220022422200000"
  "00000000222444222200002422000000"
  "00000000022222422000002242200000"
  "00000000000222242000000242200000"
  "00000000000022242000000242200000"
  "00000000000002242200000242220000"
  "00000000000002242200000224220000"
  "00000000000000242200000024222000"
  "00000000000000224200000022422200"
  "00000000000002224220000000242220"
  "20000000000022224220000000224222"
  "22222200022224424220000000022422"
  "22222222244442242220000000222244";

static const int BAKED_RANKS = 4;
static const int JOINT_RANK = 4;
static const int FACE_RANKS = 4;
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
    int px = wrapN(x + static_cast<int>(seed & m), size);
    int py = wrapN(y + static_cast<int>((seed >> 4) & m), size);
    int raw = table[py * size + px] - '0';
    return rank_to_shade(joint_at_zero ? wrapN(raw - 1, BAKED_RANKS + 1) : raw, amount, shades);
}

static float hash01(int32_t ix, int32_t iy, int32_t seed) {
    int32_t n = js_math::imul(ix, 374761393) + js_math::imul(iy, 668265263) + js_math::imul(seed, 1442695041);
    n = js_math::imul(n ^ static_cast<int32_t>(js_math::urshift(static_cast<uint32_t>(n), 13)), 1274126177);
    uint32_t uval = static_cast<uint32_t>(n ^ static_cast<int32_t>(js_math::urshift(static_cast<uint32_t>(n), 16)));
    return static_cast<float>(static_cast<double>(uval) / 4294967296.0);
}

static inline float smooth(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static float ripple_field(int x, int y, int32_t seed, float per_x) {
    const int per_y = 32;
    float fx = (static_cast<float>(x) / 32.0f) * per_x;
    int iy = ((y % per_y) + per_y) % per_y;
    int x0 = static_cast<int>(std::floor(fx));
    float u = smooth(fx - static_cast<float>(x0));
    int iper_x = static_cast<int>(per_x);
    auto h = [iy, seed, iper_x](int ix) {
        return hash01(((ix % iper_x) + iper_x) % iper_x, iy, seed);
    };
    return h(x0) * (1.0f - u) + h(x0 + 1) * u;
}

static float ripple_diag_field(int x, int y, int32_t seed, float per_diag) {
    int diag_line = wrapN(x - y, 32);
    float along = (static_cast<float>(x + y) / 32.0f) * per_diag;
    int x0 = static_cast<int>(std::floor(along));
    float u = smooth(along - static_cast<float>(x0));
    int iper_diag = static_cast<int>(per_diag);
    auto h = [diag_line, seed, iper_diag](int ix) {
        return hash01(((ix % iper_diag) + iper_diag) % iper_diag, diag_line, seed);
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
            int wx = ((ix % per) + per) % per;
            int wy = ((iy % per) + per) % per;

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
    if (seed == 0) return wrapN(cell_x, 2) + 2 * wrapN(cell_y, 2);
    return static_cast<int>(std::floor(hash01(cell_x, cell_y, seed ^ 0x9e3779b9) * static_cast<float>(FACE_RANKS)));
}

static int square_rank(int x_in, int y_in, int n_in, int32_t seed = 0) {
    double x = static_cast<double>(x_in);
    double y = static_cast<double>(y_in);
    double n = static_cast<double>(n_in);
    double S = 32.0 / n;
    auto to_grout = [S](double v) {
        double u = wrapN_f(v, S);
        return std::min(u + 1.0, S - 1.0 - u);
    };
    double e = std::min(to_grout(x), to_grout(y));
    if (e == 0.0) return JOINT_RANK;
    return deal_face_rank(static_cast<int>(std::floor(x / S)), static_cast<int>(std::floor(y / S)), seed);
}

static int iso_face_rank(int cell_x, int cell_y, int n, int32_t seed) {
    int a = wrapN(cell_x - cell_y, 4 * n);
    if (seed == 0) return wrapN(a, FACE_RANKS);
    return static_cast<int>(std::floor(hash01(a, wrapN(cell_x + cell_y, 2 * n), seed ^ 0x9e3779b9) * static_cast<float>(FACE_RANKS)));
}

static int isometric_rank(int x_in, int y_in, int n_in, int32_t seed = 0) {
    double x = static_cast<double>(x_in);
    double y = static_cast<double>(y_in);
    double n = static_cast<double>(n_in);
    double W = 16.0 / n;
    double H = 8.0 / n;

    double u = x / W;
    double v = y / H;

    int cell_x = static_cast<int>(std::floor((u + v) / 2.0));
    int cell_y = static_cast<int>(std::floor((u - v) / 2.0));

    double center_x = static_cast<double>(cell_x + cell_y + 1) * W;
    double center_y = static_cast<double>(cell_x - cell_y) * H;

    double dx = std::abs(x - center_x);
    double dy = std::abs(y - center_y);

    double dist_in_pixels = dx * H + dy * W;
    double max_dist = W * H;

    if (dist_in_pixels >= max_dist - std::max(1.0, H)) return JOINT_RANK;
    return iso_face_rank(cell_x, cell_y, n_in, seed);
}

static int isometric_grid_rank(int x_in, int y_in, int n_in) {
    double x = static_cast<double>(x_in);
    double y = static_cast<double>(y_in);
    double n = static_cast<double>(n_in);
    double W = 16.0 / n;
    double H = 8.0 / n;

    double u = x / W;
    double v = y / H;

    int cell_x = static_cast<int>(std::floor((u + v) / 2.0));
    int cell_y = static_cast<int>(std::floor((u - v) / 2.0));

    double center_x = static_cast<double>(cell_x + cell_y + 1) * W;
    double center_y = static_cast<double>(cell_x - cell_y) * H;

    double dx = std::abs(x - center_x);
    double dy = std::abs(y - center_y);

    double dist_in_pixels = dx * H + dy * W;
    double max_dist = W * H;

    if (dist_in_pixels >= max_dist - std::max(1.0, H)) return JOINT_RANK;

    double rel_x = x - center_x;
    double rel_y = y - center_y;

    if (rel_y < 0.0 && std::abs(rel_x) < W * (1.0 - std::abs(rel_y) / H)) return 2;
    return rel_x < 0.0 ? 0 : 1;
}

static int brick_bond_rank(int x_in, int y_in, int n_in) {
    double x = static_cast<double>(x_in);
    double y = static_cast<double>(y_in);
    double n = static_cast<double>(n_in);
    double bw = 32.0 / n;
    double bh = 16.0 / n;
    int course = static_cast<int>(std::floor(y / bh));
    double ry = wrapN_f(y, bh);
    double vx = wrapN_f(x - static_cast<double>(wrapN(course, 2)) * (bw / 2.0), bw);

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
    int col_idx = wrapN(bc, 2);
    int row_idx = wrapN(br + static_cast<int>(std::floor(static_cast<double>(bc) / 2.0)), 2);
    return HEX_FACES[col_idx][row_idx];
}

static int octagonal_rank(int x_in, int y_in, int n_in, int32_t seed = 0) {
    double x = static_cast<double>(x_in);
    double y = static_cast<double>(y_in);
    double n = static_cast<double>(n_in);
    double S = 32.0 / n;
    double H = S / 2.0;
    double ux = wrapN_f(x + 1.0, S);
    double uy = wrapN_f(y + 1.0, S);
    double dx = std::abs(ux - H);
    double dy = std::abs(uy - H);
    double C = js_math::round(S * 0.6875);
    double m = dx + dy;
    auto face = [x, y, S, n_in, seed]() {
        int cx = wrapN(static_cast<int>(std::floor((x + 1.0) / S)), n_in);
        int cy = wrapN(static_cast<int>(std::floor((y + 1.0) / S)), n_in);
        static const int OCT_FACES[4] = { 0, 1, 2, 0 };
        if (seed == 0) return OCT_FACES[wrapN(cx, 2) + 2 * wrapN(cy, 2)];
        return static_cast<int>(std::floor(hash01(cx, cy, seed ^ 0x9e3779b9) * 3.0f));
    };
    const int SQUARE_RANK = FACE_RANKS - 1;
    if (dx == H || dy == H) return (m <= C) ? JOINT_RANK : SQUARE_RANK;
    if (m > C) return SQUARE_RANK;
    if (m == C) return JOINT_RANK;
    return face();
}

static int nonslip_rank(int x_in, int y_in, int n_in, float amount = 1.0f) {
    double n = static_cast<double>(n_in);
    int u = std::max(1, static_cast<int>(js_math::round(4.0 / n)));
    int S = 8 * u;
    auto inR = [S](int v, int lo, int hi) {
        int w = wrapN(v, S);
        return w >= lo && w <= hi;
    };
    int s = x_in + y_in;
    int d = x_in - y_in;
    int ax = wrapN(x_in, S);
    int core = std::max(1, std::min(4 * u - 1, static_cast<int>(js_math::round(3.0 * static_cast<double>(u) * std::min(1.0, static_cast<double>(amount))))));

    if (inR(s, 2 * u, 2 * u + 1) && ax <= core - 1) return 3;
    if (inR(s, 2 * u + 2, 2 * u + 2) && ax <= core) return 1;
    if (inR(s, 2 * u + 1, 2 * u + 2) && ax == core) return 1;

    bool coreB = inR(d, S - 1, S - 1) || inR(d, 0, 0);
    if (coreB && ax >= 4 * u && ax <= 4 * u + core - 1) return 4;
    if (inR(d, S - 2, S - 2) && ax >= 4 * u && ax <= 4 * u + core - 1) return 2;
    if (coreB && ax == 4 * u + core) return 2;

    return 0;
}

template <typename F>
static int geo_shade(F rank_fn, int x, int y, uint32_t seed, float amount, int shades, int n) {
    int gx = wrapN(x + static_cast<int>(seed & 31), 32);
    int gy = wrapN(y + static_cast<int>((seed >> 4) & 31), 32);
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
    uint32_t s = js_math::urshift(static_cast<uint32_t>(seed ^ TEXTURE_SALT), 0);
    int geo = std::max(1, geo_scale);

    if (texture == "weave") return baked_shade(WEAVE, 16, x, y, s, amount, shades);
    if (texture == "paving") return baked_shade(PAVING, 32, x, y, s, amount, shades);
    if (texture == "paving3") return baked_shade(PAVING3, 32, x, y, s, amount, shades);
    if (texture == "paving5") return baked_shade(PAVING5, 32, x, y, s, amount, shades);

    static const std::unordered_set<std::string> JOINT_AT_RANK_0_SET = {
        "brick_wall", "cobbles2", "brick_floor", "breeze_block", "stone_floor"
    };
    bool rot = JOINT_AT_RANK_0_SET.count(texture) > 0;

    if (texture == "stone_floor") return baked_shade(STONE_FLOOR, 32, x, y, s, amount, shades, rot);
    if (texture == "breeze_block") return baked_shade(BREEZE_BLOCK, 32, x, y, s, amount, shades, rot);
    if (texture == "brick_wall") return baked_shade(BRICK_WALL, 32, x, y, s, amount, shades, rot);
    if (texture == "cobbles2") return baked_shade(COBBLES2, 16, x, y, s, amount, shades, rot);
    if (texture == "brick_floor") return baked_shade(BRICK_FLOOR, 16, x, y, s, amount, shades, rot);

    if (texture == "hexagon") return geo_shade([](int gx, int gy, int gn) { return hexagon_rank(gx, gy, gn); }, x, y, s, amount, shades, geo);
    if (texture == "brick_bond") return geo_shade([](int gx, int gy, int gn) { return brick_bond_rank(gx, gy, gn); }, x, y, s, amount, shades, geo);

    if (texture == "isometric") return geo_shade([seed](int gx, int gy, int gn) { return isometric_rank(gx, gy, gn, seed); }, x, y, static_cast<uint32_t>(seed), amount, shades, geo);
    if (texture == "isometric_grid") return geo_shade([](int gx, int gy, int gn) { return isometric_grid_rank(gx, gy, gn); }, x, y, static_cast<uint32_t>(seed), amount, shades, geo);
    if (texture == "octagonal") return geo_shade([seed](int gx, int gy, int gn) { return octagonal_rank(gx, gy, gn, seed); }, x, y, static_cast<uint32_t>(seed), amount, shades, geo);
    if (texture == "square") return geo_shade([seed](int gx, int gy, int gn) { return square_rank(gx, gy, gn, seed); }, x, y, static_cast<uint32_t>(seed), amount, shades, geo);

    if (texture == "water") {
        int rank = baked_shade(WATER, 32, x, y, s, 1.0f, shades);
        if (rank == 0 || amount >= 1.0f) return rank;
        return (hash01(wrapN(x, 32), wrapN(y, 32), s ^ 0x2f6e2b1) < amount) ? rank : 0;
    }
    if (texture == "field") return baked_shade(FIELD, 32, x, y, s, amount, shades);
    if (texture == "rubble") return baked_shade(RUBBLE, 32, x, y, s, amount, shades);

    if (texture == "nonslip") {
        return geo_shade([amount](int gx, int gy, int gn) { return nonslip_rank(gx, gy, gn, amount); }, x, y, s, 1.0f, shades, geo);
    }
    if (texture == "cells") {
        int c_scale = std::max(MIN_CELL_SCALE, std::min(MAX_CELL_SCALE, cell_scale));
        return cells_shade(x, y, s, c_scale, amount, shades);
    }

    float r_scale = static_cast<float>(std::max(MIN_RIPPLE_SCALE, std::min(MAX_RIPPLE_SCALE, ripple_scale)));
    float n = (texture == "ripple") ? ripple_field(x, y, s, r_scale)
            : (texture == "ripple_diag") ? ripple_diag_field(x, y, s, r_scale)
            : sample_noise(parse_noise_id(texture), x, y, s);

    float cut = 1.0f - std::min(1.0f, amount);
    if (n < cut) return 0;
    float u = (cut >= 1.0f) ? 1.0f : (n - cut) / (1.0f - cut);
    return std::min(shades, 1 + static_cast<int>(std::floor(static_cast<float>(shades) * u * u)));
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
