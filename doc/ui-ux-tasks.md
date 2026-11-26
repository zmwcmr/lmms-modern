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
| T1-1 | 硬编码颜色主题化（分批） | 1–2 d | 低 | — |
| T1-2 | 数值就地编辑 + 手势说明 | 1 d | 中（行为变化） | 评审 |
| T1-3 | 滚轮语义对齐 | 0.5 d | 中（手感变化） | 评审 |
| T1-4 | 状态栏 | 1 d | 低 | — |
| T1-5 | 撤销步数可配置 | 0.5 d | 低 | — |
| T1-6 | 主题选择下拉框（含重启提示） | 1 d | 低 | — |
| T1-7 | 字号常量统一（px → 一致的缩放策略） | 1–2 d | 低 | T0-2 |
| T1-8 | 钢琴卷帘/自动化工具栏弹性宽度 | 1 d | 低 | — |
| T1-9 | 剩余 PNG → SVG（除 QSS 必需项） | 按图标计 | 低 | — |
| T1-10 | 轨道名被省略时补 tooltip | 0.2 d | 低 | — |
| T1-11 | 标准对话框/输入框图标主题化 | 0.5 d | 低 | — |
| T2-2 | 控件右键菜单加"在自动化编辑器中显示" | 2–3 d | 中（新建 clip 逻辑） | 评审 |
| T2-3 | 快捷键映射表（不含 UI） | 2–3 d | 中 | 评审 |

T0 全部可在一天内完成；T1 每项独立 PR；T2 需要先讨论。

---

## 实施记录（截至 `6fa1ce7b8`）

下表的“已实现”表示代码已提交到当前分支，不代表所有平台和交互验收均已完成。
T0 六项与 T1-3、T1-5～T1-8、T1-10、T1-11 已实现；T1-1、T1-2 和 T1-9 部分完成，
T1-4 已实现但隐藏，T2-2 和 T2-3 尚待设计评审。

| ID | 状态 | 验证方式 |
| --- | --- | --- |
| T0-1 | ✅ 已实现 | View 菜单截图（`doc/ui-ux-shots/t0-1-view-menu.png`），Linux 文案不变 |
| T0-2 | ✅ 已实现 | 前后截图 + 像素测量（见该节表格） |
| T0-3 | ✅ 已实现 | 选中行实测 `#0f5c2c`，对比度 3.82 → **5.66:1** |
| T0-4 | ✅ 已实现 | 独立 QSS 渲染验证（见下方说明），应用内无法截图验证 |
| T0-5 | ✅ 已实现 | 仅编译验证；需在真实桌面用 Orca/accerciser 复核 |
| T0-6 | ✅ 已实现 | 仅编译验证；Linux 绑定与改前相同（`QKeySequence::Undo` = Ctrl+Z） |
| T1-1 | 🚧 进行中 | `TrackView` 已改（截图逐像素对比 = 0 差异，主题覆盖测试通过） |
| T1-2 | 🚧 验证中 | Xephyr 已抽查旋钮、推子和工具栏滑块的提交、取消、失焦，以及音量旋钮的边界和负幅值；真实桌面及其他场景仍需复核 |
| T1-10 | ✅ 已实现 | 仅编译验证；tooltip 需在交互式桌面悬停复核 |
| T1-4 | ⏸️ 已实现但隐藏 | 状态栏 + 悬停提示可用，当前 `hide()` 掉，等提示内容完善后再启用 |
| T1-3 | ✅ 已实现 | Alt+滚轮实测 0.05 dB（普通滚轮 1 dB） |
| T1-5 | ✅ 已实现 | 12 → 10 经 UI 写入配置；缺省 100 已确认 |
| T1-6 | ✅ 已实现 | 下拉框列出两套主题；切到 classic 重启生效 |
| T1-7 | ✅ 已实现 | 10 pt → 13/11/16 px；100%/150% 截图无重叠 |
| T1-8 | ✅ 已实现 | de_DE 1600 宽全部完整；1024 宽溢出菜单可用 |
| T1-9 | 🚧 进行中 | 第一批 4 个：`black_key`×3 + `combobox_bg` |
| T1-11 | ✅ 已实现 | QMessageBox / QInputDialog 图标已统一（`t1-11-dialog-icons.png`） |

**后续回归重点**：在真实桌面复核焦点描边、长轨道名 tooltip、数值编辑的取消/失焦，
并用 Orca 或 accerciser 抽查可访问名称；补充不同缩放比例、语言和 macOS 的验证。
这些是待验证项，以上历史截图和测试结果不等同于本次重新运行。

当前构建目录的 12 个测试注册在 `build-ui/tests`，可用
`ctest --test-dir build-ui/tests --output-on-failure` 运行；从 `build-ui` 根目录执行
`ctest -N` 会显示 0 个测试。2026-11-26 使用 `QT_QPA_PLATFORM=offscreen` 运行，12/12 通过；
这些测试不能替代真实桌面的交互和视觉验收。

### 实施中发现并修正的两个问题

1. **`QPushButton:focus { border-color: … }` 无效**：Qt 的 QSS 走 CSS2 特异性，`QPushButton#btn`（ID 选择器）压过 `QPushButton:focus`（类型 + 伪类）。改用 `outline` 属性（没有任何 ID 规则设置 outline），并用独立渲染测试确认按钮与树都能画出绿色描边。
2. **无窗口管理器时 `:focus` 永不触发**：截图环境是 Xephyr 裸 X 服务器，没有 WM 也就没有窗口激活，`QWidget::hasFocus()` 恒为 false（连原有的 `QLineEdit:focus` 绿边也不出现）。因此焦点样式在截图里看不到，这是环境限制；样式本身已用 `QT_QPA_PLATFORM=offscreen` 的独立渲染验证。

### T1-1 的进展与发现

已完成 `TrackView`：新增 `Q_PROPERTY(QColor trackBackground)`，默认值等于原来的硬编码值，`style.css` 里用
`qproperty-trackBackground: #202428;` 暴露给主题。

**发现：`FloatModelEditorBase::paintEvent` 在仓库内没有已知调用路径。** 子类 `Knob`、`Fader`、`BarModelEditor`、`Draggable`
各自覆盖了 `paintEvent`，该类在仓库内也没有直接实例化。已撤回对其颜色的改动；
由于该类已导出，外部插件可能继承它，删除方法之前需要评估兼容性。

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

### T1-1 硬编码颜色主题化（进行中）

**现象**：`src/gui` 有 36 处硬编码 `QColor`，其中 `TrackView.cpp:63` 让亮色主题不可能实现。

**前提（已具备）**：`LmmsPalette` 已由 QSS 驱动并全局生效（`GuiApplication.cpp:113-120`），绘制代码可直接 `palette().color(...)` 或读 `LmmsStyle::s_palette`。

**分批建议**
1. 已完成：`TrackView` 轨道底色通过 `qproperty-trackBackground` 由 QSS 控制。
2. 待决定：`ProjectNotes` 的正文颜色是否跟随主题，再评估背景和正文的处理方式。
3. 仓库内子类均覆盖 `FloatModelEditorBase::paintEvent`，不应为该实现新增主题属性；该类已导出，删除方法前需评估外部插件继承和 ABI 兼容性。
4. 其余 `Graph.cpp`、`Fader.cpp` 等逐个评估（有的颜色是语义色，如峰值红/黄，应保留但改为常量表）。

**验收**
- `grep -rn 'QColor\s*[{(]\s*[0-9]' src/gui | wc -l` 显著下降（目标 ≤ 10，且剩余均有注释说明为什么不是主题色）；
- 在 `default` 与 `classic` 两套主题下，截图验证已纳入范围的颜色随主题变化；笔记正文的预期需先确定，旋钮激活态需先确认实际绘制位置。

**不做**：不在这一步做亮色主题；只保证"主题能控制这些颜色"。`plugins/` 下另有 113 处硬编码颜色，列为后续独立任务（`ui/theme-colors-plugins`），不并入本项。

---

### T1-2 数值就地编辑 + 手势说明（已实现）

**基线现象**：双击弹出模态输入框；中键或右键菜单可归位，但中键手势不易发现；修饰键缺少说明。

**结论（已实现）**
- **不做**双击归位。中键已经能归位（`AutomatableModelView.cpp:173`），双击留给精确输入。
- **部分双击模态框已改为就地编辑**：新增 `InlineValueEditor`，覆盖
  `FloatModelEditorBase::enterValue()`、`Fader::adjustByDialog()`、`AutomatableSlider::enterValue()`。
  Enter 提交、Esc 取消、失焦提交；推子仍以 dB 显示，滑块按整数显示。
- `VolumeKnob::enterValue()` 也已改为就地编辑，保留 dBFS 换算、原幅值符号和输入范围。
- **不做静态 tooltip**：`FloatModelEditorBase` 的悬浮提示是实时数值（`floatingTextType()`），
  设成静态文案会把数值读数顶掉。手势说明放进文档 `doc/ui-ux-gestures.md`，不进控件。

**实测**（Xephyr + xwd）：
- 旋钮：双击 → 出现 `1.00` 的绿色输入框（`t1-2-knob-inline.png`）；输入 `0` → `t1-2-knob-typed.png`；
  Enter → 旋钮转到 0（`t1-2-knob-committed.png`）。
- 推子：双击 → 出现 `0.0`（dB）；输入 12 + Enter → 推子到 +12 dB（`t1-2-fader-inline.png`）。
- Esc 取消路径：代码路径为 `cancel()`，不做数值提交。
- 2026-11-26 Xephyr：轨道声像旋钮双击出现就地输入，输入 5 并按 Enter 后再次打开显示 5.0；轨道音量旋钮仍显示模态框。本次取消/失焦检查未完成。
- 后续修改后重新构建：轨道音量旋钮双击显示就地输入，输入 -6 并按 Enter 后再次打开显示 -6.0；Esc 检查时隔离会话中断，取消、失焦、边界和负幅值尚待复核。
- 继续在 Xephyr 中抽查轨道音量旋钮：输入 3 后按 Esc，再打开仍为 -6.0 dBFS；输入 -12 后点击别处，再打开显示 -12.0；输入 -99 / 99 分别限制为 -96.0 / 6.0；中键后恢复 0.0。应用在 Esc 后仍运行。负幅值模型及其他控件的回归仍待验证。
- 轨道声像旋钮：输入 5 并提交后，输入 8 再按 Esc，重新打开仍显示 5.0。此前负幅值自动化测试尝试因图形应用初始化依赖未能运行；后续改用以下交互测试。
- 2026-11-26 在隔离的 Xephyr 会话中，以临时工程加载 Flanger，读取保存的工程值验证反馈旋钮：初始 -0.5 显示 -6.0206 dBFS；输入 -12 并提交得到 -0.25118861；输入 -3 后 Esc 仍为 -0.25118861；输入 -6 后失焦得到 -0.50118721；输入 99 和 -99 分别限制为 -1 和 0。
- 同一会话中，混音器主推子输入 -6 dB 后保存为 0.50118721；输入 -12 后 Esc 保持原值，再输入 -12 并失焦后保存为 0.25118861。工具栏主音高滑块确认 Enter 提交、Esc 取消和点击可聚焦控件后的失焦提交；双击滑块轨道可能先按原有 QSlider 行为改变一格，因此取消测试从滑块手柄上双击开始。
- 本次重新构建成功，`QT_QPA_PLATFORM=offscreen ctest --test-dir build-ui/tests --output-on-failure` 为 12/12 通过。上述交互测试仅覆盖列出的控件和场景，不替代真实桌面与其他平台验证。

**风险**：行为变化，需在本分支继续试用。双击处理不区分 Ctrl 修饰键；支持就地编辑的控件上，
`Ctrl+双击` 也进入就地编辑，并不会恢复原模态框。

**验收**
- 双击旋钮、推子或工具栏滑块 → 出现就地输入框；Enter 提交，Esc 取消，失焦提交；
- 旋钮和推子中键归位；推子按 dB、滑块按整数输入；
- 手势说明以 `doc/ui-ux-gestures.md` 为准；状态栏提示目前隐藏，不能作为当前可见功能验收。

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

### T1-7 字号常量统一（已实现）

**现象**：`FontHelper.h:31-33` 的像素常量 + 41 处 `adjustedToPixelSize`（`src/gui` 25、`plugins` 16）+ QSS 里 px/pt 混用 + 两个 C++ 字符串里的 `"font-size: 10px;"`。

**实现**
1. `include/FontHelper.h` 新增 `fontScaleFactor()` / `scaledFontSize()`：
   - 基准是 9 pt（96 DPI 下即 12 px），因子 = 应用字体磅值 / 9；
   - 限制在 `[1.0, 1.5]`：不把界面缩得比设计值更小，也避免在固定高度的布局里放大到互相重叠；
   - 平台把字体指定为像素（没有磅值）时，回退到 `QFontInfo::pixelSize() / 12`。
   - `adjustedToPixelSize()` 内部改为 `setPixelSize(scaledFontSize(size))`，因此 41 处调用点（含插件）不用逐个修改就跟随系统字体。
2. **不缩放**由控件几何推导出的字号。这些值已经随布局缩放，再乘一次会双重放大：
   - `PianoRoll` 音符名（`noteTextHeight`）与键名（`keyLineHeight * 0.8`）；
   - `PianoView` 的八度标签（固定 8 px，10 px 宽的琴键放不下更大的字）；
   - `LfoGraph` 的 Hz 文本（`height() * 0.2`）；
   - `TabWidget`（标签栏固定 14/17 px，`ControllerConnectionDialog` 还把输入框绝对定位在它下方）。
   这些位置改为直接 `QFont::setPixelSize()` 并加注释说明原因。
3. **固定像素底图上的标签也不缩放**：`LcdWidget` / `LcdFloatSpinBox` 的 TEMPO/MIN/SEC/MSEC 等标签、`Oscilloscope` 的 "Click to enable" 保持 12 px。底图（LCD 数字、示波器面板）本身不随系统字体变大，标签单独放大会压过数字，看起来反而失衡。
4. 为放大后的文字补足固定布局空间：
   - `Knob` legacy 标签区：`+10` → `+ scaledFontSize(SMALL_FONT_SIZE)`；
   - `EffectView` 的 "Controls" 按钮：按字体 metrics 计算宽高，保持原来的右下角锚点。
5. `InstrumentTrackWindow` 与 `SampleTrackWindow` 删除 `labelStyleSheet = "font-size: 10px;"`，6 + 3 个标签改用 `setObjectName("trackSettingsLabel")`；两套主题的 `style.css` 增加同名规则统一控制。
6. QSS 单位统一为 px：default 的 `8pt` → `11px`（原值约 10.7 px）；classic 的 `8pt` → `11px`，`7pt` → `10px`（T0-2 当时只处理了 default 主题）。现在两套主题的 `font-size` 全部是 px。

**Qt QSS 限制（实测）**：`font-size: 1.2em` 和 `font-size: 120%` 都被 Qt 忽略，QSS 只接受 px/pt。所以 QSS 里的字号仍是固定 px，跟随系统字体的是 C++ 绘制的文字。要让 QSS 也动态缩放，需要运行时生成样式表，另立项。

**实测**
- 当前测试会话（Xephyr `:5`，10 pt）→ 因子 1.11：12/10/14 → **13/11/16 px**；11 pt → 1.22；12 pt → 1.33；13.5 pt 以上封顶 1.5。
- 100%：主窗口、Piano Roll、Settings 截图见 `doc/ui-ux-shots/t1-7-*.png`，无重叠或裁切；Settings 使用原生 Qt 控件，字号保持应用字体不变（符合预期）。
- 固定底图上的标签保持 12 px（TEMPO/MIN/SEC/MSEC/TIME SIG、Click to enable），不会压过 LCD 数字；其余 C++ 文本按上面的因子放大。
- 150%：`t1-7-hidpi-150.png`（2400×1500 屏幕，`QT_SCALE_FACTOR=1.5`），文字和布局同比放大，主窗口无重叠。
- `grep -rn 'font-size' src/gui` = **0**；`grep -nE 'font-size:\s*[0-9]+pt' data/themes/*/style.css` = **0**。
- 12 个测试可执行文件全部通过；完整构建（含插件）通过。

**不做**：不做 QSS 字号动态化；不做真正弹性的固定高度控件（属于 T1-8）；不改插件内嵌 UI 自身的字号策略。

---

### T1-8 钢琴卷帘/自动化工具栏弹性宽度（已实现）

**现象**：`PianoRoll.cpp` 八个固定像素宽度下拉框（64/64/85/105/72/155/125/96）；`AutomationEditor.cpp` 同类三个（80/80/60）。`ComboBox` 是自绘控件，没有 `sizeHint()`，所以只能写死宽度。

**实现**
- `ComboBox` 增加内容宽度支持：`sizeHint()` 遍历模型取最长条目，加上箭头按钮（18 px）、左右边距和可选的 16 px 图标宽度。默认仍返回 `QWidget::sizeHint()`，只有显式启用后才按内容计算，避免改变其它布局里的下拉框。启用状态用动态属性记录，不新增成员，类布局保持不变（不影响已编译插件的 ABI）。
- 新增 `setMinimumContentWidth(designWidth)`：启用内容宽度，取 `max(designWidth, sizeHint().width())` 作为最小宽度，并把水平 size policy 设为 `Fixed`。`QToolBar` 因此不能再把控件压到文字以下；空间不够时把控件移进已有的溢出菜单（`b324ca449`）。
- `PianoRoll` 的 8 个下拉框、`AutomationEditor` 的 3 个下拉框：`setFixedSize(w, h)` → `setFixedHeight(h)` + `setMinimumContentWidth(w)`。设计宽度保留为下限，长译文只会让控件变宽。

**实测**
- `LANG=de_DE.UTF-8`、1600×1000：`Letzte Note`、`Keine Tonart`、`Keine Tonleiter`、`Kein Akkord` 全部完整显示，无省略号（`t1-8-piano-roll-de.png`）。
- 同一语言、1024×768：`Letzte Note` 仍完整，放不下的 `Nudge` 自动进入溢出菜单，工具栏可用（`t1-8-piano-roll-1024-de.png`）。
- 自动化编辑器同语言截图见 `t1-8-automation-de.png`。

**不做**：Song Editor / Pattern Editor 的下拉框仍用固定宽度；插件控制对话框用绝对坐标，属于插件自身布局，不并入本项。

---

### T1-9 剩余 PNG → SVG（第一批）

**现象**：`data/themes/default/` 原有 78 个 PNG。

**第一批（已实现）**
- `black_key` / `black_key_pressed` / `black_key_disabled` → SVG。按原 10×37 像素图的逐行渐变与 1 px 边框重绘，保留左右透明边和底部两行高光/描边。
- `combobox_bg` → SVG（50×20 线性渐变）。仍作为 `QPixmap` 画刷使用，没有 tiling 或拉伸差异。
- 删除对应 4 个 PNG；`embed::getIconPixmap()` 已优先查找同名 SVG（`embed.cpp:99-103`），安装清单用通配符，无需改动 CMake。
- 与原 PNG 的 1× 渲染对比（Inkscape 光栅化后 RMSE）：`black_key` 0.89%，`black_key_pressed` 3.29%，`black_key_disabled` 0.47%，`combobox_bg` 0.25%。
- 运行截图 `t1-9-piano-roll.png`：键盘黑键与工具栏下拉框显示正常；删除 PNG 后启动日志无 "Error loading icon pixmap"。

**未转换（原因）**
- `horizontal_slider`：被 QSS `border-image` 引用（`style.css:598`），需先验证 `border-image` 对 SVG 的缩放行为。
- `cursor_select_left/right`：灰度光标掩码，需在真实桌面验证 `QCursor` 的透明与热点。
- `clip_rec` / `envelope_graph`：含像素字体，SVG `<text>` 依赖系统字体，需先转成路径。
- `effect_plugin`：214×57 圆角渐变 + 半透明边框，比第一批复杂，后续单独处理。
- **必须保留**：`apply_css.png`、`apply-selected_css.png`（QSS 菜单指示图标，见 `74bac7745`）；`closed_branch.png`、`open_branch.png` 仅被 `QTreeView::branch` 的 `border-image` 引用。

**验收**：删除对应 PNG 后 `embed::getIconPixmap()` 仍能找到；Piano Roll 键盘与下拉框显示正常；SVG 由 `embed` 的 2× oversampling 光栅化，不随 DPR 模糊。

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

### T1-11 标准对话框/输入框图标主题化（已实现）

**现象**：`QMessageBox` / `QInputDialog` 的图标来自系统图标主题（蓝色菱形问号、红圈 Cancel、Oxygen 风格 Save/OK），与 LMMS 的扁平白色图标不一致。

**实现**
- `GuiApplication` 新增 `StandardDialogIconSync` 事件过滤器，监听 `QEvent::Show`：
  - `QMessageBox`：按 `icon()` 把 Question → `help.svg`、Warning/Critical → `error.svg`、Information → 新增的 `info.svg`，以 48×48 光栅化；
  - 标准按钮映射：Ok/Yes → `apply`，Save → `project_save`，Discard → `discard`，Cancel/Close/No/Abort → `cancel`/`close`，Reset/Retry → `reload`，Help → `help`；按钮图标统一为 14×14，避免顶到按钮上下边缘；
  - `QInputDialog`：通过 `QDialogButtonBox` 设置 Ok → `apply`、Cancel → `cancel`。
- 新增 `data/themes/default/info.svg`（白色圆形 "i"，与 `help.svg` / `error.svg` 同一套线条）。
- 工作目录提示原本在样式表加载前弹出，现在移到 `LmmsStyle` 应用之后，因此该对话框也使用深色主题。
- **不做**：不逐个修改 57 处 `QMessageBox` 调用点；不改系统原生文件对话框。

**验收**
- 构建通过，12 个测试可执行文件通过。
- 关键截图 `t1-11-dialog-icons.png`：左侧 `QMessageBox`（工作目录询问）为扁平问号 + 扁平 No/Yes，右侧 `QInputDialog`（Set value）为扁平 Cancel/OK。

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

# 3. 字号：主题内不再混用 pt，C++ 不再内联 font-size
grep -nE 'font-size:\s*[0-9]+pt' data/themes/*/style.css
grep -rn 'font-size' src/gui

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

### 截图环境（固定使用 Xephyr，不用 Xvfb）

```bash
# 1. 嵌套 X 服务器，标题 "LMMS UI test"
Xephyr :5 -screen 1600x1000x24 -ac -no-host-grab -title "LMMS UI test" &
# 2. Xephyr 内必须有窗口管理器，否则 :focus 不触发、键盘输入进不去
DISPLAY=:5 matchbox-window-manager \
  -theme /tmp/opencode/rpms/usr/share/themes/Default/matchbox/theme.xml &
# 3. 启动 LMMS
DISPLAY=:5 QT_QPA_PLATFORM=xcb LMMS_DATA_DIR=$PWD/data \
  LMMS_PLUGIN_DIR=$PWD/build-ui/plugins ./build-ui/lmms \
  -c /tmp/opencode/lmmsrc.cfg --geometry 1600x1000+0+0 &
# 4. 在真实桌面上置前 Xephyr 窗口
DISPLAY=:0 xdotool windowactivate "$(DISPLAY=:0 xdotool search --name '^LMMS UI test$' | tail -1)"
# 5. 截图（抓 Xephyr 内部，不是宿主机）
DISPLAY=:5 xwd -root -silent | magick xwd:- shot.png
```

本会话的封装脚本：`/tmp/opencode/lmms-xephyr.sh [--cfg FILE] [--shot FILE]`。

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

## 原建议的 PR 顺序（历史规划）

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
