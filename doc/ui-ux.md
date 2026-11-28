# LMMS UI/UX 路线图与任务

> 本文件是 UI/UX 的**任务与路线图**文档；稳定的设计规范见 [`ui-spec.md`](ui-spec.md)。
>
> 本文件记录任务编号、状态、已知问题、验收和变更；规范改动请提交到 `ui-spec.md`，不要在这里改规范。
>
> 适用范围：`src/gui`、`include`、`data/themes`、插件界面。
>
> 不在本分支范围：VST/VST3/LV2/PDC 宿主实现及其设计文档。相关代码保留在既有分支历史中，但本分支只维护 UI/UX。
>
> 约束：不修改工程文件格式、插件 ABI、`Track`/`Clip`/`Mixer` 数据成员；不重写 `MainWindow`/`SubWindow`；不引入 docking 框架或 model/view；每个任务可独立提交、独立回滚。

---

## 1. 命名与优先级

本分支不再混用多套编号：

- **`UI-1` … `UI-5`**：唯一的当前路线图编号。新增工作只使用这一套。
- **旧 `T0/T1/T2` 编号已归档**：只保留在 git 历史中，不再作为新任务编号。
- **不再使用 `P0…`、`Stage …`、`阶段 A/B/C`** 作为 UI 计划编号；这些编号属于其他项目文档，已随旧文档删除。
- 任务状态只用：**完成 / 进行中 / 待开始 / 阻塞**。

当前路线图：

| 编号 | 目标 | 状态 | 风险 |
| --- | --- | --- | --- |
| `UI-1` | 侧栏与列表收尾 | 完成 | 低 |
| `UI-2` | 核心固定尺寸对话框 | 完成（视觉抽查） | 中 |
| `UI-3` | 插件固定尺寸对话框 | 完成（布局子集） | 中至高，多 PR |
| `UI-4` | 排版与文案收尾 | 完成（截图随 UI-5） | 低至中 |
| `UI-5` | 主题、动画与无障碍 | 进行中 | 低 |

---

## 2. 当前状态（2026-11-28）

已完成的主要 UI/UX 改造：

- 三档字号：标题 13 / 正文 10 / 辅助 8，QSS 与应用字体同步缩放。
- 轨道头宽高、混音器分隔条、效果器架滚动条、子窗口标题等核心布局具备弹性。
- 文件浏览器与插件浏览器统一为标准的 `QTreeWidgetItem` 行，不再使用嵌入式 `PluginDescWidget`。
- 侧栏列表使用内容式选中胶囊；分支/缩进区域保持透明；展开箭头有独立 hover；快速双击箭头可连续展开/收起。
- 系统字体缩放上限 200%；关键 QtTest 覆盖字体、侧栏、子窗口、文件浏览器、乐器轨道等。
- 图标语言 A（单色实心）已冻结；40 个基础图标（18 个 pilot + 22 个第二批）已迁入 `default`；改造前的 `default` 已快照为 `data/themes/legacy`。

仍需重点处理的是**插件自绘面板**、**图标迁移**和少量**主题细节**，不是主窗口结构。

---

## 3. 已知问题与风险

### 3.1 固定尺寸与布局风险

核心窗口：

| 位置 | 现状 / 风险 |
| --- | --- |
| `RenameDialog` | 输入框已改为布局管理，仍需大字体视觉验收。 |
| `PeakControllerDialog` | 已移除 256×64 固定尺寸和绝对坐标，使用布局 + bodyFont。 |
| `LfoControllerDialog` | 已移除 240×58 固定尺寸和绝对坐标；100% 和 200% 已目视。 |
| `LadspaBrowser` | 已改为可伸缩布局；标签页图标列 72px 固定，200% 目视待补。 |
| `SampleTrackWindow` 效果架 | 已回退为固定窗口；大字体时只考虑滚动/整体缩放。 |
| `EffectView` | 行高按字体度量计算，保留效果架行高模型。 |
| `FileBrowser` 搜索栏 | 固定高度已移除；仍需窄侧栏和长翻译验收。 |
| `EffectSelectDialog` 详情区 | 最大高 180px，长内容依赖滚动；需检查长 URL/翻译。 |

插件界面（排除 vendored 子模块，约 50 处 `setFixedSize`）：

- 已完成简单面板：Bitcrush、Delay、StereoMatrix；Compressor、SlewDistortion 仍在 UI-3/3b。
- 已完成 `StereoMatrix`：移除 160×185 固定尺寸，保留 4 个旋钮布局，背景随窗口缩放。
- 已完成 `Bitcrush`：移除 181×128 固定尺寸和绝对坐标，IN/中心/OUT 改为三段布局，LED 和旋钮随字体排布，背景随窗口缩放。
- 已完成 `CrossoverEQ`：移除 167×218 固定尺寸，保留现有旋钮/推子布局，背景随窗口缩放。
- 已完成 `Delay`：移除 300×208 固定尺寸，左侧旋钮、中央 XyPad、右侧输出推子改为三栏布局，背景随窗口缩放。
- 已回退 `Eq`：保留 500×500 固定画布；先做 200% 审计，只有实际裁切/重叠时才加滚动或整体缩放，不强制重排。
- 已完成 `DualFilter`：移除 373×109 固定尺寸，两段滤波器改为左右布局，启用 LED、旋钮和下拉框随字体排布。
- 已完成 `FrequencyShifter`：移除 288×360 固定尺寸和绝对坐标，按顶部/波形/LFO/延迟四段改为布局，背景随窗口缩放。
- 已完成 `MultitapEcho`：移除 245×300 固定尺寸，双图形和底部控制改为布局。
- 已完成 `DynamicsProcessor`：移除 224×319 固定尺寸，图形、旋钮、按钮和立体声模式改为布局。
- 已完成 `GranularPitchShifter`：移除 305×180 固定尺寸，旋钮/LCD/按钮改为 4×4 网格布局，背景随窗口缩放。
- 剩余中等：LOMM；DualFilter、DynamicsProcessor、FrequencyShifter、MultitapEcho、CrossoverEQ 已完成。
- 复杂/自绘：SlicerT、AudioFileProcessor、Sf2Player、GigPlayer、Vestige；GranularPitchShifter 已完成布局。
- 典型风险：`Sf2Player`/`GigPlayer` 文件名标签固定 11/14px 高、宽 156px；`Vestige` 文本按钮固定 200×24px；多个插件的下拉框使用固定坐标/高度。

### 3.2 手势与主题的已知偏差

| 控件 | 现状 |
| --- | --- |
| 主工具栏滑块 | `QSlider` 默认；滚轮一个 `singleStep`，忽略修饰键；中键未接入；双击就地输入整数；右键重置/复制/粘贴/控制器 |

- 主工具栏滑块仍缺少中键归位和修饰键滚轮；统一前先评估行为变化。
- 默认主题 `LmmsPalette::windowText` 仍是绿色 `#1de276`，但标签页已显式使用正文色；需在真实窗口核实所有状态。
- `classic` 主题仍有 268 个 PNG，无 `@2x`，HiDPI 下会模糊；比 `default` 缺 29 个名字，会回退到 `default` 的 SVG，形成 PNG/SVG 混搭；22 个同名图标的标称尺寸与 `default` 不同。
- 目前只有深色主题；需要浅色主题时，必须按 `ui-spec.md` §3.1/§3.2 重新设计明度和图标 tint。
- 剩余 QSS 图标（滚动条、插件 stepper、分支、slider/shadow）在浅色主题下仍需改成 style/delegate 绘制。
- `default` 已全部 SVG 化，但多数图标只是旧位图的矢量化：44 种标称尺寸、44 个 linearGradient 元素、9 个含文字、22 个自带底板，仍待按 `ui-spec.md` §3.2 迁移。
- `clock`/`tempo_sync` 仍共用时钟 glyph，作为 tempo 的时间语义 alias 保留；`note_eight` 已删除，`recover`、`progression_linear` 已区分。

### 3.3 文案待处理

- `ControllerRackView` 的 `%n connection(s)` 仍等待翻译提供 Qt 复数形式。
- `data/locale/en.ts` 由 Transifex 整体同步，本地不重新生成，避免一次上万行无意义 diff。
- 其余 Title Case、`(s)` 复数、单位空格和组标题冒号已在 UI-4 的前十批中处理。

---

## 4. UI 路线图

### UI-1：侧栏与列表收尾（完成）

目标：把最近的列表统一工作收尾，避免回归。

- 确认 My projects / My samples / My presets 等 `FileBrowser` 页面都使用 `CompactItemDelegate` 和统一的箭头行为。
- 补搜索无结果状态；确认长名称、长翻译、窄侧栏下省略和 tooltip 正常。
- 已增加 `CompactItemDelegateTest`：内容胶囊宽度、最后一个字符不裁切。
- 已增加 `FileBrowserTest::arrowDoubleClickToggles`：快速双击箭头连续切换。
- 截图沿用现有版本，最终统一重做放在 UI-5。
- 运行 `FileBrowserTest`、`SideBarWorkspaceTest`、`FontScaleTest`、`InstrumentTrackTest`。

验收：默认/经典主题，100%/125%/150%/200%，en/zh_CN/de_DE，窄侧栏；无整行色块、无文字裁切、无箭头误触发。

### UI-2：核心固定尺寸对话框（完成，视觉抽查）

目标：先处理用户最常看到的文本型固定尺寸。

- `RenameDialog`、`PeakControllerDialog`、`LfoControllerDialog`、`LadspaBrowser`、`SampleTrackWindow` 效果架、`EffectView`。
- 已完成 `PeakControllerDialog`：移除 256×64 固定尺寸，改用 `QVBoxLayout` + `bodyFont()`，修正误用的 "LFO Controller" tooltip。
- 已完成 `LfoControllerDialog`：移除 240×58 固定尺寸和绝对坐标，波形按钮/旋钮/倍率按钮改为 `QGridLayout` + `QHBoxLayout` + `QVBoxLayout`，背景随对话框尺寸缩放；100% 和 200% 下已目视检查。
- 已完成 `LadspaBrowser`：移除 500×480/400 固定尺寸，标签页改用 `QVBoxLayout`，插件列表和描述区随窗口伸缩。
- 已回退 `SampleTrackWindow`：保留固定窗口和 `SetFixedSize`；如需大字体，只加滚动/整体缩放，不改变窗口行为。
- `EffectView` 仍是效果架中的固定行高控件，高度按字体度量计算；它不是独立对话框，本轮保留其行高模型。
- 优先改用布局和字体度量；必要时加滚动区域。
- 200% 下检查文字边界、按钮/旋钮间距、长翻译和窄窗口。

验收：100%–200% 无文字裁切/重叠；对话框可调整大小；不改变工程行为和快捷键绑定。

### UI-3：插件固定尺寸对话框（布局子集完成，多 PR）

目标：处理约 50 处固定尺寸插件面板。

建议按复杂度分批，每批独立提交：

1. Bitcrush、Delay、Compressor、SlewDistortion、StereoMatrix；`Eq` 已回退到固定画布审计。
2. DualFilter、DynamicsProcessor、FrequencyShifter、MultitapEcho、LOMM、CrossoverEQ。
3. GranularPitchShifter、SlicerT、AudioFileProcessor、Sf2Player、GigPlayer、Vestige。

每批：

- 用 `QLayout`/最小尺寸/字体度量替换固定坐标和固定高度。
- 保留画布类控件的逻辑尺寸，只调整周边文本和控制区。
- 检查 100%/150%/200%、长翻译、窄窗口。
- 回归 `setFixedSize` 数量，避免新增。

验收：插件面板在 200% 下不重叠、不裁切；宿主仍能正常加载/保存。

### UI-3b：自绘面板专用重写（待开始）

以下面板不是“删掉 setFixedSize 再套布局”就能解决，必须先把自绘坐标改成相对布局：

- `Eq`：500×500 频谱/参数叠层，固定画布先审计，不强制重排。
- `SlewDistortion`：曲线、输入/输出电平表、拖拽手柄都使用固定坐标。
- `LOMM`：大量 LCD、按钮、旋钮围绕 artwork 绝对定位。
- `SlicerT`、`AudioFileProcessor`：波形/切片自绘。
- `Sf2Player`、`GigPlayer`、`Vestige`：固定尺寸文件/音色控件和嵌入窗口。

验收：先把自绘区域拆成独立 widget 或相对坐标，再补 100%/200% 目视；不与普通布局批次混在一起。

### UI-4：排版与文案收尾（完成，截图随 UI-5）

- 清理剩余固定字号和硬编码 `font-size`；确认三档标准在所有核心界面生效。
- 逐插件统一参数名为 sentence case，不做全量重命名。
- 已完成第一批：`Compressor` 参数名改为 sentence case（RMS size、Lookahead length、Input balance 等），并修正 feedback 按钮误用的 "Soft Clip" 可访问名称。
- 已完成第二批：`Dispersion` 的 "DC Offset Removal"/"Remove DC Offset" 改为 sentence case；`DualFilter` 的 "Q/Resonance" 改为 "Q/resonance"。
- 已完成第三批：`LOMM` 的 Input/Output Volume、Upward/Downward Depth、High/Mid Crossover、Above/Below Threshold/Ratio、Attack/Release High/Mid/Low 等改为 sentence case；`SlewDistortion` 的 Slew Up/Down、Dynamic Slew、DC Offset Remover、Slew Link、Oversampling 标签改为 sentence case。
- 已完成第四批：`GranularPitchShifter` 的 Grain Size、Pitch/Spray Stereo、Fade Length、Minimum Allowed Latency、Ring Buffer Length 及范围预设改为 sentence case。
- 已完成第五批：`FrequencyShifter`、`Oscilloscope`、`Nes`、`StereoMatrix`、`SlicerT` 中可见的 Title Case 参数/提示改为 sentence case（Frequency shift、Window size、Noise frequency、Left to left vol、Fade out 等）。
- 已完成第六批：`Lb302` 的 VCF/Slide/Filter 标签与提示改为 sentence case；`Monstro` 修正 Osc 3 Stereo/Sync 两处大小写。
- 已完成第七批：`LadspaBrowser` 的 Available/Unavailable/Analysis 标签、`LadspaEffect` 的 Link Channels、`LadspaPortDialog` 的 SR Dependent 改为 sentence case。
- 已完成第八批：`MidiImport` 的 MIDI file/time signature 文案、`Vibed` 的 Impulse Editor、`VstBase` 的 Open/Save Preset、`Sid` 的 Pulse Width 改为 sentence case。
- 已完成第九批：`TempoSyncKnob` 与 `TempoSyncBarModelEditor` 的 "8th/16th/32nd Note" 改为 "note"。
- 已完成第十批：`FreeBoy` 的 "Shift Register width"、`SpectrumAnalyzer` 窗口选项的 "(Off)/(Default)" 注解改为 sentence case。
- `data/locale/en.ts` 由 Transifex 整体同步，本地不重新生成，避免一次上万行无意义 diff；新增/修改字符串随下次翻译同步进入。
- `(s)` 复数与单位空格复查：仅 `ControllerRackView` 的 `%n connection(s)` 保留，等待翻译提供 Qt 复数形式；其余核心界面未发现需批量修改的单位空格或组标题冒号。
- 三档缩放截图统一重做转到 UI-5。

验收：`FontScaleTest`、`CaptionWidgetsTest`、`UiFeedbackTest` 通过；本轮不改字号，100%–200% 无新增断层；`en.ts` 不在本分支产生整体 diff。

### UI-5：主题、图标与无障碍（进行中）

目标：把图标语言 A 落到 `default`，补齐主题和可访问性，不改变主窗口结构。

图标迁移：

- 按 [`ui-spec.md`](ui-spec.md) §3.2 迁移；`legacy` 冻结，`classic` 本轮不动。
- 首批 18 个 pilot（已迁入 `default`）：`play`、`pause`、`stop`、`record`、`undo`、`redo`、`add`、`close`、`save`、`open`、`folder`、`project_file`、`sample_file`、`search`、`metronome`、`gear`、`mute`、`solo`。
- 第二批 22 个基础图标（已迁入 `default`）：`instrument_track`、`sample_track`、`pattern_track`、`automation_track`、`master_volume`、`reload`、`clock`、`tempo_sync`、`edit_draw`、`edit_erase`、`edit_select`、`edit_cut`、`edit_copy`、`edit_paste`、`quantize`、`project_new`、`project_export`、`project_import`、`star`、`home`、`plugins`、`help`。
- 首批验收：16/24/48、深/浅底非空；无灰、无渐变、无文字；除 `gear`（1.5..22.5）外，内容 bbox 在 2..22。
- `speaker_slash` 作为 mute 的 checked 状态同步迁移，由代码设置并跟随 theme ink。
- 工具：`buildtools/icon-template.svg`、`buildtools/icon-audit.py`（audit / lint / sheet，已完成）。
- 立即修：`sbarrow_left_d.svg`、`sbarrow_up_d.svg` 空文件；`sbarrow_*` 的 20%/0.01 透明度对比度（已完成）。
- 运行时：tint + 尺寸 token + `IconTest`（已完成）；track-ops、LeftRightNav 和菜单勾选已改为 code/base style，剩余 QSS 图标（滚动条、插件 stepper、分支、slider/shadow）待处理。
- 迁移顺序：18 个 pilot → 40 个基础图标（工具栏、侧栏、文件类型、编辑器工具）→ 其余 UI → 皮肤/插件 → `classic`。

主题与无障碍：

- 经典主题 PNG → SVG；默认主题已无 PNG。
- 按真实背景检查链接/正文/禁用/选中对比度，尤其是 About、Project Notes、侧栏。
- 对比度抽查：default tooltip 14.2:1、菜单选中 10.5:1；classic 菜单选中 4.7:1、链接 7.7:1；两主题禁用菜单文字由 1.9/2.8:1 提到 3.2/3.5:1。
- 补测 165 Hz/Wayland 下侧栏滑动和按钮反馈动画。
- 检查键盘焦点、Tab 顺序、可访问名称；列表行的 tooltip 和省略文本要能访问完整内容。
- 经典主题补齐按钮、树/列表/表格视图的键盘焦点轮廓（`#4afd85`，仅 outline，不改变布局）；默认主题已有对应规则。
- 已完成键盘焦点第一批：`EffectSelectDialog` 的 Tab 顺序改为搜索框 → 插件列表 → 分类按钮 → Rescan → OK/Cancel；列表和分类按钮恢复键盘焦点，列表补可访问名称/描述，Enter 直接确认；新增 `EffectSelectDialogTest` 覆盖 Tab/Shift+Tab 与可访问名称。
- 可访问名称：`FloatModelEditorBase`、`Knob`、`AutomatableButton` 从模型名或标签生成 accessible name；`CaptionWidgetsTest` 覆盖按钮名称。

验收：`default` 主题 100%–200%；40 个基础图标通过 audit；`IconTest` 通过；键盘可完成主要操作；动画可关闭且不阻塞操作。

---

## 5. 验收与回归

### 5.1 必跑测试

```bash
cmake --build build-ui -j"$(nproc)"
cd build-ui/tests
QT_QPA_PLATFORM=offscreen ./FontScaleTest
QT_QPA_PLATFORM=offscreen ./IconTest
QT_QPA_PLATFORM=offscreen ./CaptionWidgetsTest
QT_QPA_PLATFORM=offscreen ./SubWindowTest
QT_QPA_PLATFORM=offscreen ./SideBarWorkspaceTest
QT_QPA_PLATFORM=offscreen ./FileBrowserTest
QT_QPA_PLATFORM=offscreen ./CompactItemDelegateTest
QT_QPA_PLATFORM=offscreen ./InstrumentTrackTest
```

### 5.2 静态检查

```bash
# 不应新增内联固定字号
grep -rnE 'font-size:\s*[0-9]+pt' data/themes/*/style.css
grep -rn 'font-size' src/gui --include='*.cpp'

# 不应新增固定尺寸（有意识例外需说明）
grep -rn 'setFixedSize\|setFixedHeight\|setFixedWidth' src/gui plugins --include='*.cpp'

# 快捷键不应进入可译文本
grep -rn 'tr([^)]*Ctrl+' src/gui --include='*.cpp'

# 不应新增 (s) 复数
grep -rnE 'file\(s\)|note\(s\)|octave\(s\)|time\(s\)' src/gui plugins --include='*.cpp'

# 图标：只 lint 本次改动/新增的 24 网格图标
buildtools/icon-audit.py lint <changed.svg>...
buildtools/icon-audit.py sheet --theme default --only <names>
```

> 全量 lint `data/themes/default` 会报出历史图标的尺寸、渐变和文字问题；迁移时按改动文件跑，避免把存量问题当成新回归。

### 5.3 视觉验收

- 主题：默认 + 经典；`legacy` 只用于对比，不作为验收主题。
- 字体/缩放：100%、125%、150%、200%。
- 语言：en、zh_CN、de_DE；长翻译优先。
- 窗口：默认宽度 + 窄窗口；侧栏可拖动宽度。
- 重点界面：主窗口、Song Editor、Mixer、FileBrowser、PluginBrowser、EffectSelectDialog、About、RenameDialog、插件控制面板。
- 检查：文字/图标不裁切、不重叠；选中/悬停/焦点清晰；滚动条只在需要时出现；键盘可操作。

### 5.4 截图环境

使用 Xephyr + 窗口管理器，不用 Xvfb：

```bash
Xephyr :6 -screen 1600x1000x24 -ac -no-host-grab -title "LMMS UI test" &
DISPLAY=:6 matchbox-window-manager &
DISPLAY=:6 QT_QPA_PLATFORM=xcb QT_FONT_DPI=96 \
  LMMS_DATA_DIR=$PWD/data LMMS_PLUGIN_DIR=$PWD/build-ui/plugins \
  ./build-ui/lmms -c /tmp/opencode/lmmsrc.cfg --geometry 1600x1000+0+0 &
```

参考截图位于 `doc/ui-ux-shots/`，优先使用：

- `font-scale-100.png`、`font-scale-150.png`、`font-scale-200.png`
- `font-review-about.png`、`font-review-browser.png`
- `effect-select-200.png`、`lfo-wave-redraw.png`

---

### 5.5 缩放与窗口审计（2026-11-28）

已实际检查：

- 主窗口从 1600×1000 缩小到 1000×700、再放大到 1500×950：工具栏、MDI 区和混音器没有出现控件重叠；窗口变窄时右侧 MDI 子窗口会被主窗口裁剪，这是 Qt MDI 自身的布局行为。
- Song Editor 子窗口移动后，轨道头、时间线和工具栏仍保持对齐；子窗口不会随主窗口自动缩放。
- LFO 控制器在 100% 和 200% 字体下已打开检查，布局无裁切。
- 其余已改为布局驱动的插件面板目前只有构建验证，尚未逐个拖动缩放并做 100%/200% 目视。

#### 逐插件代码审计

已改为布局驱动的面板：

| 面板 | 对话框固定尺寸 | 绝对定位 | 备注 |
| --- | --- | --- | --- |
| PeakControllerDialog | 无 | 无 | 布局 + 拉伸 |
| LfoControllerDialog | 无 | 无 | 100%/200% 已目视 |
| LadspaBrowser | 无 | 无 | 标签栏 72px 图标列固定 |
| StereoMatrix | 无 | 无 | 4 旋钮布局；待目视 |
| Bitcrush | 无 | 无 | IN/中心/OUT 三段 |
| CrossoverEQ | 无 | 无 | 旋钮/推子布局 |
| Delay | 无 | 无 | 左旋钮 / 中 XyPad / 右推子 |
| DualFilter | 无 | 无 | 两段滤波器 |
| FrequencyShifter | 无 | 无 | 四段布局；旋钮自身尺寸固定 |
| MultitapEcho | 无 | 无 | 双图形 + 底部控制 |
| DynamicsProcessor | 无 | 无 | 图形 + 控件布局 |
| GranularPitchShifter | 无 | 无 | 4×4 网格 |

剩余固定尺寸只出现在 Knob、PixmapButton、Ladspa 图标列等非文本控件上，属于预期。`StereoMatrix` 和 `CrossoverEQ` 已补 `adjustSize()`。

已目视抽查 9pt/18pt 默认窗口和放大窗口：

- `StereoMatrix`：主要文字在 artwork 里，160×185 保持稳定，没有文字重叠。
- `Bitcrush`：9pt 初始 200×128，18pt 212×148，标签和旋钮没有重叠。
- `CrossoverEQ`：9pt 167×225，18pt 167×236，标签仍能放下。

其余布局驱动面板的逐项 18pt 截图因临时 offscreen harness 在 Delay 处挂起，未完成；当前只覆盖代码审计和上述抽查。

已知边界：

- MDI 子窗口默认不参与主窗口的自动排列；主窗口变窄时可能被裁剪，需要另行实现自动排列或限制。
- 固定画布插件（Eq、SlewDistortion、LOMM、SlicerT、AudioFileProcessor、Sf2/Gig/Vestige）保留原设计，优先用滚动/整体缩放而不是重排。
- 插件面板的 200% 逐项验收仍有 backlog；当前以代码审计 + 抽查为准，不作为本轮发布阻塞项。

---

---

## 6. 变更记录

- 2026-11-28：清理重复图标：`recover` 改为盾牌+勾，`progression_linear` 改为直线节点，删除未引用的 `note_eight`。
- 2026-11-28：第二批 22 个基础图标迁入 `default`；基础图标累计 40 个。
- 2026-11-28：track-ops 和 LeftRightNav 图标改由代码设置并跟随 theme ink；菜单勾选改用 base style。
- 2026-11-28：实现图标 tint（`currentColor` + icon tokens）、尺寸 token 和 `IconTest`；18 个 pilot 图标迁入 `default`。
- 2026-11-28：新增 `buildtools/icon-template.svg` 和 `buildtools/icon-audit.py`；修复 `sbarrow_*` 的空箭头与低对比度。
- 2026-11-28：拆分规范与任务；规范移至 `doc/ui-spec.md`，本文只保留路线图、任务状态、已知问题、验收和变更。
- 2026-11-28：冻结图标设计语言 A（`ui-spec.md` §3.2）；改造前的 `default` 完整快照为 `data/themes/legacy`。
- 2026-11-28：合并旧的 UI 任务、审计、文案、手势、颜色/布局文档为本分支 UI/UX 文档（现拆分为 `ui-spec.md` 与本文）；删除 VST/LV2/PDC 等非 UI 设计文档（代码保留）。
- 2026-11-28：三档字号调整为 13/10/8；侧栏列表统一为 `CompactItemDelegate`，加入内容选中胶囊、箭头 hover 和快速双击箭头切换。
- 2026-11-28：完成 UI-1 验收，新增 `CompactItemDelegateTest` 和箭头双击回归测试。
- 2026-11-28：UI-2 开始，`PeakControllerDialog` 改为弹性布局。
- 2026-11-28：混音器分隔条按实际通道宽度调整；效果器架滚动条按需显示；文件浏览器/插件浏览器选中样式统一。
- 2026-11-28：合并旧的 UI 任务、审计、文案、手势、颜色/布局文档为本文件；删除 VST/LV2/PDC 等非 UI 设计文档（代码保留）。
- 2026-11-28：`LfoControllerDialog` 改为布局驱动，移除固定尺寸和绝对坐标。
- 2026-11-28：`LadspaBrowser` 改为可伸缩布局，移除标签页固定尺寸。
- 2026-11-28：回退 `SampleTrackWindow` 的可调整大小改动，恢复固定窗口。
- 2026-11-28：UI-3 开始，`StereoMatrix` 移除固定尺寸并缩放背景。
- 2026-11-28：`Bitcrush` 改为三段布局，移除固定尺寸和绝对坐标。
- 2026-11-28：`CrossoverEQ` 移除固定尺寸并缩放背景。
- 2026-11-28：`Delay` 改为三栏布局，移除固定尺寸和绝对坐标。
- 2026-11-28：回退 `Eq` 的布局重写，恢复固定画布，后续按 200% 审计决定是否加滚动/整体缩放。
- 2026-11-28：`DualFilter` 改为左右两段布局，移除固定尺寸。
- 2026-11-28：`FrequencyShifter` 改为四段布局，移除固定尺寸和绝对坐标。
- 2026-11-28：`MultitapEcho` 和 `DynamicsProcessor` 改为布局驱动，移除固定尺寸。
- 2026-11-28：`GranularPitchShifter` 改为网格布局，移除固定尺寸和绝对坐标。
- 2026-11-28：UI-3 的普通布局面板完成；Slew/LOMM/SlicerT/AFP/Sf2/Gig/Vestige 归入 UI-3b 自绘重写。
- 2026-11-28：在 18pt 环境下检查主窗口和 LFO 控制器，无裁切/重叠；LadspaBrowser 和 SampleTrackWindow 的 200% 目视仍待补。
- 2026-11-28：UI-5 可访问性批次：`EffectSelectDialog` 键盘 Tab/Enter、经典主题焦点轮廓、旋钮/按钮可访问名称；新增 `EffectSelectDialogTest`。
- 2026-11-28：UI-5 对比度批次：两主题禁用菜单文字提升到 3:1 以上并记录抽查值。
