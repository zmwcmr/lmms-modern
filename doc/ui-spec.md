# LMMS UI 设计规范

> 本文件是 LMMS UI 的**规范**（normative）：字号、布局、控件行为、颜色、图标和主题。
>
> 本文件应慢变；修改规范需要独立 PR，并在文末「规范变更记录」登记。
> 任务、路线图、验收流程和变更记录见 [`ui-ux.md`](ui-ux.md)。
>
> 适用范围：`src/gui`、`include`、`data/themes`、插件界面。
>
> 不在本分支范围：VST/VST3/LV2/PDC 宿主实现及其设计文档。相关代码保留在既有分支历史中。
>
> 规范不改变工程文件格式、插件 ABI、`Track`/`Clip`/`Mixer` 数据成员，也不引入 docking 框架或 model/view。

---

## 1. 字号与排版

### 1.1 三档设计基准

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

### 1.2 层级使用规则

- **窗口标题、侧栏页面标题、对话框主标题**：标题 13 + DemiBold。
- **标签页、分组框标题**：正文 10 + DemiBold；靠字重/选中态区分，不额外加字号。
- **按钮、下拉框、设置项、轨道名、片段名、插件名**：正文 10。
- **旋钮标签、单位、次要提示、LCD 标注**：辅助 8。
- **缩放例外**：LCD 数字、钢琴键标记、时间线刻度按控件几何尺寸绘制，不强行套三档。

### 1.3 实现位置

| 位置 | 作用 |
| --- | --- |
| `include/FontHelper.h` | 三档常量、`TextRole`、`titleFont()` / `bodyFont()` / `auxiliaryFont()` / `groupTitleFont()` |
| `src/gui/LmmsStyle.cpp` | 加载/重载主题时把 QSS 的 `font-size: Npx` 替换为 `scaledFontSize(N)px` |
| `src/gui/widgets/GroupBox.cpp` | 分组标题使用 `groupTitleFont()` |
| `src/gui/widgets/TabWidget.cpp` | 标签页使用正文，选中项 DemiBold |
| `src/gui/modals/AboutDialog.cpp` | 主标题使用 `titleFont()` |
| `data/themes/*/style.css` | 正文 10px、辅助 8px、标题 13px |

### 1.4 文案规则

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

---

## 2. 布局与控件

### 2.1 弹性布局原则

- 优先使用 `QLayout`、字体度量、内容 `sizeHint` 和最小尺寸，不用固定像素写死文本区域。
- 对话框至少应在 100%–200% 字体、长翻译和窄窗口下不裁切文字；放不下时使用滚动区域，而不是悄悄缩小常规文字。
- 文本型控件不要 `setFixedHeight()`；需要固定的是图标/画布等非文本区域。
- 新增固定尺寸前先确认它是否真的与字体无关；有疑问时按字体度量计算。

### 2.2 侧栏与列表标准

文件浏览器、插件浏览器以及所有 `FileBrowser` 页面共用同一套列表行为：

- 行是标准 `QTreeWidgetItem`，插件行不再嵌入自定义 widget。
- `CompactItemDelegate` 负责绘制内容：图标 + 文字，选中/悬停只画内容附近的圆角胶囊，不铺满整行。
- 分支/缩进区域保持透明；箭头只在鼠标直接悬停时显示 16×16 圆角底。
- 选中胶囊使用主题选择绿，悬停使用灰色；键盘焦点用胶囊外的一圈细绿边。
- 名称按可用宽度省略，并提供完整名称 tooltip；列宽使用 `Stretch`。
- 搜索时隐藏没有匹配项的分类，并展开有匹配项的分类。
- 快速点击箭头必须连续展开/收起；箭头上的双击事件只用于还原第二次点击的切换，不触发其他双击动作。
- 插件行支持拖拽和右键菜单；插件行双击不执行打开操作。

### 2.3 数值控件手势

| 控件 | 拖动 | 滚轮 | 修饰键 | 中键 | 双击 | 右键 |
| --- | --- | --- | --- | --- | --- | --- |
| 旋钮/BarModelEditor | 调整数值 | 全范围 100 步 | Shift 粗调、Ctrl 细调、Alt 更细；Shift/Ctrl+按下可拖放值/链接 | 归位 | 就地输入 | 重置/复制/粘贴/链接/控制器 |
| 推子 | 跟随鼠标绝对位置 | 1 dB/格 | Shift 3 dB、Ctrl 0.1 dB、Alt 0.05 dB | 归位 | 就地输入 dB | 同上 |

**Ctrl 语义固定**：拖动上表示“链接”、滚轮上表示“细调”。不要改变行为，只通过状态栏提示说明。

主工具栏滑块目前仍使用 `QSlider` 默认行为，属于已知偏差，见 [`ui-ux.md`](ui-ux.md) 的「已知问题」。

---

## 3. 颜色与主题

### 3.1 颜色规则

- 普通文字、按钮、菜单和标签页使用主题正文色；标签页用背景、边框和字重表示选中，不用链接绿表示普通/选中状态。
- 可点击 URL 使用主题 `QPalette::Link`（默认/经典为 `#8cfab6`），保留下划线；已访问链接默认同色。
- 键盘焦点必须可见；焦点样式只画 outline/胶囊边，不改变布局和 padding。
- 禁用、选中、警告、错误、录音、波形和音符颜色各自保留语义；用户在 Project Notes 中设置的颜色和第三方插件界面不强制覆盖。
- 普通文字和链接按实际相邻背景检查对比度，目标至少 4.5:1；链接不能只靠颜色区分。
- 选择色：默认主题列表选择绿 `#0f5c2c`；悬停灰 `#3C444E`；焦点绿 `#0bd556`。侧栏列表用 `CompactItemDelegate` 绘制，QSS 只负责其他控件。
- 图标和正文必须使用显式颜色 token，不依赖 `LmmsPalette::windowText`；该属性在默认主题中是绿色 `#1de276`，不能当作通用前景色。

### 3.2 图标设计语言（A，冻结）

2026-11-28 冻结：采用 **A（单色实心）**，不采用 B（2px 线性）。改造前的 `default` 已完整快照到 `data/themes/legacy`，该目录只读，不再修改。

适用范围：`data/themes/default` 中的 UI 图标（动作、工具、文件类型、传输、状态）。控件皮肤（knob/fader/键盘/LED/LCD/step）、背景和插件 logo 不在本轮范围。

#### 3.2.1 画布与尺寸

| 项目 | 规范 |
| --- | --- |
| 画布 | 24×24，仅写 `viewBox="0 0 24 24"`，不写 `width`/`height` |
| 活动区 | 20×20，四边 2px；`gear` 例外为 21px（四边 1.5px） |
| 尺寸档 | 16 / 24 / 32 / 48；不再新增 14 / 17 / 22 等尺寸 |
| 最小细节 | 2px；16px 档不低于 1.5px |
| 圆角 | 外角 2px，内角 1px |
| 关键线 | 方形 18×18、圆形 20 直径、横竖 18×2、对角线 45° |
| 对齐 | 0.5px 坐标；禁止 `0.264583mm` 这类单位换算值 |
| 光学补偿 | 圆形 +0.5px，三角形 +1px；按视觉重量微调 |

> 例外：`QStyle::PM_ButtonIconSize` 仍是 20，用于带文字按钮的布局；图标资产不新增 20 这一档。

#### 3.2.2 画法与层次

- **只用一种 ink**：禁止 40% 灰、灰色 hex 和第二色；层次用透明孔、透明缝和负空间表达。
- **实心优先**：只有实心会遮住必要内部结构时才用粗轮廓（≥2.4px）；目前仅 `metronome`、`search` 例外。
- 传输控制（play/pause/stop/record）保持实心；状态指示（LED、录音点）使用语义色。
- 禁止：渐变、阴影、发光、滤镜、`<text>`、`<image>`、自带底板、emoji、3D 高光。
- 已冻结的图形约定：

| 图标 | 约定 |
| --- | --- |
| `save` | U 形托盘 + 实心箭头 |
| `folder` | 单色剪影，不再用灰色标签 |
| `open` | 单色背板 + 透明文档孔 + 单色前翻盖 |
| `project_file` | 单色页面 + 透明播放三角孔 |
| `sample_file` | 单色页面 + 透明波形孔 |
| `metronome` | 杆从**底部中心**起，无配重；允许 2.6px 轮廓 |
| `gear` | 使用 `legacy` 主题中的 6 齿圆角路径，缩放到 21px 活动区 |
| `record` | 100% 语义红环 + 实心点 |
| `search` | 粗环 + 4px 手柄 |

#### 3.2.3 颜色与运行时

| token | 默认值 | 用途 |
| --- | --- | --- |
| `icon.ink` | `#d1d8e4` | 默认图标 |
| `icon.accent` | `#0bd556` | 选中/开启 |
| `icon.record` | `#ff4757` | 录音/录制中 |
| `icon.warning` | `#f5a623` | 警告 |
| `icon.error` | `#ff5c5c` | 错误 |
| `icon.info` | `#59b0ff` | 信息（可选） |

- 图标以单色/alpha 形状交付；`getIconPixmap` 加载的图标用 `currentColor`，由 loader 解析为 `iconColor`（默认取主题正文色）。
- QSS 属性：`LmmsPalette` 的 `qproperty-iconColor`、`iconDisabledColor`、`iconAccentColor`、`iconRecordColor`、`iconWarningColor`、`iconErrorColor`、`iconInfoColor`。
- 同一份 SVG 必须同时服务深色和浅色主题；浅色主题是 tint 的验收场景。
- 菜单勾选由 base style 绘制，跟随 palette；track-ops 的 `gear`/`speaker`/`headphones`/`speaker_slash` 由代码设置图标并显式 tint；源 SVG 保留白色以兼容 QSS 的 `btn-mute-inv`。滚动条、插件 stepper、分支箭头和 slider/shadow 仍是 QSS `image:`，暂时没有 tint；这是剩余例外，不得扩散到 `getIconPixmap` 图标。
- 主形状对比度 ≥3:1；透明孔/缝不能承载唯一语义。

#### 3.2.4 命名与资产分层

- 新图标使用小写下划线：`transport_*`、`edit_*`、`file_*`、`folder*`、`view_*`、`track_*`。
- 状态用运行时 tint，不新增 `_active`/`_inactive` 文件；旧名字保留 alias，暂不大规模改调用点。
- 皮肤（knob/fader/键/LED/LCD/step）不进图标命名空间；品牌和插件 logo 单独管理。
- 改造时保留原文件名，避免修改约 900 处 `getIconPixmap` 调用。

#### 3.2.5 合规检查

- 图标必须在 16 / 24 / 48 三档渲染非空，深色和浅色底都可读。
- 无灰色填充、无渐变、无文字、无滤镜、无自带底板。
- 除 `gear`（1.5..22.5）外，内容 bbox 在 2..22。
- 新图标从 `buildtools/icon-template.svg` 开始；迁移后的图标必须通过 `buildtools/icon-audit.py` 的 lint 和 sheet 检查，并通过 `IconTest`（空图、tint、尺寸 token）。

### 3.3 主题资产

- `data/themes/legacy` 是 2026-11-28 改造前 `default` 的完整快照，可在设置中切换查看，但不再修改。
- `data/themes/default` 是唯一接收新图标的主题。
- `classic` 保持现状；其 PNG、HiDPI 和缺失图标问题由 [`ui-ux.md`](ui-ux.md) 的 UI-5 跟踪。

---

## 4. 规范变更记录

- 2026-11-28：补充图标运行时 tint（`currentColor` + icon tokens）和 QSS 例外；新增 `IconSize` / `IconRole`。
- 2026-11-28：从 `ui-ux.md` 拆出设计规范；冻结图标语言 A；登记 `legacy` 主题快照。
- 2026-11-28：收录既有规范：三档字号 13/10/8、弹性布局、侧栏列表、数值控件手势、颜色与文案规则。
