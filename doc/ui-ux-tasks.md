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
| T1-2 | 数值就地编辑 + 手势说明 | 1 d | 中（行为变化） | 评审 |
| T1-9 | 剩余 PNG → SVG（除 QSS 必需项） | 按图标计 | 低 | — |
| T2-2 | 控件右键菜单加"在自动化编辑器中显示" | 2–3 d | 中（新建 clip 逻辑） | 评审 |
| T2-3 | 快捷键映射表（不含 UI） | 2–3 d | 中 | 评审 |

T1 各项尽量独立提交；T2 需要先讨论。

---

## T1：小 PR（1–3 天）

### T1-2 数值就地编辑（剩余验证）

旋钮、推子和工具栏滑块的就地编辑与单位、范围提示已实现。用户已完成人工桌面检查，具体平台和逐项结果未记录；仍需视发布目标补充其他平台、语言和交互场景的验证。

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

## 技术债（下一 ABI 版本）

- `FloatModelEditorBase::paintEvent` 已确认是死代码（所有子类都覆盖它，类也不直接实例化）。
  因该类是导出类，删除导出符号会改变插件 ABI；本期只加注释保留，下一次允许 ABI 变更时
  删除该方法及 `QColor{3,94,97}`。

---

## 后续（可选）

- 状态栏：主工具栏、各编辑器工具栏按钮和常用菜单项已覆盖；下拉框等非按钮控件
  目前仍只有 tooltip，没有状态栏文案，需要时再逐个补 `setStatusTip`。
- 没有 tooltip 的按钮不会显示状态提示，发现时补 tooltip。

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
