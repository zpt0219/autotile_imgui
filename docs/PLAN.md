# autotile_imgui — 桌面版 blob47 tileset 批量生成器 · 实施计划

> 状态：计划稿（2026-08-13）。决策已定：**独立仓库 `D:\autotile_imgui`，C++ 移植，
> 架构照搬 `D:\tile_map_editor_imgui`（ViewModel + 命令队列 undo/redo + 事件扇出）。**

---

## 0. 定位与分工

| | web `autotile_mixer` | 桌面 `autotile_imgui` |
|---|---|---|
| 目标 | 单张 sheet 精修 + 分享码传播 | 配方库存 + 变体矩阵 + **批量导出** |
| 输入 | 交互调参 | 配方文件 / 分享码 / web 导出的 zip |
| 输出 | 一张 PNG + json + zip | 一批 PNG/json，按命名模板落盘 |
| 硬约束 | —— | **同一配方渲染出的像素必须与 web 逐字节相同** |

桌面端不复刻 web 那 2000 行 `App.tsx` 的全部旋钮。优先级：配方库 → 变体矩阵 →
批量导出 → 单张精修面板（够用即可）。

---

## 1. 目录结构

照搬 tilemap editor 的两层切分：**无 UI 依赖的核心静态库 `autotile`** + **ImGui 前端**。

```
autotile_imgui/
├─ CMakeLists.txt              # 抄 tile_map_editor_imgui 根 CMake（含 Sanitizers.cmake）
├─ CLAUDE.md
├─ cmake/Sanitizers.cmake
├─ third_party/                # 从 tile_map_editor_imgui 直接搬：
│   ├─ nlohmann/               #   配方 JSON
│   ├─ stb/                    #   PNG 读写
│   └─ miniz/                  #   zip 打包（.c 单文件，注意 -w / /w 分平台）
│
├─ src/                        # === 核心库 autotile：零 GL / 零 ImGui / 可 headless ===
│   ├─ pattern/                # web 渲染核心的 1:1 移植（本项目最重的一块）
│   │   ├─ js_math.h/.cpp          # JS 数学语义补齐层，见 §5.3
│   │   ├─ pattern_data.h/.cpp     # generated.ts 原样搬运（只读数据，禁止重算）
│   │   ├─ blob47.h/.cpp           # ← blob47.ts        (178)
│   │   ├─ blob47_pattern.h/.cpp   # ← blob47Pattern.ts (663)
│   │   ├─ pattern_noise.h/.cpp    # ← patternNoise.ts  (173)
│   │   ├─ pattern_ribbon.h/.cpp   # ← patternRibbon.ts (284)
│   │   ├─ pattern_texture.h/.cpp  # ← patternTexture.ts(1506)
│   │   ├─ pattern_paint.h/.cpp    # ← patternPaint.ts  (412) 唯一入口 paintPatternTileRGBA
│   │   └─ sheet.h/.cpp            # 48 槽拼成 256×192 RGBA
│   ├─ model/
│   │   ├─ recipe.h/.cpp           # ← recipe.ts，含 sanitizeRecipe
│   │   ├─ recipe_codec.h/.cpp     # ← recipeCodec.ts（分享码，P10）
│   │   ├─ recipe_library.h/.cpp   # 文档模型（= tilemap 的 TileMap）
│   │   └─ variant_matrix.h/.cpp   # 变体轴叉乘
│   ├─ handler/library_handler.h/.cpp
│   ├─ command/                    # 见 §3
│   │   ├─ library_command.h/.cpp
│   │   ├─ library_command_handler.h/.cpp
│   │   ├─ library_command_callbacks.h
│   │   ├─ commands_recipe.h/.cpp
│   │   ├─ commands_library.h/.cpp
│   │   └─ commands_batch.h/.cpp
│   ├─ serialize/  library_json.cpp · sheet_png.cpp · bundle_zip.cpp
│   └─ util/       image.cpp（stb 封装）· logger · fnv.h
│
├─ desktop/                    # === ImGui 前端 ===
│   ├─ CMakeLists.txt              # FetchContent imgui/glfw/tinyfd（必须加 MSVC 分支，见 §7.2）
│   └─ src/
│       ├─ main.cpp                # GLFW + GL3.3 + ImGui bootstrap；`--headless`
│       ├─ app.h/.cpp              # DockSpace / 菜单 / 全局快捷键，持有 ViewModel
│       ├─ view_model.h/.cpp       # 命令入口 + 回调扇出
│       ├─ sheet_renderer.h/.cpp   # CPU RGBA buffer + GL 纹理（= tile_renderer）
│       ├─ headless_commands.h/.cpp
│       ├─ file_dialog.h/.cpp
│       └─ panels/
│           ├─ library_panel        # 配方库列表 + 缩略图网格
│           ├─ recipe_inspector     # 属性面板（颜色/pattern/band/noise/ribbon/textureA/B）
│           ├─ sheet_view           # 48 槽预览，缩放 / 网格 / 单槽高亮
│           ├─ playground_view      # 随机地图拼贴预览（可选，验证接缝）
│           ├─ variant_matrix_panel # 变体轴定义 + 预计条数
│           ├─ batch_export_panel   # 导出目标 / 命名模板 / 进度 / 取消
│           └─ log_view
│
├─ tests/                      # doctest，ASan+UBSan 默认开
└─ corpus/                     # 从 web 仓库同步（manifest + recipes 进 git；PNG 不进）
```

---

## 2. 文档模型（对应 tilemap 的 TileMap / Layer / Object）

```cpp
namespace atm {

struct RecipeEntry {          // ≈ Layer：库里的一条，有稳定 hash
    std::string hash;         // 稳定 id，命令按 hash 解析目标（照抄 IHashed 的用法）
    std::string name;
    Recipe      recipe;       // 纯值类型，直接拷贝即可快照
    std::unordered_map<std::string, std::string> tags;
};

class RecipeLibrary {         // ≈ TileMap：整个文档
    std::vector<std::shared_ptr<RecipeEntry>> entries_;
    std::vector<VariantAxis>                  axes_;
    ExportSettings                            exportSettings_;
};

class LibraryHandler {        // ≈ TileMapHandler：文档 + 选择态 + settings
    std::unique_ptr<RecipeLibrary> library_;
    RecipeEntry*              selected_ = nullptr;
    std::vector<RecipeEntry*> multiSelected_;
};
}
```

`Recipe` 是纯值类型（没有指针、没有树），这带来两个便宜：
1. **undo 快照直接拷贝整个 `Recipe`**，不需要 tilemap 那套 `_replaceTileMap` / `replacesTileMap()`
   / `clearHistory()` 的复杂度 —— 那一整块可以不移植。
2. **渲染是纯函数**（`Recipe → RGBA`），天然线程安全，批量渲染可以直接开线程池。

---

## 3. 命令 / undo-redo / 事件扇出

### 3.1 层次映射

| tile_map_editor_imgui | autotile_imgui |
|---|---|
| `adna::TileMapCommand` | `atm::LibraryCommand` |
| `adna::TileMapCommandHandler` | `atm::LibraryCommandHandler`（队列 + undo 栈 + merge） |
| `adna::TileMapCommandCallbacks` | `atm::LibraryCallbacks` |
| `adna_desktop::ViewModel` | `atm_desktop::ViewModel`（唯一回调接收者 → `fan_out` 给面板） |
| `tile_renderer`（脏区域上传） | `sheet_renderer`（脏级别重渲染，见 §3.4） |

`LibraryCommand` 保留原基类的形状：`init() → execute() → mergeWith() → undo() / redo()`，
`Kind` 枚举 + `flag`（拖拽相位 0=begin/1=continue/2=end）+ `timeStamp` 全部照搬。
`EditorResult { bool success; std::string error_message; }` 原样抄。

### 3.2 命令清单

**进 undo 栈：**

| Kind | 说明 | merge |
|---|---|---|
| `UpdateRecipeColours` | 三个 role 颜色 / custom ramp | ✅ 同 entry + 同字段 + 时间窗 |
| `UpdateRecipePattern` | patternId / edgeSeed / outlineWidth | ✅ |
| `UpdateRecipeBand` | bandSteps / hardEdgeB / bandBias / transparentB | ✅ |
| `UpdateRecipeNoise` | noise 集合 / seed / strength / targets | ✅ |
| `UpdateRecipeRibbon` | ribbon 全组 | ✅ |
| `UpdateRecipeTexture` | textureA/B 全组（algo/amount/shades/seed/scale/ramp） | ✅ |
| `AddRecipe` / `RemoveRecipe` / `DuplicateRecipe` | 库增删 | ❌ |
| `RenameRecipe` | 改名 | ✅（打字式） |
| `ReorderRecipe` | 拖排序 | ❌ |
| `SelectRecipe` | 选中 | ❌（照抄 `SelectLayerCommand`，**进栈**：撤销一次参数修改要能跳回那条配方） |
| `ImportShareCode` | 分享码导入 | ❌ |
| `AddVariantAxis` / `UpdateVariantAxis` / `RemoveVariantAxis` | 变体矩阵 | 视情况 |

**不进 undo 栈**（IO 副作用，对齐 `save_to_file` 的处理）：
`save_library` / `load_library` / `batch_export` / `export_single`。

> ⚠️ **merge 是这个 app 的必需品，不是优化。** ImGui 的 `SliderFloat` / `ColorEdit3`
> 每一帧都返回 changed，不合并的话拖一次滑块能塞 200 条 undo。合并门控照抄 tilemap 的
> typing-style 做法：`Kind` 相同 + 目标 hash 相同 + `flag != 0`（拖拽未结束）→ merge；
> 松手（`flag == 2`）后下一条另起一格。

### 3.3 回调（事件扇出面）

```cpp
class LibraryCallbacks {
public:
    virtual ~LibraryCallbacks() = default;
    virtual void onLibraryLoaded(int flag) {}                            // 整库替换
    virtual void onLibraryListUpdated(int flag) {}                       // 增/删/改名/排序
    virtual void onRecipeSelected(RecipeEntry*, int flag) {}
    virtual void onRecipeUpdated(RecipeEntry*, DirtyMask dirty, int flag) {}
    virtual void onVariantAxesUpdated(int flag) {}
    virtual void onBatchProgress(const BatchProgress&, int flag) {}      // 后台线程 → 主线程
    virtual void onExportSettingsUpdated(int flag) {}
};
```

`ViewModel` 是 handler 的**唯一** callbacks 实现，收到后 `fan_out` 给
`register_panel()` 注册过的面板；`fan_out` 用面板列表的快照迭代（回调期间可能有注册/注销）。
每个面板自己实现 `LibraryCallbacks`，构造时注册、析构时注销 —— 与 `LogView` 完全同形。

### 3.4 `DirtyMask` —— 对应 tile_renderer 的脏区域

tilemap 那边的优化是"只上传脏矩形"。这里的等价物是**只重算脏层级**：

```cpp
enum DirtyMask : uint32_t {
    DIRTY_SILHOUETTE = 1 << 0,  // pattern / bandSteps / hardEdgeB / bandBias / edgeSeed / outlineWidth
    DIRTY_COLOUR     = 1 << 1,  // 三个 role 颜色 / custom ramp
    DIRTY_NOISE      = 1 << 2,
    DIRTY_RIBBON     = 1 << 3,
    DIRTY_TEXTURE_A  = 1 << 4,
    DIRTY_TEXTURE_B  = 1 << 5,
};
```

只有 `DIRTY_SILHOUETTE` 才需要重跑 `patternLevelsForMask`（web 侧 `LEVEL_CACHE` 的分界线
就在这里）；改颜色只需重跑上色循环。`sheet_renderer` 累积 `DirtyMask`，在 `ensure_uploaded()`
里决定重算多少，然后整张 `glTexSubImage2D`。48 个 tile 共享同一组 level grid。

> 与 tilemap 同样的铁律：**任何改变 sheet 像素的代码都必须走 `onRecipeUpdated` 回调**，
> 否则预览会 stale。

---

## 4. 桌面端功能（P8 之后）

1. **配方库**：扫描目录里的 `*.recipe.json` / 打开一个 `.atmlib` 库文件；列表 + 缩略图网格
   （缩略图就是 sheet 本身，48 槽一眼看完）；多选、标签过滤。
2. **变体矩阵**：base recipe + 若干变量轴（pattern × 调色板组 × textureA × ribbon）叉乘，
   面板实时显示"将生成 N 张"，可预览前 12 张。**这是桌面端相对 web 的核心增量。**
3. **批量导出**：命名模板（`{name}_{pattern}_{texA}_{size}px`）、输出目录结构、
   PNG + json sidecar + 可选 zip；后台线程池渲染，进度条 + 取消；导出前冲突检查。
4. **导入桥**：粘贴 web 分享码 / 拖入 web 导出的 zip → 进库。两端唯一的耦合点。
5. **headless CLI**：`autotile_mixer --headless`，stdin 收 JSON 命令
   （`{"cmd":"load_library"}` / `{"cmd":"batch_export"}` / `{"cmd":"render","recipe":{...},"out":"x.png"}`），
   形状照抄 `headless_commands.cpp`。对拍语料的批量渲染也走这条路（也可直接链核心库）。
6. **（后续）** 直出 tile_map_editor 的 palette 格式，跳过 PNG 中转。

---

## 5. 像素级一致性（本项目的验收标准）

**Ground truth 是 raw RGBA 的哈希，不是 PNG 文件。** PNG 只给人看。

### 5.1 语料 —— 已完成 ✅

就在本仓库的 `corpus/`，**1161 条，8.1 MB**。契约、失败报告格式、以及 C++ 侧要
实现的 `--render-corpus` 接口，全写在 `corpus/README.md`，开工前先读那份。

生成器留在 web 仓库（`autotile_mixer/tools/gen-corpus.ts`，`npm run gen-corpus`，
默认输出到这里），因为它必须跑它所捕获的那份 TypeScript。语料放在这边，是为了让
本项目的测试既不需要 node 也不需要另一个仓库。

| 层 | 条数 | 内容 |
|---|---:|---|
| L0 | 1 | 冒烟 |
| L1 | 209 | 11 pattern × bandSteps{3,4,5} × outlineWidth{1..4} × bandBias 端点 × hardEdgeB |
| L2 | 286 | **全部 25 种纹理 × A 面/B 面** × shades/amount/geo/cell/ripple + 配对 + transparentB |
| L3 | 97 | 全部 14 种 ribbon × shades/amount/period/invert/1px 描边 |
| L4 | 43 | noise 全部非空组合 × strength × seed × **7 种 noiseTargets 子集** × 挑色 |
| L5 | 25 | 灰地形、饱和 A、custom ramp、transparentB |
| L6 | 500 | 固定种子 fuzz（mulberry32，种子在 manifest 里） |

生成器**从应用自己的选项表枚举**（`TEXTURE_GROUPS` / `RIBBON_GROUPS` /
`PATTERN_GROUPS`），所以往应用里加一种纹理就自动进语料，不会出现"新纹理没人测"。

两个已落地的关键决定：

- **sheet 组装不经过 canvas** —— `renderSheetRGBA` 直接拼 48 次 `paintPatternTileRGBA`。
  `canvas.toBlob` 在 `transparentB` 下可能预乘 alpha 把 RGB 清零，且浏览器之间不一致。
- **同时存 level grid**（`expected/<id>.lvl.gz`），这让 Level A 白送，并且让失败能自动归因。

### 5.1b `renderSheet.ts` —— C++ 侧的移植靶子

`Recipe → 像素` 原先散在 `App.tsx` 的一堆 `useMemo` 里，现在抽成一个纯函数文件，
三个调用方共用：应用画布、语料生成器、以及（将来的）C++ 移植。本仓库
`reference/renderSheet.ts` 就是它的只读快照 —— **移植时照着它写，不要照着 App.tsx。**

抽取时发现预设缩略图那份拷贝早就漂了：它把 bandBias 读成 `(bias/100)*(ts/2)` 而不是
按 `PATTERN_OFFSET_RANGE` 缩放，也不知道 water 的两级表、pavings 的强制 amount 和
`DEFAULT_TEXTURE_COLOURS` —— 缩略图和它生成的 sheet 是两张不同的图。已修。
这正是"同一个映射写两遍必然漂"的现场证据。

浏览器里核对过：跑着的画布 hash `0xfd3975ed` == `renderSheetRGBA(应用启动态)` hash，
逐字节相同。

### 5.2 分层对拍（`tests/`，doctest 直接链核心库，不起进程）

只比整图哈希的话，一旦红了只能瞎调。所以从下往上四级依次转绿：

| 级别 | 断言 | 作用 |
|---|---|---|
| **A 轮廓** | `patternLevelsForMask()` 的 level grid 字符串**逐字符相等**（47 mask × 11 pattern × band 参数组合） | 证明烘焙数据搬对 + 阈值逻辑对，与颜色完全解耦 |
| **B 调色** | `shadeColour` / `patternRamp` / `textureRamp` / `textureColour` 在固定输入表上逐 RGB 相等 | 隔离 HSV 往返 |
| **C 纹理秩** | 每个 texture id / ribbon id 的 32×32 `k` 值矩阵相等 | 定位到"是 hexagon 错了还是 octagonal 错了" |
| **D 整图** | 整张 sheet 的 RGBA FNV-1a 对上 manifest | 最终验收 |

D 失败时自动 dump 差异图（不同像素标红）+ 打印首个差异的
`(slot, x, y, 期望 RGBA, 实际 RGBA)`。

### 5.3 `js_math` —— 移植方案里唯一的真风险

| 风险点 | 出现位置 | 处理 |
|---|---|---|
| `Math.imul` + `>>>` + `^` | 5 处哈希 | `uint32_t` 精确复刻，**零风险**，只需小心 int32 符号 |
| `Math.hypot` | `patternTexture.ts:270`(Voronoi)、`:1304`(hexagon)、`blob47.ts:120,194` | V8 用带缩放的补偿算法，`std::hypot` 实现不同，**末位可能差 1 ulp** → 复刻 V8 版本 |
| `Math.sin` | `blob47Pattern.ts:510-511`(wave)、`patternRibbon.ts:255`(rope) | V8 用 fdlibm 移植版 → vendored fdlibm |
| `Math.atan2` | `blob47Pattern.ts:690` | 同上 |
| `Math.round` | 到处 | JS 是 half-away-from-zero → 统一 `std::floor(x + 0.5)`（Python banker's rounding 的坑已踩过一次） |
| `Math.sqrt` | 多处 | IEEE 精确，**零风险** |

配套 `tests/test_js_math.cpp`：web 侧 dump 一张 `(输入, 输出 double 的位模式十六进制)`
对照表，C++ 侧**逐位**比对。**这一步必须做在移植之前**，否则后面每个 bug 都要先花时间
排除"到底是算法错了还是浮点错了"。

### 5.4 防漂移

`manifest.json` 带 `corpusVersion` + web 端 git sha。web 侧改渲染算法 → 必须重生成语料 →
diff 里能直接看到哪些配方哈希变了 → 桌面侧同步之前 CI 是红的。这就是"网页改了、桌面偷偷
不一致"的检出机制。web 现有的 10 个 locked sheet hash 保留，桌面侧引用同一份常量。

### 5.5 命令层的测试

照抄 `tests/test_command_monkey.cpp` 的做法：随机命令序列 → 全部 undo → **序列化后的库
必须与初始状态逐字节相同**；再全部 redo → 与全执行后的状态相同。`Recipe` 是纯值类型，
这个断言写起来非常干净，能一次性抓住所有漏快照的命令。

---

## 6. 阶段划分

| 阶段 | 内容 | 验收 | 依赖 |
|---|---|---|---|
| ~~**P0**~~ ✅ | web 侧：`tileSize` 已从 Recipe 移除（永远 32）、`renderSheet.ts` 抽取、语料生成器 + `verify.py` + 1161 条语料落盘 | 已完成：自检 1161/1161 通过；注入 4 类错误全部被抓且归因正确 | — |
| **P1** | 仓库骨架：CMake / third_party 搬运 / doctest / `js_math` + 位模式对照测试 | `test_js_math` 全绿 | P0 |
| **P2** | `pattern_data`（generated.ts 原样搬）+ `blob47` + `blob47_pattern` | **Level A 全绿** | P1 |
| **P3** | `pattern_paint` + `shadeColour` + `pattern_noise` | Level B 全绿；Level D 在"无 texture 无 ribbon"子集全绿 | P2 |
| **P4** | `pattern_texture`（1506 行，最大块，按 `TEXTURE_GROUPS` 分批，每批一个 commit） | Level C 逐 texture 转绿 | P3 |
| **P5** | `pattern_ribbon` | Level C ribbon 部分全绿 | P4 |
| **P6** | 全量 Level D + fuzz 语料 + `sheet` 拼图 + PNG/zip 写出 | **像素级一致达成**，得到可用的 headless CLI | P5 |
| **P7** | `model` + `command` + `handler` + 回调接口 + 猴子测试 | 命令层测试全绿 | P1（**可与 P2–P6 并行**） |
| **P8** | ImGui 前端：`app` / `view_model` / `sheet_renderer` / 四个核心面板 | 手动 | P6 + P7 |
| **P9** | 变体矩阵 + 后台线程池 + 进度扇出 + 批量导出面板 | 手动 + 导出结果过 Level D | P8 |
| **P10** | 桥接：`recipe_codec` 移植（分享码）、web zip 导入 | 分享码 round-trip 测试 | P7 |

**P0–P7 完全 headless、零 UI 代码。** UI 是简单的部分，放后面；对拍才是难点。
P7 与 P2–P6 无依赖，可以并行推进。

---

## 7. 已知的坑

1. **`generated.ts` 是机器产物且烘焙脚本不在仓库里**（当初的决定）。桌面端必须原样搬运，
   任何"我重算一下"的念头都会炸 —— 尤其 `cornerRounding` 和 5 个噪声生成器至今没解出来。
   规格见 web 仓库 `docs/AUTOTILE_PATTERN_BAKE.md`。
2. **`tile_map_editor_imgui/desktop/CMakeLists.txt` 没有 MSVC 分支**：给 tinyfd 用了
   `-w` / `-Wno-error=int-conversion` 这类 GCC 风格 flag。抄过来在 Windows 上第一次
   configure 就会挂，必须先补 `if(MSVC) ... /w ... else() ... endif()`。miniz 那段同理
   （根 CMake 里已经有正确写法，抄那一段）。
3. **批量渲染的线程边界**：渲染是纯 CPU（`Recipe → RGBA`），可以任意开线程；但 GL 纹理
   上传只能在主线程。`onBatchProgress` 从工作线程发出时要过一层线程安全队列，
   在主线程的帧循环里 drain 后再 `fan_out`（`LogView` 已经用 mutex 保护 `lines_`，
   可以参考但这里需要的是队列而非锁）。
4. **`bandSteps` 4–5 时"3 个颗粒颜色 vs 最多 5 个 band step"的不匹配**是 web 侧已知遗留
   问题。移植时**照搬行为，不要顺手修**，否则 Level D 立刻红。同理 `terrainA.val = 1.0`
   时内层 band 塌陷 —— 也是照搬。
5. **`transparentB` 下 alpha == 0 像素的 RGB 未定义**，对拍规则见 §5.1。
6. **`Uint8ClampedArray` 的赋值是 round-half-to-even**，与 `Math.round` 不同。当前
   `patternPaint.ts` 里写入的值都已经过 `clamp255`（整数），所以暂时无碍 —— 但移植时
   不要顺手引入分数写入。
7. **`sanitizeRecipe` 末尾把 `tileSize` 硬写成 32**（`recipe.ts:275`），schema 冻结时
   先决定这个字段的去留，两端要一致。

---

## 8. 开工前的前置动作

- [x] web 仓库 `autotile_mixer` 工作区干净，`main` 与 `origin/main` 同步（2026-08-13 核实）。
      唯一挂着的是 `stash@{0}`（procedural water，刻意留作参考，不影响开工）。
- [x] recipe schema 冻结：`tileSize` 已删除，永远 32。分享码 byte 13 的 bit 1 降级为
      保留位（仍然写 1、读时忽略），线上已有的分享码继续逐字节 round-trip。
- [ ] 决定 corpus 的同步方式：git submodule / 手工拷贝 / 生成脚本跨仓库调用。
      语料现在的绝对路径是 `D:\adna_tilemap_editor\corpus`，`verify.py` 的 `--corpus`
      可以指过去，所以短期不同步也能跑。
- [ ] **`noiseTargets` / `noiseColours` 不在 Recipe 里**（见 `corpus/README.md` 末节）。
      语料靠 `overrides` 字段够到它们。桌面端如果要渲染非默认颗粒目标的 sheet，
      这个得先定：要么进 schema（会动分享码格式），要么承认它是网页专属的预览控件。
