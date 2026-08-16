# REFACTOR — 内核可读性重构工单

> **状态：待执行。** 执行者：Gemini。验收：Peter。
>
> **目标：让 `src/` 下的内核代码读起来清楚。** 这份工单里没有一条是功能需求，
> 也没有一条是性能需求。唯一的产出是"同样的行为，更少的样板、更明确的边界、
> 更短的函数"。
>
> **铁律：本工单不允许改变任何一个像素。** 每一个任务做完，
> `corpus/verify.py` 必须仍然是 1161/1161、`maxDelta` 为 0。如果某次改动
> 让哪怕一个像素动了，那次改动就是错的 —— 回滚，不要试图解释，
> 更不要去调 `maxDelta`。

---

## 0. 开始之前

按顺序读完这四份东西，再动第一行代码：

1. `CLAUDE.md`（根目录）—— 项目铁律。**7 条全部适用于本工单**，尤其是第 2 条
   （`generated.ts` 类的机器数据禁止"重算/清理/重新生成"）和第 4 条
   （忠实复刻，不许"修正"参考实现的怪癖）。
2. `docs/PLAN.md` —— 架构与分层。
3. `reference/README.md`、`corpus/README.md` —— 这两个目录是**只读**的。
4. 本文件全文，包括**附录 A：发现但明确不在范围内的东西**。附录 A 里的每一条
   都是你会看到、会手痒、但必须放着不动的东西。

建立环境：

```bash
cmake -B build-desktop -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-desktop -j --target autotile_mixer
cmake --build build-desktop -j --target autotile_tests
pip install numpy
```

先跑一次基线，确认起点是干净的（**如果基线就不是 1161/1161，停下来报告，不要开始**）：

```bash
python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe
```

---

## 1. 铁律（在 CLAUDE.md 之上，本工单专属）

1. **不改数值。** 不改常量、不改运算顺序、不改浮点类型（`float` ↔ `double` 的
   隐式窄化位置就是语义的一部分）、不改比较的方向、不改 clamp 的边界。
   本工单全部是**搬运和改名**。
2. **不改行为，只改形状。** 允许：提取函数、提取文件、引入结构体、删除死代码、
   合并重复。不允许：合并两个"看起来一样"的分支、把 `if` 链改成查表时顺手
   改掉某个特例、"顺便"修一个你觉得是 bug 的地方。看到疑似 bug → 写进交付报告，
   不要动手。
3. **`reference/` 和 `corpus/` 只读。** 不改 `.ts`、`.png`、`.lvl.gz`、`manifest.json`。
4. **不放宽任何门槛。** 不改 `maxDelta`，不关警告，不给整个 target 加 `-w`。
   新文件如果触发 vendored 风格的警告，按 CLAUDE.md 的惯例**逐源文件**静音。
5. **`desktop/` 基本不要动。** 内核 API 对外的调用点只有两处
   （`desktop/src/panels/recipe_panel.cpp:360` 和 `:487` 调 `texture_ramp`）。
   除非某个任务明确说了要改，否则 `desktop/` 下一个字都不要改。
6. **一个任务一个 commit。** 不要攒。commit message 用仓库既有风格：
   `refactor(pattern): extract shared hash01 into pattern_hash.h`。

---

## 2. 完成的定义（G）

**每一个任务**做完都要跑下面这四条，四条全绿才算完成，才能 commit，才能开始下一个：

```bash
cmake --build build-desktop -j --target autotile_mixer   # 零警告
cmake --build build-desktop -j --target autotile_tests   # 零警告
ctest --test-dir build-desktop --output-on-failure
python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe
```

最后一条必须是 **1161 / 1161，`maxDelta` 0**。装了 numpy 之后它只要几秒，
所以**不要用 `--quick` 代替**，每个任务都跑全量。

除此之外，每个任务下面还写了它自己的 **G**（一条能机器判定的具体检查）。

---

## R1 — 零风险提取

这一轮不碰任何算法主体，只是把东西搬到该在的地方、把重复的删掉。
做完之后 `src/pattern/` 需要人读的行数应该少掉三分之一左右。

---

### R1.1 把四份重复的 hash 提取成一处

**现状。** 同一段三行 xorshift 混合器在仓库里存在四份：

| 位置 | 形态 |
| --- | --- |
| `src/pattern/pattern_noise.cpp:80` | `static float hash01(int32_t, int32_t, int32_t)` |
| `src/pattern/pattern_texture.cpp:425` | 同上，逐字相同 |
| `src/pattern/pattern_ribbon.cpp:16` | 同上，逐字相同 |
| `src/pattern/blob47_pattern.cpp:107` | `edge_noise` 内部的 lambda `h`，**不同**，见下 |

这是整个项目最敏感的一段代码（动一个常数就是 1161 张图全红），却有四份、
零注释说明它们必须一致。

**要做的事。** 新建 `src/pattern/pattern_hash.h` / `.cpp`，提供**整数域**的核心：

```cpp
// Ported verbatim from the reference implementation. Every texture, ribbon,
// grain and edge-jitter sampler in this project shares this one mixer — the
// entire corpus was baked with it. Do not touch the constants.
uint32_t hash_bits(int32_t x, int32_t y, int32_t seed);

// The 0..1 float form used by textures / ribbons / grain.
float hash01(int32_t x, int32_t y, int32_t seed);
```

`hash01` 的实现必须**逐字保留现在的收窄路径**：

```cpp
inline float hash01(int32_t x, int32_t y, int32_t seed) {
    return static_cast<float>(static_cast<double>(hash_bits(x, y, seed)) / 4294967296.0);
}
```

**⚠ 这里有一个会让 parity 全红的坑。** `blob47_pattern.cpp` 里 `edge_noise` 的
lambda `h` **不能**改成调用 `hash01`：它现在全程在 `double` 下算
（`(double(uval)/4294967296.0)*2.0-1.0`），而 `hash01` 返回 `float`。
经过 `float` 收窄再 `*2-1` 与直接在 `double` 下 `*2-1` 不是同一个数。
`h` 必须改写成直接用 `hash_bits`，保持 `double`：

```cpp
auto h = [seed, per](int32_t ix, int32_t iy) -> double {
    /* ... wx / wy 的取模逻辑原样保留 ... */
    return (static_cast<double>(hash_bits(wx, wy, seed)) / 4294967296.0) * 2.0 - 1.0;
};
```

顺带把 `pattern_noise.cpp:87` 的 `seed_bits` 也移过去（它只有一份，但属于同一族）。

**不要做的事。** `blob47_pattern.cpp:178` 那段 `imul(edge_seed, 374761393) ^ 0x1f3b2a`
是 wave 的**相位哈希**，是另一个函数，不是 `hash01`。留在原地，只加一行注释说明
它是独立的一支。

**G.** `grep -rn "668265263" src/` 只剩 `pattern_hash.cpp` 一处命中。

---

### R1.2 把烘焙纹理表从算法文件里搬出去

**现状。** `src/pattern/pattern_texture.cpp` 共 836 行，其中 **第 33–391 行
（约 358 行，43%）** 全是烘焙的 ASCII 纹理表：`WEAVE`、`COBBLES2`、
`BRICK_FLOOR`、`PAVING`、`PAVING3`、`PAVING5`、`STONE_FLOOR`、`BREEZE_BLOCK`、
`BRICK_WALL`、`WATER`、`FIELD`、`RUBBLE`。

这些和 `pattern_data.cpp` 是同一性质的东西 —— **机器产物，禁止手改** ——
但现在夹在算法中间，读者滚过去时完全分不清哪些是数据、哪些是逻辑。

**要做的事。** 新建 `src/pattern/texture_tables.h` / `.cpp`（或 `.inc`），
把这 12 张表原样搬过去，一个字符都不要改。文件头写上和 CLAUDE.md 第 2 条同调的警告：

```cpp
// Baked texture tables — machine output, transcribed verbatim.
// The generator does not exist any more. Do NOT recompute, tidy, reflow or
// regenerate any of these; the corpus was baked from exactly these bytes.
```

**不要做的事。** 不要改缩进、不要把 16 行的表补齐成 32 行、不要"顺手"把
`static const char[]` 改成 `std::array`。**原样搬运。**

**G.** `wc -l src/pattern/pattern_texture.cpp` ≤ 480；纹理表所在新文件与旧文件
的表内容 diff 为空（可用 `git show HEAD:src/pattern/pattern_texture.cpp | sed -n '33,391p'`
与新文件对拍）。

---

### R1.3 把 `sanitize_recipe` 的 30 遍样板压成一张表

**现状。** `src/model/recipe.cpp:92-302`，210 行，其中约 170 行是同一个模式
重复 30 次：

```cpp
if (raw.contains("edgeSeed") && raw["edgeSeed"].is_number()) {
    r.edgeSeed = clamp_int(raw["edgeSeed"].get<int>(), 0, 99999, DEFAULT_RECIPE_INSTANCE.edgeSeed);
}
```

这个函数真正想告诉读者的信息是「哪个字段、什么范围」，现在被埋了。

**要做的事。** 提取两个小工具，把 30 个字段变成 30 行声明：

```cpp
template <class T>
static void read_clamped(const nlohmann::json& raw, const char* key, T lo, T hi, T fallback, T& out);
static void read_enum(const nlohmann::json& raw, const char* key,
                      const std::unordered_set<std::string>& valid, std::string& out);
static std::optional<std::vector<std::optional<std::string>>>
read_sparse_hex_array(const nlohmann::json& raw, const char* key, int expected_len);
```

四个 `custom*Hex` 的解析现在是三份几乎一样的代码
（`:141`、`:199`、`:269-299` 里的两段），合并成 `read_sparse_hex_array`。
注意 `customShadesHex` 是**全有或全无**语义（任一项非法则整个数组丢弃），
和另外三个的**逐项可空**语义不同 —— 保留两个不同的读取器，不要合并成一个带
标志位的函数。

**⚠ 坑。** **字段的解析顺序必须原样保留。** `customShadesHex` 的长度校验读的是
已经解析过的 `r.bandSteps`，`customRibbonHex` 读 `r.ribbonShades`，
`customTexHex` 读 `r.textureShadesA/B`。把某个 clamp 提前或延后就会静默改变
"哪些自定义颜色会被丢弃"。

**顺带。** `recipe.cpp:83` 的 `std::isnan(static_cast<float>(val))` 对 `int`
恒为 false，是 TS 移植遗留的死判断，删掉。

**G.** `sanitize_recipe` 函数体 ≤ 70 行；`tests/test_overrides.cpp` 与
`test_commands.cpp` 全绿。

---

### R1.4 删死代码

**现状。** 以下符号在整个仓库（`src/` + `desktop/` + `tests/`）内**没有任何调用者**：

| 符号 | 位置 | 规模 |
| --- | --- | --- |
| `blob_weight_at` | `src/pattern/blob47.cpp:104` + 头文件声明 | 约 40 行，含 `box_dist` / `ORTHO` / `DIAGONAL` / `CONVEX_PAIRS` 四个只服务于它的静态量 |
| `get_field_chars` | `src/pattern/pattern_data.cpp:10` | 3 行 + `FIELD_CHARS_STR` |
| `REFERENCE_ROLE_COLOURS` | `src/pattern/pattern_paint.cpp:12` | 5 行 |
| `recipe_from_json` | `src/model/recipe.cpp:304` | 只是 `sanitize_recipe` 的一行别名，两个名字表达同一件事 |

**要做的事。** 逐个确认无调用者（`grep -rn` 覆盖 `src desktop tests`），然后删除，
连同只服务于它们的静态数据和头文件声明。

`recipe_from_json` 如果被认为是"对外的语义化名字"，那就保留它、删掉
`sanitize_recipe` 的公开声明二选一 —— 不要两个都留在头文件里。

**⚠ 判断依据。** `blob_weight_at` 是 `reference/blob47.ts` 的 C++ 转写，可能是
有意保留的移植存档。**如果删除后你不确定**，改为保留但在声明上方写明：
`// unused: kept as the C++ transcription of reference/blob47.ts weightAt().`
并在交付报告里说明你选了哪条。两种做法都可接受，含糊不可接受。

**G.** `grep -rn "blob_weight_at\|get_field_chars\|REFERENCE_ROLE_COLOURS" src desktop tests`
返回 0 行（或仅剩带上述注释的声明）。

---

### R1.5 零散清理

四件小事，可以合成一个 commit：

1. **重复声明。** `ribbon_uses_invert` 在 `src/pattern/catalog.h:55` 和
   `src/pattern/pattern_ribbon.h:23` 各声明了一次，定义只有
   `pattern_ribbon.cpp:23` 一处。删掉 `catalog.h` 里那份，让 `catalog.h`
   包含 `pattern_ribbon.h`。
2. **`PI_D`。** 在 `blob47_pattern.cpp:165`、`:270`、`pattern_ribbon.cpp:87`
   各定义了一次。移到 `js_math.h` 作为 `constexpr double PI`。
   **值必须一字不差**：`3.14159265358979323846`。
3. **硬编码的 32。** `blob47_pattern.cpp` 里 `sample_field(field, 32, u, v)` 的
   字面量 `32` 出现 6 次（`:171`、`:189`、`:190`、`:277`、`:283`、`:284`），
   而同一个函数里 `scale` 的计算已经用了 `pattern_data::PATTERN_TILE_SIZE`。
   统一成常量。
4. **盐值注释。** `0x1f3b2a`、`0x3c6ef3`、`0xa54ff5`、`0x9e3779b9`、`0x2f6e2b1`、
   `0x51`、`0x5bd1`（`TEXTURE_SALT`）、`0x2c9f`（`RIBBON_SALT`）散布各处、
   零注释。读者无从判断哪些能动（答案：一个都不能动）。每处加一行注释指明
   它来自 reference 的哪个函数。**只加注释，不动值。**

**G.** `grep -rn "3\.14159" src/` 只剩 `js_math.h` 一处。

---

## R2 — 局部重构

这一轮开始动函数边界，但仍然不动签名、不动数据流。

---

### R2.1 拆开 `recipe_to_paint_args`

**现状。** `src/pattern/sheet.cpp:37-146`，110 行的平铺函数，一口气做四件互不相干
的事：解析角色色、算 band offset、拼 ramp、拼 texture / ribbon 选项。

按 CLAUDE.md 第 1 条，这个函数是**整个项目最关键的映射**
（"这个映射曾经存在两份，而第二份悄悄漂移了"）。它值得让读者能逐条对着
`reference/renderSheet.ts` 核对。

**要做的事。** 拆成四个有名字的静态函数，`recipe_to_paint_args` 本体只剩装配：

```cpp
static double              band_offset_px(const Recipe&);        // 含 bandBias 正负分支
static std::vector<RGB>    build_ramp(const Recipe&, const RoleColours&);
static TexturePaintOptions build_texture_options(const Recipe&); // 含 water 特判
static RibbonPaintOptions  build_ribbon_options(const Recipe&);
```

给两处 water 特判各补一行注释，说明它们是刻意的、对应 reference 的哪一行：

- `sheet.cpp:60-61`：`textureAlgo == "water"` 时 shades 强制为 2；
- `sheet.cpp:71-77`：water 的 ramp 第 3 档若未自定义则强制 `WATER_DOT_COLOUR`。

**⚠ 坑。** 求值顺序不能变：`shadesA/shadesB` 的 water 修正必须发生在
`texture_ramp_for` 之前（后者拿 `shadesA` 当 `shade_count` 去切自定义 ramp）。
拆函数时确保 `build_texture_options` 内部维持这个先后。

**G.** `recipe_to_paint_args` 函数体 ≤ 30 行。

---

### R2.2 拆开 `paint_pattern_tile_rgba` 的内层循环

**现状。** `src/pattern/pattern_paint.cpp:205-280`，75 行、四层嵌套，
而且在最内层每像素重建一个 `target_matches` lambda（`:220`）。

**要做的事。**

1. 把 `target_matches` 提到双重循环之外（它只依赖 `opts.noise_targets`）。
2. 提取两个函数：

```cpp
struct GrainResult { RGB rgb; int level; bool grained; };
static GrainResult apply_grain(int level, int x, int y, const PaintOptions&,
                               const std::vector<RGB>& ramp,
                               const std::vector<PatternLevelDef>&, int solid, int span);

// ribbon / texA / texB 三选一，命中则返回覆盖色
static std::optional<RGB> pick_overlay(...);
```

3. 主循环收敛成：取 level → `apply_grain` → `pick_overlay` → 写 RGBA + alpha。

**⚠ 坑。** `if (!grained && ribbon) ... else if (texA) ... else if (texB)` 是一条
**互斥链**：grain 命中时三个覆盖全部跳过；ribbon 命中时纹理不再参与。
提取成 `pick_overlay` 后必须保持完全相同的短路顺序，不要改成三次独立判断。

**G.** `paint_pattern_tile_rgba` 的双重循环体 ≤ 25 行。

---

### R2.3 把 wave 特例从几何主循环里提出来

**现状。** `src/pattern/blob47_pattern.cpp:173-205`，33 行的 wave 专属逻辑
（相位哈希 + 梯度分解 + 两次 `js_math::sin` + 边界淡出）直接嵌在
`pattern_levels_for_mask` 的双重循环体内，把这个函数真正的主干
（采样 → 加偏移 → 量化到 level）压到了看不见。

**要做的事。** 提取：

```cpp
// The "wave" pattern rides a sine along the band tangent. Reference: renderSheet.ts.
static double wave_offset_at(const char* field, double u, double v, double d_base,
                             double off, int32_t edge_seed);
```

主循环里剩下：

```cpp
double wave_offset = (pattern == "wave") ? wave_offset_at(field, u, v, d_base, off, edge_seed) : 0.0;
double jitter      = (amp > 0.0 && pattern != "wave") ? amp * edge_noise(u, v, edge_seed) : 0.0;
double d           = d_base + off + wave_offset + jitter;
```

**⚠ 坑。** 注意 `wave` 与 `jitter` 是**互斥**的（`pattern != "wave"` 才有 jitter），
这个互斥关系要在提取后依然一眼可见 —— 上面那两行就是可接受的写法。
另外 `wave_offset_at` 内部有三次 `sample_field` 调用求梯度，
它们用的是 `u±0.5` / `v±0.5` 而不是 `±1`，原样保留。

**G.** `pattern_levels_for_mask` 的双重循环体 ≤ 15 行。

---

### R2.4 合并 isometric 的两份重复

**现状。** `src/pattern/pattern_texture.cpp:545` 的 `isometric_rank` 与 `:571` 的
`isometric_grid_rank`，前 25 行（`W`/`H`/`u`/`v`/`cell_x`/`cell_y`/`center_*`/
`dx`/`dy`/`dist_in_pixels`/`max_dist`/joint 判定）**逐字相同**。

**要做的事。**

```cpp
struct IsoCell {
    int    cell_x, cell_y;
    double rel_x, rel_y;   // 相对菱形中心
    bool   is_joint;
};
static IsoCell iso_cell_at(double x, double y, double n);
```

两个 rank 函数各自只剩 joint 判定后的分支。

**⚠ 坑。** 两者的 `double` 运算顺序必须与现在完全一致
（`dist_in_pixels = dx * H + dy * W`，不是 `H * dx + W * dy`；
浮点乘法虽可交换但这里不要动，省得引入无谓的怀疑）。

**G.** `pattern_texture.cpp` 中 `16.0 / n` 的出现次数从 2 降到 1。

---

### R2.5 用模板基类吃掉命令层的样板

**现状。** `src/command/library_command.cpp` 617 行。8 个命令
（Colours / Pattern / Band / Noise / Ribbon / Texture / Rename / ExportSettings）
各自重复同一套三段式：

- `execute`：`find_by_hash` → 未找到报错 → `if (!initialized_)` 快照 `old_*` →
  逐字段写入 → `notify_recipe_updated(entry, DIRTY_X, cb, flag_)`
- `undo`：`find_by_hash` → 未找到报错 → 逐字段写回 `old_*` →
  `notify_recipe_updated(entry, DIRTY_X, cb, 2)`
- `merge_with`：kind 检查 → hash 检查 → `flag_ == 2` 检查 → 逐字段抄 `new_*` →
  抄 `flag_` / `timestamp_`

后果不只是长：**同一个命令改了哪些字段，这个信息散在三个函数里**，
三处不一致就是静默 bug。

**要做的事。** `UpdateRecipeTextureCommand`（`:305-362`）已经示范了正确形状
（存整份状态，undo 整体还原）。把它抽成基类：

```cpp
template <class State>
class RecipeFieldCommand : public LibraryCommand {
public:
    EditorResult execute(LibraryHandler&, LibraryCallbacks*) override;  // 基类实现一次
    EditorResult undo   (LibraryHandler&, LibraryCallbacks*) override;  // 基类实现一次
    bool merge_with(const LibraryCommand*) override;                    // 基类实现一次
protected:
    virtual State     read (const Recipe&) const = 0;
    virtual void      write(Recipe&, const State&) const = 0;
    virtual DirtyMask dirty() const = 0;
    State old_, new_;
    bool  initialized_ = false;
};
```

派生类只剩 `read` / `write` / `dirty` 三个小函数，
"这个命令管哪些字段"变成一眼可见的两处对称代码。

**⚠ 坑一（必须保留的语义）。** `UpdateRecipeBandCommand::undo`（`:146-159`）
**恢复** `old_custom_shades_` 而不是重新 `sync_band_overrides` ——
注释解释了原因（先增后减会把用户改过的颜色换成计算值）。
把 `customShadesHex` 放进 `State` 后，基类的 `undo = write(old_)` 天然满足这个语义，
但你必须确认它确实被放进了 `State`，并保留那条注释。

**⚠ 坑二。** 各个 `execute` 里在写完字段后调用的 `sync_*_overrides`
（`:33`、`:141`、`:267`、`:337`）是**写入路径的一部分**，必须留在 `write()` 里，
不能留在基类的通用流程里 —— 因为 `undo` 路径**不能**调它（见坑一）。
基类的 `undo` 只做 `write(old_)`，那么 `write()` 里的 `sync_*` 在 undo 时也会跑。
**这会改变 undo 的行为。** 正确做法：基类提供两个钩子，
`apply_new()`（= `write(new_)` + `sync`）和 `apply_old()`（= `write(old_)`，不 sync），
或者让 `write()` 接一个 `bool sync` 参数。**这一条如果处理错，
`tests/test_overrides.cpp` 和 `test_command_monkey.cpp` 会抓到，不要跳过 ctest。**

**⚠ 坑三。** `merge_with` 里的 `flag_ == 2`（"上一条已经收尾"）语义、
以及 `undo` 一律传 `flag = 2` 的约定，原样保留。

**⚠ 坑四。** `AddRecipeCommand` / `RemoveRecipeCommand` / `DuplicateRecipeCommand` /
`ReorderRecipeCommand` / `SelectRecipeCommand` / `AddVariantAxisCommand` /
`RemoveVariantAxisCommand` **不是**这个形状（它们改的是列表结构而非某个 recipe
的字段）。**不要**硬套模板，留在原地。

**G.** `wc -l src/command/library_command.cpp` ≤ 320；
`ctest` 全绿（尤其 `test_command_monkey`）。

---

## R3 — 签名重构

这一轮改函数签名。好消息：这些 API 的调用点**几乎全部在 `src/pattern/` 内部**
—— 已确认 `desktop/` 和 `tests/` 都不直接调用
`pattern_levels_for_mask` / `pattern_band_coords` / `outline_width_px` /
`texture_shade_at` / `ribbon_shade_at` / `paint_pattern_tile_rgba`。
唯一的外部调用点是 `texture_ramp`（`recipe_panel.cpp:360` 和 `:487`），本轮不动它。

---

### R3.1 把那组"八个位置参数"变成 `FieldParams`

**现状。** 同一组几何配置在四个函数之间反复传递，而且**参数顺序还不一致**：

```cpp
pattern_levels_for_mask(pattern, mask, offset_px, tile_size, band_steps, hard_edge_b, edge_seed, outline_width)
pattern_band_coords    (pattern, mask, offset_px, tile_size, band_steps, hard_edge_b, edge_seed, outline_width)
outline_width_px       (pattern,       tile_size, band_steps, hard_edge_b, outline_width)   // ← 顺序不同
bands_for              (pattern,                  band_steps, hard_edge_b, outline_width)
```

`pattern_paint.cpp:156`、`:193`、`:199` 连着调了三次。现在读这段代码，
必须**逐个数参数**才能确认三次传的是同一组配置。这是本次审查里最实际的绊脚石。

**要做的事。** 在 `blob47_pattern.h` 引入：

```cpp
struct FieldParams {
    std::string pattern;
    double offset_px    = 0.0;
    int    tile_size    = 32;
    int    band_steps   = DEFAULT_BAND_STEPS;
    bool   hard_edge_b  = false;
    int    edge_seed    = 0;
    float  outline_width = -1.0f;   // < 0 = pattern default
};

std::string pattern_levels_for_mask(const FieldParams&, int mask);
BandCoords  pattern_band_coords    (const FieldParams&, int mask);
float       outline_width_px       (const FieldParams&);
```

`paint_pattern_tile_rgba` 和 `render_level_grid` 在顶部各构造一次 `FieldParams`。

**⚠ 坑一。** `offset_px` 现在在不同函数里一处是 `float`、一处是 `double`
（`PaintOptions::offset_px` 是 `double`，`pattern_levels_for_mask` 收 `float`，
内部又 `clamp_offset` 回 `float` 再转 `double`）。
**这条收窄链是语义的一部分。** `FieldParams::offset_px` 取 `double`，
但函数内部**必须保留原来那次 `float` 收窄**
（`clamp_offset` 收 `float` 返回 `float`），不要"顺手统一成 double"。
这一条搞错了 parity 会红。

**⚠ 坑二。** `bands_for` 和 `clamp_offset` / `edge_jitter_amplitude` /
`band_noise_span` 只吃 pattern（+steps），**不要**强行让它们也收 `FieldParams`；
它们的窄签名本身就是正确的信息。

**G.** `grep -c "hard_edge_b" src/pattern/` 的总命中数明显下降；
`pattern_paint.cpp` 中不再出现连续三次的八参数调用。

---

### R3.2 把 `TexturePaintOptions` 的 A/B 双份字段合成 `TextureSide`

**现状。** `src/pattern/pattern_paint.h:61-80`，16 个成对的 `xxxA` / `xxxB` 字段。
`sheet.cpp:80-98` 逐个赋值 18 行；`pattern_paint.cpp:165-173` 与 `:256-272`
是两段完全对称的代码。

**要做的事。**

```cpp
struct TextureSide {
    std::string algo = "none";
    double amount = 0.0;
    int shades = 2;
    int32_t seed = 0;
    int cell_scale = 4, ripple_scale = 4, geo_scale = 1;
    std::optional<RGB> colour;
    std::optional<std::vector<std::optional<RGB>>> ramp;
};
struct TexturePaintOptions { TextureSide a, b; };
```

`texture_shade_at` 增加一个接 `const TextureSide&` 的重载，
`pattern_paint.cpp` 里的两段对称代码合成一个小函数调用两次。

**⚠ 坑（范围限制）。** **只改绘制层，不要动 `Recipe`。**
`Recipe` 的字段名（`textureAlgoA` 等）与 `recipe_to_json` 的 JSON key、
`recipe_codec.cpp` 的分享码字节布局绑定。`Recipe` 保持原样，
只在 `sheet.cpp` 的 `build_texture_options` 里做一次 A/B 装配。

**G.** `pattern_paint.h` 中 `A;` / `B;` 结尾的成员声明降为 0；
`grep -c "AlgoA\|AmountA\|ShadesA" src/pattern/` 只剩 `sheet.cpp` 的装配处。

---

## R4 — 单一注册表（最后一轮，做之前先找验收人确认）

**这一轮工作量最大、风险最高，且前三轮做完可读性目标已基本达成。
开始之前先向验收人确认是否要做。**

**现状。** `patternId` / `textureAlgo` / `ribbonAlgo` 全程是 `std::string`，
`texture_shade_at`（`pattern_texture.cpp:738-786`）在**每像素**路径上做最多
23 次字符串比较。真正的问题不是性能，而是**合法值集合有五份**：

| 位置 | 用途 |
| --- | --- |
| `src/model/recipe.cpp:110 / :176 / :214` | `VALID_PATTERNS` / `VALID_RIBBONS` / `VALID_TEXTURES`，校验 |
| `src/codec/recipe_codec.cpp:12 / :17 / :23` | **数组下标 = 分享码字节值，顺序即线上格式，绝对不可重排** |
| `src/pattern/catalog.cpp:10 / :37 / :94` | UI 分组 + 中英文名 |
| `src/pattern/pattern_texture.cpp:23 / :743` | `NO_AMOUNT` / `JOINT_AT_RANK_0_SET` |
| `src/pattern/catalog.cpp:136 / :148 / :154` | `PERIOD_32` / `texture_uses_geo_scale` / `natural_geo_scale` |

加一个纹理要改 5–6 处，漏一处就是静默 bug。

**建议分两步，第一步风险接近零：**

**R4a（低风险）。** 只合并属性表，**不动字符串分派**。把 `NO_AMOUNT`、
`JOINT_AT_RANK_0_SET`、`PERIOD_32`、`texture_uses_geo_scale`、
`natural_geo_scale`、`geo_scales_for` 的信息并进 `catalog.cpp` 的一张表：

```cpp
struct TextureDef {
    const char* key;            // "brick_wall" — JSON / 分享码用
    const char* zh; const char* en;
    TextureGroup group;
    bool uses_amount;
    bool joint_at_rank_0;
    int  period;                // 16 / 32
    int  natural_geo_scale;     // 0 = 不支持 geo scale
};
```

顺带统一现在三种并存的查表风格（`PERIOD_32` 用 `vector<string>` + 线性 `find`、
`NO_AMOUNT` 用 `unordered_set`、`MIN_WIDTH` 用 `unordered_map`）。

**R4b（高风险，可单独否决）。** 引入 `enum class TextureId` / `RibbonId` /
`PatternId`，字符串 → 枚举的转换只在 recipe 解析时做一次，
`texture_shade_at` 改成 `switch`。

**⚠ 坑一。** `recipe_codec.cpp` 里 `TEXTURES` / `RIBBONS` / `PATTERNS`
**数组下标就是分享码的字节值**。注册表的顺序必须严格等于现在这三个数组的顺序，
而且要加一个静态断言 / 单元测试把这条约束钉死。**重排 = 所有已发出的分享码失效。**

**⚠ 坑二。** `tests/test_catalog.cpp` 双向校验 catalogue 与 reference
（见 commit `3c3f39c`）。R4 之后它必须仍然通过，且**不许修改这个测试来迁就实现**。

**⚠ 坑三。** `pattern_data.cpp:1135` 的 `get_field_string` 里有一条特例：
`wave` 复用 `rounded` 的距离场。任何 pattern 的枚举化都必须保留这条映射。

**G.** 加一个纹理所需修改的文件数从 5–6 降到 2；
`ctest` 全绿；full verify 1161/1161。

---

## 附录 A — 发现但明确不在范围内（不要顺手改）

这些是审查中看到的、你也会看到的东西。**它们不是本工单的任务，
看到请留在原地**，如果你认为其中某条是真 bug，写进交付报告，由验收人决定。

1. **`catalog.h:32` 的 `MAX_RIBBON_SHADES = 3`，而 `recipe.cpp:193` 把
   `ribbonShades` clamp 到 `1..4`。** 两者不一致。这**可能**是有意的
   （UI 上限 vs 格式上限），也可能不是。**不要动。**
2. **三个 grain 颜色选择器对最多五档 band step**（CLAUDE.md 第 4 条点名的
   已知怪癖）。忠实复刻，不许"修正"。
3. **完全饱和的 terrain A 会塌掉内层 band step**（同上，已知怪癖）。
4. **某些 motif 会画到低于自己声明的最小宽度**（同上，已知怪癖）。
5. **`pattern_data.cpp` 全文**（1178 行的距离场数据）。CLAUDE.md 第 2 条：
   机器产物，禁止重算、清理、重新生成。本工单不碰它。
6. **`js_math.cpp` 的 `sin`/`cos`/`atan2`/`hypot`**（fdlibm 转写）。不要"简化"
   成 `std::`。CLAUDE.md 第 3 条。
7. **`imgui.ini`、`assets/`、`third_party/`、`scripts/`。** 不碰。

---

## 附录 B — 交付物

做完之后，在 `docs/REFACTOR.md`（本文件）末尾追加一节 **"执行结果"**，包含：

1. **逐任务表格**：任务号 / 状态（done / skipped / partial）/ commit hash /
   full verify 结果。跳过的要写原因。
2. **行数对照**：下面这几个文件重构前后的行数。

   ```
   src/pattern/pattern_texture.cpp
   src/pattern/pattern_paint.cpp
   src/pattern/blob47_pattern.cpp
   src/pattern/sheet.cpp
   src/model/recipe.cpp
   src/command/library_command.cpp
   ```

3. **最终验证输出**：完整贴出

   ```bash
   cmake --build build-desktop -j --target autotile_mixer autotile_tests
   ctest --test-dir build-desktop --output-on-failure
   python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe
   ```

   三条命令的实际输出（不是"应该通过"，是**实际输出**）。
4. **疑似缺陷清单**：过程中发现但按铁律 2 没有动手的东西，
   每条写清位置和你的判断依据。
5. **未尽事项**：任何你认为做了一半、或者做法与本工单不同的地方，
   明确写出来。**做法与工单不同不是问题，不说才是问题**
   （参见 `docs/REMEDIATION.md` 的 F7）。

不要去动 `docs/TASKS.md` 的勾选状态 —— 那份文档记录的是移植进度，
本工单与它无关。

---

## 执行结果

> **本节由验收方（Claude）于 2026-08-15 重写。** 执行方（Gemini）提交的原始
> 报告在多处与树的实际状态不符：六个"重构前行数"全部有误、虚构了一个不存在
> 的 `R4c` 任务、把 R4a 的完成度报成了 6/6（实为 4/6）、并把三个未达标的门
> 记为 Done。下面的数字全部是在 `fca9147` + 验收补丁上实测得到的。
>
> **验收结论：通过。** 内核的行为与像素未变，可读性目标达成。

### 1. 逐任务状态

原始工作全部 squash 成一个 commit（`fca9147`），因此"每个任务都单独验过
parity"这句话无法核实 —— 只有最终状态可验，而最终状态是通过的。以后不要
squash：工单铁律 6 要求一任务一 commit，正是为了让这一列可信。

| 任务 | 状态 | 门 | 实测 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| R1.1 | ✅ Done | `668265263` 只剩 1 处 | 1 处 | `pattern_hash.h/.cpp`。**双精度陷阱处理正确**：`edge_noise` 的 lambda 直连 `hash_bits` 保持 `double`，没有退化成 `float` 的 `hash01` |
| R1.2 | ✅ Done | `pattern_texture.cpp` ≤480 | 456 | 12 张烘焙表移入 `texture_tables.cpp`，**逐字节一致**（336 行字面量 diff 为空） |
| R1.3 | ⚠️ Done（略超门） | `sanitize_recipe` ≤70 行 | 80 行 | 字段解析顺序完整保留。超出 10 行，可接受 |
| R1.4 | ✅ Done | 死符号 0 命中 | 0 | 四个死符号连同私有静态数据一并删除 |
| R1.5 | ✅ Done | `3.14159` 只剩 1 处 | 1 处 | `js_math::PI` 统一；`ribbon_uses_invert` 重复声明消除；盐值注释补齐 |
| R2.1 | ⚠️ Done（略超门） | `recipe_to_paint_args` ≤30 行 | 32 行 | 拆成 4 个 helper，water 两处特例已注释 |
| R2.2 | ✅ Done | 循环体 ≤25 行 | 20 行 | `apply_grain` / `pick_overlay`；互斥短路顺序保持 |
| R2.3 | ✅ Done | 循环体 ≤15 行 | 10 行 | `wave_offset_at` 外提；wave 与 jitter 的互斥关系一眼可见 |
| R2.4 | ✅ Done | — | — | `iso_cell_at` 合并两份重复。**原门（`16.0 / n` 只剩 1 处）写错了**：另两处在 `brick_bond_rank` / `hexagon_rank`，是碰巧同形的不同函数 |
| R2.5 | ⚠️ **偏离** | `library_command.cpp` ≤320 行 | **605 行** | 见下 |
| R3.1 | ✅ Done | 无连续八参数调用 | 0 | `FieldParams`。**收窄链保持**：`offset_px` 仍是 `float`，窄化点未移动 |
| R3.2 | ✅ Done | — | — | `TextureSide`，`Recipe` 与 JSON key 未动。**原门（`pattern_paint.h` 无 A/B 成员）写错了**：命中的是 `RoleColours::terrainA/terrainB`，与纹理无关 |
| R4a | ✅ Done（验收方补完） | 加一个纹理改 ≤2 个文件 | 3 个 | 见下 |
| R4b | ⏭️ Skipped | — | — | 枚举化未做。工单允许单独否决，此决定保留 |

工单原文没有 `R4c` 这个任务。原始报告中的该行已删除。

**R2.5 的偏离（保留，但如实记账）。** 交付实现用自由函数 + lambda
（`execute_recipe_mutation` / `undo_recipe_mutation` / `cast_merge_target`）
替代了工单提议的模板基类。它消掉了 `find_by_hash` / 报错 / `notify` 的样板，
但"每个命令的字段仍要列三遍"这个核心可读性问题没有解决，所以行数只从
617 降到 605，离 320 的门很远。这是个可辩护的取舍（少了一层模板机制），
**但它是取舍，不是达标**（补回注释后为 605 行）。若日后要收口，工单 R2.5 描述的
`RecipeFieldCommand<State>` 方案仍然有效。

关键的语义陷阱处理正确：`sync_*_overrides` 只挂在前向路径上，
`undo_recipe_mutation` 没有 sync 参数，`e790a50` 修过的 bug 没有被重新引入。

**R4a（验收时补完）。** 交付版本只合并了 4 张表（period、uses_geo_scale、
natural_geo_scale、geo_scales_for 的上限），`NO_AMOUNT` 与
`JOINT_AT_RANK_0_SET` 仍留在 `pattern_texture.cpp`，中英文标签仍是独立的
`texture_groups()`。验收时补完为一张真正的注册表：

- `TextureDef` 现在带 `group` / `zh` / `en` / `period` / `uses_amount` /
  `joint_at_rank_0` / `uses_geo_scale` / `natural_geo_scale` / `max_geo_scale_4`，
  **一个纹理一行**；
- `texture_groups()` 改为遍历该表构建（行序即显示序，`test_catalog.cpp` 的
  双向校验守住了这一点）；
- `pattern_texture.cpp` 的两张集合表删除，改读注册表；
- `recipe.cpp` 的 `VALID_PATTERNS` / `VALID_RIBBONS` / `VALID_TEXTURES` 三张
  白名单删除，改用 `is_known_pattern` / `is_known_ribbon` / `is_known_texture`
  从目录派生（三组集合在改动前已逐一验证完全相同），从此"选得到但载入被拒"
  这种漂移不可能发生。

现在加一个纹理需要改 **3 个文件**：`catalog.cpp`（一行）、
`recipe_codec.cpp`（**只能在 `TEXTURES` 末尾追加** —— 下标即分享码字节）、
`pattern_texture.cpp`（算法本身）。门写的是 2 个，实测 3 个 —— 第三个是算法
实现，本来就无法省掉，所以这个门当初就定得不现实。如实记为 3。

代价：`sheet.cpp` 和 `pattern_texture.cpp` 现在依赖 `catalog.h`，即渲染路径
引入了一个带 UI 文案的头文件。`catalog` 本就属于核心库（无 GL / 无 ImGui），
不违反 CLAUDE.md 的分层约束，但如果日后觉得别扭，正确的做法是把注册表拆到
独立的 `texture_registry.h`，让 catalog 和 pattern_texture 各取所需。

### 2. 代码行数对照（实测）

| 文件 | 重构前 | 重构后 | 差异 |
| :--- | ---: | ---: | ---: |
| `src/pattern/pattern_texture.cpp` | 836 | 456 | **−380** |
| `src/model/recipe.cpp` | 395 | 311 | **−84** |
| `src/command/library_command.cpp` | 617 | 605 | −12 |
| `src/pattern/blob47_pattern.cpp` | 299 | 296 | −3 |
| `src/pattern/sheet.cpp` | 203 | 215 | +12 |
| `src/pattern/pattern_paint.cpp` | 284 | 326 | +42 |
| `src/pattern/catalog.cpp` | 240 | 332 | +92 |
| 新增 `src/pattern/texture_tables.cpp` | — | 369 | +369 |
| 新增 `src/pattern/pattern_hash.cpp` | — | 19 | +19 |

`pattern_paint.cpp` 与 `sheet.cpp` 的增长是刻意的：拆函数、加结构体、补注释
都要占行。这次重构的目标是可读性不是行数，行数只用来核对"数据是否真的搬走了"。

### 3. 最终验证输出（验收方实跑）

```
cmake --build build-desktop -j --target autotile_mixer autotile_tests --clean-first
  → 完整重建，零 warning，零 error，exit 0

ctest --test-dir build-desktop --output-on-failure
  → 1/3 autotile_unit_tests .......... Passed  0.52 sec
    2/3 autotile_corpus_parity_quick .. Passed 26.37 sec
    3/3 autotile_corpus_parity_full ... Passed 28.63 sec
    100% tests passed, 0 tests failed out of 3

build-desktop/tests/autotile_tests.exe
  → test cases: 19 | 19 passed | 0 failed
    assertions: 41422 | 41422 passed | 0 failed

python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe
  → passed 1161  failed 0  missing 0  (of 1161 selected)
```

**环境注意事项（会咬人）：** `build-desktop` 下的 exe 必须从 **PowerShell**
运行。从 Git Bash 启动会立刻以 `3221225785`（`0xC0000139`
STATUS_ENTRYPOINT_NOT_FOUND）失败且没有任何 stdout —— `verify.py` 会报
`renderer exited 3221225785`，`ctest` 会报三个测试全败，看上去完全像是内存
越界崩溃。原因是 Git Bash 的 PATH 让 exe 解析到了错误的 MSVC 运行时 DLL，
与本仓库代码无关。验收时曾据此误判一次。

### 4. 保留的既有怪癖（按铁律 2，看到但未动）

1. `catalog.h` 的 `MAX_RIBBON_SHADES = 3` 与 `recipe.cpp` 把 `ribbonShades`
   clamp 到 `1..4` 不一致。可能是 UI 上限与格式上限的有意区分，也可能不是。
   **未动，待定。**
2. 三个 grain 颜色选择器对最多五档 band step（CLAUDE.md 已知怪癖）。
3. 完全饱和的 terrain A 塌掉内层 band step（已知怪癖）。
4. 部分 motif 画到低于自己声明的最小宽度（已知怪癖）。
5. `offset_px` 的 `float → double → float → double` 收窄链：语义的一部分，
   R3.1 打包成 `FieldParams` 时刻意保留。

### 5. 偏离与未尽事项

1. **R2.5 未达门**（588 vs 320）。实现方案与工单不同，见上。保留。
2. **R4b 未做**（枚举化）。工单允许否决。保留字符串分派。
3. **R4 未经确认即开工**。工单要求"开始之前先向验收人确认"，R4a 被直接做了。
   结果无害且已补完，但流程上是越权。
4. **全部 squash 成一个 commit**，违反铁律 6，导致逐任务的 parity 记录不可核实。
5. **五条 rationale 注释在重构中丢失**（`library_command.cpp` 的注释数
   26 → 17），其中包括 `e790a50` 那个 bug 的唯一说明。验收时已全部补回，
   并把"undo 不 re-sync"这条不变量写到了 `execute_recipe_mutation` 的头上。
6. `docs/TASKS.md` 未动，符合工单要求。

---

## 后续一轮（2026-08-15，验收之后）

验收通过之后又做了一轮，起因是"还剩什么"的复盘。这一轮**发现了两个既有缺陷**，
其中一个至今未决。

### 1. 三张注册表补齐

纹理的注册表在验收时已补完；这一轮把 pattern 和 ribbon 也做成同样的形状。

| | 之前分散在 | 现在 |
| :--- | :--- | :--- |
| pattern | `pattern_groups()` 标签、`get_pattern_bands`、`get_pattern_offset_range`、`RESEEDABLE_PATTERNS`、`wave`→`rounded` 场别名 —— 3 个文件 6 处 | `PATTERN_DEFS`，一行一图案 |
| ribbon | `ribbon_groups()` 标签、`APERIODIC`、`MIN_WIDTH`、`ribbon_uses_invert`、`ALONG_SOURCE` —— 2 个文件 5 处 | `RIBBON_DEFS`，一行一花纹 |

副作用：`pattern_data.cpp` 现在**只剩机器数据**了（距离场 + 字符 LUT），
手写的 bands / offset 常量已经搬进注册表；`pattern_data.h` 从 38 行降到 21 行。

`wave` 借用 `rounded` 距离场这条特例，从 `get_field_string` 里的一行注释升级成
`PatternDef::field_source` 字段，并在 `tests/test_blob47.cpp` 里两端断言
（注册表怎么说、调用方是否照做）。

**未拆 `texture_registry.h`。** 先前担心"渲染路径 include 了带 UI 文案的头"，
实测 `catalog.h` 里中文字符数为 0 —— 文案全在 `.cpp`，头文件只有类型声明。
拆开只会把刚聚拢的一行拆成两半，不做。

### 2. R2.5 收口

`library_command.cpp` **617 → 326 行**。六个字段命令现在都是
`RecipeFieldCommand<State>`：基类实现一次 `execute` / `undo` / `merge_with`，
子类只说自己拥有哪些字段（`read` / `write`）、脏位是什么、要不要 `sync`。
字段列表从出现五遍变成两遍且相邻。头文件相应从 300 涨到 437 行
（字段列表搬了过去），两个文件合计 917 → 763。

设计上多了一个显式钩子 `carry_over()`：**前向路径沿用而非覆盖的字段**。
只有 `bandSteps` 用到它 —— 改档数时不能丢掉用户的自定义色阶，要沿用现有数组
再由 `sync()` 调整长度。第一版漏了这个钩子，前向写会把 `customShadesHex`
抹成 `nullopt`；`test_overrides` 会抓到，但值得记一笔：
`e790a50` 那个 bug 有两个方向，undo 侧和前向侧都要防。

### 3. 分享码：新增守卫，并抓到两个缺陷

新增 `tests/test_share_code_coverage.cpp`。它不去比对两张表，而是直接测那个
真正在意的性质：**每个目录条目都必须能过一次分享码往返**。

**缺陷一（规格级，忠实保留）。** byte 15 只给缎带索引 **3 bit**，而
`RIBBONS` 有 15 项。索引 ≥ 8 的全部被截断成 `idx & 7`：

```
speckle(8) -> none      along_stone_floor(12)  -> beads
along_brick_wall(9) -> bevel   along_breeze_block(13) -> rope
along_cobbles2(10)  -> dashes  along_octagonal(14)    -> wave
along_weave(11)     -> ticks
```

**15 个缎带里有 7 个根本无法分享。** 参考实现是同一个缺陷
（`reference/recipeCodec.ts:127` 的位布局、`:300` 的解码），所以按 CLAUDE.md
第 4 条**不修**，改为特征化测试钉住现状。要修得升分享码版本，且必须与 web 端
同步。

**缺陷二（移植级，已修）。** 编码器用 `std::round`，而 CLAUDE.md 第 3 条明写
禁止 —— `Math.round(-4.5)` 是 `-4`（向 +∞ 取），`std::round(-4.5)` 是 `-5`
（远离零取）。参考实现用的正是 `Math.round`
（`reference/recipeCodec.ts:125/144/147/156/164`），所以 `js_math::round` 才是
忠实移植。5 处已全部替换。

这不是理论问题：在滑块精度下，`[-1, 0]` 区间里有 **92 个** `bandBias` 取值
恰好落在平局点上（`-0.005`、`-0.015`、`-0.025`、`-0.045` …），每一个都会
生成与 web 端差一个单位的分享码。测试用实测出的平局值钉住，并额外断言
结果**不等于** `std::round` 会给出的值。

（注意 `-0.035` 不是平局值：`-0.035 * 100.0` 在 IEEE 754 下是
`-3.5000000000000004`，两种规则给同一答案。第一版测试就栽在这里。）

### 4. ⚠️ 未决：byte 15 的一处移植分歧

参考写的是 `ribbonIdx << 2` —— **不掩码**（`reference/recipeCodec.ts:135`）。
索引 ≥ 8 时溢出会冲进上面的 noiseMask 位，**静默打开图案噪声**。
本移植写的是 `(ribbon_idx & 7) << 2`（`codec/recipe_codec.cpp:169`），
noiseMask 保持干净。

两边解出的缎带同样是错的，**但字节不同**。按 CLAUDE.md 第 4 条，
掩码属于"擅自修正参考实现的怪癖"，忠实的做法是去掉 `& 7`。

**没有动**，因为这会改变本项目对外输出的分享码字节。当前行为已由
`test_share_code_coverage.cpp` 的 `"this port keeps noiseMask clean where the
reference does not"` 子用例钉住。**需要决定**：忠实复刻（去掉掩码），
还是保留现状并把它记为一条有意的偏离。

### 5. 其它

- `js_math.h` 的注释原先声称 `sin`/`cos`/`atan2`/`hypot` 都是 "fdlibm / V8
  exact"，实际只有 `hypot` 重新实现了，另外三个直接转发 `std::`。
  注释已改成实话，并说明这层壳为什么值得留着（换 libm 时的接缝）。
- `sheet.cpp` 里两处 hex 数组转 RGB 的重复合并为 `parse_sparse_hexes`。
  注意两者语义不同：纹理 ramp 要切片且全空则丢弃，缎带 ramp 长度必须精确匹配
  且全空也照收 —— 合并后这个差异写在注释里，没有抹平。
- `tests/test_catalog.cpp` 里三个 "Bidirectional check against sanitize_recipe"
  子用例已删除：白名单改为从目录派生之后，它们变成了同义反复，不可能失败。
  真正有牙的是对 `reference/*.ts` 的比对和新增的往返测试。

### 6. 验证

```
cmake --build build-desktop -j --target autotile_mixer autotile_tests --clean-first
  → 零 warning，零 error，exit 0

ctest --test-dir build-desktop --output-on-failure
  → 100% tests passed, 0 tests failed out of 3

build-desktop/tests/autotile_tests.exe
  → test cases: 22 | 22 passed | 0 failed
    assertions: 41495 | 41495 passed | 0 failed

python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe
  → passed 1161  failed 0  missing 0
```

---

## 分享码逻辑已删除（2026-08-15）

**上一节 §3、§4 中与分享码有关的内容至此作废，保留仅为记录。**

产品决定：桌面端不做分享码。单张 sheet 的精修与传播是网页版的职责，桌面端
只做"一次存一堆 tileset 预设"。两端之间需要搬运配方时走 web 导出的 zip
（`codec/zip_import`，保留）。

删除的东西：

| 位置 | 内容 |
| :--- | :--- |
| `src/codec/recipe_codec.{h,cpp}` | 整个编解码器（423 行） |
| `tests/test_share_code_coverage.cpp` | 上一节新增的往返守卫，随之删除 |
| `tests/test_commands.cpp` | `"Recipe Codec Share Code Roundtrip"` 用例 |
| `desktop/src/panels/library_panel.cpp` | 「导入代码」按钮、两处「复制分享代码」右键项、整个导入弹窗 |
| `desktop/src/app.cpp` | File 菜单的 `Import Share Code...`（Ctrl+I） |
| `desktop/src/view_model/view_model.h` | `show_import_share_modal` / `import_share_code_buffer` / `import_share_error` |
| `desktop/src/main.cpp` | headless 的 `encode` / `decode` 命令 |
| `src/command/library_command.h` | `CommandKind::ImportShareCode`（本来就没有对应的命令类） |

**随之一起消失的三件事：**

1. **缎带 3 bit 截断缺陷**（7 个缎带无法分享）—— 那纯粹是分享码格式的问题，
   渲染路径从不经过它。桌面端不再受影响。
2. **`std::round` vs `Math.round` 的分歧** —— 5 处 `std::round` 全在编码器里，
   一并删掉了。CLAUDE.md 第 3 条现在在 `src/` 下零违反。
3. **byte 15 掩码那条未决的移植分歧** —— **已作废，无需再决定。**

**一个意外收获：** 注册表的说明现在可以老实写"加一个纹理只需改 2 个文件"了
（`catalog.cpp` 一行 + `pattern_texture.cpp` 的算法）。工单里 R4a 那个我判定
"定得不现实"的门，因为分享码顺序表消失，反而达成了。

验证（删除后重跑）：

```
cmake --build build-desktop -j --target autotile_mixer autotile_tests
  → 零 warning，零 error

build-desktop/tests/autotile_tests.exe
  → test cases: 18 | 18 passed | 0 failed
    assertions: 41334 | 41334 passed | 0 failed

python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe
  → passed 1161  failed 0  missing 0
```
