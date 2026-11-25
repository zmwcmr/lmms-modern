# LMMS UI/UX 审计报告

| 项目 | 值 |
| --- | --- |
| 基线 | `develop` @ `ca4fc9687`（分支 `feature/ui-ux-improvements`） |
| 范围 | `src/gui/**`、`include/*.h`、`data/themes/**`、`plugins/**` 的界面代码 |
| 计数口径 | 本文所有数字除特别说明外均为 **`src/gui`** 范围；`plugins/` 另计（硬编码颜色 +113、固定尺寸 +59、字号调用 +16） |
| 方法 | 静态阅读 + 全仓 `grep` 计数；每条结论附可复现的检索命令（附录 A） |
| 判据 | ① 是否影响日常操作频次；② 是否有验收标准；③ 是否会阻塞后续改进 |
| 说明 | 不含主观"好不好看"评价；不含 MVVM/停靠系统之类的架构级构想，那些单独讨论 |

严重度：**S1** 每天都要撞上 / **S2** 常见但可绕过 / **S3** 边缘或一致性瑕疵。

> 本文件记录的是 `develop@ca4fc9687` 的基线状态。已在 `feature/ui-ux-improvements` 上修复的条目
> 不改写本文，而是记录在 `doc/ui-ux-tasks.md` 的《实施记录》一节，便于对照"改前/改后"。

---

## 一、摘要

| # | 问题 | 严重度 | 是否可小步修 |
| --- | --- | --- | --- |
| 1 | 没有设计令牌层，颜色/字号散落在 QSS、C++ 画布、翻译字符串三处 | S1 | 是 |
| 2 | 尺寸全为像素硬编码，字体/语言/缩放一变就截断 | S1 | 是 |
| 3 | 同一手势在旋钮/推子/滑块上语义不同，且无任何提示 | S1 | 是 |
| 4 | 快捷键不可改，且部分快捷键文案硬编码在最外层菜单文本里 | S2 | 部分 |
| 5 | 没有状态栏，没有持久反馈面 | S2 | 是 |
| 6 | 轨道头 / 混音通道之间缺少"中间层"信息 | S1 | 部分 |
| 7 | 控件全部手绘，无 model/view，缺少可复用的数据层 | S2（结构性） | 否（本期不做） |
| 8 | Pattern / Clip / Automation 三种抽象视觉无差别 | S2 | 部分 |
| 9 | 键盘焦点几乎不可见，零 `accessibleName` | S2 | 是 |
| 10 | 主题切换靠填目录字符串，且必须重启 | S3 | 是 |

关键结论：**前 6 条里，有 5 条可以在不触碰数据模型、不重写窗口系统的前提下修掉。** 第 7 条是真正需要架构决策的部分，不应混进短期任务。

---

## 二、视觉系统

### 2.1 没有令牌层

`data/themes/default/style.css` 共 1211 行，其中：

- **243 个十六进制颜色字面量**，去重后 **127 个不同的颜色值**；
- 同一个颜色被以不同大小写重复书写（`#262b30` 出现 20 次，`#262B30` 出现 6 次）；
- 简写与全写混用（`#fff` / `#ffffff`、`#000` / `#000000`）；
- 169 个规则块、251 条 `qproperty-*`（145 个不同属性）。

也就是说：**主题可配置的"钩子"其实不算少（145 个属性），缺的是唯一真值来源。** 改一个"边框色"要改十几处，且无法保证一致。

复现：`grep -oE '#[0-9a-fA-F]{3,8}' data/themes/default/style.css | sort | uniq -c | sort -rn`

### 2.2 硬编码颜色绕过主题

全仓库 `src/gui` 中有 **36 处** 直接用 `QColor(r,g,b)` 取色（`plugins/` 另有 113 处），其中影响最大的几处：

| 文件:行 | 颜色 | 影响 |
| --- | --- | --- |
| `src/gui/tracks/TrackView.cpp:63` | `QColor(32,36,40)` | 轨道底色，**亮色主题在此彻底不可能** |
| `src/gui/widgets/FloatModelEditorBase.cpp:305` | `QColor{3,94,97}` | 旋钮激活态前景色，绕过 QSS |
| `src/gui/ProjectNotes.cpp:58,92` | `QColor(64,64,64)` / `QColor(224,224,224)` | 项目笔记窗口无视主题 |
| `src/gui/widgets/Graph.cpp:48,406` | `#FFAA00` / `#707C91` | 通用波形/包络控件 |
| `src/gui/widgets/Fader.cpp:721` | `QColor(255,255,255,216)` | 0 dB 刻度线 |

注意：`LmmsPalette`（`include/LmmsPalette.h`、`data/themes/default/style.css:1200`）已经提供了一个**由 QSS 驱动、且已通过 `QApplication::setPalette()` 全局生效**的调色板（`src/gui/GuiApplication.cpp:113-120`）。上面这些代码只是**没有用它**。这意味着统一取色是一条低风险路径，不需要新建机制。

### 2.3 字号

- `include/FontHelper.h:31-33`：`DEFAULT_FONT_SIZE = 12`、`SMALL_FONT_SIZE = 10`、`LARGE_FONT_SIZE = 14`，单位是**像素**；
- 31 处 `adjustedToPixelSize()` 调用依赖这三个常量（`plugins/` 另有 16 处）；
- QSS 里另有 **px 与 pt 混用**的 14 处 `font-size`，最小到 **7pt**（`style.css:794`、`:806`，PeakIndicator 与 TimeLineWidget，约 9.3 px，低于可读下限）、**8pt**（`:45`，FileBrowser 复选框，约 10.7 px，尚在可读范围）；
- C++ 字符串里也有：`src/gui/instrument/InstrumentTrackWindow.cpp:130` → `QString labelStyleSheet = "font-size: 10px;"`。

后果：系统字号或 Qt 字体设置一变，`setPixelSize` 不跟随；7pt 在 1080p 下约 9px，低于可读下限；同一个界面里出现 4 套字号来源。

### 2.4 像素尺寸

`src/gui` + `include` 中 **96 处** `setFixedSize/setFixedWidth/setFixedHeight`（`plugins/` 另有 59 处）。典型：

- `include/TrackView.h:38` `TRACK_OP_WIDTH = 78`（轨道头按钮区）；
- `src/gui/MainWindow.cpp:195` `m_toolBar->setFixedHeight( 64 )`（主工具栏）；
- `src/gui/editors/PianoRoll.cpp:5337-5398` **八个**固定宽下拉框（64 / 64 / 85 / 105 / 72 / 155 / 125 / 96 px）；
- `src/gui/MixerChannelView.cpp:104-118`：通道名是**旋转 −90° 的 `QLineEdit` 放进 `QGraphicsView`**，固定 65 px。

后果：125%/150% 缩放、中文/德语长词、用户改字号三种情况下都会截断或错位。这不是审美问题，是排版没做弹性。

### 2.5 位图残留

主主题已有 220 个 SVG，但仍有 **78 个 PNG**（`data/themes/default/*.png`），其中 `black_key*.png`、`combobox_bg.png`、`horizontal_slider.png`、`closed_branch.png`、`cursor_select_*.png` 等仍参与 HiDPI 渲染。

其中 `apply_css.png`、`apply-selected_css.png` 由 QSS 菜单指示样式直接引用（见提交 `74bac7745`：Qt 样式表在这些位置无法可靠使用 SVG），`closed_branch.png`/`open_branch.png` 仅被 `QTreeView::branch` 的 `border-image` 使用；其余应逐个评估。

### 2.6 主题切换流程

- 没有外观设置页。主题 = `Settings → Paths → Theme directory` 里的一段**目录路径文本**（`src/gui/modals/SetupDialog.cpp:862`）；
- 样式只在启动时加载一次（`src/gui/LmmsStyle.cpp:139`），改完必须重启；
- 仓库里只有 `default` 与 `classic` 两套，均为深色。

### 2.7 选择态语义不一致

- QSS 里树/列表选中是绿色 `#17793b`（`style.css` 的 `QTreeWidget::item:selected`）；
- 而全局调色板的 `highlight` 是 `#262b30`（`style.css:1209`），几乎等于背景色 `#262b30`。

于是"选中"在不同控件里是两种视觉，其中一种在非 QSS 控件里几乎不可见。这是"状态反馈不可靠"的根源之一。

---

## 三、交互一致性

### 3.1 滚轮：同一手势两种数学

| | 旋钮/滑块（`FloatModelEditorBase::wheelEvent`，`FloatModelEditorBase.cpp:323`） | 推子（`Fader::wheelEvent`，`Fader.cpp:270`） |
| --- | --- | --- |
| 默认 | 整个范围分 100 步 | 1 dB / 格 |
| Shift | 10 步扫完（粗） | 3 dB（粗） |
| Ctrl | 1000 步扫完（细） | 0.1 dB（细） |
| Alt | 2000 步扫完（更细） | **返回 0，完全不响应**（`Fader.cpp:283-297`） |

同一界面里"Ctrl = 细调"的强度差一个数量级，Alt 在推子上是哑的。另外 `FloatModelEditorBase` 对 `inverted()` 做了自然滚动适配，`Fader` 也做了，但两者对 `Alt` 的处理相反。

### 3.2 双击与归位

- 双击旋钮：弹出模态 `QInputDialog`"Please enter a new value between X and Y"（`FloatModelEditorBase.cpp:500-520`）；
- 双击推子：同样弹模态框（`Fader.cpp:128-160`）；
- 归位：只能右键菜单点 `Reset (初始值)`（`AutomatableModelView.cpp:62`），没有手势。

现代 DAW 的惯例是双击归位、点数值就地编辑。LMMS 两者都不是，且模态框会打断工作流。

### 3.3 修饰键零文档

全仓库检索不到任何面向用户的"Shift=粗调 / Ctrl=细调 / Alt=更细"说明（`grep -rn 'fine.*adjust' src/gui data` 只有代码注释）。用户只能靠试。

### 3.4 焦点与键盘可达

- QSS 里只有 **2 条** `:focus` 规则（1211 行样式表）；
- 14 处显式 `Qt::NoFocus`；
- 零 `setAccessibleName`。

键盘用户无法判断当前焦点在哪，屏幕阅读器读不出图标按钮。

### 3.5 拖放

`StringPairDrag` 是统一机制，但 `processDragEnterEvent()`（`src/gui/StringPairDrag.cpp:83-102`）只做一件事：根据 mime key 决定 `accept` 还是 `ignore`。**没有任何目标高亮或区域预览**，用户只能从光标变化猜"这里能放"。`ClipView` 的拖动提示靠 tooltip 文案承载（`ClipView.cpp`："Press <Ctrl> or <Alt> for unquantized splitting…"）。

---

## 四、快捷键

- 实际绑定是程序化的、可移植的（`keySequence(Qt::CTRL, Qt::Key_1)`，`MainWindow.cpp:435-459`）；
- **但菜单文案把 `Ctrl+1` 写死成字符串**：`MainWindow.cpp:1047-1079` 使用 `tr("Song Editor") + "\tCtrl+1"`；
  - 在 macOS 上实际绑定是 ⌘+1，菜单却显示 Ctrl+1；
  - `include/KeyboardShortcuts.h:38` 明明提供了 `UI_CTRL_KEY`（macOS 下为 "⌘"）却没有被这些地方使用。
- 没有快捷键映射表，也没有 `Settings → Keyboard` 页（`SetupDialog.cpp:890-902` 只有 General / Performance / Audio / MIDI / Paths）。
- 14 处 tooltip 把快捷键写在 `tr()` 里，译者可以改坏。
- **更严重的一处**：`src/gui/ProjectNotes.cpp` 有 12 处用**显示字符**直接当绑定：
  `a->setShortcut( tr( "%1+Z" ).arg(UI_CTRL_KEY) )`（`:114`、`:120`、`:126`…）。
  Linux 上 `UI_CTRL_KEY == "Ctrl"`，`QKeySequence("Ctrl+Z")` 能解析；macOS 上 `UI_CTRL_KEY == "⌘"`，而 `QKeySequence::fromString` 不认识 `⌘` 符号，**这些快捷键在 macOS 上极可能解析失败**（需实测确认）。
  正确做法是 `setShortcut(QKeySequence::Undo)` 或 `keySequence(Qt::CTRL, Qt::Key_Z)`。

结论：**"可自定义快捷键"目前为零**，但菜单文案这一层是可以立刻修的一致性问题。

---

## 五、信息架构

### 5.1 没有 model/view

`TrackView`、`MixerChannelView`、`PianoRoll`、`AutomationEditor` 都是手写 `paintEvent` + 直接读写模型。后果：

- 无法做轨道列表的筛选/排序/搜索；
- 无法做"选中多条轨道批量操作"的通用逻辑；
- 任何"检查器面板"都要重新写一套取数代码。

这是结构性债务，**本期不处理**，但它解释了为什么很多 issue 挂了多年没人做。

### 5.2 轨道头信息不足

轨道头 78 px 内只有：拖动柄、齿轮菜单、Mute、Solo（`src/gui/tracks/TrackOperationsWidget.cpp`）。

- 音量、声像、输入/输出、发送量都只能在乐器/采样轨道的独立窗口里改；
- 轨道名由 `TrackLabelButton` 绘制：`elideName()` 会做省略（`TrackLabelButton.cpp:225`），但**只有宽度小到 `maxTextWidth < 1` 时才设置 tooltip**，文本被省略时反而 `setToolTip("")`（`:231-234`）——用户看不到完整轨道名；
- 顺带一个代码味道：`TrackOperationsWidget` 的 `setObjectName("automationEnabled")`（`TrackOperationsWidget.cpp:66`），说明样式是靠"借"别的对象名实现的。

### 5.3 混音器

- `MixerChannel` 只有 `m_volumeModel`、`m_muteModel`、`m_soloModel`、`m_fxChain`（`include/Mixer.h:45-72`）——**没有 pan**；
- 峰值指示有左右两路，但没有 dB 刻度、没有峰值保持数值；
- 路由：发送是一个 `SendButtonIndicator` + 一个小旋钮，收/发各占一行，通道间关系只能靠悬停提示；
- 通道名的旋转 `QLineEdit` 方案导致超过 65 px 的名字被截断。

### 5.4 编辑器缺少"当前对象"的统一表达

- 钢琴卷帘（Ctrl+3）编辑 `MidiClip`；
- Pattern Editor（Ctrl+2）编辑 Pattern；
- 自动化编辑器（Ctrl+4）编辑 `AutomationClip` 绑定的模型。

三个入口、三种对象，UI 上没有任何"我正在编辑 X，它属于轨道 Y"的常驻指示。

---

## 六、反馈与状态

- **没有状态栏**：全仓库 `grep statusBar|showMessage src/gui` 为空；
- 只有模态对话框和 `TextFloat`（悬停数值）两种反馈通道；
- 撤销历史不可见，只有一个 `Edit → Undo/Redo` 条目，且 `MAX_UNDO_STATES = 100` 写死，源码注释自己写着 `// TODO: make this configurable in settings`（`src/core/ProjectJournal.cpp:42`）；
- 插件扫描、工程渲染、自动保存等长任务没有统一进度承载点。

---

## 七、可访问性

| 项 | 现状 | 结论 |
| --- | --- | --- |
| 对比度（正文） | `#d1d8e4` on `#111314` = 13.0:1 | 合格 |
| 对比度（树选中） | `#d1d8e4` on `#17793b` = **3.82:1** | 低于 AA (4.5) |
| 对比度（工具按钮） | `#fff` on `#636c7a` = 5.31:1 | 合格（底部渐变更好） |
| 字号 | 7pt / 8pt 存在 | 不合格 |
| 键盘焦点 | 2 条 `:focus`、14 处 `NoFocus` | 不合格 |
| 屏幕阅读器 | 0 个 `accessibleName` | 不合格 |
| 色觉 | 状态主要靠颜色（绿=选中、红=clip）且无形状/图标冗余 | 待改进 |

值得说明的是：**对比度整体其实不差**，问题集中在字号下限、焦点可见性和语义化标签三处，不能用"LMMS 对比度差"这种笼统说法。

---

## 八、概念模型：Pattern / Clip / Automation

- `PatternTrack` 引用 `PatternStore`（`include/PatternTrack.h`）；
- `MidiClip` 挂在 `InstrumentTrack` 上；
- `AutomationClip` 在 `AutomationTrack` 上，且同一轨道可放多个 clip 指向不同模型。

三者在 Song Editor 里都渲染成"轨道 + 一种 clip"，视觉几乎一致，语义完全不同。LMMS 因此同时存在 FL 式 pattern 和 DAW 式 piano-roll clip 两套心智模型，这是它区别于其他 DAW 的**独有**认知成本。

此外，自动化没有任何"从控件出发"的入口：旋钮右键菜单里没有"在自动化编辑器中显示"，用户必须先加自动化轨、再双击 clip、再在下拉框里找模型（`AutomationEditor.cpp:1057` 的空白提示就是"请去 Song Editor 双击自动化 clip"，循环指路）。

---

## 九、国际化风险

1. 快捷键写进 `tr()`（见 §4）；
2. 固定像素宽度 + 长译文 = 截断（§2.4）；
3. 单位后缀直接拼字符串（`setHintText(tr("Volume:"), "%")`），部分语言需要前置单位；
4. `tr("...") + " (Ctrl+1)"` 把标点与快捷键混进可译文本。

---

## 十、被夸大、已解决或不适用的说法

为避免后续决策被误导，明确列出：

| 说法 | 实际 |
| --- | --- |
| "HiDPI 位图资源有问题" | 主主题已 SVG 化并按 2× DPR 光栅化（`74bac7745`）；剩余 78 个 PNG 属于收尾工作 |
| "撤销覆盖率严重不足" | 钢琴卷帘 18 处、自动化编辑器 5 处打点；真正问题是**快照制 + 100 步上限 + 不可配置 + 无历史面板** |
| "MDI 完全不可用" | `SubWindow` 已支持 detach/attach（`include/SubWindow.h`），缺的是"不遮挡的默认排法" |
| "对比度普遍偏低" | 正文 13:1、按钮 5.3:1，仅树选中 3.82:1 不达标 |
| "缺少自定义快捷键" | 事实成立，但绑定层是程序化可移植的，缺的是映射表与 UI，不是重写 |
| "必须重构为统一工作区" | 结构性方案；在没有选择上下文与取数层之前做，只会把耦合搬个位置 |

---

## 十一、优先级建议

按"每日痛感 × 修复成本"排：

1. **设计令牌 + 清除硬编码颜色** —— 解锁一切视觉工作，且是纯机械改动；
2. **手势语义统一 + 可发现性**（双击归位、修饰键 tooltip、滚轮一致）；
3. **尺寸弹性**（固定 px → `sizeHint`/`minimumWidth`，先从钢琴卷帘工具栏开始）；
4. **反馈层**（状态栏、撤销步数可配）；
5. **可访问性**（字号下限、焦点样式、accessibleName）；
6. **主题选择流程**（下拉框替代路径字符串）；
7. 结构性议题（model/view、docking、pattern/clip 统一）——**单独立项，不进本分支**。

对应可执行清单见 `doc/ui-ux-tasks.md`。

---

## 附录 A：证据命令

```bash
# 颜色散落
grep -oE '#[0-9a-fA-F]{3,8}' data/themes/default/style.css | sort | uniq -c | sort -rn
grep -rn 'QColor\s*[{(]\s*[0-9]' src/gui | wc -l        # 36

# 尺寸硬编码
grep -rn 'setFixed\(Size\|Width\|Height\)' src/gui include | wc -l   # 96

# 字号
grep -rn 'adjustedToPixelSize' src/gui | wc -l      # 31
grep -n 'font-size' data/themes/default/style.css    # 14（px/pt 混用）

# 快捷键写死
grep -n 'Ctrl+[0-9]' src/gui/MainWindow.cpp          # 14

# 反馈面缺失
grep -rn 'statusBar\|showMessage' src/gui | wc -l        # 0

# 可访问性
grep -rn 'setAccessibleName' src/gui | wc -l             # 0
grep -c ':focus' data/themes/default/style.css           # 2
grep -rn 'Qt::NoFocus' src/gui | wc -l                   # 14

# 撤销上限
grep -n 'MAX_UNDO_STATES' src/core/ProjectJournal.cpp

# 主题加载时机
grep -n 'resources:style.css' src/gui/LmmsStyle.cpp
```

## 附录 B：主要文件索引

| 主题 | 文件 |
| --- | --- |
| 主窗口 / 工具栏 / 菜单 | `src/gui/MainWindow.cpp` |
| 子窗口（MDI） | `include/SubWindow.h`、`src/gui/SubWindow.cpp` |
| 主题样式 | `data/themes/default/style.css`、`src/gui/LmmsStyle.cpp` |
| 调色板 | `include/LmmsPalette.h`、`src/gui/LmmsPalette.cpp` |
| 字体常量 | `include/FontHelper.h` |
| 旋钮/滑块基类 | `include/FloatModelEditorBase.h`、`src/gui/widgets/FloatModelEditorBase.cpp` |
| 推子 | `src/gui/widgets/Fader.cpp` |
| 通用右键菜单 | `src/gui/AutomatableModelView.cpp` |
| 轨道头 | `src/gui/tracks/TrackOperationsWidget.cpp`、`src/gui/tracks/TrackView.cpp` |
| 混音器 | `src/gui/MixerView.cpp`、`src/gui/MixerChannelView.cpp`、`include/Mixer.h` |
| 钢琴卷帘 | `src/gui/editors/PianoRoll.cpp`、`include/PianoRoll.h` |
| 自动化编辑器 | `src/gui/editors/AutomationEditor.cpp` |
| 设置对话框 | `src/gui/modals/SetupDialog.cpp` |
| 撤销 | `src/core/ProjectJournal.cpp` |
