# UI/UX 可执行任务清单（低风险子集）

约束条件（本清单所有任务都必须满足）：

- 不改工程文件格式（`.mmp`/`.mmpz`）、不改插件 ABI、不改 `Track`/`Clip`/`Mixer` 的数据成员；
- 不重写 `MainWindow`/`SubWindow`、不引入 docking 框架、不引入 model/view；
- 每个任务可独立成 PR、可独立回滚；
- 单个 PR 建议 < 400 行 diff，涉及文件 < 8 个；
- 不改变已有快捷键的**绑定**（只改显示方式）。

对应的问题编号见 `doc/ui-ux-audit.md`。

---

## 汇总表

| ID | 任务 | 估时 | 风险 | 依赖 |
| --- | --- | --- | --- | --- |
| T0-1 | 修正 macOS 下菜单快捷键文案 | 0.5 h | 无 | — |
| T0-2 | 字号下限（7pt/8pt → 可读值） | 0.5 h | 无 | — |
| T0-3 | 修选中态对比度 | 0.5 h | 无 | — |
| T0-4 | 焦点可见性（`:focus` 样式） | 1 h | 低 | — |
| T0-5 | 补 icon-only 控件的 accessibleName | 2 h | 无 | — |
| T0-6 | 修复 Project Notes 的字符串快捷键绑定（macOS） | 1 h | 无 | — |
| T1-1 | 颜色收口到 `palette()`（36 处，分批） | 1–2 d | 低 | — |
| T1-2 | 旋钮/推子双击归位 + 修饰键提示 | 1 d | 中（行为变化） | 评审 |
| T1-3 | 滚轮语义对齐 | 0.5 d | 中（手感变化） | 评审 |
| T1-4 | 状态栏 | 1 d | 低 | — |
| T1-5 | 撤销步数可配置 | 0.5 d | 低 | — |
| T1-6 | 主题选择下拉框（含重启提示） | 1 d | 低 | — |
| T1-7 | 字号常量统一（px → 一致的缩放策略） | 1–2 d | 低 | T0-2 |
| T1-8 | 钢琴卷帘/自动化工具栏弹性宽度 | 1 d | 低 | — |
| T1-9 | 剩余 PNG → SVG（除 QSS 必需项） | 按图标计 | 低 | — |
| T1-10 | 轨道名被省略时补 tooltip | 0.2 d | 低 | — |
| T2-2 | 控件右键菜单加"在自动化编辑器中显示" | 2–3 d | 中（新建 clip 逻辑） | 评审 |
| T2-3 | 快捷键映射表（不含 UI） | 2–3 d | 中 | 评审 |

T0 全部可在一天内完成；T1 每项独立 PR；T2 需要先讨论。

---

## 实施记录（T0 第一批，已合入本分支）

| ID | 状态 | 验证方式 |
| --- | --- | --- |
| T0-1 | ✅ 已实现 | View 菜单截图（`doc/ui-ux-shots/t0-1-view-menu.png`），Linux 文案不变 |
| T0-2 | ✅ 已实现 | 前后截图 + 像素测量（见该节表格） |
| T0-3 | ✅ 已实现 | 选中行实测 `#0f5c2c`，对比度 3.82 → **5.66:1** |
| T0-4 | ✅ 已实现 | 独立 QSS 渲染验证（见下方说明），应用内无法截图验证 |
| T0-5 | ✅ 已实现 | 仅编译验证；需在真实桌面用 Orca/accerciser 复核 |
| T0-6 | ✅ 已实现 | 仅编译验证；Linux 绑定与改前相同（`QKeySequence::Undo` = Ctrl+Z） |
| T1-1 | 🚧 进行中 | `TrackView` 已改（截图逐像素对比 = 0 差异，主题覆盖测试通过） |
| T1-10 | ✅ 已实现 | 仅编译验证；tooltip 需在交互式桌面悬停复核 |
| T1-4 | ⏸️ 已实现但隐藏 | 状态栏 + 悬停提示可用，当前 `hide()` 掉，等提示内容完善后再启用 |
| T1-3 | ✅ 已实现 | Alt+滚轮实测 0.05 dB（普通滚轮 1 dB） |
| T1-5 | ✅ 已实现 | 12 → 10 经 UI 写入配置；缺省 100 已确认 |
| T1-6 | ✅ 已实现 | 下拉框列出两套主题；切到 classic 重启生效 |

### 实施中发现并修正的两个问题

1. **`QPushButton:focus { border-color: … }` 无效**：Qt 的 QSS 走 CSS2 特异性，`QPushButton#btn`（ID 选择器）压过 `QPushButton:focus`（类型 + 伪类）。改用 `outline` 属性（没有任何 ID 规则设置 outline），并用独立渲染测试确认按钮与树都能画出绿色描边。
2. **无窗口管理器时 `:focus` 永不触发**：截图环境是 Xephyr 裸 X 服务器，没有 WM 也就没有窗口激活，`QWidget::hasFocus()` 恒为 false（连原有的 `QLineEdit:focus` 绿边也不出现）。因此焦点样式在截图里看不到，这是环境限制；样式本身已用 `QT_QPA_PLATFORM=offscreen` 的独立渲染验证。

### T1-1 的进展与发现

已完成 `TrackView`：新增 `Q_PROPERTY(QColor trackBackground)`，默认值等于原来的硬编码值，`style.css` 里用
`qproperty-trackBackground: #202428;` 暴露给主题。

**发现：`FloatModelEditorBase::paintEvent` 是死代码。** 三个子类 `Knob`、`Fader`、`BarModelEditor`、`Draggable`
都各自覆盖了 `paintEvent`，且该类从不被直接实例化，所以审计报告里列出的
`FloatModelEditorBase.cpp:305` 的 `QColor{3, 94, 97}` 根本不会被绘制。已撤回针对它的改动，
改为一个独立的清理项：**删除该死方法**（不要为它加主题属性）。

**待定：`ProjectNotes`。** 它的背景（`#404040`）可以照 `TrackView` 的办法处理，但正文颜色（`#e0e0e0`）
是写进文档字符格式的，改了主题也不会影响已有内容。需要先决定"笔记正文是否应跟随主题文字色"，
再动手。

### T0-5 的实现方式（与最初方案不同）

原方案是"新增 helper，在几个按钮构造处调用"。实际改为在 `GuiApplication` 里安装一个应用级事件过滤器，监听 `QEvent::ToolTipChange`，把 tooltip 同步到 `accessibleName`（若为空）。这样 133 处已有 tooltip 全部自动获得可读名，不必逐个改调用点，新增控件也自动生效。

---

## T0：当天可完成

### T0-1 修正 macOS 下菜单快捷键文案

**现象**：`MainWindow.cpp:1047-1079` 使用 `tr("Song Editor") + "\tCtrl+1"` 写死文案；而实际绑定走 `keySequence(Qt::CTRL, Qt::Key_1)`（`MainWindow.cpp:435-459`），在 macOS 上是 ⌘。于是菜单显示 Ctrl+1，实际按 ⌘+1。同一文件 433–459 行的 ToolButton 文案也写死了 `(Ctrl+1)`。

**改动点**
- `src/gui/MainWindow.cpp` 新增文件内 helper：
  ```cpp
  QString withShortcutHint(const QString& text, int key, bool forMenu)
  {
      const auto sequence = QStringLiteral("%1+%2").arg(UI_CTRL_KEY).arg(key);
      return forMenu ? text + QLatin1Char('\t') + sequence
                     : text + QStringLiteral(" (%1)").arg(sequence);
  }
  ```
- 窗口按钮（`MainWindow.cpp` 构造里 7 处）用 `forMenu=false`，View 菜单（`updateViewMenu()` 里 7 处）用 `forMenu=true`。
- 补 `#include "KeyboardShortcuts.h"`。

**不要做**：不要给菜单 `QAction` 再 `setShortcut()` —— ToolButton 上已有相同快捷键，会产生 ambiguous shortcut 警告。

**验收**
- Linux：菜单仍显示 `Ctrl+1`，功能不变；
- macOS：菜单显示 `⌘+1`；
- `git diff` 中不出现新的可译字符串拼接快捷键（`tr()` 内无 `Ctrl`）。

---

### T0-2 字号下限

**现象**：`data/themes/default/style.css` 中有 7pt（`:794`、`:806`）与 8pt（`:45`）。

**先算清楚再改**（Qt 默认 96 DPI，1pt = 4/3 px）：

| 位置 | 现值 | 折算像素 | 10px 之后 |
| --- | --- | --- | --- |
| `:794` PeakIndicator | 7pt | **9.33 px** | 10 px（+0.67） |
| `:806` TimeLineWidget | 7pt | **9.33 px** | 10 px（+0.67） |
| `:45` FileBrowser 复选框 | 8pt | 10.67 px | 10 px（**−0.67，反而变小**） |

所以**只改前两处**；8pt 已经在 10px 之上，改成 10px 属于"为了统一单位而缩水"，属于不应有的视觉改动。若日后要统一单位，应写成 `11px`。

**改动点**：`data/themes/default/style.css`
- `:794` `lmms--gui--PeakIndicator` 7pt → `10px`
- `:806` `lmms--gui--TimeLineWidget` 7pt → `10px`（保留 `min-height/max-height: 1.5em`，高度随字号同比变化是原作者写在注释里的设计意图）
- `:45` **不动**

**已实测的排版影响**（1600×1000、100% 缩放、对比截图见 `doc/ui-ux-shots/`）：

| 指标 | 改前 | 改后 | 说明 |
| --- | --- | --- | --- |
| 混音器 `-inf` 字形 | 14 × 7 px | 16 × 9 px | 仍在通道内，无裁切 |
| 混音器通道分隔线位置 | — | 完全一致 | **通道宽度未变** |
| 时间轴标尺高度 | 19 px | 21 px | `1.5em` 随字号增长（+2px） |
| 时间轴下方轨道区起点 | y=149 | y=151 | 整体下移 2px |
| 标尺刻度文字 | 9 px 高 | 11 px 高 | 无重叠、无裁切 |

**验收**
- `grep -n "7pt\|8pt" data/themes/default/style.css` 无结果（8pt 那条应保留，见上表）；
- 混音器通道总宽度、通道数、效果链面板宽度与改前一致（截图叠加比对）；
- Song Editor / Pattern Editor / Piano Roll / Automation Editor 四个使用 `TimeLineWidget` 的编辑器，标尺文字都不裁切；
- 100%/125%/150% 缩放各看一次（pt 与 px 在 Qt 里都按逻辑 DPI 缩放，相对大小关系不变）。

---

### T0-3 选中态对比度

**现象**：`#d1d8e4` on `#17793b` = 3.82:1，低于 WCAG AA 4.5:1。

**改动点**：`#17793b` 在样式表里出现两处，都要改：
- `QTreeWidget::item:selected` / `:branch:selected` 的 `background-color`；
- 文本输入的 `selection-background-color`（`QTextEdit, QLineEdit:focus, QComboBox:focus, …`）。

**取值**：`#0f5c2c`（对 `#d1d8e4` = 5.66:1，对 `#ffffff` = 8.12:1）。

**验收**：用附录脚本计算 ≥ 4.5:1；文件浏览器选中项在深色下仍能区分于 hover 态（hover 为 `#3C444E`）。实测选中行像素 `rgb(15,92,44)`，对比度 5.66:1 ✅

---

### T0-4 焦点可见性

**现象**：1211 行样式表只有 2 条 `:focus` 规则；14 处 `Qt::NoFocus`。

**改动点**：`data/themes/default/style.css` 追加：

```css
QToolButton:focus,
QPushButton:focus { outline: 1px solid #0bd556; outline-offset: -2px; }

QTreeView:focus,
QListView:focus,
QTableView:focus { outline: 1px solid #0bd556; outline-offset: -1px; }
```

**必须用 `outline` 而不是 `border-color`**：Qt 的 QSS 遵循 CSS2 特异性，`QPushButton#btn`（ID 选择器）会压过 `QPushButton:focus`，改边框色对绝大多数按钮无效。`outline` 没有被任何 ID 规则设置，且不参与布局，不会造成位移。

**不要动**那 14 处 `NoFocus`（多数是有意的）。**不覆盖** QComboBox / QSpinBox：它们由 `LmmsStyle` 自绘，QSS 的 `:focus` 对它们无效，需要后续在 `LmmsStyle` 里处理。

**验收**：Tab 键遍历时焦点可见；`QLineEdit:focus` 的既有绿边保持不变。注意：无窗口管理器的截图环境里 `:focus` 永远不会触发（窗口未激活），必须在真实桌面复核。

---

### T0-5 补 icon-only 控件的 accessibleName

**现象**：全仓库 0 个 `setAccessibleName`，但已有 133 处 `setToolTip`。

**改动点**（与最初设想不同，实际用一个应用级事件过滤器覆盖全部控件）
- `src/gui/GuiApplication.cpp` 新增 `AccessibleNameSync`（匿名命名空间），监听 `QEvent::ToolTipChange`，把 tooltip 同步到空的 `accessibleName`。
- 在 `m_mainWindow = new MainWindow` 之前 `qApp->installEventFilter(...)`，之后创建的所有控件都能被覆盖；新增控件也自动生效，无需逐个改调用点。

```cpp
bool eventFilter(QObject* watched, QEvent* event) override
{
    if (event->type() == QEvent::ToolTipChange)
        if (auto* w = qobject_cast<QWidget*>(watched);
            w && w->accessibleName().isEmpty() && !w->toolTip().isEmpty())
            w->setAccessibleName(w->toolTip());
    return QObject::eventFilter(watched, event);
}
```

**验收**：`grep -rn setAccessibleName src/gui` 无直接调用（由过滤器统一设置）；用 `accerciser` 或 `orca` 抽查至少 3 个图标按钮能读出名称；hover tooltip 行为不变。

**注意**：`setAccessibleName` 不会反过来触发 `ToolTipChange`，不存在递归。

**不做**：不做完整的无障碍审计，不引入 QAccessible 自定义接口。

---

### T0-6 修复 Project Notes 的字符串快捷键绑定

**现象**：`src/gui/ProjectNotes.cpp:114-204` 共 12 处写成
`a->setShortcut( tr( "%1+Z" ).arg(UI_CTRL_KEY) )`。
`UI_CTRL_KEY` 是**显示用**修饰符（macOS 上是 `⌘`），而 `QKeySequence` 只认 `Ctrl/Shift/Alt/Meta` 这样的名字。Linux 上碰巧成立（"Ctrl+Z"），macOS 上大概率解析为空。

**改动点**：把这 12 处换成标准序列，例如：
```cpp
a->setShortcut(QKeySequence::Undo);          // 撤销
m_actionTextBold->setShortcut(keySequence(Qt::CTRL, Qt::Key_B));
```
（`keySequence()` 定义在 `include/DeprecationHelper.h:127`，`MainWindow` 已在用。）

**验收**：Linux 上快捷键功能不变；`git diff` 中 `ProjectNotes.cpp` 不再出现 `tr("%1+`；菜单里显示的快捷键由 Qt 自动渲染（macOS 显示 ⌘，不再需要手写）。

**实际映射**：Undo/Redo/Copy/Cut/Paste/Underline 用 `QKeySequence` 标准键；Bold/Italic/AlignLeft/AlignCenter/AlignRight/AlignJustify 用 `keySequence(Qt::CTRL, Qt::Key_*)`。改完后 `ProjectNotes.cpp` 不再使用 `KeyboardShortcuts.h`，该 include 已移除。

**说明**：此处只修绑定，不改 `UI_CTRL_KEY` 的用途——它仍然是 tooltip/文案里的正确工具。

---

## T1：小 PR（1–3 天）

### T1-1 颜色收口到 `palette()`

**现象**：`src/gui` 有 36 处硬编码 `QColor`，其中 `TrackView.cpp:63` 让亮色主题不可能实现。

**前提（已具备）**：`LmmsPalette` 已由 QSS 驱动并全局生效（`GuiApplication.cpp:113-120`），绘制代码可直接 `palette().color(...)` 或读 `LmmsStyle::s_palette`。

**分批建议**
1. `src/gui/tracks/TrackView.cpp:63`：轨道底色改为 `palette().color(QPalette::Base)`（或新增 `qproperty-trackBackground` 由 QSS 控制）。
2. `src/gui/ProjectNotes.cpp:58,92`：改用 `palette()`。
3. `src/gui/widgets/FloatModelEditorBase.cpp:305`：旋钮激活色改用 `LmmsStyle::s_palette->highlight()` 或新增 qproperty。
4. 其余 `Graph.cpp`、`Fader.cpp` 等逐个评估（有的颜色是语义色，如峰值红/黄，应保留但改为常量表）。

**验收**
- `grep -rn 'QColor\s*[{(]\s*[0-9]' src/gui | wc -l` 显著下降（目标 ≤ 10，且剩余均有注释说明为什么不是主题色）；
- 在 `default` 与 `classic` 两套主题下，轨道底色、笔记窗口、旋钮激活态都跟随主题变化（截图）。

**不做**：不在这一步做亮色主题；只保证"主题能控制这些颜色"。`plugins/` 下另有 113 处硬编码颜色，列为后续独立任务（`ui/theme-colors-plugins`），不并入本项。

---

### T1-2 双击归位 + 修饰键提示

**现象**：双击 = 弹模态输入框（`FloatModelEditorBase.cpp:500`、`Fader.cpp:128`）；归位只能右键（`AutomatableModelView.cpp:62`）；修饰键无任何说明。

**结论（已实现）**
- **不做**双击归位。中键已经能归位（`AutomatableModelView.cpp:173`），双击留给精确输入。
- **双击的模态框已改为就地编辑**：新增 `InlineValueEditor`，覆盖
  `FloatModelEditorBase::enterValue()`、`Fader::adjustByDialog()`、`AutomatableSlider::enterValue()`。
  Enter 提交、Esc 取消、失焦提交；推子仍以 dB 显示，滑块按整数显示。
- **不做静态 tooltip**：`FloatModelEditorBase` 的悬浮提示是实时数值（`floatingTextType()`），
  设成静态文案会把数值读数顶掉。手势说明放进文档 `doc/ui-ux-gestures.md`，不进控件。

**实测**（Xephyr + xwd）：
- 旋钮：双击 → 出现 `1.00` 的绿色输入框（`t1-2-knob-inline.png`）；输入 `0` → `t1-2-knob-typed.png`；
  Enter → 旋钮转到 0（`t1-2-knob-committed.png`）。
- 推子：双击 → 出现 `0.0`（dB）；输入 12 + Enter → 推子到 +12 dB（`t1-2-fader-inline.png`）。
- Esc 取消路径：代码路径为 `cancel()`，不做数值提交。

**风险**：行为变化。缓解：先在本分支试两周；`Ctrl+双击` 保留原行为，等于没有功能损失。

**验收**
- 双击任意旋钮/推子 → 回到初始化值，并有数值浮层；
- `Ctrl+双击` → 仍能输入精确值；
- tooltip 在 Linux 与 macOS 上分别显示 `Ctrl` / `⌘`。

---

### T1-3 滚轮语义对齐（已实现）

**现象**：`Fader::determineAdjustmentDelta` 对 `Alt` 返回 0（完全不响应），而旋钮用 Alt 做"最细"。

**实现**
- Alt 档从 `0` 改为 **0.05 dB**（比 Ctrl 的 0.1 dB 细一倍，与旋钮"Alt 比 Ctrl 更细"的比例一致）。
- 同时把旋钮里已有的 Alt 兼容逻辑搬到推子：某些系统会把 Alt+滚轮上报成水平滚动（`angleDelta().x()`），
  `Fader::wheelEvent` 现在在 `y == 0` 时回退到 `x`，否则方向和幅度都会丢。
- 状态栏文案同步加上 `Alt+wheel finest`。

**实测**（Xephyr，抓值提示窗口读取模型值）：
- 普通滚轮一格：`-4.04 dB` → 上一步基准 = -5.04 dB，即 **+1 dB**。
- `Alt`+滚轮一格：`-4.04 → -3.99 dB`，即 **+0.05 dB**，不再是 0。

**没有做**：滑块（`AutomatableSlider`）仍然不吃修饰键——它走 `QSlider` 默认实现，
需要单独改造，见矩阵里的第 4 条。

---

### T1-4 状态栏（已实现，含手势提示）

**现象**：无状态栏，长任务与操作结果无承载点（`grep statusBar src/gui` = 0）。

**实现**
- `MainWindow` 构造里启用 `statusBar()`（`MainWindow` 本来就是 `QMainWindow`，central widget 不变，只是高度少 20px），关闭 size grip。
- 新增两个槽：`showStatusHint(text)` / `clearStatusHint(text)`。后者只有在"当前提示仍是自己"时才清空，
  避免从控件 A 移到控件 B 时出现闪烁。
- 三类数值控件在 `enterEvent`/`leaveEvent` 里调用它，文案按控件族区分（见下）。
- `guiSaveProjectAs()` 成功后在状态栏显示 `Saved: <path>`，4 秒后消失。

**为什么用状态栏而不是 tooltip**：`FloatModelEditorBase` 的悬停提示是**实时数值**
（`floatingTextType()` 在悬停时返回 `Dynamic`），设成静态文案会把数值读数顶掉。
状态栏与浮层在空间上分开：浮层给值，状态栏给"能做什么"。

**文案**

| 控件 | 提示 |
| --- | --- |
| 旋钮（`FloatModelEditorBase`） | Drag to adjust · Shift+wheel coarse · Ctrl+wheel fine · Middle-click resets · Double-click to type a value |
| 推子（`Fader`） | Drag to adjust · Wheel 1 dB · Shift+wheel coarse · Ctrl+wheel fine · Middle-click resets · Double-click to type a value |
| 工具栏滑块（`AutomatableSlider`） | Drag to adjust · Wheel steps · Double-click to type a value |

修饰键文案用 `UI_CTRL_KEY`，macOS 上显示 ⌘。

**实测**（`doc/ui-ux-shots/t1-4-status-hints.png`）：悬停旋钮/推子/滑块分别显示对应文案，
移开后清空；整窗截图见 `t1-4-status-bar.png`。

**现状：已隐藏。** 这一版的状态栏只有手势提示和保存消息，功能还不完整，
所以在 `MainWindow` 构造里调用 `statusBar()->hide()`，让主窗口布局回到加状态栏之前。
代码与文案全部保留，去掉那一行 `hide()` 即可恢复。

**不做**：不改 `TextFloat`，不改导出对话框；加载完成提示留待以后。

---

### T1-5 撤销步数可配置（已实现）

**现象**：`ProjectJournal.cpp` 写死 `MAX_UNDO_STATES = 100`，源码注释本来就有
`// TODO: make this configurable in settings`。

**实现**
- `ProjectJournal` 去掉静态常量，改为 `DefaultUndoStates/MinUndoStates/MaxUndoStates`
  三个常量和成员 `m_maxUndoStates`；构造时从 `ui/undosteps` 读取（缺省 100），
  用 `std::clamp` 限到 [10, 1000]。
- 新增 `setMaxUndoStates()`：设值时立即裁剪已有历史，所以调小不需要等下一次操作。
- `SetupDialog` General 页 GUI 分组新增 "Undo steps" `QSpinBox`（范围取自同一组常量），
  在 `accept()` 里写回配置并对当前 `ProjectJournal` 立即生效。

**实测**
- 配置里写 `undosteps="12"` 启动 → 对话框显示 **12**（`t1-5-undo-steps.png`）。
- 点两次下箭头（12 → 10）→ 点 OK → 正常退出后配置文件为 `undosteps="10"`。
- 配置里没有这个键 → 日志确认日志实例取到 **100**（默认值未变）。
- 超出范围由 `std::clamp` 处理（5 → 10，5000 → 1000）。

**限制**：对话框里的键盘输入在这个无窗口管理器的截图环境里进不去，所以"输入框直接打字"
这一步没做端到端测试，改用鼠标点上下箭头完成同样的写入验证。

---

### T1-6 主题选择下拉框（已实现）

**现象**：主题 = Paths 页里填目录字符串，用户得知道主题装在哪，且必须重启（`LmmsStyle.cpp:139`）。

**实现**
- Paths 页的 "Theme directory" 文本框换成 `QComboBox` + 浏览按钮：
  - 枚举 `data:/themes` 下含 `style.css` 的子目录（当前仓库里是 `classic`、`default`）；
  - 当前配置的主题如果不在其中（自定义目录），额外加一项；
  - 选中后调用 `ConfigManager::setThemeDir()` 并显示既有的 "Some changes require restarting." 提示。
- 浏览按钮走原来的 `FileDialog`，选中的目录会加入下拉框并选中。
- 删掉了不再使用的 `m_themeDirLineEdit`。

**实测**
- 下拉框列出 `classic`、`default`（`t1-6-theme-combo.png`），选中后出现重启提示；
- 选 `classic` → OK → 退出后配置写入 `theme="data:/themes/classic/"`；
- 用该配置重启，界面切换为 classic 主题（浅色、旧蓝色 clip，`t1-6-classic-theme.png`）。

**兼容性**：配置里存的仍是目录路径，旧的 `paths/theme` 配置可直接读。

**不做**：不做运行时热切换（涉及 `resources:` 搜索路径与已加载 pixmap，属于另一项）。

---

### T1-7 字号常量统一

**现象**：`FontHelper.h:31-33` 的像素常量 + 31 处 `adjustedToPixelSize` + 14 处 QSS `font-size`（px/pt 混用）+ C++ 字符串里的 `"font-size: 10px;"`。

**改动点**
1. `FontHelper.h`：给出一个按 DPI/字体缩放的取值函数（`scaledFontSize()`），常量保留为"基准像素"。
2. `InstrumentTrackWindow.cpp:130`：把 `labelStyleSheet` 里的 `font-size: 10px` 删掉，改用对象名 + QSS 统一控制。
3. `style.css`：px/pt 统一为 px（Qt 的 px 是 device-independent，缩放策略更可预测）。

**验收**：`grep -rn 'font-size' src/gui` 剩余条目均有理由；在 `QT_SCALE_FACTOR=1.5` 下控件文字不重叠。

---

### T1-8 钢琴卷帘/自动化工具栏弹性宽度

**现象**：`PianoRoll.cpp:5337-5398` 八个固定像素宽度下拉框；`AutomationEditor.cpp` 同类。

**改动点**：`setFixedSize(w, h)` → `setMinimumWidth(w)` + 保留高度；或实现 `sizeHint()` 取 `max(当前宽度, 最长条目的 sizeHint)`。

**验收**：`LANG=de_DE` 或 `LANG=zh_CN` 下，工具栏文字不截断、无省略号；窗口缩到 1024 宽时工具栏仍可用（溢出按钮已有，见 `b324ca449`）。

---

### T1-9 剩余 PNG → SVG

**现象**：`data/themes/default/` 仍有 78 个 PNG。

**改动点**
- 候选（图标类）：`black_key*.png`、`combobox_bg.png`、`horizontal_slider.png`、`cursor_select_*.png`、`clip_rec.png`、`effect_plugin.png`、`envelope_graph.png`。
- **必须保留**：`apply_css.png`、`apply-selected_css.png`（QSS 菜单指示图标，见提交 `74bac7745` 的说明：Qt 样式表在这些位置无法可靠使用 SVG）；`closed_branch.png`、`open_branch.png` 仅被 `QTreeView::branch` 的 `border-image` 引用，转换前需先验证 `border-image` 对 SVG 的缩放行为。
- 每次 PR 转 5–10 个，改动 `data/themes/CMakeLists.txt` 的安装清单。

**验收**：删除对应 PNG 后 `embed::getIconPixmap()` 仍能找到（`embed.cpp` 已实现优先取 SVG）；在 200% 缩放下无模糊。

---

### T1-10 轨道名被省略时补 tooltip

**现象**：`TrackLabelButton::elideName()`（`src/gui/tracks/TrackLabelButton.cpp:225-236`）只在 `maxTextWidth < 1` 时设置 tooltip，文本**被省略**的分支反而调用 `setToolTip("")`，用户拿不到完整轨道名。

**改动点**：改为"只要 `elidedName != name` 就设 tooltip"，即：
```cpp
const QString elidedName = metrics.elidedText(name, Qt::ElideRight, maxTextWidth);
setToolTip(elidedName == name ? QString() : name);
```
（`maxTextWidth < 1` 的早退分支保留即可，或合并进同一逻辑。）

**验收**：把一条轨道改名为超长中文名并拖窄轨道头 → 悬停显示完整名称；名字未被省略时无 tooltip。

---

## T2：需要先评审

### T2-2 控件右键菜单加"在自动化编辑器中显示"

现状：必须"加自动化轨 → 双击 clip → 下拉框找模型"。理想：右键旋钮 → 在该轨新建 `AutomationClip` 并打开编辑器。**风险**：涉及 `AutomationTrack::createClip` 与模型绑定，属于行为面扩展，需设计确认（是否自动创建轨道/是否复用已有 clip）。

### T2-3 快捷键映射表（不含 UI）

把硬编码 `keySequence()` 收进一张 `QMap<QString, QKeySequence>`，配置文件持久化。**不做**可视化编辑器（那是独立项目）。风险：需要迁移所有 `setShortcut` 调用点。

---

## 明确不做（本期）

| 事项 | 原因 |
| --- | --- |
| 停靠面板 / 分屏 / 布局保存 | 需要重写 `MainWindow` + 所有编辑器的容器假设 |
| model/view 重构 | 触及轨道、混合器、时间线的数据层 |
| Pattern / Clip / Automation 概念合并 | 涉及工程文件格式与引擎 |
| 亮色主题 | 依赖 T1-1 全部完成，且需要重新设计 220 个 SVG 的明度 |
| 完整快捷键设置界面 | 先做 T2-3 的映射表，UI 另立项 |
| 无障碍完整审计（QAccessible 自定义） | 成本高于收益，先做 T0-5 |

---

## 回归验证脚本

每个 PR 合并前跑一遍（不要求全部通过，只要求"没有变差"）：

```bash
# 1. 硬编码颜色不应增加
grep -rn 'QColor\s*[{(]\s*[0-9]' src/gui | wc -l

# 2. 固定尺寸不应增加
grep -rn 'setFixed\(Size\|Width\|Height\)' src/gui include | wc -l

# 3. 字号下限
grep -nE 'font-size:\s*[0-9]+pt' data/themes/default/style.css

# 4. 快捷键不应出现在可译文本里
grep -rn 'tr([^)]*Ctrl+' src/gui

# 5. 焦点与可访问性
grep -c ':focus' data/themes/default/style.css
grep -rn 'setAccessibleName' src/gui | wc -l

# 6. 视觉抽查（三档缩放 × 两种语言）
QT_SCALE_FACTOR=1.0 LANG=en_US.UTF-8 ./build/lmms
QT_SCALE_FACTOR=1.5 LANG=zh_CN.UTF-8 ./build/lmms
QT_SCALE_FACTOR=2.0 LANG=de_DE.UTF-8 ./build/lmms
```

对比度计算（T0-3 验收用）：

```bash
python3 - <<'EOF'
def lum(h):
    h=h.lstrip('#'); r,g,b=[int(h[i:i+2],16)/255 for i in (0,2,4)]
    f=lambda c: c/12.92 if c<=0.03928 else ((c+0.055)/1.055)**2.4
    return 0.2126*f(r)+0.7152*f(g)+0.0722*f(b)
def cr(a,b):
    la,lb=lum(a),lum(b); hi,lo=max(la,lb),min(la,lb)
    return round((hi+0.05)/(lo+0.05),2)
print(cr('#d1d8e4','#0f5c2c'))
EOF
```

---

## 建议的 PR 顺序

1. `ui/tokens-and-contrast` —— T0-1 ~ T0-4（纯样式与文案，零风险）
2. `ui/accessibility-labels` —— T0-5 + T0-6
3. `ui/theme-colors` —— T1-1（分 3 个 commit）
4. `ui/control-gestures` —— T1-2 + T1-3
5. `ui/status-bar` —— T1-4
6. `ui/settings` —— T1-5 + T1-6
7. `ui/font-scaling` —— T1-7
8. `ui/toolbar-layout` —— T1-8 + T1-10
9. `ui/svg-cleanup` —— T1-9（持续）

前两个 PR 合起来就能让"这软件很旧"的第一印象明显变化，且不触碰任何底层结构。
