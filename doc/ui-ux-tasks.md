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
| T1-1 | 硬编码颜色主题化（分批） | 1–2 d | 低 | — |
| T1-2 | 数值就地编辑 + 手势说明 | 1 d | 中（行为变化） | 评审 |
| T1-4 | 状态栏提示补全 + 启用 | 1–2 d | 低 | — |
| T1-9 | 剩余 PNG → SVG（除 QSS 必需项） | 按图标计 | 低 | — |
| T2-2 | 控件右键菜单加"在自动化编辑器中显示" | 2–3 d | 中（新建 clip 逻辑） | 评审 |
| T2-3 | 快捷键映射表（不含 UI） | 2–3 d | 中 | 评审 |

T1 各项尽量独立提交；T2 需要先讨论。

---

## 进行中的事项

用户已确认完成真实桌面人工检查；具体平台与逐项结果未记录。

### T1-1 的进展与发现

已完成 `TrackView`：新增 `Q_PROPERTY(QColor trackBackground)`，默认值等于原来的硬编码值，`style.css` 里用
`qproperty-trackBackground: #202428;` 暴露给主题。

`Graph` 十字光标：默认和 classic 主题均通过 `qproperty-pointerColor` 指定原有的 `#707c91`；
未设置该属性的主题沿用原色。WaveShaper 的绘图区域在默认主题和未设置该属性的临时主题下均与修改前逐像素一致；临时主题将光标改为洋红后，
只有十字光标的 402 个像素变化，插件设置的波形颜色保持不变。`Graph` 是导出类，颜色覆盖使用动态属性保存，未增加实例成员。

`Fader` 刻度：主题可分别设置 `qproperty-zeroTickColor` 和 `qproperty-tickColor`，
未设置时保留原有颜色与透明度；动态属性不增加导出类的实例成员。开启推子刻度后，默认主题的推子区域与修改前逐像素一致；
临时主题将刻度设为洋红时，只有刻度位置的 66 个像素变化。

**发现：`FloatModelEditorBase::paintEvent` 在仓库内没有已知调用路径。** 子类 `Knob`、`Fader`、`BarModelEditor`、`Draggable`
各自覆盖了 `paintEvent`，该类在仓库内也没有直接实例化。已撤回对其颜色的改动；
由于该类已导出，外部插件可能继承它，删除方法之前需要评估兼容性。

**待定：`ProjectNotes`。** 它的背景（`#404040`）可以照 `TrackView` 的办法处理，但正文颜色（`#e0e0e0`）
是写进文档字符格式的，改了主题也不会影响已有内容。需要先决定"笔记正文是否应跟随主题文字色"，
再动手。

## T1：小 PR（1–3 天）

### T1-1 硬编码颜色主题化（进行中）

**现象**：`src/gui` 有 36 处硬编码 `QColor`，其中 `TrackView.cpp:63` 让亮色主题不可能实现。

**前提（已具备）**：`LmmsPalette` 已由 QSS 驱动并全局生效（`GuiApplication.cpp:113-120`），绘制代码可直接 `palette().color(...)` 或读 `LmmsStyle::s_palette`。

**分批建议**
1. 已完成：`TrackView` 轨道底色通过 `qproperty-trackBackground` 由 QSS 控制。
2. 待决定：`ProjectNotes` 的正文颜色是否跟随主题，再评估背景和正文的处理方式。
3. 仓库内子类均覆盖 `FloatModelEditorBase::paintEvent`，不应为该实现新增主题属性；该类已导出，删除方法前需评估外部插件继承和 ABI 兼容性。
4. 按用途成批评估剩余颜色：其他图形遮罩和语义色；波形和峰值等颜色按各自用途处理。

`LmmsStyle` 标题栏、MDI 按钮和内边框颜色已接入主题；未配置时沿用原色。
未发现仓库内调用的 `hoverColors` 暂不纳入主题化范围。

**验收**
- 对剩余颜色按主题色、兼容默认值、语义色和透明遮罩分类；不能仅凭颜色字面量数量判断是否完成；
- 在 `default` 与 `classic` 两套主题下，截图验证已纳入范围的颜色随主题变化；笔记正文的预期需先确定，旋钮激活态需先确认实际绘制位置。

**不做**：不在这一步做亮色主题；只保证"主题能控制这些颜色"。`plugins/` 下另有 113 处硬编码颜色，列为后续独立任务（`ui/theme-colors-plugins`），不并入本项。

---

### T1-2 数值就地编辑（剩余验证）

旋钮、推子和工具栏滑块的就地编辑与单位、范围提示已实现。用户已完成人工桌面检查，具体平台和逐项结果未记录；仍需视发布目标补充其他平台、语言和交互场景的验证。

---

### T1-4 状态栏（已实现，暂缓启用）

状态栏和悬停手势提示已实现，但 `MainWindow` 构造里仍调用 `statusBar()->hide()`。暂缓启用的主要原因不是布局，而是**提示覆盖不全**：

- 目前只有三类数值控件在 `enterEvent`/`leaveEvent` 里写入状态栏：`FloatModelEditorBase`（旋钮）、`Fader`、`AutomatableSlider`；另外保存成功后会显示 4 秒消息。
- 主工具栏按钮、常用菜单项、编辑器操作、轨道/混音器按钮等绝大多数动作都没有 `statusTip` 或等价的悬停文案。现在启用的话，状态栏在大部分操作中都是空的，看起来像坏掉的 UI。
- 加载、导出等长任务没有“进行中/完成”消息。

启用前至少需要：

1. 给主工具栏、常用菜单项和文件操作补 `setStatusTip()`（或统一的悬停提示）；
2. 给加载/保存/导出补开始与完成消息；
3. 再移除 `statusBar()->hide()`，并检查主窗口高度与各编辑器布局。

现有手势说明见 `ui-ux-gestures.md`。

---

### T1-9 剩余 PNG → SVG

**未转换（原因）**
- `horizontal_slider`：被 QSS `border-image` 引用（`style.css:598`），需先验证 `border-image` 对 SVG 的缩放行为。
- `cursor_select_left/right`：灰度光标掩码，需在真实桌面验证 `QCursor` 的透明与热点。
- `clip_rec` / `envelope_graph`：含像素字体，SVG `<text>` 依赖系统字体，需先转成路径。
- `effect_plugin`：214×57 圆角渐变 + 半透明边框，比第一批复杂，后续单独处理。
- **必须保留**：`apply_css.png`、`apply-selected_css.png`（QSS 菜单指示图标，见 `74bac7745`）；`closed_branch.png`、`open_branch.png` 仅被 `QTreeView::branch` 的 `border-image` 引用。

**验收**：转换后资源仍能加载，在对应控件及不同缩放比例下显示正常。

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
| 无障碍完整审计（QAccessible 自定义） | 成本高于本期范围 |

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
LMMS_TEST_XEPHYR_PID=$!
# 2. Xephyr 内必须有窗口管理器，否则 :focus 不触发、键盘输入进不去
DISPLAY=:5 matchbox-window-manager \
  -theme /tmp/opencode/rpms/usr/share/themes/Default/matchbox/theme.xml &
LMMS_TEST_WM_PID=$!
# 3. 启动 LMMS
DISPLAY=:5 QT_QPA_PLATFORM=xcb LMMS_DATA_DIR=$PWD/data \
  LMMS_PLUGIN_DIR=$PWD/build-ui/plugins ./build-ui/lmms \
  -c /tmp/opencode/lmmsrc.cfg --geometry 1600x1000+0+0 &
LMMS_TEST_APP_PID=$!
# 4. 在真实桌面上置前 Xephyr 窗口
DISPLAY=:0 xdotool windowactivate "$(DISPLAY=:0 xdotool search --name '^LMMS UI test$' | tail -1)"
# 5. 截图（抓 Xephyr 内部，不是宿主机）
DISPLAY=:5 xwd -root -silent | magick xwd:- shot.png
# 6. 测试结束后只关闭本次启动的进程
kill "$LMMS_TEST_APP_PID" "$LMMS_TEST_WM_PID" "$LMMS_TEST_XEPHYR_PID" 2>/dev/null || true
```
