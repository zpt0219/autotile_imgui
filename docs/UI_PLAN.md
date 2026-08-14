# UI_PLAN — 补齐桌面端控件的方案

现状：核心引擎与网页版逐字节一致，但**前端只暴露了配方的一部分**。这份文档
盘点差距、给出架构方案和分阶段计划。

参照物有两个：

- `reference/` 里的 TypeScript —— 它不只有渲染逻辑，**还带着整套 UI 元数据**
  （分组、中英文标签、每个算法用哪些参数）。这部分一行都没移植。
- `D:\tile_map_editor_imgui\desktop\` —— 成熟的 ImGui 前端，有 `ui_widgets.h`、
  `ui_constants.h`、`file_dialog`、`palette_edit_view` 可以直接抄形状。

排版和交互规范沿用 `CLAUDE.md` 的硬规则，尤其是**规则 5（面板不得直接改模型，
一律走 `LibraryCommand`）**和**规则 6（改像素必须发 `onRecipeUpdated`）**。

---

## 1. 差距盘点

### 1.1 完全没有 UI 的配方字段（12 个）

`Recipe` 有 34 个可调项，Inspector 暴露了 22 个。缺的是这些：

| 字段 | 类型 | 合法范围 | 缺失影响 |
| --- | --- | --- | --- |
| `textureShadesA` / `textureShadesB` | int | 1..4 | 纹理层次数写死在默认值，网页版可调 |
| `cellScaleA` / `cellScaleB` | int | 2..6 | `cells`（Voronoi）的细胞大小不可调 |
| `rippleScaleA` / `rippleScaleB` | int | 2..8 | `ripple` / `ripple_diag` 的波长不可调 |
| `geoScaleA` / `geoScaleB` | int | 1/2/4/8 | 7 种几何纹理的砖块尺寸不可调（32/16/8/4px） |
| `customShadesHex` | `string[]?` | 长度 == `bandSteps + 2` | **色带逐级改色**，桌面端精修的核心卖点 |
| `customRibbonHex` | `(string?)[]?` | 长度 == `ribbonShades + 1` | 花纹逐级改色 |
| `customTexHexA` / `customTexHexB` | `(string?)[]?` | 长度 == `textureShades{A,B} + 1` | 纹理逐级改色 |

前 8 个是普通滑块/下拉，属于漏做。后 4 个是**颜色覆盖数组**，是整块没做的功能。

### 1.2 没移植的 UI 元数据（这是根因）

`reference/` 里躺着现成的分组目录，全是纯数据、零逻辑：

| 来源 | 内容 |
| --- | --- |
| `blob47Pattern.ts:31-55` | `PATTERN_GROUPS` — 2 组 11 项，每项带 `zh` / `en` 描述 |
| `patternTexture.ts:30-84` | `TEXTURE_GROUPS` — 5 组 26 项，带中英文描述 |
| `patternRibbon.ts:62-...` | `RIBBON_GROUPS` — 15 项，带中英文描述 |
| `patternTexture.ts:874-878` | `GEO_SCALES` — `{1:'32px · 原尺寸', 2:'16px', 4:'8px', 8:'4px'}` |

当前 `recipe_panel.cpp:10-27` 用的是三个扁平 `const char*` 数组，**只有裸 id**：
26 种纹理挤在一个下拉里，显示 `ripple_diag`、`along_breeze_block` 这种字符串，
没有分组也没有说明。网页版是分组 + 中英双语描述。

### 1.3 没移植的"条件可见性"辅助函数

reference 里有一批函数，存在的唯一目的就是让 UI 知道**当前算法用得上哪些参数**：

| 函数 | C++ 状态 | 用途 |
| --- | --- | --- |
| `textureUsesAmount` | ✅ 已移植 | Amount 滑块是否有效 |
| `textureUsesGeoScale` | ❌ 缺 | 是否显示尺寸下拉 |
| `geoScalesFor` | ❌ 缺 | 该纹理允许哪几档尺寸 |
| `naturalGeoScale` | ❌ 缺 | 切换纹理时的默认档（`nonslip` 是 4，其余 1） |
| `usedTextureShades` | ❌ 缺 | 实际用到几级 → 决定色板画几个格子 |
| `usedRibbonShades` | ❌ 缺 | 同上，花纹侧 |
| `ribbonUsesPeriod` | ❌ 缺 | Period 滑块是否有效 |
| `ribbonUsesInvert` | ✅ 已移植 | Invert 勾选是否有效 |
| `RIBBON_MIN_WIDTH` | ❌ 缺 | 花纹的最小带宽，带太窄该提示 |
| `MIN/MAX_TEXTURE_SHADES`、`MIN/MAX_RIBBON_SHADES` | ❌ 缺 | 滑块上下限 |

**这不只是"少了个提示"**。现在 Inspector 对所有算法一视同仁地显示
Amount / Period / Invert，用户拖一个对当前算法无效的滑块，画面纹丝不动 —— 看起来
像 bug。26 种纹理各自吃 `{amount, shades, seed, cellScale, rippleScale, geoScale}`
的不同子集，没有这批函数就没法做对。

### 1.4 基础设施缺口

| 缺口 | 现状 | 参照 |
| --- | --- | --- |
| 原生文件对话框 | 无。ZIP 导入 (`library_panel.cpp:163`) 和导出目录让用户**手打路径** | `tile_map_editor_imgui` 的 `fd::open_file/save_file/select_folder`（tinyfd 封装） |
| 共享布局常量 | 无 | `ui_constants.h`：标签宽度、输入框宽度、缩放档位表 |
| 可复用控件 | 无 | `ui_widgets.h` |
| 库面板缩略图 | 只有文字列表 | `PLAN.md:73` 明确写的是"配方库列表 **+ 缩略图网格**" |
| 预览缩放 | 4 个单选钮 1x–4x，无平移 | `kZoomScales` 13 档 + Ctrl+滚轮 |

---

## 2. 架构方案

### 2.1 新增 `src/pattern/catalog.{h,cpp}` —— 放核心库

把四份目录和条件可见性函数逐字移植进核心库 `autotile`，**不放 desktop**：

```cpp
namespace atm {
struct CatalogItem { const char* id; const char* zh; const char* en; };
struct CatalogGroup { const char* zh; const char* en; std::vector<CatalogItem> items; };

const std::vector<CatalogGroup>& pattern_groups();
const std::vector<CatalogGroup>& texture_groups();
const std::vector<CatalogGroup>& ribbon_groups();
const std::vector<GeoScaleItem>& geo_scales();

bool texture_uses_geo_scale(const std::string& tex);
std::vector<GeoScaleItem> geo_scales_for(const std::string& tex);
int  natural_geo_scale(const std::string& tex);
int  used_texture_shades(...);   // 签名对齐 patternTexture.ts:1485
int  used_ribbon_shades(...);    // 对齐 patternRibbon.ts:284
bool ribbon_uses_period(const std::string& id);
int  ribbon_min_width(const std::string& id);
}
```

放核心库的理由：纯数据 + 纯函数，无 GL 无 ImGui，符合 CLAUDE.md 的分层约定；
可被单元测试覆盖；顺带能给 CLI 加 `--list-textures` 之类。

> **移植纪律同规则 2：逐字抄，不要"整理"。** 这些目录是规范的一部分，
> `nonslip` 的 `naturalGeoScale` 是 4 而不是 1 这种事，不抄就会错。
> 加一个测试：目录里的每个 id 都能在 `VALID_*` 集合里找到，且数量对得上
> （pattern 11 / texture 26 / ribbon 15）。

### 2.2 新增 `desktop/src/ui/` —— 共享层

- `ui_constants.h` —— 抄 tilemap 的布局常量 + 缩放档位表。
- `widgets.h` —— 三个自研控件：
  - `grouped_combo()` —— 吃 `CatalogGroup`，画带分组标题的下拉，
    hover 出 tooltip 显示描述。一次写好，pattern/texture/ribbon 三处共用。
  - `shade_strip()` —— **色带覆盖编辑器**，见 §2.3。
  - `drag_int_with_dice()` —— 种子输入 + 骰子按钮，现在这个模式在
    `recipe_panel.cpp` 里重复了三遍。
- `file_dialog.{h,cpp}` + `third_party/tinyfd/` —— 从 tilemap 仓库整体搬运。

### 2.3 `shade_strip()` —— 最要紧的新控件

四个覆盖数组共用一个控件，但**语义有两种，必须区别对待**：

| 数组 | 长度约束 | 语义 |
| --- | --- | --- |
| `customShadesHex` | `bandSteps + 2` | **全有或全无** —— 任一格非法 → 整个数组作废（`recipe.ts:174-178`） |
| `customRibbonHex` | `ribbonShades + 1` | 逐格可选 —— 非法格变 `undefined`，数组存活 |
| `customTexHex.terrainA` | `textureShadesA + 1` | 同上 |
| `customTexHex.terrainB` | `textureShadesB + 1` | 同上 |

所以色带条要有两种模式：色带用"整体覆盖 / 恢复计算值"开关，花纹和纹理用
逐格"覆盖 / 清除"。

> ### ⚠️ 这里有个必踩的坑
>
> **每个覆盖数组的长度都由另一个控件决定。** 用户拖 `bandSteps` 从 4 到 5，
> `customShadesHex` 的长度就从 6 变成 7 —— 不匹配，下次 `sanitize_recipe`
> 直接把整个数组丢掉，用户的手调颜色**无声消失**。`ribbonShades` 对
> `customRibbonHex`、`textureShadesA/B` 对 `customTexHexA/B` 同理。
>
> 对策：改数量的命令必须**在同一个 `LibraryCommand` 里**同步重建覆盖数组
> （变长补 `undefined`/计算值，变短截断），不能拆成两条命令 —— 否则 undo
> 会停在一个数组已作废的中间态。这条要写进命令的单元测试。

### 2.4 命令层要补的

现在 `UpdateRecipeTextureCommand` 是整个 `Recipe` 拷贝进去（`recipe_panel.cpp:250`），
粗暴但能用。新增控件时保持这个路子即可，但要注意：

- 拖动类控件（滑块、色带格子）必须走 `mergeWith`，`flag` 传拖拽阶段
  （0=按下 / 1=拖动中 / 2=松手），否则 undo 栈会被一帧一条记录淹掉。
  现有代码里 `flag` 的算法（`recipe_panel.cpp:119`）是对的，照抄。
- 所有新命令一律发 `onRecipeUpdated` 并带正确的 `DirtyMask`，否则预览不刷新。

---

## 3. 面板逐个设计

### 3.1 Recipe Inspector（改动最大）

按渲染管线顺序重排，每节标题给出该节影响哪一层：

```
▼ 调色板与角色              [Palette & Roles]
    Terrain A / Terrain B / Edge      三个取色器（已有）
    ─────────────────────────────
    色带 (bandSteps + 2 级)            ← 新增 shade_strip
    [■][■][■][■][■][■]  □ 手动覆盖全部 / 恢复计算值

▼ 轮廓与边界                [Silhouette & Boundary]
    Pattern      ← 改成 grouped_combo：规整边缘 / 不规则边缘
    Edge Seed [   ] 🎲
    Outline Width  ──●──  1..4
    Band Steps     ──●──  3..5    ⚠ 改这个会重建上面的色带
    Band Bias      ──●──  -1..1
    ☐ Hard Edge B   ☐ Transparent B

▼ 颗粒噪声                  [Grain Noise]
    ☐ White  ☐ Blue  ☐ Ordered        （已有）
    Strength / Seed 🎲                （已有）

▼ 花纹                      [Ribbon Motif]
    Motif        ← grouped_combo
    Amount       ──●──
    Period       ──●──   ← ribbon_uses_period() 为假时置灰
    Shades       ──●──   1..3
    ☐ Invert             ← ribbon_uses_invert() 为假时置灰
    花纹配色 [■][■][■]    ← shade_strip，格数 = used_ribbon_shades()
    ⚠ 当前带宽 3px < 本花纹最小 4px    ← RIBBON_MIN_WIDTH 提示

▼ 表面纹理                  [Surface Textures]
  ┌ Terrain A ─┬ Terrain B ─┐        ← 改成 TabBar，A/B 并列太挤
    Texture      ← grouped_combo，5 组 26 项
    Amount       ──●──   ← texture_uses_amount() 为假时置灰
    Shades       ──●──   1..4                        【新增】
    Seed [   ] 🎲
    Cell Scale   ──●──   2..6   ← 仅 cells                【新增】
    Ripple Scale ──●──   2..8   ← 仅 ripple/ripple_diag   【新增】
    Size         [32px · 原尺寸 ▾] ← geo_scales_for()     【新增】
    纹理配色 [■][■][■]  ← shade_strip，格数 = used_texture_shades()  【新增】
```

**置灰而非隐藏。** 控件消失会让人以为程序坏了；置灰 + tooltip 说明"本算法不使用
此参数"，同时保留了值（切回别的算法还在）。

### 3.2 Library Panel

补 `PLAN.md` 里写了但没做的缩略图网格：

- 列表 / 网格 两种视图切换。
- 缩略图 = 该配方渲染成 256×192 后缩到 ~64×48 的 GL 纹理，LRU 缓存。
- **渲染在 worker 线程，上传在主线程** —— 和批量导出同一套队列机制，
  沿用 `drain_progress_queue()` 的形状。GL 调用只能在主线程，这条不能破。
- ZIP 导入按钮换成 `fd::open_file`，不再手打路径。

### 3.3 Sheet Preview

- 缩放换成 `kZoomScales` 13 档，Ctrl+滚轮缩放，中键拖拽平移。
- hover 已经能显示 slot/mask（`preview_panel.cpp:106`），补一条：显示该 mask 的
  8 位邻接图示（3×3 小方格），比裸数字直观得多。
- 加"仅显示单块"模式，配合 Inspector 调参时盯住一块看。

### 3.4 Batch Export

- 输出目录换 `fd::select_folder`。
- 命名模板旁边加实时预览：`{name}_{pattern}` → `我的配方_rounded.png`。
- 补 `scale` 导出倍率（之前作为死字段删掉了，UI 做好后再连回去）。

---

## 4. 分阶段计划

沿用 TASKS.md 的体例，**一次一个，过了 gate 再下一个**。

- [ ] **U1 目录与条件可见性移植。** `src/pattern/catalog.{h,cpp}`，逐字抄
      四份目录 + 九个辅助函数。
      **G:** 单测断言三份目录的条目数为 11 / 26 / 15，每个 id 都在对应
      `VALID_*` 集合内；`natural_geo_scale("nonslip") == 4`；
      **且 `verify.py` 全量仍 1161/1161**（这一步不该动任何像素）。

- [ ] **U2 基础设施。** 搬 tinyfd + `file_dialog`，建 `desktop/src/ui/`，
      写 `grouped_combo()` 和 `drag_int_with_dice()`。
      **G:** 三个下拉全部换成分组版，鼠标悬停出中英描述；ZIP 导入和导出目录
      走原生对话框；全量重编译零警告。

- [ ] **U3 补齐 8 个普通控件。** `textureShades`、`cellScale`、`rippleScale`、
      `geoScale`（各 A/B），并接上置灰逻辑。
      **G:** 26 种纹理逐个点过去，无关控件均置灰且带 tooltip；
      随手调出的配方存盘再读回，参数不丢。

- [ ] **U4 色带覆盖编辑器。** `shade_strip()` + 四个数组接线 + 长度联动命令。
      **G:** 改 `bandSteps` / `ribbonShades` / `textureShades` 后覆盖数组
      长度同步且不作废；一条命令 undo 一次到位；
      **命令 monkey 测试扩展到覆盖数组，全 undo 后 JSON 字节一致。**

- [ ] **U5 库缩略图 + 预览增强。** 缩略图网格（worker 渲染 / 主线程上传）、
      13 档缩放、平移、mask 邻接图示。
      **G:** 200 条配方的库滚动流畅无卡顿，缩略图异步填充；
      TSan/ASan 构建下滚动 + 批量导出并行无告警。

---

## 5. 优先级建议

若只做一件事，做 **U1 + U3**：这是"网页版有、我们没有"最直接的部分，且风险最低
（纯加控件，不碰渲染）。

**U4 是桌面端相对网页版的差异化所在** —— 网页版做一张图，桌面端管一个库；
逐级改色配合变体矩阵才是这个工具存在的理由。但它也是最容易出错的一块，
长度联动那个坑必须先有测试再写 UI。

U2 的文件对话框虽小，但"手打路径"是目前最伤日常使用的一处。可以并到 U1 一起做。
