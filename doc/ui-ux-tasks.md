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
| T1-9 | 默认主题素材矢量化（已完成） | 按素材计 | 低至中 | 平台图标生成与 Qt 5 样式验证 |
| T1-10 | 动画：控件反馈、编辑确认、轻量浮层 | 分项评估 | 低至中 | 按优先级逐项实现 |
| T2-2 | 控件右键菜单加"在自动化编辑器中显示" | 2–3 d | 中（新建 clip 逻辑） | 已实现定位；无匹配时的创建规则待评审 |
| T2-3 | 快捷键映射表（不含 UI） | 2–3 d | 中 | 已覆盖两个编辑器工具栏与主窗口的窗口切换键 |

T1 各项尽量独立提交；T2 需要先讨论。

---

## T1：小 PR（1–3 天）

### T1-2 数值就地编辑（剩余验证）

旋钮、推子和工具栏滑块的就地编辑与单位、范围提示已实现。用户已完成人工桌面检查，具体平台和逐项结果未记录；仍需视发布目标补充其他平台、语言和交互场景的验证。

共享编辑器的 QtTest 已在 Linux Qt 5/X11（Xephyr）下通过 100%／en_US、150%／zh_CN、200%／de_DE 三组缩放与区域设置。覆盖回车、Esc、点击外部、逗号小数、越界、非法输入、提示位置和模型销毁；不替代主窗口内各控件的逐项视觉验收。

同一环境下已用 LMMS 主窗口复测三档缩放：速度框回车与点击外部提交、Esc 保留原值，拍号分子可直接编辑；简中和德语的范围提示已补译并截图确认未裁切。混音通道 LCD 已在真实 Wayland 桌面（XWayland，200% 缩放）复测：双击打开就地编辑器，显示 `Range: 0 to 0`（工程只有一个混音通道），提交后控件本身短暂着色。浮点 LCD 已通过 `LcdFloatSpinBox` 的 QtTest。Microtuner 页面也已在 Linux Qt 5/Xephyr 的 100%、125%、150%、200% 缩放下复测了双击打开、范围提示、回车提交三位小数和 Esc 保留原值。

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

**已迁移的其他默认主题素材**
- `sin_wave_active.png` → `sin_wave_active.svg`，并把 `sin_wave_inactive.svg` 一起改为**一个完整周期的光滑正弦曲线**（描边路径），不再使用 potrace 的块状轮廓。
- `white_noise_wave_active.png`、`white_noise_wave_inactive.png` → SVG：改用可缩放的折线路径。
- `track_op_grip.png` → `track_op_grip.svg`：`TrackGrip` 已使用 `embed::logicalSize()`，尺寸保持 6×25。
- `led_green/red/yellow/off.png` → SVG：`LedCheckBox` 改用 `embed::logicalSize()` 计算布局和文字偏移，LED 尺寸仍为 16×14。
- `recording_playpos_marker.png` → SVG：`TimeLineWidget` 改用 `embed::logicalSize()` 定位和绘制，标记尺寸仍为 17×9。
- 删除未引用资源：`loop_point.png`、`pr_black_key*.png`、`pr_white_key_*.png`（6 个）、`shadow_c.png`、`toolbar_bg.png`、`track_shadow_c.png`、`led_blue.png`。
- `white_key`、`white_key_pressed`、`white_key_disabled` → SVG，保留 10×57 的琴键尺寸。
- `step_btn_*` 的五种步进状态 → 24×24 SVG。Pattern 编辑器按逻辑尺寸计算点击和滚轮区域，并按设备像素比缓存绘制结果。
- `mixer_send_off/on` → SVG，`SEND` 使用内置的 Liberation Sans Bold 字体子集（SIL OFL 1.1），混音器仍以 29×20 的逻辑尺寸显示。
- LFO 的 ×1、×100、÷100 开关各两态，LFO 面板和图表底图，示波器底图，以及 CPU 电平槽和渐变条，共 11 张改为 SVG。倍率与图表字样使用内置的 Oxanium Bold 字体子集（SIL OFL 1.1）。图表和电平仍按逻辑像素布局；CPU 电平按主题的步长裁切，渐变位置固定。
- 10 张 LCD 数字与小数点精灵图改为 SVG，保留原数字轮廓、调色和禁用状态。布局使用逻辑单元尺寸，切片使用栅格化后的源像素坐标。
- 菜单勾选复用 `apply.svg` 和 `apply-selected.svg`，通用树箭头和按钮阴影使用新的 SVG。树箭头保留了 Qt 5 原有的显示尺寸。
- 应用大图标、小图标和启动画面使用 LMMS 官方美术仓库的矢量源（CC0 1.0）。启动窗口仍为 681×573 个逻辑像素。
- 默认主题的 PNG 数量从 59 降到 0。Windows 和 macOS 应用图标仍以平台格式随打包分发，可通过 `python3 buildtools/generate-app-icons.py` 从已有的应用图标 SVG 重新生成。该命令需要 Inkscape 和 icotool。

经典主题仍有自己的 PNG 素材；本轮只处理默认主题。

在 Linux Qt 5 离屏环境的 100%、150%、200% 缩放下，已复测白键三态、步进启停与播放高亮、步进点击和滚轮边界、混音发送开关、LFO 倍率切换与面板底图、图表尺寸以及 CPU 电平裁切。经典主题的同名 PNG 和缺失素材回退也已验证。

LCD 在 Linux Qt 5 离屏环境的 100%、125%、150%、200% 缩放下通过布局、补位、负号、小数点和禁用态检查。10 张素材在 100% 下与原 PNG 的像素和透明度一致。经典主题的 PNG 优先和缺失素材回退也已复测。

菜单勾选、通用树箭头、选中按钮阴影、窗口图标及启动画面均通过同样四档缩放检查。已检查默认主题的暂存安装目录，并在 Linux 上解码验证了 Windows ICO 和 macOS ICNS 的所有尺寸。

---

### T1-10 界面动画（分项推进）

侧栏滑动已实现：光标移入侧栏或关闭按钮时预缓存工作区快照，直到光标离开侧栏区域或工作区画面变化。开合都不在点击时截图；无有效快照时裁剪实时工作区，动画结束时再排版（可见原生子窗口保留正常布局）。只对暴露的背景区域补画。Wayland 可用 `LMMS_SIDEBAR_FRAME_SYNC=1` 试验 Qt 窗口更新调度；默认保留定时器，因为本机测试的长帧更少。侧栏作为性能和交互的对照，不纳入以下待办。新增动画优先用于说明操作结果或状态变化，不为了动效让用户等待。

| 优先级 | 候选 | 建议做法 |
| --- | --- | --- |
| P1 | 工具栏按钮反馈（已实现） | 主工具栏和四个编辑器的常规编辑按钮在悬停和移开时过渡背景色。按下和选中状态即时反馈。 |
| P1 | 编辑确认（片段拖动/放置与就地数值提交已实现） | 片段移动或粘贴成功后短暂着色片段本身；就地数值成功提交后短暂着色控件本身，不额外添加方框。吸附、提交、撤销和 Esc 取消仍即时生效。 |
| P2 | 就地编辑器与范围提示（已实现） | 出现时做约 120 ms 淡入，不移动输入框；键盘焦点立即可用，关闭不等动画；动画关闭时直接显示。 |
| P2 | 浏览器分组展开（已实现） | 展开/折叠时先让树形箭头以约 140 ms 旋转，列表仍立即展开或折叠；不逐帧改变整棵树的行高。动画关闭时直接切换箭头。 |
| P3 | 非模态编辑器窗口切换（已实现） | 内置 `SubWindow` 标题栏在激活/失活时约 160 ms 交叉淡入；不移动窗口、不改层级；原生插件窗口先不做。动画关闭时直接切换。 |

**顺序与验收**

- 主工具栏首批已在默认主题 100% 和 200% 缩放下视觉检查。钢琴卷帘、歌曲、Pattern 和自动化编辑器的常规编辑按钮已在 Linux Qt 5/Xephyr 的 100%、125%、150%、200% 缩放下复测，覆盖快速悬停反向、播放中切换编辑模式、快捷键、动画开关、量化分体按钮和窄工具栏溢出菜单。`ToolButtonTest` 覆盖工具栏和菜单共用同一动作时的触发、选中状态与图标样式同步。
- 编辑确认已在真实 Wayland 桌面（XWayland，200% 缩放）验证：片段拖动后片段本身出现约 200 ms 的着色高亮，就地数值提交后控件本身出现同样高亮；高亮不拦截鼠标，也不添加额外方框，动画关闭时不改变控件。`UiFeedbackTest` 覆盖着色、恢复和动画关闭路径。
- 就地编辑器与范围提示在出现时约 120 ms 淡入，输入框不移动，焦点和键盘输入不等待动画；关闭仍即时。`InlineValueEditorTest` 覆盖淡入和动画关闭路径。
- 文件浏览器分组箭头在展开/折叠时旋转，列表仍立即展开或折叠；默认与经典主题都隐藏静态箭头，由 `FileBrowserTreeWidget` 自绘并遵守动画开关。`FileBrowserTest` 覆盖展开动画和动画关闭路径。
- 内置编辑器窗口切换时标题栏背景交叉淡入约 160 ms；不移动窗口、不改层级，原生插件窗口不参与。`SubWindowTest` 覆盖激活动画和动画关闭路径。
- 按钮反馈和编辑确认分别验收。位移动效默认先快后慢，不为追求帧率改动时长或缓动。
- 优先只更新发生变化的控件，不在动画每帧重排或截图整个工作区。当前和后续新增的界面动效统一使用 `LmmsStyle::uiAnimationDuration()`，由设置 → 常规设置 → 图形用户界面 →「启用界面动画」控制。返回 0 时直接应用最终状态，跳过截图等仅为动画服务的工作；播放头、电平等实时信息不受影响。
- 测首次触发、快速重复、切换后立即关闭，以及播放中操作；检查 100% 至 200% 缩放与 60 Hz／165 Hz 下的点击到首帧和视觉回归。记录帧间隔 P95、最大值及超过两个刷新周期的比例，不以平均值或提交次数代替。能获取呈现反馈时测实际呈现，否则只报告提交间隔。
- 钢琴卷帘在 60 Hz Xephyr 下记录了 Qt 绘制事件，播放中松开按钮到首次绘制的 P95 为 1.4–2.7 ms。悬停绘制间隔 P95 为 16.5–16.9 ms，最大为 17.2 ms，240 个间隔样本均在两个刷新周期内。165 Hz Wayland 屏幕已识别，锁屏期间没采到有效的按钮绘制帧，需在桌面解锁后补测。
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

右键菜单的「在自动化编辑器中显示」已接入歌曲与 Pattern 编辑器。一个已绑定片段时直接打开；多个时在各自时间轴持续高亮，单击片段打开，按 Esc 或单击空白处及无关控件取消。时间轴滚动条、编辑器切换按钮和 Pattern 选择器可用于定位其他高亮片段。匹配也包括已经绑定但尚无控制点的片段。

片段高亮与旋钮编辑确认共用主题着色。等待选择时，片段本身以 1 秒为周期从亮于原色渐变到暗于原色，再恢复到亮相。关闭界面动画时保持静态高亮。

没有匹配片段时，仍需评审创建命令、落点和轨道规则。

### T2-3 快捷键映射表（不含 UI）

把硬编码 `keySequence()` 收进一张 `QMap<QString, QKeySequence>`，配置文件持久化。**不做**可视化编辑器（那是独立项目）。风险：需要迁移所有 `setShortcut` 调用点。

试点覆盖钢琴卷帘的编辑模式、剪贴板和音符修改工具，以及自动化编辑器的编辑模式。关闭 LMMS 后，在配置文件的 `<lmms>` 节点内添加或编辑 `<shortcuts piano_roll_draw="Alt+D" automation_editor_draw="Alt+D"/>`，重启生效。键名见 `src/gui/editors/EditorShortcuts.cpp`；按键使用 Qt PortableText 写法，界面提示按当前平台显示。

第二批覆盖主窗口的 7 个窗口切换动作，例如 `<shortcuts window_song_editor="Alt+1"/>`。工具栏实际绑定与「视图」菜单提示一致，未配置时仍用 Ctrl/Command+1 至 7。窗口切换会检查同组动作、两个试点编辑器的有效绑定，以及文件、编辑菜单的注册快捷键和全屏、分离、附着快捷键。编辑器不能占用窗口切换的默认键，以免窗口组回退时发生冲突。

省略属性、留空或填写无效值会保留该动作的默认键。重复或前缀冲突会让对应组使用整组默认键并记录警告。两个编辑器可复用同一键，按焦点触发。文件、编辑等菜单动作尚未迁移；播放与导航等直接处理的按键也未迁移或纳入冲突检查，配置时需避开。

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
