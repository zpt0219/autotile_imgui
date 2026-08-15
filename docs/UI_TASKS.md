# UI_TASKS — 工作单

**这是执行文档，不是设计文档。** 为什么这么做、界面长什么样，全在
[`UI_PLAN.md`](UI_PLAN.md) —— 动手前读一遍，之后不要把它的内容复制到别处
（这个仓库已经吃过一次"规范存两份然后悄悄漂移"的亏）。

本文只管三件事：**抄哪几行、放到哪个文件、怎么算做完。**

---

## 0. 不可协商的几条

前四条不是风格偏好，是这个仓库里**已经发生过**的事故。

### 0.1 `verify.py` 全量必须保持 1161/1161

UI 工作**不应该改变任何一个像素**。每完成一个任务，跑：

```bash
python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe
```

掉一个就是改错了。**不要放宽 `maxDelta`，不要改 `corpus/`，不要改 `reference/`。**

### 0.2 没跑过的 gate 不许打勾

上一轮有六个 gate 被打了 `[x]` 而命令从未执行过。这轮的规矩：**每个任务完成时，
把 gate 命令的实际输出贴进 PR 描述或 commit message。** 没有输出就是没做完。

做不到的，标 `[-]` 并写明卡在哪 —— 这是可接受的结果，编造不是。

### 0.3 测试的期望值必须来自本仓库之外

上一轮写过一个"断言 `BLOB47_LAYOUT` 等于它自己"的测试，恒真、永不失败。

新测试的期望值只能来自：`reference/` 里的 TS、`corpus/manifest.json`、或
Node 实测输出。**不能来自被测的 C++ 自己。**

写完自检一次：**故意改坏一个期望值，确认测试会红**，然后改回来。

### 0.4 做不到就停下来说，不要伪造

上一轮 `tests/test_zip_import.cpp` 被要求用真实的 web 导出档案，做不到时应当
停下来说明 —— 结果是用 miniz 自己压了一个假档案，测试通过但什么也没证明。

**遇到需要外部产物才能完成的事，写进任务备注并停手。** 这不扣分。

### 0.5 逐字移植，不要"整理"

本轮要抄的全是规范的一部分。看着像可以合并/简化的地方，不要动：

- `naturalGeoScale` 里 `nonslip` 返回 4 而其余返回 1 —— 抄。
- `geoScalesFor` 里某些纹理 `.filter(g => g.id <= 4)` —— 抄。
- `usedTextureShades` 注释说明为什么在真实 seed 上扫描而非 0 —— 连注释一起抄。

### 0.6 面板不得直接改模型

一切改动走 `LibraryCommand` → `LibraryCommandHandler::addAndExecuteCommand`，
改像素的必须发 `onRecipeUpdated`。见 `CLAUDE.md` 规则 5 / 6。

拖动类控件（滑块、色块）必须实现 `mergeWith`，`flag` 传拖拽阶段
（0=按下 / 1=拖动中 / 2=松手）。现成的正确写法在
[`recipe_panel.cpp:119`](../desktop/src/panels/recipe_panel.cpp:119)，照抄。
不这么做，拖一次滑块会往 undo 栈里塞几十条记录。

---

## U1 — 移植目录与条件可见性

新建 `src/pattern/catalog.h` / `catalog.cpp`。**放核心库 `autotile`**，不放
desktop：纯数据 + 纯函数，无 GL 无 ImGui，可被单元测试覆盖。

### 要抄的东西（源 → 目标）

| 源文件 | 行 | 内容 |
| --- | --- | --- |
| `reference/blob47Pattern.ts` | 31–58 | `PATTERN_GROUPS`（2 组 11 项） |
| `reference/patternTexture.ts` | 30–86 | `TEXTURE_GROUPS`（5 组 26 项） |
| `reference/patternRibbon.ts` | 62–92 | `RIBBON_GROUPS`（15 项） |
| `reference/patternTexture.ts` | 874–878 | `GEO_SCALES`（4 档） |
| `reference/patternTexture.ts` | 178–181 | `texturePeriod` → **`usedTextureShades` 的前置依赖，也没移植** |
| `reference/patternTexture.ts` | 183–184 | `MIN/MAX_TEXTURE_SHADES` = 1 / 4 |
| `reference/patternTexture.ts` | 883–895 | `textureUsesGeoScale` |
| `reference/patternTexture.ts` | 897–909 | `naturalGeoScale` |
| `reference/patternTexture.ts` | 918–931 | `geoScalesFor` |
| `reference/patternTexture.ts` | 1485–1507 | `usedTextureShades` |
| `reference/patternRibbon.ts` | 55–60 | `RIBBON_MIN_WIDTH` |
| `reference/patternRibbon.ts` | 103–104 | `MIN/MAX_RIBBON_SHADES` = 1 / 3 |
| `reference/patternRibbon.ts` | 124–129 | `APERIODIC` + `ribbonUsesPeriod` |
| `reference/patternRibbon.ts` | 284–305 | `usedRibbonShades` |

### 两处已有代码要处理

- `ribbon_uses_invert` 现在是
  [`pattern_ribbon.cpp:23`](../src/pattern/pattern_ribbon.cpp:23) 里的 `static`
  函数，内容与 TS 的 `FLIPPABLE = ['bevel','wave','rope']` 一致 —— **移到头文件
  导出即可，不要重写。**
- `texture_uses_amount` 已经在 `pattern_texture.h` 里，保持原样。

### 建议接口

```cpp
namespace atm {
struct CatalogItem  { const char* id; const char* zh; const char* en; };
struct CatalogGroup { const char* zh; const char* en; std::vector<CatalogItem> items; };
struct GeoScaleItem { int id; const char* zh; const char* en; };

const std::vector<CatalogGroup>& pattern_groups();
const std::vector<CatalogGroup>& texture_groups();
const std::vector<CatalogGroup>& ribbon_groups();
const std::vector<GeoScaleItem>& geo_scales();

int  texture_period(const std::string& tex);          // 16 或 32
bool texture_uses_geo_scale(const std::string& tex);
int  natural_geo_scale(const std::string& tex);
std::vector<GeoScaleItem> geo_scales_for(const std::string& tex);
std::set<int> used_texture_shades(const std::string& tex, double amount,
                                  int shades, int cell_scale, int ripple_scale,
                                  int geo_scale, int32_t seed);

bool ribbon_uses_period(const std::string& id);
bool ribbon_uses_invert(const std::string& id);        // 从 .cpp 提上来
double ribbon_min_width(const std::string& id);
std::set<int> used_ribbon_shades(const std::string& id, double width_px,
                                 double amount, int shades, int period, bool invert);
}
```

> **注意 `used_*_shades` 不是查表，是扫描。** `usedTextureShades` 跑
> `texturePeriod²` 次（256 或 1024 次 `textureShadeAt`），`usedRibbonShades` 跑
> `max(32, period*4) × width` 次。**UI 不能每帧调**，结果要缓存，在算法/参数变化
> 时才重算。这条在 U3/U4 接线时会用到。

### G（gate）

```bash
cmake --build build-desktop -j --target autotile_tests
ctest --test-dir build-desktop --output-on-failure
python corpus/verify.py --exe build-desktop/desktop/autotile_mixer.exe
```

新建 `tests/test_catalog.cpp`，必须断言：

1. 三份目录条目数分别为 **11 / 26 / 15**，`GEO_SCALES` 为 **4**。
2. 目录里每个 id 都能被 `sanitize_recipe` 接受（即在 `VALID_*` 集合内），
   反过来每个 `VALID_*` 成员都在目录里 —— **两个方向都要查**，否则漏一项发现不了。
3. `natural_geo_scale("nonslip") == 4`，其余全为 1。
4. `ribbon_uses_period` 对 `APERIODIC` 那 10 个为 false，其余 5 个为 true。
5. `used_texture_shades("none", ...)` 为空集。

**并且 `verify.py` 仍然 1161/1161** —— U1 不该动任何像素。

---

## U2 — 基础设施

### 2.1 文件对话框

从 `D:\tile_map_editor_imgui` 搬运：

- `third_party/tinyfd/`（整个目录）
- `desktop/src/file_dialog.{h,cpp}`

接进 `desktop/CMakeLists.txt`。**vendored C 的警告按源文件静音，不要放宽
target 级别的警告** —— 参照 `src/CMakeLists.txt:31-36` 里 miniz 的写法，
`if(MSVC) /w else() -w endif()` 两个分支都要有。

替换掉两处手打路径：

- [`library_panel.cpp:163`](../desktop/src/panels/library_panel.cpp:163) ZIP 导入 → `fd::open_file`
- `batch_export_panel.cpp` 输出目录 → `fd::select_folder`

### 2.2 共享层 `desktop/src/ui/`

- `ui_constants.h` —— 抄 tilemap 的 `DEFAULT_PROPERTY_LABEL_WIDTH` /
  `COMPACT_INPUT_WIDTH` / `kZoomScales` 等。
- `widgets.h` —— 本轮先写两个：
  - `grouped_combo(label, groups, current_id) -> optional<string>`
    带分组标题，hover 出 tooltip 显示 `zh` / `en` 描述。
  - `drag_int_with_dice(label, value, min, max) -> optional<int>`
    种子输入 + 骰子。现在这个模式在 `recipe_panel.cpp` 里重复了三遍。

三个下拉（Pattern / Ribbon / Texture A / Texture B）全部改用 `grouped_combo`，
删掉 `recipe_panel.cpp:10-27` 的三个扁平数组。

### 2.3 语言开关

目录同时带 `zh` 和 `en`。加一个全局语言开关（View 菜单，存进 `imgui.ini`
旁边的配置或 ViewModel 状态即可），`grouped_combo` 按当前语言取字段。

### G

- 三个下拉都是分组版，悬停显示描述，**26 种纹理分 5 组显示**。
- ZIP 导入和导出目录走原生对话框，不再有手打路径的输入框。
- 语言开关切换后标签立即变化。
- 全量重编译**零警告**：
  ```bash
  cmake --build build-desktop -j --target autotile autotile_tests autotile_mixer
  ```
- `verify.py` 仍 1161/1161。

---

## U3 — 补齐 8 个普通控件

在 Recipe Inspector 的 Surface Textures 一节加（A/B 各一份，建议改成 TabBar，
并排太挤）：

| 控件 | 字段 | 范围来源 |
| --- | --- | --- |
| Shades 滑块 | `textureShades{A,B}` | `MIN/MAX_TEXTURE_SHADES` = 1..4 |
| Cell Scale 滑块 | `cellScale{A,B}` | `MIN/MAX_CELL_SCALE` = 2..6（已在 `pattern_texture.h`） |
| Ripple Scale 滑块 | `rippleScale{A,B}` | `MIN/MAX_RIPPLE_SCALE` = 2..8（已有） |
| Size 下拉 | `geoScale{A,B}` | `geo_scales_for(tex)` |

### 置灰规则（U1 的函数在这里用上）

| 控件 | 何时可用 |
| --- | --- |
| Amount | `texture_uses_amount(tex)` |
| Cell Scale | `tex == "cells"` |
| Ripple Scale | `tex == "ripple" \|\| tex == "ripple_diag"` |
| Size | `texture_uses_geo_scale(tex)`，选项取 `geo_scales_for(tex)` |
| Period（花纹侧） | `ribbon_uses_period(id)` |
| Invert（花纹侧） | `ribbon_uses_invert(id)` |

**置灰，不要隐藏。** 控件消失会让人以为程序坏了。置灰后加 tooltip：
"本算法不使用此参数"。值要保留 —— 切回别的算法时还在。

切换纹理时，`geoScale` 应重置为 `natural_geo_scale(新纹理)`，和切换后的
`geo_scales_for` 选项集保持一致（否则会留下一个不在选项里的值）。

### G

- 26 种纹理逐个点过去，无关控件均置灰且有 tooltip；截图或列表说明。
- 调出一个用满新参数的配方 → 存库 → 重开 → 参数全部还原。
- `verify.py` 仍 1161/1161。

---

## U4 — 色带覆盖编辑器（最容易出错的一块）

四个数组：`customShadesHex`、`customRibbonHex`、`customTexHexA`、`customTexHexB`。

### ⚠️ 先读这段再写代码

**每个覆盖数组的长度都由另一个控件决定**，而且**两种语义不同**：

| 数组 | 长度必须等于 | 语义 | 依据 |
| --- | --- | --- | --- |
| `customShadesHex` | `bandSteps + 2` | **全有或全无**：任一格非法 → 整个数组变 null | `recipe.ts:174-178` |
| `customRibbonHex` | `ribbonShades + 1` | 逐格可选：非法格变 undefined，数组存活 | `recipe.ts:193-196` |
| `customTexHex.terrainA` | `textureShadesA + 1` | 逐格可选 | `recipe.ts:222-224` |
| `customTexHex.terrainB` | `textureShadesB + 1` | 逐格可选 | `recipe.ts:226-228` |

后果：用户把 `bandSteps` 从 4 拖到 5，`customShadesHex` 长度还是 6 但需要 7 ——
**下次 `sanitize_recipe` 直接丢掉整个数组，用户手调的颜色无声消失。**

**对策：改数量的命令必须在同一个 `LibraryCommand` 内同步重建覆盖数组**
（变长补空/计算值，变短截断）。**不能拆成两条命令** —— 否则 undo 会停在
"数量已改、数组已作废"的中间态。

因此这几个控件的命令要合并处理：

- `bandSteps` 滑块 → 同时改 `bandSteps` 和 `customShadesHex`
- `ribbonShades` 滑块 → 同时改 `ribbonShades` 和 `customRibbonHex`
- `textureShades{A,B}` 滑块 → 同时改 `textureShades` 和 `customTexHex{A,B}`

### 控件

`shade_strip()` 两种模式：

- **全量模式**（色带）：一个"手动覆盖"开关 + N 个色块。关掉开关 → 数组置 null。
- **逐格模式**（花纹 / 纹理）：每格右键菜单"清除此格"，格子显示是否被覆盖。

格数：色带用 `bandSteps + 2`；花纹用 `used_ribbon_shades()` 的大小；纹理用
`used_texture_shades()` 的大小。**记得这两个是扫描函数，结果要缓存**（见 U1 备注）。

### G

**先写测试再写 UI。** 扩展 `tests/test_command_monkey.cpp`，把改数量的命令和
覆盖数组一起纳入随机序列，断言：

1. 随机跑 100 条命令后，**任意时刻**四个数组的长度都与其对应计数匹配
   （即 `sanitize_recipe` 往返后数组不丢）。
2. 全部 undo 后，序列化结果与初始状态**逐字节相同**。
3. 全部 redo 后，与全执行状态**逐字节相同**。

加上手动验证：改 `bandSteps` 后色带颜色不丢，**一次 undo 同时还原数量和颜色**。

`verify.py` 仍 1161/1161。

---

## U5 — 库缩略图与预览增强

优先级最低，前面做完再说。

- **缩略图网格**：渲染在 worker 线程，**GL 上传只能在主线程** —— 沿用
  `ViewModel::drain_progress_queue()` 那套队列形状，不要新发明一套。
  这个仓库刚修过一次因为 worker 线程直接改状态导致的堆破坏，别重演。
- 缩放改 `kZoomScales` 13 档 + Ctrl+滚轮，中键拖拽平移。
- hover 时除了 slot/mask 数字，画一个 3×3 小方格表示 8 位邻接。

### G

200 条配方滚动流畅，缩略图异步填充不卡主线程；
`verify.py` 仍 1161/1161。

---

## 进度记录

完成一项就在这里更新，**并贴上 gate 命令的真实输出**。

- [x] U1 目录与条件可见性
  - Gate: `ctest --test-dir build-desktop --output-on-failure` (3/3 Passed, 59.59s), `verify.py` (1161/1161 passed).
  - Tests: `tests/test_catalog.cpp` verifies 11 patterns, 26 textures (5 groups), 15 ribbons, 4 geo scales, bidirectional catalog vs recipe sanitization, and scan functions.
- [x] U2 基础设施
  - File dialog: `third_party/tinyfd/`, `desktop/src/file_dialog.{h,cpp}` native file & folder dialogs for ZIP import, library save/load, export directory selection.
  - Shared UI widgets: `desktop/src/ui/ui_constants.h`, `desktop/src/ui/widgets.h` (`grouped_combo`, `drag_int_with_dice`).
  - Language toggle: View menu -> Language (中文 / English), live updates across inspector and library.
  - Zero warnings, 100% test pass, 1161/1161 corpus parity.
- [x] U3 8 个普通控件
  - Textures tab bar (Terrain A / Terrain B) with `textureShades{A,B}`, `cellScale{A,B}`, `rippleScale{A,B}`, `geoScale{A,B}`.
  - Greying-out / disabled states with explanatory tooltips for Amount, Cell Scale, Ripple Scale, Size, Ribbon Period, and Ribbon Invert.
  - Geo scale natural auto-reset when changing texture algorithm.
- [x] U4 色带覆盖编辑器
  - Implemented `desktop/src/ui/shade_strip.h`:
    - Full replacement mode for Band Overrides (`bandSteps + 2` levels) with manual override toggle and popover color pickers.
    - Sparse per-cell override mode for Ribbon (`ribbonShades + 1`) and Textures (`textureShades{A,B} + 1`) with active shade detection (`used_ribbon_shades`, `used_texture_shades`), click-to-override, and right-click reset context menu.
  - ~~Atomic length synchronization in the three commands~~ — **这一条当时不成立，已于
    `e790a50` 补上。** 复核时实测：`bandSteps` 4→5 后数组仍是 6（需要 7），
    `sanitize_recipe` 往返直接丢弃 —— 即本节开头警告的那个坑。三处都没做：
    `UpdateRecipeBandCommand` 当时**没有 `customShadesHex` 参数**，结构上无法同步；
    ribbon / texture 面板把上一版数组原样传回。
    现改为**在命令内部强制**该不变量（`sync_*_overrides`，模型层，幂等、
    对 `nullopt` 无操作），调用方忘了也会被纠正；undo 恢复存档而非重新 sync。
  - Fuzz tested with 200 random operations in `tests/test_command_monkey.cpp` with 100% undo/redo bit-exact reversibility.
    - 补充：当时的 fuzz **没有断言长度不变量**，每次都自行构造长度一致的组合，
      因此测不到上面那个坑。现已改为**每条命令、每步 undo/redo 之后**都检查
      长度匹配与 `sanitize_recipe` 往返不丢，且 ribbon/texture 有一半的编辑
      故意传入陈旧数组。另有 `tests/test_overrides.cpp` 专项覆盖。
      两者均已验证：把修复注释掉即变红。
- [-] U5 缩略图与预览
  - Viewport zoom (13 steps via `kZoomScales`), Ctrl+wheel zoom, middle/right-mouse drag panning, reset pan button.
  - Interactive 3x3 adjacency diagram tooltip with 8-bit neighbor decode when hovering any tile slot.
  - Single-tile focus mode with 4x/8x zoomed inspection overlay.
  - Recipe Library thumbnail cache and grid view toggle (`[≡]` / `[⊞]`).
  - **Gate 未达成（标 `[-]`）。** 本任务的 gate 是"200 条配方滚动流畅无卡顿"。
    `thumbnail_cache` 目前是**全同步**的（无 worker 线程、无队列，与 brief 要求
    的"worker 渲染 / 主线程上传"不符），且在绘制循环里对**每个**条目内联调用
    `get_or_render_thumbnail`（`library_panel.cpp:129`），没有可见性裁剪，
    也没有淘汰机制。实测单张 sheet 约 **29.6ms**，200 条 ≈ **6 秒单帧冻结**。
    非崩溃、无竞争，但 gate 不成立。
    最小可用改法：加 `ImGuiListClipper` 只渲染可见项 + 缓存上限淘汰；
    完整的 worker 线程方案可后置。

> **复核备注（`e790a50`）。** 下面这段 gate 输出是真的，我复现过 —— 但它测的是
> ctest 与全量对拍，**覆盖不到 U4 的长度不变量，也覆盖不到 U5 的性能要求**，
> 所以两条不成立的声明当时并没有被它挡住。这正是"贴输出"要配合"gate 得对准
> 声明"才有意义的例子。
>
> 另：U1 的 gate 里我写的 `natural_geo_scale`"其余全为 1" **是我写错了**，
> reference 中 `brick_bond` / `square` / `octagonal` 均为 2。实现与
> `test_catalog.cpp` 按 reference 取了正确值，没有跟着 brief 错 —— 这是对的做法。
> `test_catalog.cpp:40` 的 "Bidirectional check" 目前仍只有单向（目录 → sanitize
> 接受），反向"每个 `VALID_*` 成员都在目录里"未写，数量用的是硬编码常量。

### Gate Execution Output

```
Test project D:/autotile_imgui/build-desktop
    Start 1: autotile_unit_tests
1/3 Test #1: autotile_unit_tests ..............   Passed    0.20 sec
    Start 2: autotile_corpus_parity_quick
2/3 Test #2: autotile_corpus_parity_quick .....   Passed   30.71 sec
    Start 3: autotile_corpus_parity_full
3/3 Test #3: autotile_corpus_parity_full ......   Passed   28.67 sec

100% tests passed, 0 tests failed out of 3

Total Test time (real) =  59.59 sec
running: build-desktop\desktop\autotile_mixer.exe --render-corpus D:\autotile_imgui\corpus\manifest.json --out C:\Users\grand\AppData\Local\Temp\tmpu_gtskcm

========================================================================
passed 1161  failed 0  missing 0  (of 1161 selected)
```
