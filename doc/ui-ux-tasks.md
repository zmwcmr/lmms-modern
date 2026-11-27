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
| T1-9 | 默认主题清单内的 PNG → SVG（已转换，保留 QSS 例外） | 按图标计 | 低 | — |
| T1-10 | 动画：控件反馈、编辑确认、轻量浮层 | 分项评估 | 低至中 | 按优先级逐项实现 |
| T2-2 | 控件右键菜单加"在自动化编辑器中显示" | 2–3 d | 中（新建 clip 逻辑） | 评审 |
| T2-3 | 快捷键映射表（不含 UI） | 2–3 d | 中 | 评审 |

T1 各项尽量独立提交；T2 需要先讨论。

---

## T1：小 PR（1–3 天）

### T1-2 数值就地编辑（剩余验证）

旋钮、推子和工具栏滑块的就地编辑与单位、范围提示已实现。用户已完成人工桌面检查，具体平台和逐项结果未记录；仍需视发布目标补充其他平台、语言和交互场景的验证。

共享编辑器的 QtTest 已在 Linux Qt 5/X11（Xephyr）下通过 100%／en_US、150%／zh_CN、200%／de_DE 三组缩放与区域设置。覆盖回车、Esc、点击外部、逗号小数、越界、非法输入、提示位置和模型销毁；不替代主窗口内各控件的逐项视觉验收。

同一环境下已用 LMMS 主窗口复测三档缩放：速度框回车与点击外部提交、Esc 保留原值，拍号分子可直接编辑；简中和德语的范围提示已补译并截图确认未裁切。混音通道 LCD 已在真实 Wayland 桌面（XWayland，200% 缩放）复测：双击打开就地编辑器，显示 `Range: 0 to 0`（工程只有一个混音通道），提交后控件本身短暂着色。浮点 LCD 已在 `LcdFloatSpinBox` 层通过 QtTest（双击、范围提示、三位小数、提交），仍待 Microtuner 页面和其他平台的桌面视觉验收。

---

### T1-9 剩余 PNG → SVG

**已完成**
- `black_key*.png`、`combobox_bg.png`（第一批）。
- `data/themes/default/horizontal_slider.png`：改为 `horizontal_slider.svg`，
  QSS `border-image` 已验证；SVG 使用 2× 内禀尺寸（20×52），在 200% 下保持清晰。
- `data/themes/default/main_slider.png`：改为 `main_slider.svg`，同样使用 2× 内禀尺寸（52×20）。
- `data/themes/default/sbarrow_*`（8 个）：改为 5×5 SVG，QSS 从 `background-image`
  改为 `image:`，使 Qt 按设备像素比栅格化；100% 下与原 PNG 一致，200% 下清晰。
- `data/themes/default/clip_rec.png`：potrace 转成路径 SVG；`AutomationClipView`
  改用 `embed::logicalSize()` 取高度，避免 SVG 的 HiDPI 过采样影响绘制位置。

- `cursor_select_left/right`：改用 SVG 路径，交给 `QCursor` 前按原逻辑尺寸栅格化，保持原透明区域与热点。
- `envelope_graph`：改用 SVG 渐变和字形路径，不依赖系统字体；保留 182×32 的逻辑最小尺寸。
- `effect_plugin`：改用 214×57 的 SVG 圆角和渐变。经典主题仍优先使用自己的素材。

**未转换（原因）**
- **必须保留**：`apply_css.png`、`apply-selected_css.png`（QSS 菜单指示图标，见 `74bac7745`）；`closed_branch.png`、`open_branch.png` 仅被 `QTreeView::branch` 的 `border-image` 引用。

**验收**：转换后资源仍能加载，在对应控件及不同缩放比例下显示正常。

---

### T1-10 界面动画（分项推进）

侧栏滑动已实现：光标移入侧栏或关闭按钮时预缓存工作区快照，直到光标离开侧栏区域或工作区画面变化。开合都不在点击时截图；无有效快照时裁剪实时工作区，动画结束时再排版（可见原生子窗口保留正常布局）。只对暴露的背景区域补画。Wayland 可用 `LMMS_SIDEBAR_FRAME_SYNC=1` 试验 Qt 窗口更新调度；默认保留定时器，因为本机测试的长帧更少。侧栏作为性能和交互的对照，不纳入以下待办。新增动画优先用于说明操作结果或状态变化，不为了动效让用户等待。

| 优先级 | 候选 | 建议做法 |
| --- | --- | --- |
| P1 | 工具栏按钮反馈（主工具栏常规按钮已实现） | 悬停/移开时短暂过渡背景色；按下和选中状态即时反馈，快速扫过时不排队。菜单按钮保持原行为。 |
| P1 | 编辑确认（片段拖动/放置与就地数值提交已实现） | 片段移动或粘贴成功后短暂着色片段本身；就地数值成功提交后短暂着色控件本身，不额外添加方框。吸附、提交、撤销和 Esc 取消仍即时生效。 |
| P2 | 就地编辑器与范围提示（已实现） | 出现时做约 120 ms 淡入，不移动输入框；键盘焦点立即可用，关闭不等动画；动画关闭时直接显示。 |
| P2 | 浏览器分组展开（已实现） | 展开/折叠时先让树形箭头以约 140 ms 旋转，列表仍立即展开或折叠；不逐帧改变整棵树的行高。动画关闭时直接切换箭头。 |
| P3 | 非模态编辑器窗口切换（已实现） | 内置 `SubWindow` 标题栏在激活/失活时约 160 ms 交叉淡入；不移动窗口、不改层级；原生插件窗口先不做。动画关闭时直接切换。 |

**顺序与验收**

- 主工具栏首批已在默认主题 100% 和 200% 缩放下视觉检查；测试覆盖背景过渡、快速反向、即时按下与选中，以及样式关闭动画。编辑器工具栏仍待评估。编辑确认已在真实 Wayland 桌面（XWayland，200% 缩放）验证：片段拖动后片段本身出现约 200 ms 的着色高亮，就地数值提交后控件本身出现同样高亮；高亮不拦截鼠标，也不添加额外方框，动画关闭时不改变控件。`UiFeedbackTest` 覆盖着色、恢复和动画关闭路径。
- 就地编辑器与范围提示在出现时约 120 ms 淡入，输入框不移动，焦点和键盘输入不等待动画；关闭仍即时。`InlineValueEditorTest` 覆盖淡入和动画关闭路径。
- 文件浏览器分组箭头在展开/折叠时旋转，列表仍立即展开或折叠；默认与经典主题都隐藏静态箭头，由 `FileBrowserTreeWidget` 自绘并遵守动画开关。`FileBrowserTest` 覆盖展开动画和动画关闭路径。
- 内置编辑器窗口切换时标题栏背景交叉淡入约 160 ms；不移动窗口、不改层级，原生插件窗口不参与。`SubWindowTest` 覆盖激活动画和动画关闭路径。
- 先做按钮反馈，再做编辑确认，各自独立提交和验收。位移动效默认先快后慢，不为追求帧率改动时长或缓动。
- 优先只更新发生变化的控件，不在动画每帧重排或截图整个工作区。当前和后续新增的界面动效统一使用 `LmmsStyle::uiAnimationDuration()`，由设置 → 常规设置 → 图形用户界面 →「启用界面动画」控制。返回 0 时直接应用最终状态，跳过截图等仅为动画服务的工作；播放头、电平等实时信息不受影响。
- 测首次触发、快速重复、切换后立即关闭，以及播放中操作；检查 100% 至 200% 缩放与 60 Hz／165 Hz 下的点击到首帧和视觉回归。记录帧间隔 P95、最大值及超过两个刷新周期的比例，不以平均值或提交次数代替。能获取呈现反馈时测实际呈现，否则只报告提交间隔。
- 本期不为播放头、拖拽预览、暂停／静音等实时反馈加延迟；不做全窗口转场，也不动画化轨道或片段布局。

---

## 技术债（下一 ABI 版本）

- `FloatModelEditorBase::paintEvent` 已确认是死代码（所有子类都覆盖它，类也不直接实例化）。
  因该类是导出类，删除导出符号会改变插件 ABI；本期只加注释保留，下一次允许 ABI 变更时
  删除该方法及 `QColor{3,94,97}`。

---

## 后续（可选）

- 下拉交互：关闭的 Qt 下拉框不响应滚轮；设置页的数字框、滑块也把滚轮留给页面。鼠标松开后才展开选择列表，列表选项另点一次。编辑器中 LMMS 自绘下拉框原有的滚轮调参不变。
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
