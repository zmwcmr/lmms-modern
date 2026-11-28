# LMMS UI 文案风格指南（附现状审计）

| 项目 | 值 |
| --- | --- |
| 基线 | `feature/ui-ux-improvements` @ `a060904e5` + 本次文案修复 |
| 范围 | `src/gui`、`include`、`plugins`（排除 vendored 的 Carla、LADSPA 子模块等） |
| 方法 | `lupdate-qt5` 提取当前源码的可翻译字符串，脚本按词形做启发式分类，再人工复核 |
| 样本 | 修复前 **2,822** 条；修复后 **2,823** 条（去重后 2,318 条） |

> 注：`data/locale/en.ts` 最后更新于 2026-09-15，已落后于当前源码。本文统计使用
> 当前源码临时生成的 `.ts`，不是仓库里的 `en.ts`。做批量改动前应先重新跑 `lupdate`。

---

## 一、结论

1. **全大写、全小写、首字母大写本身都不是错误**，它们分别承担不同角色：
   - 全大写：设备面板最底层的控件短标签、缩写、LCD 读数；
   - 全小写：单位、符号、文件扩展名、命令名；
   - 首字母大写：句子、菜单命令、普通标签。
2. 修复前的主要问题是**同一个角色在不同位置用了不同写法**，以及少量写法超出了它应有的范围。
   典型如 `Automation Editor` / `Automation editor` / `automation editor`，
   `Piano Roll` / `Piano-Roll` / `piano-roll`，`dB` / `db`，`MIDI` / `Midi`。
3. 采用下面的默认规则：

   > **默认 sentence case；专有名称保持官方拼写；只有设备面板最底层的控件短标签
   > 允许 ALL CAPS；单位按国际符号书写。**

4. 本次已一次性处理核心界面的 阶段 A/B 和结构层问题（见“修复记录”）。
   插件参数名（阶段 C）仍建议按插件分批，不要继续全量重命名；否则已有翻译会大面积退回
   “未完成”状态。

---

## 二、修复前后数据

按启发式分类（单数、专有名词和缩写会影响归类，数字用于看趋势）：

| 范围 | 版本 | 总数 | Sentence case | Title Case | ALL CAPS | 全小写 | 其他 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 核心 GUI（`src/gui` + `include`） | 修复前 | 1,009 | 828（82.1%） | 102（10.1%） | 52（5.2%） | 21（2.1%） | 6 |
| 核心 GUI（`src/gui` + `include`） | 修复后 | 1,008 | 886（87.9%） | 61（6.1%） | 33（3.3%） | 22（2.2%） | 6 |
| 插件（`plugins`） | 修复前 | 1,813 | 1,414（78.0%） | 327（18.0%） | 49（2.7%） | 23（1.3%） | 0 |
| 插件（`plugins`） | 修复后 | 1,815 | 1,432（78.9%） | 317（17.5%） | 49（2.7%） | 17（0.9%） | 0 |

结论：

- **sentence case 已经是绝对主流**，核心 GUI 修复后接近九成。把规则定为 sentence case，
  是顺着现有代码走，而不是逆着改。
- Title Case 在插件里比例更高，但其中很大一部分是参数名、算法名和专有名词，
  不能简单按“首字母大写”一律判错。
- ALL CAPS 绝大多数是旋钮、LCD、面板标题，属于有意为之的硬件面板风格；
  本次只把结构层的全大写（组标题、标签页）改成了 sentence case。

### 修复记录（2026-11-28）

已一次性完成：

- 统一 `Piano Roll`、`Song Editor`、`Pattern Editor`、`Automation Editor`、
  `Project Notes`、`Controller Rack`、`MIDI`、`dB`、`VST sync` 等术语。
- 菜单、窗口标题、对话框标题、工具栏按钮、右键菜单中的普通命令改为 sentence case。
- 设备面板的**组标题和子标签页**改为 sentence case（`EFFECTS CHAIN` → `Effects chain`、
  `FILTER` → `Filter`、`TARGET` / `VOLUME` / `CUTOFF` / `RESO` → `Target` / `Volume` /
  `Cutoff` / `Resonance`），旋钮/LCD 短标签保持 ALL CAPS。
- 插件描述首字母大写，文件过滤器统一 sentence case。
- `can not` → `cannot`，`Project NOT saved.` → `Project not saved.`，
  `Cycle Detected.` → `Cycle detected.`，`NOT FOUND` → `Not found`。
- `1/%1 Bar`、`%1 Bars` 改为 `1/%1 bar`、`%1 bars`。
- 字号收敛为三档：标题 14 DemiBold、正文 12 Regular、辅助 10 Regular；
  主题 QSS 的 `font-size` 在加载时按系统字体缩放，标签页和分组标题改用字重/选中态区分。
- 插件里硬编码的 `Slew Distortion Help`、`Granular Pitch Shifter Help` 和
  `THE PLUGIN'S CPU USAGE...` 改为可翻译的 sentence case。

仍未处理（见阶段 C/D）：

- 插件参数名里的 Title Case（约 300 条），按插件逐个统一。
- `time(s)`、`note(s)`、`octave(s)`、`file(s)` 这类 `(s)` 复数，需要改成 Qt 的 `%n` 复数机制。
- `data/locale/en.ts` 尚未重新生成；翻译需要在 Transifex 里同步。

验证：`cmake --build build-ui` 通过；`FontScaleTest`、`CaptionWidgetsTest`、
`EditorShortcutsTest`、`FileBrowserTest`、`InstrumentTrackTest`、`AutomationTrackTest`、
`MidiClipTest`、`SongAutomationTest`、`LcdFloatSpinBoxTest` 等 QtTest 全部通过。

---

## 三、哪些写法是合理的

### 3.1 设备面板最底层的控件标签

`setLabel()` 的 20 条核心字符串中有 19 条是全大写，例如：

- 旋钮：`VOL`、`PAN`、`PITCH`、`FREQ`、`RESO`、`ATT`、`DEC`、`REL`
- LCD/走带：`TEMPO`、`TIME SIG`、`BAR`、`BEAT`、`TICK`

这是 LMMS 乐器/效果器面板的既有视觉语言，**可以保留**，但要满足两个条件：

1. 只用于最底层的**控件标签**（旋钮、LCD、LED、短按钮），不用于标签页和分组框标题；
2. 只用于短标签或标准缩写，不把完整句子/描述性短语全大写。

原因是大写本身是一种视觉强调。界面层级从高到低是：

```
窗口标题 > 标签页 > 分组框标题 > 控件标签
```

如果标签页写 `Effects`，里面的分组框却写 `EFFECTS CHAIN`，子级比父级更“响”，
层级就倒置了。同理，父标签页写 `Envelope, filter & LFO`，子标签页却写
`TARGET` / `VOLUME` / `CUTOFF` / `RESO`，同一层级的写法也不一致。

因此：**标签页和分组框标题统一用 sentence case；ALL CAPS 只留给最底层的控件标签。**
`EFFECTS CHAIN`、`FILTER`、`STACKING` 这类组标题已改为 `Effects chain`、`Filter`、
`Stacking`；`TARGET` / `VOLUME` / `CUTOFF` / `RESO` 已改为 `Target` / `Volume` /
`Cutoff` / `Resonance`。旋钮上的 `VOL`、`PAN`、`FREQ` 不受影响。

历史上 `EFFECTS CHAIN` 从 2006 年第一版效果器框架（`95be1a963`）就存在，和
`FILTER` 等组标题属于同一套旧式全大写风格；这不是新写错的一条，而是这次已统一掉的
旧层级写法。

### 3.2 单位的国际符号

以下写法按国际符号或既有约定书写，不应按普通单词改大小写：

- `ms`、`s`、`dB`、`Hz`、`kHz`
- `cents`、`semitones`、`octaves`、`degrees`
- `bars`、`beats`、`ticks`
- 文件扩展名：`.mmp`、`.mmpz`、`.wav`
- 命令名：`lmms-vst3-host`

### 3.3 专有名称的 Title Case

以下名称应保持固定拼写，即使出现在句子中间也不改：

- `LMMS`
- `Song Editor`、`Pattern Editor`、`Piano Roll`、`Automation Editor`
- `Mixer`、`Controller Rack`、`Project Notes`
- 插件名：`AudioFileProcessor`、`TripleOscillator`、`ZynAddSubFX` 等
- 算法/曲线名：`Blackman-Harris`、`Cubic Hermite`、`RC Low-pass`、`SV Notch`

### 3.4 句子和普通标签的 sentence case

菜单、工具提示、设置项、消息等已经是 sentence case，例如：

- `Show or hide the Piano Roll`
- `Enable tooltips`
- `Attach and show when closed`
- `The project %1 is now saved.`

这部分是正确方向，应继续沿用。

---

## 四、修复前的不一致（历史记录）

> 以下记录的是 2026-11-28 修复前的状态，保留用于对照；对应问题已在本次修改中处理。
> 行号是修复前的位置，改动后可能已偏移。

### 4.1 同一术语多种写法

| 术语 | 现存写法 | 位置示例 |
| --- | --- | --- |
| Automation Editor | `Automation Editor` / `Automation editor` / `automation editor` | `MainWindow.cpp:1135`、`AutomationClipView.cpp:163`、`GuiApplication.cpp:379` |
| Piano Roll | `Piano Roll` / `Piano-Roll` / `piano roll` / `piano-roll` | `MainWindow.cpp:1129`、`PianoRoll.cpp:5513`、`SetupDialog.cpp:245`、`MidiClipView.cpp:199` |
| Song Editor | `Song Editor` / `Song-Editor` / `Song editor` / `song editor` | `MainWindow.cpp:1119`、`SongEditor.cpp:1038`、`SongEditor.cpp:1049`、`SetupDialog.cpp:462` |
| Pattern Editor | `Pattern Editor` / `pattern editor` | `MainWindow.cpp:1124`、`GuiApplication.cpp:371` |
| Project Notes | `Project Notes` / `project notes` | `MainWindow.cpp:1147`、`MainWindow.cpp:1150` |
| Controller Rack | `Controller Rack` / `controller rack` | `MainWindow.cpp:1142`、`MainWindow.cpp:1145` |
| Mixer Channel | `Mixer Channel` / `mixer channel` | `TrackOperationsWidget.cpp:258`、`MixerChannelView.cpp:334` |
| MIDI | `MIDI` / `Midi` / `midi` | `SlicerTView.cpp:97`、`SlicerTView.cpp:243` |
| dB | `dB` / `db` / `DB` | `CompressorControlDialog.cpp:154` 使用 `db` |
| VST sync | `VST Sync` / `VST sync` | `Vestige.cpp:1123`、`VstEffectControls.cpp:470` |

### 4.2 普通命令用了 Title Case

这些不是专有名称，建议改为 sentence case：

| 当前位置 | 建议 |
| --- | --- |
| `MainWindow.cpp:331` `Save &As...` | `Save &as...` |
| `MainWindow.cpp:334` `Save as New &Version` | `Save as new &version` |
| `MainWindow.cpp:350` `Export &Tracks...` | `Export &tracks...` |
| `MainWindow.cpp:419` `Online Help` | `Online help` |
| `MainWindow.cpp:1643` `Project NOT saved.` | `Project not saved.` |
| `ControllerConnectionDialog.cpp:141` `Connection Settings` | `Connection settings` |
| `MicrotunerConfig.cpp:68` `Microtuner Configuration` | `Microtuner configuration` |
| `VersionedSaveDialog.cpp:73` `Save Options` | `Save options` |
| `ControllerRackView.cpp:126` `Confirm Delete` | `Confirm delete` |
| `ProjectNotes.cpp:114` `Edit Actions` | `Edit actions` |
| `ProjectNotes.cpp:146` `Format Actions` | `Format actions` |
| `SetupDialog.cpp:300` `Default Autoscroll Mode` | `Default autoscroll mode` |
| `Editor.cpp:124` `Toggle Step Recording` | `Toggle step recording` |
| `PianoRoll.cpp:5296` `Reverse Notes` | `Reverse notes` |
| `AutomationEditor.cpp:347` `Edit Value` | `Edit value` |
| `MidiClipView.cpp:225` `Merge Selection` | `Merge selection` |
| `MidiClipView.cpp:253` / `PatternEditor.cpp:344` `Clone Steps` | `Clone steps` |
| `PianoRoll.cpp:232-233` `Note Velocity` / `Note Panning` | `Note velocity` / `Note panning` |
| `PianoRoll.cpp:5197` `Pitch Bend mode` | `Pitch bend mode` |
| `MeterDialog.cpp:44-45` `Meter Numerator` / `Meter Denominator` | `Meter numerator` / `Meter denominator` |
| `EffectView.cpp:70` `Wet Level:` | `Wet level:` |
| `MidiCCRackView.cpp:69` `MIDI CC Knobs:` | `MIDI CC knobs`（组标题不加冒号） |
| `AutomationClipView.cpp:186` `Flip Horizontally (Visible)` | `Flip horizontally (visible)` |
| `VersionedSaveDialog.cpp:189` `Save As Project Bundle (with resources)` | `Save as project bundle (with resources)` |
| `Lv2ProcView.cpp:182` `Reload Plugin` | `Reload plugin` |
| `ControllerConnectionDialog.cpp:185` `Auto Detect` | `Auto detect` |

> 菜单项的 `&` 是快捷键标记，改大小写时保留 `&` 的位置或重新指定助记键。

### 4.3 ALL CAPS 用在了结构层

| 位置 | 现状 | 问题 |
| --- | --- | --- |
| `EffectRackView.cpp:52` | `EFFECTS CHAIN` | 分组框标题，不应全大写 |
| `InstrumentSoundShapingView.cpp:64` | `FILTER` | 分组框标题 |
| `InstrumentFunctionViews.cpp:45/98` | `STACKING` / `ARPEGGIO` | 分组框标题 |
| `InstrumentTuningView.cpp:54/72` | `GLOBAL TRANSPOSITION` / `MICROTUNER` | 分组框标题 |
| `InstrumentMidiIOView.cpp:53/84/161` | `ENABLE MIDI INPUT` / `ENABLE MIDI OUTPUT` / `VELOCITY MAPPING` | 分组框标题 |
| `InstrumentSoundShapingView.cpp:57-59` | `VOLUME` / `CUTOFF` / `RESO` | 子标签页，不应全大写 |
| `ControllerConnectionDialog.cpp:156` | `MIDI CONTROLLER` | 普通对话框的组标题 |
| `ControllerConnectionDialog.cpp:181-182` | `CHANNEL`、`CONTROLLER` | 普通对话框的字段标签 |
| `ControllerConnectionDialog.cpp:212` | `USER CONTROLLER` | 同上 |
| `DummyEffect` | `NOT FOUND` | 错误提示 |

这些应改为 sentence case：`Effects chain`、`Filter`、`Stacking`、`Arpeggio`、
`Global transposition`、`Microtuner`、`Enable MIDI input`、`Enable MIDI output`、
`Velocity mapping`、`Target`、`Volume`、`Cutoff`、`Resonance`、`MIDI controller`、
`Channel`、`Controller`、`User controller`、`Not found`。

### 4.4 全小写超出了单位/命令范围

插件浏览器里的描述目前以小写开头：

- `DynamicsProcessor.cpp:49` `plugin for processing dynamics in a flexible way`
- `LadspaEffect.cpp:58` `plugin for using arbitrary LADSPA-effects ...`
- `Lv2Effect.cpp:47` `plugin for using arbitrary LV2-effects inside LMMS.`
- `Lv2Instrument.cpp:55` `plugin for using arbitrary LV2 instruments inside LMMS.`
- `VstEffect.cpp:49` `plugin for using arbitrary VST effects inside LMMS.`
- `WaveShaper.cpp:45` `plugin for waveshaping`

这些是完整描述，应改为 sentence case：`Plugin for ...`、`Plugin for waveshaping`。

`MainWindow.cpp:1512` 的 `untitled` 用作默认文件名，属于有意的小写，可以保留；
如果它出现在窗口标题或按钮上，则应改为 `Untitled`。

### 4.5 标点、缩写和复数

| 问题 | 现状 | 建议 |
| --- | --- | --- |
| 否定强调 | `Project NOT saved.` | `Project not saved.`；需要强调时用界面颜色/图标，不用大写单词 |
| can not / cannot | `TrackOperationsWidget.cpp:129` 用 `can not`，`MicrotunerConfig` 用 `cannot` | 统一 `cannot` |
| `(s)` 复数 | `time(s)`、`note(s)`、`octave(s)`、`file(s)` | 用 Qt 的 `%n` 复数形式 |
| 单位首字母 | `SongEditor.cpp:393-397` `1/%1 Bar`、`%1 Bars` | `1/%1 bar`、`%1 bars` |
| 组标题冒号 | `MIDI CC Knobs:` | 组标题不加冒号 |
| 菜单省略号 | `Open...`、`Save As...` 用 `...` | 统一 ASCII `...`；只有会弹出对话框的命令才加 |
| 缩写不统一 | `AMT` / `AMNT` / `AMOUNT`，`ATT` / `ATCK` / `ATTACK`，`DEC` / `DCAY` / `DECAY`，`REL` / `RELEASE`，`VELOC` / `VELOCITY` | 每个概念只保留一个缩写，见 5.3 |

---

## 五、统一规则

### 5.1 按界面元素决策

| 界面元素 | 规则 | 示例 |
| --- | --- | --- |
| 菜单栏、右键菜单 | sentence case；不加句号；弹窗命令加 `...` | `Save as new version`、`Export tracks...`、`Open in Piano Roll` |
| 按钮 | sentence case；系统标准按钮保持原样 | `Apply keymap changes`、`Rescan plugins`、`OK`、`Cancel` |
| 窗口/对话框标题 | sentence case；专有窗口名除外 | `Connection settings`、`Automation Editor - no clip` |
| 普通对话框的组标题 | sentence case | `Detached window behavior`、`Save options` |
| 设备面板的标签页、组标题 | sentence case | `Effects`、`Effects chain`、`Filter`、`Target` |
| 设备面板的旋钮/LCD/LED 标签 | 允许 ALL CAPS，仅限短标签或标准缩写 | `VOL`、`PAN`、`FREQ`、`TEMPO`、`TIME SIG` |
| 复选框/单选框 | sentence case | `Enable tooltips`、`Mute automation tracks during solo` |
| 工具提示/状态栏提示 | sentence case；短语不加句号 | `Show or hide the Piano Roll`、`Horizontal zooming` |
| 消息框 | sentence case；完整句子加句号 | `The project was not saved.` |
| 文件过滤器 | sentence case，格式为 `描述 (*.扩展名)` | `MIDI file (*.mid)`、`LMMS project (*.mmp *.mmpz)` |
| 下拉框选项 | sentence case | `Attach and show when closed` |
| 单位 | 按国际符号书写，数值与单位之间留空格 | `-6 dB`、`440 Hz`、`10 ms`、`2 bars` |
| 专有名称 | 官方拼写 | `Piano Roll`、`Song Editor`、`LMMS` |
| 缩写 | 全大写 | `MIDI`、`LFO`、`VST3`、`BPM` |
| 快捷键 | 不放进可翻译文本；按平台显示 | `Ctrl+Z` / `⌘Z` |

### 5.2 固定术语表

| 概念 | 固定写法 | 不要写成 |
| --- | --- | --- |
| 主程序 | `LMMS` | `Lmms`、`lmms`（命令名除外） |
| 歌曲编辑器 | `Song Editor` | `Song-Editor`、`Song editor`、`song editor` |
| Pattern 编辑器 | `Pattern Editor` | `Pattern editor`、`pattern editor` |
| 钢琴卷帘 | `Piano Roll` | `Piano-Roll`、`piano roll`、`piano-roll` |
| 自动化编辑器 | `Automation Editor` | `Automation editor`、`automation editor` |
| 混音器 | `Mixer` | `mixer`（作为窗口名时） |
| 控制器架 | `Controller Rack` | `controller rack` |
| 工程笔记 | `Project Notes` | `project notes`（作为窗口名时） |
| 乐器轨道 | `instrument track` | `Instrument Track`、`instrument-track` |
| 混音通道 | `mixer channel` | `Mixer Channel`、`mixer-channel` |
| 自动化片段 | `automation clip` | `Automation Clip` |
| MIDI | `MIDI` | `Midi`、`midi` |

> 规则：指窗口/编辑器本身时用固定名称；指一般概念时用小写。
> 例如 `Open in Piano Roll`，但 `the piano roll is zoomed` 这种普通叙述可以用小写。
> 为避免争议，界面里优先使用固定名称。

### 5.3 面板缩写表

设备面板最底层的控件标签允许缩写，但同一概念只能有一个写法：

| 概念 | 建议缩写 |
| --- | --- |
| Amount | `AMT` |
| Attack | `ATT` |
| Decay | `DEC` |
| Release | `REL` |
| Sustain | `SUST` |
| Hold | `HOLD` |
| Channel | `CHAN` |
| Velocity | `VEL` |
| Frequency | `FREQ` |
| Resonance | `RESO` |
| Volume | `VOL` |
| Panning | `PAN` |
| Rate | `RATE` |
| Time | `TIME` |
| Phase | `PHASE` |
| Width | `WIDTH` |
| Threshold | `TRSH` |
| Feedback | `FDBK` |
| Delay | `DEL` |
| Speed | `SPD` |

如果完整单词在布局里放得下，优先用完整单词；不要在同一个面板里混用 `AMT` 和 `AMOUNT`。

### 5.4 标点规则

- 菜单、按钮、标签、工具提示：**不加句号**。
- 消息框、错误提示：完整句子**加句号**。
- 只有会打开对话框、需要用户继续输入的命令才加 `...`；切换类命令不加。
- 统一使用 ASCII `...`，不要混用 `…`。
- 标签后接数值时用冒号（`Threshold:`）；组标题、按钮、菜单不加冒号。
- 用 `cannot`，不用 `can not`；用 `%n` 处理复数，不写 `(s)`。
- 快捷键不写进 `tr()`；用 `QKeySequence` 和平台原生显示。

### 5.5 不建议采用的方案

**不建议全站改成 Title Case。** 原因：

- 英语 Title Case 规则本身有争议（介词、连词、短语动词如何处理），译者更容易出错；
- 当前核心 GUI 只有 10% 是 Title Case，改成 Title Case 要动 80% 以上的字符串；
- 长工具提示和消息无法 Title Case，最终仍会是两套规则；
- 对翻译记忆的破坏远大于收益。

---

## 六、字号层级（三档）

### 6.1 设计基准

正文以 **12** 为设计基准，所有档位按应用字体整体缩放，不再各写一套固定像素：

| 层级 | 设计字号 | 字重 | 用途 |
| --- | ---: | --- | --- |
| 标题 | 14 | DemiBold（600） | 窗口标题、对话框内主标题、页面标题 |
| 正文 | 12 | Regular | 按钮、下拉框、设置项、轨道名、片段名 |
| 辅助 | 10 | Regular | 旋钮短标签、单位、次要提示 |

三类来源现在使用同一个比例：

- 普通 Qt 控件继承系统字体；
- 自绘控件使用 `FontHelper` 的 `titleFont()` / `bodyFont()` / `auxiliaryFont()`；
- 主题 QSS 里的 `font-size: Npx` 在加载时按同一比例替换成缩放后的像素。

### 6.2 层级区分

- **标签页**：正文 12；选中项用 DemiBold 和选中底色区分，不再单独设一档字号；标签栏高度跟随字体。
- **分组框标题**：正文 12 + DemiBold，和正文同字号，只靠字重区分。
- **窗口标题、对话框内主标题/页面标题**：标题 14 + DemiBold。
- **按钮、下拉框、设置项、轨道名、片段名**：正文 12。
- **旋钮标签、单位、次要提示**：辅助 10。
- **LCD 标注**（`TEMPO`、`TIME SIG`、`MIN`、`SEC`、`MSEC` 等）：使用辅助 10，随应用字体缩放。
- **缩放例外**：LCD 数字、钢琴键标记、时间线刻度保留自己的设计尺寸。

### 6.3 实现位置

| 位置 | 作用 |
| --- | --- |
| `include/FontHelper.h` | 三档常量、`TextRole`、`titleFont()` / `bodyFont()` / `auxiliaryFont()` / `groupTitleFont()`；缩放上限 2.0 |
| `src/gui/LmmsStyle.cpp` | 加载和重载主题时，把 `font-size: Npx` 统一替换为 `scaledFontSize(N)px` |
| `src/gui/widgets/GroupBox.cpp` | 分组标题使用 `groupTitleFont()` |
| `src/gui/widgets/TabWidget.cpp` | 标签页使用正文 12，选中项 DemiBold，标签栏高度随字体 |
| `src/gui/modals/AboutDialog.cpp` | 主标题使用 `titleFont()`，移除 `.ui` 里的 12pt 固定字号 |
| `src/gui/widgets/LcdWidget.cpp` | LCD 标注使用辅助字号，按字体度量预留宽高 |
| `data/themes/*/style.css` | 正文 12px、辅助 10px；10px 同时用于时间线刻度等例外 |

### 6.4 截图

- `doc/ui-ux-shots/font-scale-100.png`（9 pt，约 100%）
- `doc/ui-ux-shots/font-scale-150.png`（13.5 pt，约 150%）
- `doc/ui-ux-shots/font-scale-200.png`（18 pt，200%）
- `doc/ui-ux-shots/effect-select-200.png`（200% 效果选择对话框）

### 6.5 已知边界

- QSS 在启动时缩放；运行中修改系统字体通常需要重启应用才能重新加载主题字号。
- LCD 数字使用固定尺寸素材。
- 示波器提示和 CPU 标注使用辅助字号，并按实际字宽和字高调整显示空间。
- 效果选择对话框的插件分类按钮使用标准对话框按钮样式和正文 12px，宽度按最宽标签统一计算；`Rescan` 是次级动作，放在插件列表右下方，右边缘对齐列表内容区（不含滚动条），不与分类按钮并列。`QDialogButtonBox` 的 OK/Cancel 统一使用 `apply`/`cancel` SVG 图标。
- 缩放上限为 200%；超过 200% 仍需要进一步的弹性布局工作。
- 插件自绘控件中由几何尺寸推导出的字号（如钢琴键、LCD 数字）属于缩放例外；LCD 标注本身仍归辅助 10。

---

## 七、迁移顺序

### 阶段 A：术语和单位（已完成）

- 统一 `Piano Roll`、`Song Editor`、`Pattern Editor`、`Automation Editor`、
  `Project Notes`、`Controller Rack`。
- `MIDI` / `Midi`、`dB` / `db`、`VST Sync` / `VST sync`。
- 插件描述首字母大写。
- `can not` → `cannot`，`Project NOT saved.` → `Project not saved.`。
- `(s)` 复数仍需逐项检查并改用 Qt 的 `%n` 复数机制；控制器删除确认已使用 `%n`，但源文案仍保留 `connection(s)`。

### 阶段 B：核心界面（已完成）

- 主菜单和右键菜单统一 sentence case。
- 窗口/对话框标题、普通组标题、工具栏按钮统一 sentence case。
- 修正 `Piano-Roll`、`Song-Editor` 这类窗口标题。

### 阶段 C：插件参数名（按插件分批）

- 插件参数名里的 Title Case 数量较大（约 300 条），且包含算法名、专有名词。
- **不要全量重命名**；在插件下次改动时，按本指南统一该插件的参数名。
- 新插件和新参数直接使用 sentence case。

### 阶段 D：检查与防回归

- 给新文案加 review checklist（见 5.1）。
- 可选：增加一个静态检查脚本，至少拦住以下情况：
  - `Midi`、`Vst`、`db`、`HZ`、`MS` 等错误写法；
  - `Piano-Roll`、`Song-Editor`、`piano-roll`、`automation editor` 等旧写法；
  - `tr()` 中出现 `Ctrl+`、`%1+Z` 等快捷键；
  - `file(s)`、`note(s)` 等 `(s)` 复数；
  - 普通对话框中的新 ALL CAPS 字符串。

### 翻译影响

每改一条源字符串，Qt/Transifex 都会把它视为新文案，已有翻译需要重新确认。
因此：

- 阶段 A/B 已在本轮完成；后续按阶段 C/D 分批，不要一次性改几百条；
- 改完先跑 `lupdate`，检查 `en.ts` diff；
- 在 Transifex 里同步术语表，避免译者把 `Piano Roll` 又译回小写；
- 中文、日文等无大小写语言不受规则本身影响，但专有名词和缩写的译法仍需统一。

---

## 八、复现与检查

当前统计的生成方式：

```bash
# 1. 提取当前源码的翻译字符串（需要 Qt 5 的 lupdate）
git ls-files 'src/gui/**' 'plugins/**' 'include/**' \
  | grep -E '\.(cpp|h|ui)$' \
  | grep -vE '^plugins/(CarlaBase/carla|LadspaEffect/(calf|cmt|swh|tap)|FreeBoy/game-music-emu|MidiImport/portsmf|OpulenZ/adplug|Sid/resid|Xpressive/exprtk|ZynAddSubFx/zynaddsubfx)/' \
  > /tmp/ui-files.txt
lupdate-qt5 $(cat /tmp/ui-files.txt) -ts /tmp/ui-current.ts

# 2. 快速抽查
grep -rhoE 'tr\(\s*"[^"]*"' src/gui --include='*.cpp' | wc -l
grep -rnE 'Piano-Roll|Song-Editor|piano-roll|Automation editor|automation editor' src/gui --include='*.cpp'
grep -rnE '"(Midi|Vst|db|HZ|MS)"[^,)]' src/gui plugins --include='*.cpp'
grep -rn 'tr([^)]*Ctrl+' src/gui --include='*.cpp'
grep -rnE 'file\(s\)|note\(s\)|octave\(s\)|time\(s\)' src/gui plugins --include='*.cpp'
```

## 九、主要文件索引

| 主题 | 文件 |
| --- | --- |
| 主窗口菜单/工具栏/状态提示 | `src/gui/MainWindow.cpp` |
| 设置对话框 | `src/gui/modals/SetupDialog.cpp` |
| 片段右键菜单 | `src/gui/clips/*.cpp` |
| 钢琴卷帘/自动化编辑器 | `src/gui/editors/PianoRoll.cpp`、`src/gui/editors/AutomationEditor.cpp` |
| 歌曲编辑器 | `src/gui/editors/SongEditor.cpp` |
| 轨道/混音器 | `src/gui/tracks/*.cpp`、`src/gui/MixerChannelView.cpp` |
| 乐器面板 | `src/gui/instrument/*.cpp` |
| 控制器连接 | `src/gui/modals/ControllerConnectionDialog.cpp` |
| 插件参数名 | `plugins/*/*Controls.cpp`、`plugins/*/*View.cpp` |
