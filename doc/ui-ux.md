# LMMS UI/UX 规范与路线图

> 本文件是本分支 UI/UX 的**唯一文档**。旧的任务清单、审计、文案规范、手势矩阵和颜色/布局审计已合并到这里并从 `doc/` 删除。
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
| `UI-2` | 核心固定尺寸对话框 | 进行中 | 中 |
| `UI-3` | 插件固定尺寸对话框 | 进行中 | 中至高，多 PR |
| `UI-4` | 排版与文案收尾 | 待开始 | 低至中 |
| `UI-5` | 主题、动画与无障碍 | 待开始 | 低 |

---

## 2. 当前状态（2026-11-28）

已完成的主要 UI/UX 改造：

- 三档字号：标题 13 / 正文 10 / 辅助 8，QSS 与应用字体同步缩放。
- 轨道头宽高、混音器分隔条、效果器架滚动条、子窗口标题等核心布局具备弹性。
- 文件浏览器与插件浏览器统一为标准的 `QTreeWidgetItem` 行，不再使用嵌入式 `PluginDescWidget`。
- 侧栏列表使用内容式选中胶囊；分支/缩进区域保持透明；展开箭头有独立 hover；快速双击箭头可连续展开/收起。
- 系统字体缩放上限 200%；关键 QtTest 覆盖字体、侧栏、子窗口、文件浏览器、乐器轨道等。

仍需重点处理的是**固定尺寸对话框**和少量**文案/主题细节**，不是主窗口结构。

---

## 3. 字号与排版

### 3.1 三档设计基准

正文以 10 为设计基准，所有档位按应用字体整体缩放：

| 层级 | 设计字号 | 字重 | 用途 |
| --- | ---: | --- | --- |
| 标题 | 13 | DemiBold（600） | 窗口标题、侧栏页面标题、对话框主标题 |
| 正文 | 10 | Regular | 按钮、下拉框、设置项、轨道名、片段名、插件名 |
| 辅助 | 8 | Regular | 旋钮短标签、单位、次要提示、LCD 标注 |

缩放公式：

```text
实际字号 = round(设计字号 × clamp(系统字体 pt / 9, 1.0, 2.0))
```

实际显示：

| 系统字体 | 标题 | 正文 | 辅助 |
| ---: | ---: | ---: | ---: |
| 9pt | 13px | 10px | 8px |
| 10pt | 14px | 11px | 9px |
| 11pt | 16px | 12px | 10px |
| 13.5pt | 20px | 15px | 12px |
| 18pt | 26px | 20px | 16px |

### 3.2 层级使用规则

- **窗口标题、侧栏页面标题、对话框主标题**：标题 13 + DemiBold。
- **标签页、分组框标题**：正文 10 + DemiBold；靠字重/选中态区分，不额外加字号。
- **按钮、下拉框、设置项、轨道名、片段名、插件名**：正文 10。
- **旋钮标签、单位、次要提示、LCD 标注**：辅助 8。
- **缩放例外**：LCD 数字、钢琴键标记、时间线刻度按控件几何尺寸绘制，不强行套三档。

### 3.3 实现位置

| 位置 | 作用 |
| --- | --- |
| `include/FontHelper.h` | 三档常量、`TextRole`、`titleFont()` / `bodyFont()` / `auxiliaryFont()` / `groupTitleFont()` |
| `src/gui/LmmsStyle.cpp` | 加载/重载主题时把 QSS 的 `font-size: Npx` 替换为 `scaledFontSize(N)px` |
| `src/gui/widgets/GroupBox.cpp` | 分组标题使用 `groupTitleFont()` |
| `src/gui/widgets/TabWidget.cpp` | 标签页使用正文，选中项 DemiBold |
| `src/gui/modals/AboutDialog.cpp` | 主标题使用 `titleFont()` |
| `data/themes/*/style.css` | 正文 10px、辅助 8px、标题 13px |

### 3.4 文案规则

默认 **sentence case**；专有名称保持官方拼写；单位按国际符号；只有设备面板最底层的短标签允许 ALL CAPS。

| 界面元素 | 规则 | 示例 |
| --- | --- | --- |
| 菜单、右键菜单 | sentence case；不加句号；弹窗命令加 `...` | `Save as new version`、`Export tracks...` |
| 按钮 | sentence case；系统标准按钮保持原样 | `Rescan plugins`、`OK`、`Cancel` |
| 窗口/对话框标题 | sentence case；专有窗口名除外 | `Connection settings`、`Piano Roll` |
| 设备面板标签页、组标题 | sentence case | `Effects`、`Effects chain`、`Filter` |
| 旋钮/LCD/LED 短标签 | 允许 ALL CAPS，仅限短标签/标准缩写 | `VOL`、`PAN`、`TEMPO` |
| 工具提示、状态栏 | sentence case；短语不加句号 | `Show or hide the Piano Roll` |
| 消息框 | sentence case；完整句子加句号 | `The project was not saved.` |
| 文件过滤器 | `描述 (*.扩展名)` | `LMMS project (*.mmp *.mmpz)` |
| 单位 | 国际符号，数值与单位间留空格 | `-6 dB`、`440 Hz`、`10 ms` |
| 缩写 | 全大写 | `MIDI`、`LFO`、`VST3`、`BPM` |
| 快捷键 | 不放进可翻译文本 | `Ctrl+Z` / `⌘Z` |

固定术语：

| 概念 | 固定写法 | 不要写成 |
| --- | --- | --- |
| 主程序 | `LMMS` | `Lmms`、`lmms`（命令名除外） |
| 歌曲编辑器 | `Song Editor` | `Song-Editor`、`Song editor` |
| Pattern 编辑器 | `Pattern Editor` | `Pattern editor` |
| 钢琴卷帘 | `Piano Roll` | `Piano-Roll`、`piano roll` |
| 自动化编辑器 | `Automation Editor` | `Automation editor` |
| 混音器 | `Mixer` | `mixer`（作为窗口名时） |
| 控制器架 | `Controller Rack` | `controller rack` |
| 工程笔记 | `Project Notes` | `project notes`（作为窗口名时） |
| 乐器轨道 | `instrument track` | `Instrument Track` |
| 混音通道 | `mixer channel` | `Mixer Channel` |
| 自动化片段 | `automation clip` | `Automation Clip` |
| MIDI | `MIDI` | `Midi`、`midi` |

面板缩写：`AMT`、`ATT`、`DEC`、`REL`、`SUST`、`HOLD`、`CHAN`、`VEL`、`FREQ`、`RESO`、`VOL`、`PAN`、`RATE`、`TIME`、`PHASE`、`WIDTH`、`TRSH`、`FDBK`、`DEL`、`SPD`。同一概念只保留一个缩写；完整单词放得下时优先完整单词。

标点：

- 菜单、按钮、标签、工具提示不加句号。
- 消息框、错误提示用完整句子并加句号。
- 只有会打开对话框的命令才加 `...`；统一 ASCII `...`。
- 标签后接数值用冒号（`Threshold:`）；组标题、按钮、菜单不加冒号。
- 用 `cannot`，不用 `can not`；复数用 `%n`，不写 `(s)`。

### 3.5 待处理

- 插件参数名里的 Title Case 仍需按插件逐个统一，不做全量重命名。
- `data/locale/en.ts` 尚未按最新源码重新生成并在 Transifex 同步。
- `time(s)`、`note(s)`、`octave(s)`、`file(s)` 等 `(s)` 复数仍需改用 Qt 的 `%n`。

---

## 4. 布局与控件

### 4.1 弹性布局原则

- 优先使用 `QLayout`、字体度量、内容 `sizeHint` 和最小尺寸，不用固定像素写死文本区域。
- 对话框至少应在 100%–200% 字体、长翻译和窄窗口下不裁切文字；放不下时使用滚动区域，而不是悄悄缩小常规文字。
- 文本型控件不要 `setFixedHeight()`；需要固定的是图标/画布等非文本区域。
- 新增固定尺寸前先确认它是否真的与字体无关；有疑问时按字体度量计算。

### 4.2 侧栏与列表标准

文件浏览器、插件浏览器以及所有 `FileBrowser` 页面共用同一套列表行为：

- 行是标准 `QTreeWidgetItem`，插件行不再嵌入自定义 widget。
- `CompactItemDelegate` 负责绘制内容：图标 + 文字，选中/悬停只画内容附近的圆角胶囊，不铺满整行。
- 分支/缩进区域保持透明；箭头只在鼠标直接悬停时显示 16×16 圆角底。
- 选中胶囊使用主题选择绿，悬停使用灰色；键盘焦点用胶囊外的一圈细绿边。
- 名称按可用宽度省略，并提供完整名称 tooltip；列宽使用 `Stretch`。
- 搜索时隐藏没有匹配项的分类，并展开有匹配项的分类。
- 快速点击箭头必须连续展开/收起；箭头上的双击事件只用于还原第二次点击的切换，不触发其他双击动作。
- 插件行支持拖拽和右键菜单；插件行双击不执行打开操作。

### 4.3 数值控件手势

| 控件 | 拖动 | 滚轮 | 修饰键 | 中键 | 双击 | 右键 |
| --- | --- | --- | --- | --- | --- | --- |
| 旋钮/BarModelEditor | 调整数值 | 全范围 100 步 | Shift 粗调、Ctrl 细调、Alt 更细；Shift/Ctrl+按下可拖放值/链接 | 归位 | 就地输入 | 重置/复制/粘贴/链接/控制器 |
| 推子 | 跟随鼠标绝对位置 | 1 dB/格 | Shift 3 dB、Ctrl 0.1 dB、Alt 0.05 dB | 归位 | 就地输入 dB | 同上 |
| 主工具栏滑块 | QSlider 默认 | 一个 singleStep，忽略修饰键 | 无 | 未接入 | 就地输入整数 | 重置/复制/粘贴/控制器 |

已知不一致：主工具栏滑块仍缺少中键归位和修饰键滚轮；统一前先评估行为变化。Ctrl 在拖动上表示“链接”、在滚轮上表示“细调”，不要改行为，只通过状态栏提示说明。

### 4.4 固定尺寸与已知布局风险

核心窗口：

| 位置 | 现状 / 风险 |
| --- | --- |
| `RenameDialog` | 输入框已改为布局管理，仍需大字体视觉验收。 |
| `PeakControllerDialog` | 256×64，需改为按字体/内容计算。 |
| `LfoControllerDialog` | 240×58，需检查标签与旋钮在 200% 下的间距。 |
| `LadspaBrowser` | 500×480/400，需检查窄窗口和长插件名。 |
| `SampleTrackWindow` 效果架 | 245×242，需随字体/内容调整。 |
| `EffectView` | 245×60，需检查旋钮、标签和按钮在 200% 下是否重叠。 |
| `FileBrowser` 搜索栏 | 固定高度已移除；仍需窄侧栏和长翻译验收。 |
| `EffectSelectDialog` 详情区 | 最大高 180px，长内容依赖滚动；需检查长 URL/翻译。 |

插件界面（排除 vendored 子模块，约 50 处 `setFixedSize`）：

- 简单控制面板：Bitcrush、Eq、Delay、Compressor、SlewDistortion、StereoMatrix。
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
- 中等：DualFilter、DynamicsProcessor、FrequencyShifter、MultitapEcho、LOMM、CrossoverEQ。
- 复杂/自绘画布：GranularPitchShifter、SlicerT、AudioFileProcessor、Sf2Player、GigPlayer、Vestige。
- 典型风险：`Sf2Player`/`GigPlayer` 文件名标签固定 11/14px 高、宽 156px；`Vestige` 文本按钮固定 200×24px；多个插件的下拉框使用固定坐标/高度。

---

## 5. 颜色与主题

- 普通文字、按钮、菜单和标签页使用主题正文色；标签页用背景、边框和字重表示选中，不用链接绿表示普通/选中状态。
- 可点击 URL 使用主题 `QPalette::Link`（默认/经典为 `#8cfab6`），保留下划线；已访问链接默认同色。
- 键盘焦点必须可见；焦点样式只画 outline/胶囊边，不改变布局和 padding。
- 禁用、选中、警告、错误、录音、波形和音符颜色各自保留语义；用户在 Project Notes 中设置的颜色和第三方插件界面不强制覆盖。
- 普通文字和链接按实际相邻背景检查对比度，目标至少 4.5:1；链接不能只靠颜色区分。
- 选择色：默认主题列表选择绿 `#0f5c2c`；悬停灰 `#3C444E`；焦点绿 `#0bd556`。侧栏列表用 `CompactItemDelegate` 绘制，QSS 只负责其他控件。
- 默认主题 `LmmsPalette::windowText` 仍是绿色 `#1de276`，但标签页已显式使用正文色；需在真实窗口核实所有状态。
- 默认主题 PNG 已全部迁移为 SVG；经典主题仍有自己的 PNG 素材，亮色主题需要重新设计明度。

---

## 6. UI 路线图

### UI-1：侧栏与列表收尾（完成）

目标：把最近的列表统一工作收尾，避免回归。

- 确认 My projects / My samples / My presets 等 `FileBrowser` 页面都使用 `CompactItemDelegate` 和统一的箭头行为。
- 补搜索无结果状态；确认长名称、长翻译、窄侧栏下省略和 tooltip 正常。
- 已增加 `CompactItemDelegateTest`：内容胶囊宽度、最后一个字符不裁切。
- 已增加 `FileBrowserTest::arrowDoubleClickToggles`：快速双击箭头连续切换。
- 截图沿用现有版本，最终统一重做放在 UI-4。
- 运行 `FileBrowserTest`、`SideBarWorkspaceTest`、`FontScaleTest`、`InstrumentTrackTest`。

验收：默认/经典主题，100%/125%/150%/200%，en/zh_CN/de_DE，窄侧栏；无整行色块、无文字裁切、无箭头误触发。

### UI-2：核心固定尺寸对话框（进行中）

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

### UI-3：插件固定尺寸对话框（进行中，多 PR）

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

### UI-4：排版与文案收尾（待开始）

- 清理剩余固定字号和硬编码 `font-size`；确认三档标准在所有核心界面生效。
- 逐插件统一参数名 Title Case，不做全量重命名。
- 用 `lupdate-qt5` 重新生成 `data/locale/en.ts`，同步 Transifex 术语表。
- 处理 `(s)` 复数、单位空格、组标题冒号等剩余文案问题。
- 更新三档缩放截图和文案检查记录。

验收：`FontScaleTest` 通过；100%–200% 无字号断层；`en.ts` diff 可审查。

### UI-5：主题、动画与无障碍（待开始）

- 经典主题 PNG → SVG；默认主题已无 PNG。
- 按真实背景检查链接/正文/禁用/选中对比度，尤其是 About、Project Notes、侧栏。
- 补测 165 Hz/Wayland 下侧栏滑动和按钮反馈动画。
- 检查键盘焦点、Tab 顺序、可访问名称；列表行的 tooltip 和省略文本要能访问完整内容。

验收：默认/经典主题，100%–200%，键盘可完成主要操作；动画可关闭且不阻塞操作。

---

## 7. 验收与回归

### 7.1 必跑测试

```bash
cmake --build build-ui -j"$(nproc)"
cd build-ui/tests
QT_QPA_PLATFORM=offscreen ./FontScaleTest
QT_QPA_PLATFORM=offscreen ./CaptionWidgetsTest
QT_QPA_PLATFORM=offscreen ./SubWindowTest
QT_QPA_PLATFORM=offscreen ./SideBarWorkspaceTest
QT_QPA_PLATFORM=offscreen ./FileBrowserTest
QT_QPA_PLATFORM=offscreen ./CompactItemDelegateTest
QT_QPA_PLATFORM=offscreen ./InstrumentTrackTest
```

### 7.2 静态检查

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
```

### 7.3 视觉验收

- 主题：默认 + 经典。
- 字体/缩放：100%、125%、150%、200%。
- 语言：en、zh_CN、de_DE；长翻译优先。
- 窗口：默认宽度 + 窄窗口；侧栏可拖动宽度。
- 重点界面：主窗口、Song Editor、Mixer、FileBrowser、PluginBrowser、EffectSelectDialog、About、RenameDialog、插件控制面板。
- 检查：文字/图标不裁切、不重叠；选中/悬停/焦点清晰；滚动条只在需要时出现；键盘可操作。

### 7.4 截图环境

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

## 8. 缩放与窗口审计（2026-11-28）

已实际检查：

- 主窗口从 1600×1000 缩小到 1000×700、再放大到 1500×950：工具栏、MDI 区和混音器没有出现控件重叠；窗口变窄时右侧 MDI 子窗口会被主窗口裁剪，这是 Qt MDI 自身的布局行为。
- Song Editor 子窗口移动后，轨道头、时间线和工具栏仍保持对齐；子窗口不会随主窗口自动缩放。
- LFO 控制器在 100% 和 200% 字体下已打开检查，布局无裁切。
- 其余已改为布局驱动的插件面板目前只有构建验证，尚未逐个拖动缩放并做 100%/200% 目视。

已知边界：

- MDI 子窗口默认不参与主窗口的自动排列；主窗口变窄时可能被裁剪，需要另行实现自动排列或限制。
- 固定画布插件（Eq、SlewDistortion、LOMM、SlicerT、AudioFileProcessor、Sf2/Gig/Vestige）保留原设计，优先用滚动/整体缩放而不是重排。
- 插件面板的 200% 逐项验收仍未完成。

---

## 9. 变更记录

- 2026-11-28：合并旧的 UI 任务、审计、文案、手势、颜色/布局文档为本文件；删除 VST/LV2/PDC 等非 UI 设计文档（代码保留）。
- 2026-11-28：三档字号调整为 13/10/8；侧栏列表统一为 `CompactItemDelegate`，加入内容选中胶囊、箭头 hover 和快速双击箭头切换。
- 2026-11-28：完成 UI-1 验收，新增 `CompactItemDelegateTest` 和箭头双击回归测试。
- 2026-11-28：UI-2 开始，`PeakControllerDialog` 改为弹性布局。
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
- 2026-11-28：混音器分隔条按实际通道宽度调整；效果器架滚动条按需显示；文件浏览器/插件浏览器选中样式统一。
